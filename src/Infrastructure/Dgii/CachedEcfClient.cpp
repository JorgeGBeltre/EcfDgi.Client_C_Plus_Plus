#include "Infrastructure/Dgii/CachedEcfClient.h"

#include <spdlog/spdlog.h>

namespace ecf::infra {

using namespace ecf::domain;

CachedEcfClient::CachedEcfClient(std::shared_ptr<IEcfClient> innerClient,
                                 std::shared_ptr<ICacheService> cacheService)
    : innerClient_(std::move(innerClient)), cacheService_(std::move(cacheService)) {
    if (!innerClient_) throw std::invalid_argument("innerClient");
    if (!cacheService_) throw std::invalid_argument("cacheService");
}

EcfRecepcionResponse CachedEcfClient::sendEcf(const std::string& xmlContent,
                                              const std::string& fileName) {
    return innerClient_->sendEcf(xmlContent, fileName);
}

RfceRecepcionResponse CachedEcfClient::sendRfce(Rfce& rfce) {
    return innerClient_->sendRfce(rfce);
}

ConsultaResultadoResponse CachedEcfClient::consultarResultado(const std::string& trackId) {
    return innerClient_->consultarResultado(trackId);
}

ConsultaEstadoResponse CachedEcfClient::consultarEstado(
    const std::string& rncEmisor, const std::string& eNcf,
    const std::optional<std::string>& rncComprador,
    const std::optional<std::string>& codigoSeguridad) {
    return innerClient_->consultarEstado(rncEmisor, eNcf, rncComprador, codigoSeguridad);
}

std::vector<TrackIdDetalle> CachedEcfClient::consultarTrackIds(const std::string& rncEmisor,
                                                               const std::string& eNcf) {
    return innerClient_->consultarTrackIds(rncEmisor, eNcf);
}

RfceConsultaResponse CachedEcfClient::consultarRfce(const std::string& rncEmisor,
                                                    const std::string& eNcf,
                                                    const std::string& codigoSeguridad) {
    return innerClient_->consultarRfce(rncEmisor, eNcf, codigoSeguridad);
}

TimbreResponse CachedEcfClient::validarTimbreEcf(const TimbreEcfRequest& request) {
    return innerClient_->validarTimbreEcf(request);
}

TimbreFcResponse CachedEcfClient::validarTimbreFc(const TimbreFcRequest& request) {
    return innerClient_->validarTimbreFc(request);
}

std::vector<DirectorioContribuyente> CachedEcfClient::consultarDirectorio() {
    const std::string cacheKey = "ecf:directory:all";
    if (auto cached = cacheService_->getObject<std::vector<DirectorioContribuyente>>(cacheKey)) {
        if (!cached->empty()) {
            spdlog::debug("Directorio retrieved from cache.");
            return *cached;
        }
    }

    auto result = innerClient_->consultarDirectorio();
    if (!result.empty()) {
        cacheService_->setObject(cacheKey, result, std::chrono::hours(24));
    }
    return result;
}

std::vector<EstatusServicio> CachedEcfClient::consultarEstatusServicios() {
    const std::string cacheKey = "ecf:services:status";
    if (auto cached = cacheService_->getObject<std::vector<EstatusServicio>>(cacheKey)) {
        if (!cached->empty()) {
            spdlog::debug("Estatus de servicios retrieved from cache.");
            return *cached;
        }
    }

    auto result = innerClient_->consultarEstatusServicios();
    if (!result.empty()) {
        cacheService_->setObject(cacheKey, result, std::chrono::minutes(5));
    }
    return result;
}

std::vector<VentanaMantenimiento> CachedEcfClient::consultarVentanasMantenimiento() {
    const std::string cacheKey = "ecf:maintenance:windows";
    if (auto cached = cacheService_->getObject<std::vector<VentanaMantenimiento>>(cacheKey)) {
        if (!cached->empty()) {
            spdlog::debug("Ventanas de mantenimiento retrieved from cache.");
            return *cached;
        }
    }

    auto result = innerClient_->consultarVentanasMantenimiento();
    if (!result.empty()) {
        cacheService_->setObject(cacheKey, result, std::chrono::hours(1));
    }
    return result;
}

std::string CachedEcfClient::verificarEstadoAmbiente(AmbienteEnum ambiente) {
    return innerClient_->verificarEstadoAmbiente(ambiente);
}

AnulacionResponse CachedEcfClient::anularRangos(const std::string& xmlContent) {
    return innerClient_->anularRangos(xmlContent);
}

}  // namespace ecf::infra
