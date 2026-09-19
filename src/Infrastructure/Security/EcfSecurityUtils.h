#pragma once

#include <optional>
#include <string>

#include "Domain/Entities/ResponseModels.h"

namespace ecf::infra::EcfSecurityUtils {

// DGII SecurityCode: First 6 characters of the Base64 <SignatureValue>, preserving case.
std::string calcularCodigoSeguridad(const std::string& signedXml);

std::string extractSignatureValue(const std::string& signedXml);

// Extracts the <FechaHoraFirma> inner text from the signed XML, if present.
std::optional<std::string> extractFechaHoraFirma(const std::string& signedXml);

std::string buildTimbreUrl(const std::string& baseUrl,
                           const domain::TimbreEcfRequest& req);
std::string buildTimbreFcUrl(const std::string& baseUrl,
                             const domain::TimbreFcRequest& req);

}  // namespace ecf::infra::EcfSecurityUtils

