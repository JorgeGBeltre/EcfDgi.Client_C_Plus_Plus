#pragma once

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "Domain/Interfaces/ITenantSignerResolver.h"
#include "Domain/Interfaces/IEcfXmlSigner.h"

namespace ecf::infra {

class TenantSignerResolver : public domain::ITenantSignerResolver {
public:
    TenantSignerResolver(std::string connectionString,
                         std::shared_ptr<domain::IEcfXmlSigner> defaultSigner,
                         std::string masterKey = "");
    ~TenantSignerResolver() override = default;

    std::shared_ptr<domain::IEcfXmlSigner> resolveSigner(const std::string& rnc) override;
    std::shared_ptr<domain::IEcfXmlSigner> resolveTenantSignerOnly(const std::string& rnc) override;

    static std::string decryptPasswordIfEncrypted(const std::string& encryptedText, const std::string& masterKey = "");

private:
    struct CachedSigner {
        std::shared_ptr<domain::IEcfXmlSigner> signer;
        std::chrono::system_clock::time_point expiresAt;
    };

    std::string connectionString_;
    std::shared_ptr<domain::IEcfXmlSigner> defaultSigner_;
    std::string masterKey_;
    std::mutex mutex_;
    std::unordered_map<std::string, CachedSigner> cache_;
};

}  // namespace ecf::infra
