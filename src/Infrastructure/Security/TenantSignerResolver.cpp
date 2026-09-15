#include "Infrastructure/Security/TenantSignerResolver.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <pqxx/pqxx>
#include <spdlog/spdlog.h>

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

TenantSignerResolver::TenantSignerResolver(std::string connectionString,
                                           std::shared_ptr<domain::IEcfXmlSigner> defaultSigner)
    : connectionString_(std::move(connectionString)),
      defaultSigner_(std::move(defaultSigner)) {}

std::shared_ptr<domain::IEcfXmlSigner> TenantSignerResolver::resolveSigner(const std::string& rnc) {
    if (rnc.empty()) {
        return defaultSigner_;
    }

    std::string cleanRnc = extractDigits(rnc);
    if (cleanRnc.empty()) {
        return defaultSigner_;
    }

    auto now = std::chrono::system_clock::now();

    // 1. Check in-memory cache
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = cache_.find(cleanRnc);
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
            pqxx::result r = n.exec_params(
                "SELECT \"Code\", \"CompanyName\", \"CertificateRawData\", \"CertificatePasswordEncrypted\" "
                "FROM \"Tenants\" "
                "WHERE REPLACE(REPLACE(\"Rnc\", '-', ''), ' ', '') = $1 "
                "  AND \"IsActive\" = true "
                "LIMIT 1;",
                cleanRnc);

            if (!r.empty()) {
                std::string code = r[0]["Code"].is_null() ? "" : r[0]["Code"].as<std::string>();
                std::string companyName = r[0]["CompanyName"].is_null() ? code : r[0]["CompanyName"].as<std::string>();
                std::string password = r[0]["CertificatePasswordEncrypted"].is_null() ? "" : r[0]["CertificatePasswordEncrypted"].as<std::string>();

                if (!r[0]["CertificateRawData"].is_null()) {
                    auto rawData = conn.unesc_bin(r[0]["CertificateRawData"].as<std::string_view>());
                    if (!rawData.empty()) {
                        try {
                            std::vector<unsigned char> bytes(rawData.size());
                            std::memcpy(bytes.data(), rawData.data(), rawData.size());
                            auto signer = std::make_shared<EcfXmlSigner>(bytes, password);
                            spdlog::info("[TenantSignerResolver] Certificado digital cargado desde BD para RNC {} ({})", cleanRnc, companyName);

                            std::lock_guard<std::mutex> lock(mutex_);
                            cache_[cleanRnc] = CachedSigner{signer, now + std::chrono::minutes(5)};
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
    std::vector<std::string> certDirs = {"/app/certificates", "certificates"};
    for (const auto& dir : certDirs) {
        std::string pfxPath = dir + "/" + cleanRnc + ".pfx";
        std::error_code ec;
        if (std::filesystem::exists(pfxPath, ec)) {
            try {
                auto signer = std::make_shared<EcfXmlSigner>(pfxPath, "Fy7g6Q9W");
                spdlog::info("[TenantSignerResolver] Certificado digital cargado desde disco para RNC {} ({})", cleanRnc, pfxPath);

                std::lock_guard<std::mutex> lock(mutex_);
                cache_[cleanRnc] = CachedSigner{signer, now + std::chrono::minutes(5)};
                return signer;
            } catch (const std::exception& ex) {
                spdlog::warn("[TenantSignerResolver] Fallo al cargar certificado en disco {}: {}", pfxPath, ex.what());
            }
        }
    }

    // 4. Safe fallback
    return defaultSigner_;
}

}  // namespace ecf::infra
