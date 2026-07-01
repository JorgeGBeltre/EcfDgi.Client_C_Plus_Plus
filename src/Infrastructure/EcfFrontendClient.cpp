#include "Infrastructure/EcfFrontendClient.h"

#include <stdexcept>

#include "Infrastructure/Dgii/DgiiDirectTransport.h"
#include "Infrastructure/Dgii/EcfEnvironmentConfig.h"
#include "Infrastructure/Dgii/EcfTokenManager.h"
#include "Infrastructure/Security/EcfXmlSigner.h"

namespace ecf::infra {

using namespace ecf::domain;

namespace {
AmbienteEnum toAmbiente(EcfEnvironment env) {
    switch (env) {
        case EcfEnvironment::Test: return AmbienteEnum::PreCertificacion;
        case EcfEnvironment::Cert: return AmbienteEnum::Certificacion;
        case EcfEnvironment::Prod: return AmbienteEnum::Produccion;
    }
    return AmbienteEnum::PreCertificacion;
}
}  // namespace

EcfFrontendClient::EcfFrontendClient(EcfClientOptions options,
                                     std::shared_ptr<IEcfTransport> transport) {
    if (transport) {
        transport_ = std::move(transport);
        return;
    }

    if (options.mode != IntegrationMode::DgiiDirect)
        throw std::runtime_error("Only DgiiDirect mode is supported.");

    auto envConfig = EcfEnvironmentConfig::getConfig(toAmbiente(options.environment));

    std::shared_ptr<EcfTokenManager> tokenManager;  // may stay null (queries w/o token)
    if (options.certificatePath && !options.certificatePath->empty() &&
        options.rncEmisor && !options.rncEmisor->empty()) {
        auto signer = std::make_shared<EcfXmlSigner>(
            *options.certificatePath, options.certificatePassword.value_or(""));
        tokenManager =
            std::make_shared<EcfTokenManager>(signer, envConfig, *options.rncEmisor);
    }

    transport_ = std::make_shared<DgiiDirectTransport>(tokenManager, envConfig);
}

ConsultaEstadoResponse EcfFrontendClient::consultarEstado(
    const std::string& rncEmisor, const std::string& eNcf,
    const std::optional<std::string>& rncComprador,
    const std::optional<std::string>& codigoSeguridad) {
    return transport_->consultarEstado(
        ConsultaEstadoRequest(rncEmisor, eNcf, rncComprador, codigoSeguridad));
}

RfceConsultaResponse EcfFrontendClient::consultarRfce(const std::string& rncEmisor,
                                                      const std::string& eNcf,
                                                      const std::string& codigoSeguridad) {
    return transport_->consultarRfce(rncEmisor, eNcf, codigoSeguridad);
}

TimbreResponse EcfFrontendClient::validarTimbreEcf(const TimbreEcfRequest& request) {
    return transport_->consultarTimbre(request);
}

TimbreFcResponse EcfFrontendClient::validarTimbreFc(const TimbreFcRequest& request) {
    return transport_->consultarTimbreFc(request);
}

}  // namespace ecf::infra
