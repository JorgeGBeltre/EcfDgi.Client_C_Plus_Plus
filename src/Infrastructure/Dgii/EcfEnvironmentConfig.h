#pragma once
// Holds the per-environment DGII endpoint base URLs.

#include <string>

#include "Domain/Entities/EcfClientOptions.h"

namespace ecf::infra {

struct EcfEnvironmentConfig {
    std::string autenticacionUrl;
    std::string recepcionUrl;
    std::string recepcionFcUrl;
    std::string consultaResultadoUrl;
    std::string consultaEstadoUrl;
    std::string consultaTrackIdsUrl;
    std::string consultaRfceUrl;
    std::string aprobacionComercialUrl;
    std::string anulacionRangosUrl;
    std::string directorioUrl;
    std::string timbreUrl;
    std::string timbreFcUrl;
    std::string estatusServiciosUrl = "https://statusecf.dgii.gov.do";

    static EcfEnvironmentConfig getConfig(domain::AmbienteEnum ambiente);
};

}  // namespace ecf::infra
