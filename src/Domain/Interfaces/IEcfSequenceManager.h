#pragma once

#include <string>

namespace ecf::domain {

class IEcfSequenceManager {
public:
    virtual ~IEcfSequenceManager() = default;

    virtual std::string getNextEncf(const std::string& tenantId, const std::string& tipoComprobante) = 0;
    virtual void releaseUnusedEncf(const std::string& tenantId, const std::string& tipoComprobante, const std::string& encf) = 0;
};

} // namespace ecf::domain
