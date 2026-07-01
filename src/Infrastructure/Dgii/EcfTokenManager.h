#pragma once
// Handles the DGII challenge-response auth: GET semilla -> sign -> validate ->
// cache the bearer token until ~5 minutes before expiry.

#include <chrono>
#include <memory>
#include <mutex>
#include <string>

#include "Infrastructure/Dgii/EcfEnvironmentConfig.h"
#include "Infrastructure/Security/EcfXmlSigner.h"

namespace ecf::infra {

class EcfTokenManager {
public:
    EcfTokenManager(std::shared_ptr<EcfXmlSigner> signer,
                    EcfEnvironmentConfig config, std::string rncEmisor);

    std::string getToken();

private:
    void renewToken();

    std::shared_ptr<EcfXmlSigner> signer_;
    EcfEnvironmentConfig config_;
    std::string rncEmisor_;

    std::string cachedToken_;
    std::chrono::system_clock::time_point tokenExpiry_{};
    std::mutex renewMutex_;
};

}  // namespace ecf::infra
