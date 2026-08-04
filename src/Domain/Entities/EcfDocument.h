#pragma once

#include <optional>
#include <string>

#include "Domain/Common/AuditableEntity.h"

namespace ecf::domain {

struct EcfDocument : AuditableEntity {
    std::string eNcf;
    std::string rncEmisor;
    std::optional<std::string> rncComprador;
    std::string tenantId = "default-tenant";
    std::string sourceTxnId;
    std::string documentKind = "Invoice";
    std::optional<std::string> ncf;
    std::optional<std::string> trackId;
    std::string state = "Received"; // Received, SequenceAllocated, Signed, SentToDgii, AcceptedByDgii, RejectedByDgii, Uncertain
    double totalAmount = 0;
    double itbisAmount = 0;
    std::optional<std::string> securityCode;
    std::string xmlContent;
    std::optional<std::string> signedXmlContent;
    std::optional<std::string> dgiiResponseXml;
    std::optional<std::string> receiptDate;  // ISO-8601 UTC
};

}  // namespace ecf::domain
