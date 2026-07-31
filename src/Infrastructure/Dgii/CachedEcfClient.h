#pragma once

#include <memory>

#include "Domain/Interfaces/ICacheService.h"
#include "Domain/Interfaces/IEcfClient.h"

namespace ecf::infra {

class CachedEcfClient : public domain::IEcfClient {
public:
    CachedEcfClient(std::shared_ptr<domain::IEcfClient> innerClient,
                     std::shared_ptr<domain::ICacheService> cacheService);
    ~CachedEcfClient() override = default;

    domain::EcfRecepcionResponse sendEcf(const std::string& xmlContent,
                                         const std::string& fileName) override;
    domain::RfceRecepcionResponse sendRfce(domain::Rfce& rfce) override;
    domain::ConsultaResultadoResponse consultarResultado(const std::string& trackId) override;
    domain::ConsultaEstadoResponse consultarEstado(
        const std::string& rncEmisor, const std::string& eNcf,
        const std::optional<std::string>& rncComprador = std::nullopt,
        const std::optional<std::string>& codigoSeguridad = std::nullopt) override;
    std::vector<domain::TrackIdDetalle> consultarTrackIds(
        const std::string& rncEmisor, const std::string& eNcf) override;
    domain::RfceConsultaResponse consultarRfce(const std::string& rncEmisor,
                                               const std::string& eNcf,
                                               const std::string& codigoSeguridad) override;
    domain::TimbreResponse validarTimbreEcf(const domain::TimbreEcfRequest& request) override;
    domain::TimbreFcResponse validarTimbreFc(const domain::TimbreFcRequest& request) override;

    std::vector<domain::DirectorioContribuyente> consultarDirectorio() override;
    std::vector<domain::EstatusServicio> consultarEstatusServicios() override;
    std::vector<domain::VentanaMantenimiento> consultarVentanasMantenimiento() override;

    std::string verificarEstadoAmbiente(domain::AmbienteEnum ambiente) override;
    domain::AnulacionResponse anularRangos(const std::string& xmlContent) override;

private:
    std::shared_ptr<domain::IEcfClient> innerClient_;
    std::shared_ptr<domain::ICacheService> cacheService_;
};

}  // namespace ecf::infra
