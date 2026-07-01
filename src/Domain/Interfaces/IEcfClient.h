#pragma once

#include <string>
#include <vector>

#include "Domain/Entities/EcfClientOptions.h"
#include "Domain/Entities/ResponseModels.h"
#include "Domain/Entities/Rfce.h"

namespace ecf::domain {

class IEcfClient {
public:
    virtual ~IEcfClient() = default;

    virtual EcfRecepcionResponse sendEcf(const std::string& xmlContent,
                                         const std::string& fileName) = 0;
    virtual RfceRecepcionResponse sendRfce(Rfce& rfce) = 0;
    virtual ConsultaResultadoResponse consultarResultado(const std::string& trackId) = 0;
    virtual ConsultaEstadoResponse consultarEstado(
        const std::string& rncEmisor, const std::string& eNcf,
        const std::optional<std::string>& rncComprador = std::nullopt,
        const std::optional<std::string>& codigoSeguridad = std::nullopt) = 0;
    virtual std::vector<TrackIdDetalle> consultarTrackIds(const std::string& rncEmisor,
                                                          const std::string& eNcf) = 0;
    virtual RfceConsultaResponse consultarRfce(const std::string& rncEmisor,
                                               const std::string& eNcf,
                                               const std::string& codigoSeguridad) = 0;
    virtual TimbreResponse validarTimbreEcf(const TimbreEcfRequest& request) = 0;
    virtual TimbreFcResponse validarTimbreFc(const TimbreFcRequest& request) = 0;
    virtual std::vector<DirectorioContribuyente> consultarDirectorio() = 0;
    virtual std::vector<EstatusServicio> consultarEstatusServicios() = 0;
    virtual std::vector<VentanaMantenimiento> consultarVentanasMantenimiento() = 0;
    virtual std::string verificarEstadoAmbiente(AmbienteEnum ambiente) = 0;
    virtual AnulacionResponse anularRangos(const std::string& xmlContent) = 0;
};

}  // namespace ecf::domain
