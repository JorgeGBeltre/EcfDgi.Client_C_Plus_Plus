#include "Api/Controllers/EcfController.h"

#include <exception>

#include "Api/AppServices.h"
#include "Api/JsonMapping.h"
#include "Application/Common/Behaviors/Behaviors.h"
#include "Application/Ecf/EcfHandlers.h"
#include "Api/Security/IdempotencyHandler.h"

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
}  // namespace

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
            app::validateOrThrow(cmd);  // throws ValidationException -> 400

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

            app::validateOrThrow(cmd);  // throws ValidationException -> 400

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

}  // namespace ecf::api
