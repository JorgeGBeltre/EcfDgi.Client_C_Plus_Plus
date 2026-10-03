#pragma once

#include <string>
#include "Domain/Interfaces/IEcfSequenceManager.h"

namespace ecf::infra {

class EcfSequenceManager : public domain::IEcfSequenceManager {
public:
    explicit EcfSequenceManager(std::string connectionString)
        : connectionString_(std::move(connectionString)) {}

    std::string getNextEncf(const std::string& tenantId, const std::string& tipoComprobante) override;
    void releaseUnusedEncf(const std::string& tenantId, const std::string& tipoComprobante, const std::string& encf) override;

private:
    std::string connectionString_;
};

} // namespace ecf::infra
