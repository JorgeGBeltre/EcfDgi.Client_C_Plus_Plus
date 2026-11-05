#include "Infrastructure/Security/TenantSignerResolver.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/sha.h>
#include <pqxx/pqxx>
#include <spdlog/spdlog.h>
#include <drogon/utils/Utilities.h>

#include "Infrastructure/Security/EcfXmlSigner.h"

namespace ecf::infra {

namespace {

std::string extractDigits(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (std::isdigit(static_cast<unsigned char>(c))) out.push_back(c);
    }
    return out;
}

std::string trimWhitespace(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

bool deriveHkdfSha256(const std::string& secret, const std::string& info, unsigned char outKey[32]) {
    EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
    if (!pctx) return false;
    size_t outLen = 32;
    bool ok = (EVP_PKEY_derive_init(pctx) > 0 &&
               EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha256()) > 0 &&
               EVP_PKEY_CTX_set1_hkdf_salt(pctx, reinterpret_cast<const unsigned char*>(""), 0) > 0 &&
               EVP_PKEY_CTX_set1_hkdf_key(pctx, reinterpret_cast<const unsigned char*>(secret.data()), static_cast<int>(secret.size())) > 0 &&
               EVP_PKEY_CTX_add1_hkdf_info(pctx, reinterpret_cast<const unsigned char*>(info.data()), static_cast<int>(info.size())) > 0 &&
               EVP_PKEY_derive(pctx, outKey, &outLen) > 0 &&
               outLen == 32);
    EVP_PKEY_CTX_free(pctx);
    return ok;
}

bool aes256GcmDecrypt(const unsigned char key[32],
                      const std::string& nonce,
                      const std::string& cipherBytes,
                      const std::string& tag,
                      const std::string& aad,
                      std::string& outPlain) {
    if (nonce.size() != 12 || tag.size() != 16) return false;
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return false;

    outPlain.assign(cipherBytes.size(), '\0');
    int len = 0;
    int plainLen = 0;
    bool ok = false;

    do {
        if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) break;
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(nonce.size()), nullptr) != 1) break;
        if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key, reinterpret_cast<const unsigned char*>(nonce.data())) != 1) break;
        if (!aad.empty()) {
            int aadLen = 0;
            if (EVP_DecryptUpdate(ctx, nullptr, &aadLen,
                                  reinterpret_cast<const unsigned char*>(aad.data()),
                                  static_cast<int>(aad.size())) != 1) {
                break;
            }
        }
        if (!cipherBytes.empty()) {
            if (EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(&outPlain[0]), &len,
                                  reinterpret_cast<const unsigned char*>(cipherBytes.data()),
                                  static_cast<int>(cipherBytes.size())) != 1) {
                break;
            }
            plainLen = len;
        }
        if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, static_cast<int>(tag.size()), const_cast<char*>(tag.data())) != 1) break;
        if (EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(&outPlain[0]) + plainLen, &len) != 1) break;
        plainLen += len;
        outPlain.resize(plainLen);
        ok = true;
    } while (false);

    EVP_CIPHER_CTX_free(ctx);
    return ok;
}

} // namespace

std::string TenantSignerResolver::decryptPasswordIfEncrypted(const std::string& encryptedText,
                                                             const std::string& masterKey,
                                                             const std::string& tenantCode) {
    if (encryptedText.empty()) {
        return "";
    }

    bool isV1 = encryptedText.rfind("enc:v1:", 0) == 0;
    bool isV2 = encryptedText.rfind("enc:v2:", 0) == 0;
    bool isV3 = encryptedText.rfind("enc:v3:", 0) == 0;
    if (!isV1 && !isV2 && !isV3) {
        return encryptedText; // Legacy plaintext
    }

    try {
        std::string payload = encryptedText.substr(7); // skip "enc:vX:"
        std::vector<std::string> parts;
        size_t start = 0;
        while (true) {
            size_t pos = payload.find(':', start);
            if (pos == std::string::npos) {
                parts.push_back(payload.substr(start));
                break;
            }
            parts.push_back(payload.substr(start, pos - start));
            start = pos + 1;
        }

        std::string nonce, cipherBytes, tag;
        bool useHkdfCandidate = (isV2 || isV3);

        if ((isV1 && parts.size() == 3) || (isV2 && parts.size() == 4)) {
            // SaaS-Ecf CryptoEncryptionHelper format:
            // enc:v1:{nonce}:{cipher}:{tag} or enc:v2:{keyId}:{nonce}:{cipher}:{tag}
            size_t offset = isV2 ? 1 : 0;
            nonce = drogon::utils::base64Decode(parts[offset]);
            cipherBytes = drogon::utils::base64Decode(parts[offset + 1]);
            tag = drogon::utils::base64Decode(parts[offset + 2]);
        } else if (parts.size() == 1 || (isV2 && parts.size() == 2)) {
            // Combined envelope format (nonce[12] || tag[16] || cipher[N]):
            // enc:v1:{combinedB64}, enc:v2:{combinedB64} (or enc:v2:{keyId}:{combinedB64}), enc:v3:{combinedB64}
            const std::string& combinedB64 = parts.back();
            std::string combined = drogon::utils::base64Decode(combinedB64);
            if (combined.size() < 28) {
                spdlog::warn("[TenantSignerResolver] Envelope cifrado combinado demasiado corto: {}B", combined.size());
                return "";
            }
            nonce = combined.substr(0, 12);
            tag = combined.substr(12, 16);
            cipherBytes = combined.substr(28);
        } else {
            return "";
        }

        if (nonce.size() != 12 || tag.size() != 16) {
            spdlog::warn("[TenantSignerResolver] Formato de payload cifrado inválido: nonce={}B, tag={}B", nonce.size(), tag.size());
            return "";
        }

        std::vector<std::string> candidateSecrets;
        auto pushSecret = [&](const std::string& s) {
            std::string trimmed = trimWhitespace(s);
            if (!trimmed.empty() && std::find(candidateSecrets.begin(), candidateSecrets.end(), trimmed) == candidateSecrets.end()) {
                candidateSecrets.push_back(trimmed);
            }
        };

        if (!masterKey.empty()) pushSecret(masterKey);
        const char* envNames[] = {
            "CERT_ENCRYPTION_KEY",
            "STORAGE_ENCRYPTION_KEY",
            "Security__CertificateEncryptionKey",
            "JWT_SECRET",
            "JWT_SECRET_KEY",
            "JWT__SecretKey",
            "JwtSettings__Secret",
            "WORKER_SECRET_KEY",
            "WorkerSecretKey"
        };
        for (const char* name : envNames) {
            const char* v = std::getenv(name);
            if (v && std::strlen(v) > 0) pushSecret(v);
        }

        if (candidateSecrets.empty()) {
            spdlog::error("[TenantSignerResolver] Clave de cifrado no configurada (CERT_ENCRYPTION_KEY / STORAGE_ENCRYPTION_KEY / JWT_SECRET requeridos) (SEC-025)");
            return "";
        }

        std::string tenantTag = trimWhitespace(tenantCode);
        if (tenantTag.empty()) tenantTag = "default";
        std::transform(tenantTag.begin(), tenantTag.end(), tenantTag.begin(), ::tolower);
        std::string aadV3 = isV3 ? ("tenant:" + tenantTag + "|field:CertificatePassword|v3") : "";

        for (const auto& secret : candidateSecrets) {
            std::string plainText;
            if (useHkdfCandidate) {
                unsigned char hkdfKey[32];
                if (deriveHkdfSha256(secret, "ecf:certificate-password:v1", hkdfKey)) {
                    if (aes256GcmDecrypt(hkdfKey, nonce, cipherBytes, tag, aadV3, plainText)) {
                        return plainText;
                    }
                    if (isV3 && tenantTag != "default") {
                        std::string defaultAad = "tenant:default|field:CertificatePassword|v3";
                        if (aes256GcmDecrypt(hkdfKey, nonce, cipherBytes, tag, defaultAad, plainText)) {
                            return plainText;
                        }
                    }
                }
            }

            if (!isV3) {
                unsigned char shaKey[SHA256_DIGEST_LENGTH];
                SHA256(reinterpret_cast<const unsigned char*>(secret.data()), secret.size(), shaKey);
                if (aes256GcmDecrypt(shaKey, nonce, cipherBytes, tag, "", plainText)) {
                    return plainText;
                }
            }
        }

        spdlog::warn("[TenantSignerResolver] Error de autenticación/integridad al descifrar contraseña (tag inválido o clave incorrecta) (SEC-026)");
        return "";
    } catch (const std::exception& ex) {
        spdlog::warn("[TenantSignerResolver] Excepción descifrando contraseña: {}", ex.what());
        return "";
    } catch (...) {
        spdlog::warn("[TenantSignerResolver] Excepción desconocida descifrando contraseña");
        return "";
    }
}

TenantSignerResolver::TenantSignerResolver(std::string connectionString,
                                           std::shared_ptr<domain::IEcfXmlSigner> defaultSigner,
                                           std::string masterKey)
    : connectionString_(std::move(connectionString)),
      defaultSigner_(std::move(defaultSigner)),
      masterKey_(std::move(masterKey)) {}

std::shared_ptr<domain::IEcfXmlSigner> TenantSignerResolver::resolveTenantSignerOnly(const std::string& rnc, const std::string& tenantId) {
    if (rnc.empty()) {
        return nullptr;
    }

    std::string cleanRnc = extractDigits(rnc);
    if (cleanRnc.empty()) {
        return nullptr;
    }

    std::string cacheKey = cleanRnc + ":" + tenantId;
    auto now = std::chrono::system_clock::now();

    // 1. Check in-memory cache
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(cacheKey);
        if (it != cache_.end() && it->second.expiresAt > now) {
            return it->second.signer;
        }
    }

    // 2. Try loading from database table "Tenants"
    if (!connectionString_.empty() &&
        connectionString_ != "InMemory" &&
        connectionString_ != "inmemory") {
        try {
            pqxx::connection conn(connectionString_);
            pqxx::nontransaction n(conn);
            
            pqxx::result r;
            if (!tenantId.empty() && tenantId != "default" && tenantId != "default-tenant") {
                // SEC-047: Restringir material criptográfico estrictamente a la empresa autenticada
                r = n.exec_params(
                    "SELECT \"Code\", \"CompanyName\", \"CertificateRawData\", \"CertificatePasswordEncrypted\" "
                    "FROM \"Tenants\" "
                    "WHERE REPLACE(REPLACE(\"Rnc\", '-', ''), ' ', '') = $1 "
                    "  AND (\"Code\" = $2 OR \"Id\"::text = $2) "
                    "  AND \"IsActive\" = true "
                    "LIMIT 1;",
                    cleanRnc, tenantId);
            } else {
                r = n.exec_params(
                    "SELECT \"Code\", \"CompanyName\", \"CertificateRawData\", \"CertificatePasswordEncrypted\" "
                    "FROM \"Tenants\" "
                    "WHERE REPLACE(REPLACE(\"Rnc\", '-', ''), ' ', '') = $1 "
                    "  AND \"IsActive\" = true "
                    "LIMIT 1;",
                    cleanRnc);
            }

            if (!r.empty()) {
                std::string code = r[0]["Code"].is_null() ? "" : r[0]["Code"].as<std::string>();
                std::string companyName = r[0]["CompanyName"].is_null() ? code : r[0]["CompanyName"].as<std::string>();
                std::string rawPassword = r[0]["CertificatePasswordEncrypted"].is_null() ? "" : r[0]["CertificatePasswordEncrypted"].as<std::string>();
                std::string password = decryptPasswordIfEncrypted(rawPassword, masterKey_, code);

                if (!r[0]["CertificateRawData"].is_null()) {
                    auto rawData = conn.unesc_bin(r[0]["CertificateRawData"].as<std::string_view>());
                    if (!rawData.empty()) {
                        try {
                            std::vector<unsigned char> bytes(rawData.size());
                            std::memcpy(bytes.data(), rawData.data(), rawData.size());
                            auto signer = std::make_shared<EcfXmlSigner>(bytes, password);
                            spdlog::info("[TenantSignerResolver] Certificado digital cargado desde BD para RNC {} ({})", cleanRnc, companyName);

                            std::lock_guard<std::mutex> lock(mutex_);
                            cache_[cacheKey] = CachedSigner{signer, now + std::chrono::minutes(5)};
                            return signer;
                        } catch (const std::exception& ex) {
                            spdlog::warn("[TenantSignerResolver] Fallo al instanciar certificado digital para Tenant {} (RNC {}) desde BD: {}",
                                         code, cleanRnc, ex.what());
                        }
                    }
                }
            }
        } catch (const std::exception& ex) {
            spdlog::warn("[TenantSignerResolver] Error consultando tabla Tenants en BD para RNC {}: {}", cleanRnc, ex.what());
        }
    }

    // 3. Check disk (/app/certificates or certificates)
    // SEC-047: Si se indicó tenantId específico, no permitir cross-tenant en disco
    if (tenantId.empty() || tenantId == cleanRnc || tenantId == "default" || tenantId == "default-tenant") {
        const char* envPwd = std::getenv("ECF_CERTIFICATE_PASSWORD");
        std::string diskCertPassword = envPwd ? envPwd : "";

        std::vector<std::string> certDirs = {"/app/certificates", "certificates"};
        for (const auto& dir : certDirs) {
            std::string pfxPath = dir + "/" + cleanRnc + ".pfx";
            std::error_code ec;
            if (std::filesystem::exists(pfxPath, ec)) {
                try {
                    auto signer = std::make_shared<EcfXmlSigner>(pfxPath, diskCertPassword);
                    spdlog::info("[TenantSignerResolver] Certificado digital cargado desde disco para RNC {} ({})", cleanRnc, pfxPath);

                    std::lock_guard<std::mutex> lock(mutex_);
                    cache_[cacheKey] = CachedSigner{signer, now + std::chrono::minutes(5)};
                    return signer;
                } catch (const std::exception& ex) {
                    spdlog::warn("[TenantSignerResolver] Fallo al cargar certificado en disco {}: {}", pfxPath, ex.what());
                }
            }
        }
    }

    return nullptr;
}

std::shared_ptr<domain::IEcfXmlSigner> TenantSignerResolver::resolveSigner(const std::string& rnc) {
    auto signer = resolveTenantSignerOnly(rnc);
    return signer ? signer : defaultSigner_;
}

}  // namespace ecf::infra
