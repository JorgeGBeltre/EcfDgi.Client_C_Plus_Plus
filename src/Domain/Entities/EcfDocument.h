#pragma once

#include <optional>
#include <string>

#include "Domain/Common/AuditableEntity.h"

namespace ecf::domain {

struct EcfDocument : AuditableEntity {
    std::string eNcf;
    std::string rncEmisor;
    std::optional<std::string> rncComprador;
    std::optional<std::string> trackId;
    std::string state;
    double totalAmount = 0;
    double itbisAmount = 0;
    std::optional<std::string> securityCode;
    std::string xmlContent;
    std::optional<std::string> receiptDate;  // ISO-8601 UTC
};

}  // namespace ecf::domain
