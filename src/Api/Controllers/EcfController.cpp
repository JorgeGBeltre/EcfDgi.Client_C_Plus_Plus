#include "Api/Controllers/EcfController.h"

#include <exception>

#include "Api/AppServices.h"
#include "Api/JsonMapping.h"
#include "Application/Common/Behaviors/Behaviors.h"
#include "Application/Ecf/EcfHandlers.h"

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
    auto body = req->getJsonObject();
    if (!body) { callback(json(err("Invalid JSON body."), k400BadRequest)); return; }

    app::SendEcfCommand cmd;
    cmd.xmlContent = (*body).get("xmlContent", "").asString();
    cmd.fileName = (*body).get("fileName", "").asString();
    cmd.rncEmisor = (*body).get("rncEmisor", "").asString();
    cmd.eNcf = (*body).get("eNcf", "").asString();
    if ((*body).isMember("rncComprador") && (*body)["rncComprador"].isString())
        cmd.rncComprador = (*body)["rncComprador"].asString();
    cmd.totalAmount = (*body).get("totalAmount", 0.0).asDouble();
    cmd.itbisAmount = (*body).get("itbisAmount", 0.0).asDouble();

    app::LoggingScope _log("SendEcfCommand");
    app::validateOrThrow(cmd);  // throws ValidationException -> 400

    try {
        auto& services = AppServices::instance();
        auto scope = services.makeScope(mapping::currentUserFrom(req));
        app::SendEcfCommandHandler handler(services.ecfClient(), scope.docs, scope.uow);
        auto result = handler.handle(cmd);
        if (result.isFailure()) { callback(json(err(*result.error()), k400BadRequest)); return; }
        callback(json(mapping::toJson(*result.value()), k200OK));
    } catch (const std::exception& ex) {
        callback(json(err(std::string("e-CF service unavailable: ") + ex.what()),
                      k500InternalServerError));
    }
}

void EcfController::sendRfce(const HttpRequestPtr& req,
                             std::function<void(const HttpResponsePtr&)>&& callback) {
    auto body = req->getJsonObject();
    if (!body) { callback(json(err("Invalid JSON body."), k400BadRequest)); return; }

    app::SendRfceCommand cmd;
    app::LoggingScope _log("SendRfceCommand");

    // Accept either { "rfceModel": {...} } or the RFCE object directly.
    const Json::Value& rfceJson =
        (*body).isMember("rfceModel") ? (*body)["rfceModel"] : *body;
    cmd.rfceModel = mapping::rfceFromJson(rfceJson);
    app::validateOrThrow(cmd);  // throws ValidationException -> 400

    try {
        auto& services = AppServices::instance();
        auto scope = services.makeScope(mapping::currentUserFrom(req));
        app::SendRfceCommandHandler handler(services.ecfClient(), services.serializer(),
                                            scope.docs, scope.uow);
        auto result = handler.handle(cmd);
        if (result.isFailure()) { callback(json(err(*result.error()), k400BadRequest)); return; }
        callback(json(mapping::toJson(*result.value()), k200OK));
    } catch (const std::exception& ex) {
        callback(json(err(std::string("RFCE service error: ") + ex.what()),
                      k500InternalServerError));
    }
}

void EcfController::status(const HttpRequestPtr& req,
                           std::function<void(const HttpResponsePtr&)>&& callback) {
    app::GetEcfStatusQuery q;
    q.rncEmisor = req->getParameter("rncEmisor");
    q.eNcf = req->getParameter("eNcf");

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
