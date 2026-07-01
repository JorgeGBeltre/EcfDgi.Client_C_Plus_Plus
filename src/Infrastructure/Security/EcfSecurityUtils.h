#pragma once

#include <string>

#include "Domain/Entities/ResponseModels.h"

namespace ecf::infra::EcfSecurityUtils {

// SHA-256 of the <SignatureValue>, hex-encoded, first 6 chars (lowercase).
std::string calcularCodigoSeguridad(const std::string& signedXml);

std::string extractSignatureValue(const std::string& signedXml);

std::string buildTimbreUrl(const std::string& baseUrl,
                           const domain::TimbreEcfRequest& req);
std::string buildTimbreFcUrl(const std::string& baseUrl,
                             const domain::TimbreFcRequest& req);

}  // namespace ecf::infra::EcfSecurityUtils
