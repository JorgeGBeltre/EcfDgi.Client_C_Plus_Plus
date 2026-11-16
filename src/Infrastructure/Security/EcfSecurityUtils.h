#pragma once

#include <optional>
#include <string>

#include "Domain/Entities/ResponseModels.h"

namespace ecf::infra::EcfSecurityUtils {

std::string calcularCodigoSeguridad(const std::string& signedXml);

std::string extractSignatureValue(const std::string& signedXml);

std::optional<std::string> extractFechaHoraFirma(const std::string& signedXml);

std::string urlEncode(const std::string& value);

std::string buildTimbreUrl(const std::string& baseUrl,
                           const domain::TimbreEcfRequest& req);
std::string buildTimbreFcUrl(const std::string& baseUrl,
                             const domain::TimbreFcRequest& req);

}
