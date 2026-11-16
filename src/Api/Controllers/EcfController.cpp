#include "Api/Controllers/EcfController.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <exception>
#include <mutex>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include "Api/AppServices.h"
#include "Api/JsonMapping.h"
#include "Application/Common/Behaviors/Behaviors.h"
#include "Application/Ecf/EcfHandlers.h"
#include "Api/Security/IdempotencyHandler.h"
#include "Infrastructure/Dgii/EcfEnvironmentConfig.h"
#include "Shared/Common/Sys.h"

using namespace drogon;

namespace ecf::api {

namespace {
HttpResponsePtr json(const Json::Value& body, HttpStatusCode code) {
    auto resp = HttpResponse::newHttpJsonResponse(body);
    resp->setStatusCode(code);
    return resp;
}
Json::Value err(const std::string& message) {
    Json::Value v;
    v["error"] = message;
    return v;
}
}

void EcfController::send(const HttpRequestPtr& req,
                         std::function<void(const HttpResponsePtr&)>&& callback) {
    std::string tenantId = "default-tenant";
    if (req->attributes()->find("tenantId")) {
        tenantId = req->attributes()->get<std::string>("tenantId");
    }
    std::string workerKeyId = "default-worker";
    if (req->attributes()->find("workerKeyId")) {
        workerKeyId = req->attributes()->get<std::string>("workerKeyId");
    }

    auto& services = AppServices::instance();
    IdempotencyHandler::handle(
        services.idempotencyStore(),
        req,
        tenantId,
        workerKeyId,
        std::move(callback),
        [req, &services](std::function<void(const HttpResponsePtr&)>&& cb) {
            auto body = req->getJsonObject();
            if (!body || !body->isObject()) { cb(json(err("Invalid JSON body."), k400BadRequest)); return; }

            if (body->isMember("xmlContent") && !(*body)["xmlContent"].isString()) {
                cb(json(err("Field 'xmlContent' must be a string."), k400BadRequest));
                return;
            }
            if (body->isMember("fileName") && !(*body)["fileName"].isString()) {
                cb(json(err("Field 'fileName' must be a string."), k400BadRequest));
                return;
            }
            if (body->isMember("rncEmisor") && !(*body)["rncEmisor"].isString()) {
                cb(json(err("Field 'rncEmisor' must be a string."), k400BadRequest));
                return;
            }
            if (body->isMember("eNcf") && !(*body)["eNcf"].isString()) {
                cb(json(err("Field 'eNcf' must be a string."), k400BadRequest));
                return;
            }
            if (body->isMember("rncComprador") && !(*body)["rncComprador"].isString()) {
                cb(json(err("Field 'rncComprador' must be a string."), k400BadRequest));
                return;
            }
            if (body->isMember("totalAmount") && !(*body)["totalAmount"].isNumeric()) {
                cb(json(err("Field 'totalAmount' must be a numeric value."), k400BadRequest));
                return;
            }
            if (body->isMember("itbisAmount") && !(*body)["itbisAmount"].isNumeric()) {
                cb(json(err("Field 'itbisAmount' must be a numeric value."), k400BadRequest));
                return;
            }

            app::SendEcfCommand cmd;
            cmd.xmlContent = body->isMember("xmlContent") ? (*body)["xmlContent"].asString() : "";
            cmd.fileName = body->isMember("fileName") ? (*body)["fileName"].asString() : "";
            cmd.rncEmisor = body->isMember("rncEmisor") ? (*body)["rncEmisor"].asString() : "";
            cmd.eNcf = body->isMember("eNcf") ? (*body)["eNcf"].asString() : "";
            if (body->isMember("rncComprador"))
                cmd.rncComprador = (*body)["rncComprador"].asString();
            cmd.totalAmount = body->isMember("totalAmount") ? (*body)["totalAmount"].asDouble() : 0.0;
            cmd.itbisAmount = body->isMember("itbisAmount") ? (*body)["itbisAmount"].asDouble() : 0.0;

            app::LoggingScope _log("SendEcfCommand");
            app::validateOrThrow(cmd);

            try {
                auto scope = services.makeScope(mapping::currentUserFrom(req));
                app::SendEcfCommandHandler handler(services.ecfClient(), scope.docs, scope.uow);
                auto result = handler.handle(cmd);
                if (result.isFailure()) { cb(json(err(*result.error()), k400BadRequest)); return; }
                cb(json(mapping::toJson(*result.value()), k200OK));
            } catch (const std::exception& ex) {
                cb(json(err(std::string("e-CF service unavailable: ") + ex.what()),
                              k500InternalServerError));
            }
        }
    );
}

void EcfController::sendRfce(const HttpRequestPtr& req,
                             std::function<void(const HttpResponsePtr&)>&& callback) {
    std::string tenantId = "default-tenant";
    if (req->attributes()->find("tenantId")) {
        tenantId = req->attributes()->get<std::string>("tenantId");
    }
    std::string workerKeyId = "default-worker";
    if (req->attributes()->find("workerKeyId")) {
        workerKeyId = req->attributes()->get<std::string>("workerKeyId");
    }

    auto& services = AppServices::instance();
    IdempotencyHandler::handle(
        services.idempotencyStore(),
        req,
        tenantId,
        workerKeyId,
        std::move(callback),
        [req, &services](std::function<void(const HttpResponsePtr&)>&& cb) {
            auto body = req->getJsonObject();
            if (!body || !body->isObject()) { cb(json(err("Invalid JSON body."), k400BadRequest)); return; }

            app::SendRfceCommand cmd;
            app::LoggingScope _log("SendRfceCommand");

            const Json::Value& rfceJson =
                body->isMember("rfceModel") ? (*body)["rfceModel"] : *body;
            if (!rfceJson.isObject()) {
                cb(json(err("Field 'rfceModel' must be a valid JSON object."), k400BadRequest));
                return;
            }

            try {
                cmd.rfceModel = mapping::rfceFromJson(rfceJson);
            } catch (const std::exception& ex) {
                cb(json(err(std::string("RFCE JSON parsing error: ") + ex.what()), k400BadRequest));
                return;
            }

            app::validateOrThrow(cmd);

            try {
                auto scope = services.makeScope(mapping::currentUserFrom(req));
                app::SendRfceCommandHandler handler(services.ecfClient(), services.serializer(),
                                                    scope.docs, scope.uow);
                auto result = handler.handle(cmd);
                if (result.isFailure()) { cb(json(err(*result.error()), k400BadRequest)); return; }
                cb(json(mapping::toJson(*result.value()), k200OK));
            } catch (const std::exception& ex) {
                cb(json(err(std::string("RFCE service error: ") + ex.what()),
                              k500InternalServerError));
            }
        }
    );
}

void EcfController::status(const HttpRequestPtr& req,
                           std::function<void(const HttpResponsePtr&)>&& callback) {
    app::GetEcfStatusQuery q;
    q.rncEmisor = req->getParameter("rncEmisor");
    q.eNcf = req->getParameter("eNcf");

    if (q.rncEmisor.empty() || q.eNcf.empty()) {
        callback(json(err("Los parámetros 'rncEmisor' y 'eNcf' son obligatorios."), k400BadRequest));
        return;
    }

    try {
        auto& services = AppServices::instance();
        auto scope = services.makeScope(mapping::currentUserFrom(req));
        app::GetEcfStatusQueryHandler handler(services.ecfClient(), scope.docs, scope.uow);
        auto result = handler.handle(q);
        if (result.isFailure()) { callback(json(err(*result.error()), k400BadRequest)); return; }
        callback(json(mapping::toJson(*result.value()), k200OK));
    } catch (const std::exception& ex) {
        callback(json(err(std::string("Status service error: ") + ex.what()),
                      k500InternalServerError));
    }
}

void EcfController::servicesStatus(const HttpRequestPtr& req,
                                   std::function<void(const HttpResponsePtr&)>&& callback) {
    auto& services = AppServices::instance();
    std::string rawEnv = req->getParameter("environment");
    if (rawEnv.empty()) {
        rawEnv = req->getHeader("X-Environment");
    }

    domain::AmbienteEnum ambiente = services.ecfClientOptions().toAmbiente(services.ecfClientOptions().environment);
    if (!rawEnv.empty()) {
        std::string lower = rawEnv;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
        if (lower == "test" || lower == "testecf" || lower.find("precert") != std::string::npos || lower == "1") {
            ambiente = domain::AmbienteEnum::PreCertificacion;
        } else if (lower == "cert" || lower == "certecf" || lower.find("certific") != std::string::npos || lower == "2") {
            ambiente = domain::AmbienteEnum::Certificacion;
        } else if (lower == "prod" || lower == "prd" || lower == "production" || lower == "ecf" || lower.find("producc") != std::string::npos || lower == "4") {
            ambiente = domain::AmbienteEnum::Produccion;
        }
    }

    std::string cacheKey = "ecf:services:probe:" + std::to_string(static_cast<int>(ambiente));
    auto cache = services.cacheService();
    if (cache) {
        auto cachedStr = cache->get(cacheKey);
        if (cachedStr.has_value() && !cachedStr->empty()) {
            Json::CharReaderBuilder rb;
            Json::Value parsed;
            std::string errs;
            std::istringstream iss(*cachedStr);
            if (Json::parseFromStream(rb, iss, &parsed, &errs) && parsed.isArray() && !parsed.empty()) {
                callback(json(parsed, k200OK));
                return;
            }
        }
    }

    auto envConfig = infra::EcfEnvironmentConfig::getConfig(ambiente);
    std::string statusUrl = envConfig.estatusServiciosUrl + "/api/estatusservicios/obtenerestatus";
    std::string nowIso = sys::utcNowIso();
    bool gatewayOnline = false;
    int latencyMs = 0;
    std::string probeBody;

    try {
        static std::mutex probeMutex;
        static cpr::Session probeSession;
        std::lock_guard<std::mutex> lock(probeMutex);

        probeSession.SetUrl(cpr::Url{statusUrl});
        probeSession.SetHeader(cpr::Header{{"Accept", "application/json"}, {"Connection", "keep-alive"}});
        probeSession.SetTimeout(cpr::Timeout{3000});
        probeSession.SetConnectTimeout(cpr::ConnectTimeout{2000});

        auto t0 = std::chrono::steady_clock::now();
        auto r1 = probeSession.Get();
        auto t1 = std::chrono::steady_clock::now();
        if (r1.error.code == cpr::ErrorCode::OK && r1.status_code > 0 && r1.status_code < 500) {
            gatewayOnline = true;
            probeBody = r1.text;
            latencyMs = std::max(1, static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()));
        }
        if (gatewayOnline && latencyMs > 30) {
            probeSession.SetTimeout(cpr::Timeout{2000});
            auto w0 = std::chrono::steady_clock::now();
            auto r2 = probeSession.Get();
            auto w1 = std::chrono::steady_clock::now();
            if (r2.error.code == cpr::ErrorCode::OK && r2.status_code > 0 && r2.status_code < 500) {
                int warmMs = std::max(1, static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(w1 - w0).count()));
                if (warmMs < latencyMs) {
                    latencyMs = warmMs;
                }
                if (!r2.text.empty()) {
                    probeBody = r2.text;
                }
            }
        }
    } catch (...) {
        gatewayOnline = false;
        latencyMs = 0;
    }

    std::vector<domain::EstatusServicio> officialStatuses;
    if (gatewayOnline && !probeBody.empty()) {
        try {
            auto j = nlohmann::json::parse(probeBody, nullptr, false);
            if (j.is_array()) {
                for (const auto& item : j) {
                    domain::EstatusServicio es;
                    if (item.contains("servicio") && item["servicio"].is_string()) es.servicio = item["servicio"].get<std::string>();
                    if (item.contains("estatus") && item["estatus"].is_string()) es.estatus = item["estatus"].get<std::string>();
                    if (item.contains("ambiente") && item["ambiente"].is_string()) es.ambiente = item["ambiente"].get<std::string>();
                    officialStatuses.push_back(std::move(es));
                }
            }
        } catch (...) {}
    }
    if (gatewayOnline && officialStatuses.empty()) {
        try {
            officialStatuses = services.ecfClient()->consultarEstatusServicios();
        } catch (...) {}
    }

    auto containsIgnoreCase = [](const std::string& haystack, const std::string& needle) {
        if (needle.empty()) return true;
        auto it = std::search(
            haystack.begin(), haystack.end(),
            needle.begin(), needle.end(),
            [](char ch1, char ch2) {
                return std::tolower(static_cast<unsigned char>(ch1)) ==
                       std::tolower(static_cast<unsigned char>(ch2));
            });
        return it != haystack.end();
    };

    auto isOfficialAvailable = [&](const std::string& keyword) -> bool {
        if (!gatewayOnline) return false;
        if (officialStatuses.empty()) return true;
        for (const auto& s : officialStatuses) {
            if (containsIgnoreCase(s.servicio, keyword)) {
                if (s.estatus.empty()) return true;
                return !containsIgnoreCase(s.estatus, "No Disponible") &&
                       !containsIgnoreCase(s.estatus, "Mantenimiento");
            }
        }
        return true;
    };

    auto makeItem = [&](const std::string& name, bool avail) {
        Json::Value item;
        item["servicio"] = name;
        item["disponible"] = avail;
        item["latencia"] = gatewayOnline ? latencyMs : 0;
        item["ultimaVerificacion"] = nowIso;
        return item;
    };

    Json::Value arr(Json::arrayValue);
    arr.append(makeItem("Recepción e-CF", isOfficialAvailable("Recepci")));
    arr.append(makeItem("Consulta Estado", isOfficialAvailable("Consulta Estado")));
    arr.append(makeItem("Timbre / QR", isOfficialAvailable("Timbre")));
    arr.append(makeItem("Directorio Contribuyentes", isOfficialAvailable("Directorio")));
    arr.append(makeItem("Aprobación Comercial", isOfficialAvailable("Aprobaci")));

    if (gatewayOnline && cache) {
        Json::StreamWriterBuilder wb;
        wb["indentation"] = "";
        cache->set(cacheKey, Json::writeString(wb, arr), std::chrono::seconds(45));
    }

    callback(json(arr, k200OK));
}

}
