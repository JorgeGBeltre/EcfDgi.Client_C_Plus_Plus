#pragma once

#include <memory>
#include <string>

#include "Application/Services/EcfValidator.h"
#include "Domain/Entities/EcfClientOptions.h"
#include "Domain/Interfaces/IEcfClient.h"
#include "Domain/Interfaces/IEcfSequenceProvider.h"
#include "Domain/Interfaces/IEcfTransport.h"
#include "Infrastructure/Serialization/EcfXmlSerializer.h"

namespace ecf::infra {

class EcfClient : public domain::IEcfClient {
public:
    // Full construction from options (builds a DgiiDirectTransport internally).
    explicit EcfClient(domain::EcfClientOptions options,
                       std::shared_ptr<domain::IEcfSequenceProvider> sequenceProvider = nullptr);

    // Injectable construction (used by tests / DI).
    EcfClient(domain::EcfClientOptions options,
              std::shared_ptr<domain::IEcfTransport> transport,
              std::shared_ptr<domain::IEcfSequenceProvider> sequenceProvider);

    domain::EcfRecepcionResponse sendEcf(const std::string& xmlContent,
                                         const std::string& fileName) override;
    domain::RfceRecepcionResponse sendRfce(domain::Rfce& rfce) override;
    domain::ConsultaResultadoResponse consultarResultado(const std::string& trackId) override;
    domain::ConsultaEstadoResponse consultarEstado(
        const std::string& rncEmisor, const std::string& eNcf,
        const std::optional<std::string>& rncComprador = std::nullopt,
        const std::optional<std::string>& codigoSeguridad = std::nullopt) override;
    std::vector<domain::TrackIdDetalle> consultarTrackIds(const std::string& rncEmisor,
                                                          const std::string& eNcf) override;
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
    domain::EcfClientOptions options_;
    std::shared_ptr<domain::IEcfTransport> transport_;
    std::shared_ptr<domain::IEcfSequenceProvider> sequenceProvider_;
    app::EcfValidator validator_;
    EcfXmlSerializer serializer_;
};

}  // namespace ecf::infra
