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
        cacheService_->setObject(cacheKey, result, std::chrono::seconds(86400));  // Cache for 24 hours
    }
    return result;
}

DirectorioContribuyente CachedEcfClient::consultarDirectorioPorRnc(const std::string& rnc) {
    const std::string cacheKey = "ecf:directory:" + rnc;
    if (auto cached = cacheService_->getObject<DirectorioContribuyente>(cacheKey)) {
        if (!cached->rnc.empty()) {
            spdlog::debug("Directorio for RNC {} retrieved from cache.", rnc);
            return *cached;
        }
    }

    auto result = innerClient_->consultarDirectorioPorRnc(rnc);
    if (!result.rnc.empty()) {
        cacheService_->setObject(cacheKey, result, std::chrono::seconds(86400));  // Cache for 24 hours
    }
    return result;
}

std::vector<EstatusServicio> CachedEcfClient::consultarEstatusServicios() {
    const std::string cacheKey = "ecf:services:status";
    if (auto cached = cacheService_->getObject<std::vector<EstatusServicio>>(cacheKey)) {
        if (!cached->empty()) {
            spdlog::debug("Estatus servicios retrieved from cache.");
            return *cached;
        }
    }

    auto result = innerClient_->consultarEstatusServicios();
    if (!result.empty()) {
        cacheService_->setObject(cacheKey, result, std::chrono::seconds(300));  // Cache for 5 minutes
    }
    return result;
}

std::vector<VentanaMantenimiento> CachedEcfClient::consultarVentanasMantenimiento() {
    const std::string cacheKey = "ecf:maintenance:windows";
    if (auto cached = cacheService_->getObject<std::vector<VentanaMantenimiento>>(cacheKey)) {
        if (!cached->empty()) {
            spdlog::debug("Ventanas mantenimiento retrieved from cache.");
            return *cached;
        }
    }

    auto result = innerClient_->consultarVentanasMantenimiento();
    if (!result.empty()) {
        cacheService_->setObject(cacheKey, result, std::chrono::seconds(3600));  // Cache for 1 hour
    }
    return result;
}

std::string CachedEcfClient::verificarEstadoAmbiente(AmbienteEnum ambiente) {
    return innerClient_->verificarEstadoAmbiente(ambiente);
}

AnulacionResponse CachedEcfClient::anularRangos(const std::string& xmlContent) {
    return innerClient_->anularRangos(xmlContent);
}

AprobacionComercialResponse CachedEcfClient::sendAprobacionComercial(
    const std::string& xmlContent, const std::string& fileName) {
    return innerClient_->sendAprobacionComercial(xmlContent, fileName);
}

}  // namespace ecf::infra
