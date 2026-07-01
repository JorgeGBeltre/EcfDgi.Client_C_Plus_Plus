#pragma once
// Talks to the live DGII REST endpoints (bearer token from EcfTokenManager).

#include <memory>
#include <string>
#include <vector>

#include "Domain/Interfaces/IEcfTransport.h"
#include "Infrastructure/Dgii/EcfEnvironmentConfig.h"
#include "Infrastructure/Dgii/EcfTokenManager.h"
#include "Infrastructure/Serialization/EcfXmlSerializer.h"

namespace ecf::infra {

class DgiiDirectTransport : public domain::IEcfTransport {
public:
    DgiiDirectTransport(std::shared_ptr<EcfTokenManager> tokenManager,
                        EcfEnvironmentConfig config);

    domain::EcfRecepcionResponse sendEcf(const std::string& xmlContent,
                                         const std::string& fileName) override;
    domain::RfceRecepcionResponse sendRfce(const std::string& xmlContent,
                                           const std::string& fileName) override;
    domain::ConsultaResultadoResponse consultarResultado(const std::string& trackId) override;
    domain::ConsultaEstadoResponse consultarEstado(
        const domain::ConsultaEstadoRequest& req) override;
    std::vector<domain::TrackIdDetalle> consultarTrackIds(const std::string& rncEmisor,
                                                          const std::string& eNcf) override;
    domain::RfceConsultaResponse consultarRfce(const std::string& rncEmisor,
                                               const std::string& eNcf,
                                               const std::string& codigoSeguridad) override;
    domain::AprobacionComercialResponse sendAprobacionComercial(
        const std::string& xmlContent, const std::string& fileName) override;
    domain::AnulacionResponse anularRangos(const std::string& xmlContent) override;
    std::vector<domain::DirectorioContribuyente> consultarDirectorio() override;
    domain::DirectorioContribuyente consultarDirectorioPorRnc(const std::string& rnc) override;
    domain::TimbreResponse consultarTimbre(const domain::TimbreEcfRequest& req) override;
    domain::TimbreFcResponse consultarTimbreFc(const domain::TimbreFcRequest& req) override;
    std::vector<domain::EstatusServicio> consultarEstatusServicios() override;
    std::vector<domain::VentanaMantenimiento> consultarVentanasMantenimiento() override;
    std::string verificarEstadoAmbiente(domain::AmbienteEnum ambiente) override;

private:
    // Fetches the bearer token; throws if no token manager was supplied
    // (e.g. EcfFrontendClient built without a signing certificate).
    std::string getAuthToken();

    std::shared_ptr<EcfTokenManager> tokenManager_;
    EcfEnvironmentConfig config_;
    EcfXmlSerializer xmlSerializer_;
};

}  // namespace ecf::infra
