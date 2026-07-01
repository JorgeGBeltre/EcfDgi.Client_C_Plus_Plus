#include "Infrastructure/EcfClient.h"

#include <memory>
#include <stdexcept>

#include "Domain/Exceptions/EcfException.h"
#include "Infrastructure/Dgii/DgiiDirectTransport.h"
#include "Infrastructure/Dgii/EcfEnvironmentConfig.h"
#include "Infrastructure/Dgii/EcfTokenManager.h"
#include "Infrastructure/Persistence/MemorySequenceProvider.h"
#include "Infrastructure/Security/EcfXmlSigner.h"

namespace ecf::infra {

using namespace ecf::domain;

namespace {

// Maps the SDK environment enum to the DGII AmbienteEnum.
// Environment=Test targets the PreCertificacion (test) endpoints.
AmbienteEnum toAmbiente(EcfEnvironment env) {
    switch (env) {
        case EcfEnvironment::Test: return AmbienteEnum::PreCertificacion;
        case EcfEnvironment::Cert: return AmbienteEnum::Certificacion;
        case EcfEnvironment::Prod: return AmbienteEnum::Produccion;
    }
    return AmbienteEnum::PreCertificacion;
}

}  // namespace

EcfClient::EcfClient(EcfClientOptions options,
                     std::shared_ptr<IEcfSequenceProvider> sequenceProvider)
    : options_(std::move(options)),
      sequenceProvider_(sequenceProvider ? std::move(sequenceProvider)
                                         : std::make_shared<MemorySequenceProvider>()) {
    if (options_.mode != IntegrationMode::DgiiDirect)
        throw std::runtime_error("Only DgiiDirect mode is supported.");

    if (!options_.rncEmisor || options_.rncEmisor->empty())
        throw std::runtime_error("RncEmisor is required for Direct mode.");
    if (!options_.certificatePath || options_.certificatePath->empty())
        throw std::runtime_error("CertificatePath is required for Direct mode.");

    auto signer = std::make_shared<EcfXmlSigner>(
        *options_.certificatePath, options_.certificatePassword.value_or(""));
    auto envConfig = EcfEnvironmentConfig::getConfig(toAmbiente(options_.environment));
    auto tokenManager =
        std::make_shared<EcfTokenManager>(signer, envConfig, *options_.rncEmisor);

    transport_ = std::make_shared<DgiiDirectTransport>(tokenManager, envConfig);
}

EcfClient::EcfClient(EcfClientOptions options,
                     std::shared_ptr<IEcfTransport> transport,
                     std::shared_ptr<IEcfSequenceProvider> sequenceProvider)
    : options_(std::move(options)),
      transport_(std::move(transport)),
      sequenceProvider_(sequenceProvider ? std::move(sequenceProvider)
                                         : std::make_shared<MemorySequenceProvider>()) {
    if (!transport_) throw std::invalid_argument("transport");
}

EcfRecepcionResponse EcfClient::sendEcf(const std::string& xmlContent,
                                        const std::string& fileName) {
    return transport_->sendEcf(xmlContent, fileName);
}

RfceRecepcionResponse EcfClient::sendRfce(Rfce& rfce) {
    auto validation = validator_.validateRfce(rfce);
    if (!validation.isValid())
        throw EcfValidationException(validation.errors());

    const std::string xml = serializer_.serialize(rfce);
    const std::string fileName =
        serializer_.getFileName(rfce.encabezado.emisor.rncEmisor, rfce.encabezado.idDoc.eNcf);

    auto response = transport_->sendRfce(xml, fileName);

    if (options_.autoRetryOnReuseableSequence && response.estado == "Rechazado" &&
        !response.secuenciaUtilizada) {
        const std::string newENcf =
            sequenceProvider_->getNext(rfce.encabezado.emisor.rncEmisor);
        rfce.encabezado.idDoc.eNcf = newENcf;
        return sendRfce(rfce);
    }

    return response;
}

ConsultaResultadoResponse EcfClient::consultarResultado(const std::string& trackId) {
    return transport_->consultarResultado(trackId);
}

ConsultaEstadoResponse EcfClient::consultarEstado(
    const std::string& rncEmisor, const std::string& eNcf,
    const std::optional<std::string>& rncComprador,
    const std::optional<std::string>& codigoSeguridad) {
    return transport_->consultarEstado(
        ConsultaEstadoRequest(rncEmisor, eNcf, rncComprador, codigoSeguridad));
}

std::vector<TrackIdDetalle> EcfClient::consultarTrackIds(const std::string& rncEmisor,
                                                         const std::string& eNcf) {
    return transport_->consultarTrackIds(rncEmisor, eNcf);
}

RfceConsultaResponse EcfClient::consultarRfce(const std::string& rncEmisor,
                                              const std::string& eNcf,
                                              const std::string& codigoSeguridad) {
    return transport_->consultarRfce(rncEmisor, eNcf, codigoSeguridad);
}

TimbreResponse EcfClient::validarTimbreEcf(const TimbreEcfRequest& request) {
    return transport_->consultarTimbre(request);
}

TimbreFcResponse EcfClient::validarTimbreFc(const TimbreFcRequest& request) {
    return transport_->consultarTimbreFc(request);
}

std::vector<DirectorioContribuyente> EcfClient::consultarDirectorio() {
    return transport_->consultarDirectorio();
}

std::vector<EstatusServicio> EcfClient::consultarEstatusServicios() {
    return transport_->consultarEstatusServicios();
}

std::vector<VentanaMantenimiento> EcfClient::consultarVentanasMantenimiento() {
    return transport_->consultarVentanasMantenimiento();
}

std::string EcfClient::verificarEstadoAmbiente(AmbienteEnum ambiente) {
    return transport_->verificarEstadoAmbiente(ambiente);
}

AnulacionResponse EcfClient::anularRangos(const std::string& xmlContent) {
    return transport_->anularRangos(xmlContent);
}

}  // namespace ecf::infra
