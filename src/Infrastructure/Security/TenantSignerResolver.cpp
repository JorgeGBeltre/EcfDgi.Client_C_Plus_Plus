#include "Infrastructure/Security/TenantSignerResolver.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <openssl/evp.h>
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

} // namespace

std::string TenantSignerResolver::decryptPasswordIfEncrypted(const std::string& encryptedText, const std::string& masterKey) {
    if (encryptedText.empty()) {
        return "";
    }

    bool isV1 = encryptedText.rfind("enc:v1:", 0) == 0;
    bool isV2 = encryptedText.rfind("enc:v2:", 0) == 0;
    if (!isV1 && !isV2) {
        return encryptedText; // Legacy plaintext
    }

    try {
        std::string nonceB64, cipherB64, tagB64;
        if (isV1) {
            std::string payload = encryptedText.substr(7); // skip "enc:v1:"
            size_t pos1 = payload.find(':');
            if (pos1 == std::string::npos) return "";
            size_t pos2 = payload.find(':', pos1 + 1);
            if (pos2 == std::string::npos) return "";
            if (payload.find(':', pos2 + 1) != std::string::npos) return "";

            nonceB64 = payload.substr(0, pos1);
            cipherB64 = payload.substr(pos1 + 1, pos2 - pos1 - 1);
            tagB64 = payload.substr(pos2 + 1);
        } else {
            // enc:v2:{keyId}:{nonce}:{cipher}:{tag}
            std::string payload = encryptedText.substr(7); // skip "enc:v2:"
            size_t pos1 = payload.find(':'); // after keyId
            if (pos1 == std::string::npos) return "";
            size_t pos2 = payload.find(':', pos1 + 1); // after nonce
            if (pos2 == std::string::npos) return "";
            size_t pos3 = payload.find(':', pos2 + 1); // after cipher
            if (pos3 == std::string::npos) return "";
            if (payload.find(':', pos3 + 1) != std::string::npos) return "";

            nonceB64 = payload.substr(pos1 + 1, pos2 - pos1 - 1);
            cipherB64 = payload.substr(pos2 + 1, pos3 - pos2 - 1);
            tagB64 = payload.substr(pos3 + 1);
        }

        std::string nonce = drogon::utils::base64Decode(nonceB64);
        std::string cipherBytes = drogon::utils::base64Decode(cipherB64);
        std::string tag = drogon::utils::base64Decode(tagB64);

        if (nonce.size() != 12 || tag.size() != 16) {
            spdlog::warn("[TenantSignerResolver] Formato de payload cifrado inválido: nonce={}B, tag={}B", nonce.size(), tag.size());
            return "";
        }

        std::string rawKey = !masterKey.empty() ? masterKey : "";
        if (rawKey.empty()) {
            const char* envCert = std::getenv("CERT_ENCRYPTION_KEY");
            if (envCert && std::strlen(envCert) > 0) {
                rawKey = envCert;
            }
        }
        if (rawKey.empty()) {
            const char* envJwt = std::getenv("JWT_SECRET");
            if (envJwt && std::strlen(envJwt) > 0) {
                rawKey = envJwt;
            }
        }
        if (rawKey.empty()) {
            spdlog::error("[TenantSignerResolver] Clave de cifrado no configurada (CERT_ENCRYPTION_KEY o JWT_SECRET requeridos) (SEC-025)");
            return "";
        }

        unsigned char key[SHA256_DIGEST_LENGTH];
        SHA256(reinterpret_cast<const unsigned char*>(rawKey.data()), rawKey.size(), key);

        EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
        if (!ctx) return "";

        std::string plainText;
        plainText.resize(cipherBytes.size());
        int len = 0;
        int plainLen = 0;
        bool ok = false;

        do {
            if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) break;
            if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(nonce.size()), nullptr) != 1) break;
            if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key, reinterpret_cast<const unsigned char*>(nonce.data())) != 1) break;
            if (!cipherBytes.empty()) {
                if (EVP_DecryptUpdate(ctx, reinterpret_cast<unsigned char*>(&plainText[0]), &len,
                                      reinterpret_cast<const unsigned char*>(cipherBytes.data()), static_cast<int>(cipherBytes.size())) != 1) {
                    break;
                }
                plainLen = len;
            }
            if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, static_cast<int>(tag.size()), const_cast<char*>(tag.data())) != 1) break;
            if (EVP_DecryptFinal_ex(ctx, reinterpret_cast<unsigned char*>(&plainText[0]) + plainLen, &len) != 1) break;
            plainLen += len;
            plainText.resize(plainLen);
            ok = true;
        } while (false);

        EVP_CIPHER_CTX_free(ctx);

        if (ok) {
            return plainText;
        } else {
            spdlog::warn("[TenantSignerResolver] Error de autenticación/integridad al descifrar contraseña (tag inválido o clave incorrecta) (SEC-026)");
            return "";
        }
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
                std::string password = decryptPasswordIfEncrypted(rawPassword, masterKey_);

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
