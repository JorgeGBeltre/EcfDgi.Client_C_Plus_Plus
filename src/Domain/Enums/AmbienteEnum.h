#pragma once

#include "Domain/Entities/EcfClientOptions.h"
#include <string>
#include <algorithm>
#include <cctype>

namespace ecf::domain {

inline bool tryResolveAmbienteEnum(const std::string& rawEnv, AmbienteEnum& outAmb) {
    if (rawEnv.empty()) return false;
    std::string lower = rawEnv;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    while (!lower.empty() && std::isspace(static_cast<unsigned char>(lower.front()))) lower.erase(lower.begin());
    while (!lower.empty() && std::isspace(static_cast<unsigned char>(lower.back()))) lower.pop_back();

    if (lower == "test" || lower == "testecf" || lower.find("precert") != std::string::npos || lower == "1") {
        outAmb = AmbienteEnum::PreCertificacion;
        return true;
    }
    if (lower == "cert" || lower == "certecf" || lower.find("certific") != std::string::npos || lower.find("homolog") != std::string::npos || lower == "3") {
        outAmb = AmbienteEnum::Certificacion;
        return true;
    }
    if (lower == "prod" || lower == "prd" || lower == "production" || lower == "ecf" || lower.find("producc") != std::string::npos || lower == "2") {
        outAmb = AmbienteEnum::Produccion;
        return true;
    }
    return false;
}

inline AmbienteEnum resolveAmbienteEnum(const std::string& rawEnv, AmbienteEnum defaultAmbiente) {
    AmbienteEnum resolved;
    if (tryResolveAmbienteEnum(rawEnv, resolved)) {
        return resolved;
    }
    return defaultAmbiente;
}

} // namespace ecf::domain
