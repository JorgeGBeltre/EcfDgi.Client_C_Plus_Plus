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
    std::string editSequence;
    std::string documentKind = "Invoice";
    std::optional<std::string> ncf;
    std::optional<std::string> trackId;
    std::string state = "Received"; // Received, SequenceAllocated, AwaitingTransmission, Signed, AcceptedByDgii, RejectedByDgii, Uncertain, Unsigned, RequiresManualReview
    double totalAmount = 0;
    double itbisAmount = 0;
    std::optional<std::string> securityCode;
    std::string xmlContent;
    std::optional<std::string> signedXmlContent;
    std::optional<std::string> dgiiResponseXml;
    std::optional<std::string> receiptDate;  // ISO-8601 UTC

    // Post-send status polling (EcfStatusReconciler)
    std::optional<std::string> sentToDgiiAt;
    std::optional<std::string> lastStatusCheckAt;
    int statusCheckAttempts = 0;

    // Multi-environment support (PreCertificacion, Certificacion, Produccion)
    std::optional<std::string> ambiente;
};

}  // namespace ecf::domain
