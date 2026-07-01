#pragma once
// Read/write access to the DGII REST endpoints. Operations are exposed as
// synchronous, blocking calls.

#include <string>
#include <vector>

#include "Domain/Entities/EcfClientOptions.h"
#include "Domain/Entities/ResponseModels.h"

namespace ecf::domain {

class IEcfTransport {
public:
    virtual ~IEcfTransport() = default;

    virtual EcfRecepcionResponse sendEcf(const std::string& xmlContent,
                                         const std::string& fileName) = 0;
    virtual RfceRecepcionResponse sendRfce(const std::string& xmlContent,
                                           const std::string& fileName) = 0;
    virtual ConsultaResultadoResponse consultarResultado(const std::string& trackId) = 0;
    virtual ConsultaEstadoResponse consultarEstado(const ConsultaEstadoRequest& req) = 0;
    virtual std::vector<TrackIdDetalle> consultarTrackIds(const std::string& rncEmisor,
                                                          const std::string& eNcf) = 0;
    virtual RfceConsultaResponse consultarRfce(const std::string& rncEmisor,
                                               const std::string& eNcf,
                                               const std::string& codigoSeguridad) = 0;
    virtual AprobacionComercialResponse sendAprobacionComercial(
        const std::string& xmlContent, const std::string& fileName) = 0;
    virtual AnulacionResponse anularRangos(const std::string& xmlContent) = 0;
    virtual std::vector<DirectorioContribuyente> consultarDirectorio() = 0;
    virtual DirectorioContribuyente consultarDirectorioPorRnc(const std::string& rnc) = 0;
    virtual TimbreResponse consultarTimbre(const TimbreEcfRequest& req) = 0;
    virtual TimbreFcResponse consultarTimbreFc(const TimbreFcRequest& req) = 0;
    virtual std::vector<EstatusServicio> consultarEstatusServicios() = 0;
    virtual std::vector<VentanaMantenimiento> consultarVentanasMantenimiento() = 0;
    virtual std::string verificarEstadoAmbiente(AmbienteEnum ambiente) = 0;
};

}  // namespace ecf::domain
