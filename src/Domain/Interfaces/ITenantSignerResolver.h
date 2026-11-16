#pragma once

#include <memory>
#include <string>

#include "Domain/Interfaces/IEcfXmlSigner.h"

namespace ecf::domain {

class ITenantSignerResolver {
public:
    virtual ~ITenantSignerResolver() = default;

    virtual std::shared_ptr<IEcfXmlSigner> resolveSigner(const std::string& rnc) = 0;

    virtual std::shared_ptr<IEcfXmlSigner> resolveTenantSignerOnly(const std::string& rnc, const std::string& tenantId = "") = 0;
};

}
