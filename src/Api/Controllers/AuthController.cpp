#include "Api/Controllers/AuthController.h"

#include "Api/AppServices.h"
#include "Api/JsonMapping.h"
#include "Application/Auth/AuthHandlers.h"
#include "Application/Common/Behaviors/Behaviors.h"

using namespace drogon;

namespace ecf::api {

namespace {
HttpResponsePtr json(const Json::Value& body, HttpStatusCode code) {
    auto resp = HttpResponse::newHttpJsonResponse(body);
    resp->setStatusCode(code);
    return resp;
}
Json::Value errorBody(const shared::Result& r) {
    Json::Value v;
    v["error"] = r.error().has_value() ? Json::Value(*r.error()) : Json::Value();
    Json::Value errs(Json::arrayValue);
    for (const auto& e : r.errors()) errs.append(e);
    v["errors"] = errs;
    return v;
}
}  // namespace

void AuthController::registerUser(const HttpRequestPtr& req,
                                  std::function<void(const HttpResponsePtr&)>&& callback) {
    auto body = req->getJsonObject();
    if (!body) { callback(json(Json::Value("Invalid JSON body."), k400BadRequest)); return; }

    app::RegisterUserCommand cmd;
    cmd.username = (*body).get("username", "").asString();
    cmd.email = (*body).get("email", "").asString();
    cmd.password = (*body).get("password", "").asString();
    cmd.role = (*body).get("role", "").asString();

    app::LoggingScope _log("RegisterUserCommand");
    app::validateOrThrow(cmd);  // throws ValidationException -> 400

    auto& services = AppServices::instance();
    auto scope = services.makeScope(mapping::currentUserFrom(req));
    app::RegisterUserCommandHandler handler(scope.users, services.passwordHasher(),
                                            services.tokenService(), scope.uow);
    auto result = handler.handle(cmd);

    if (result.isFailure()) { callback(json(errorBody(result), k400BadRequest)); return; }
    callback(json(mapping::toJson(*result.value()), k200OK));
}

void AuthController::login(const HttpRequestPtr& req,
                           std::function<void(const HttpResponsePtr&)>&& callback) {
    auto body = req->getJsonObject();
    if (!body) { callback(json(Json::Value("Invalid JSON body."), k400BadRequest)); return; }

    app::LoginUserCommand cmd;
    cmd.username = (*body).get("username", "").asString();
    cmd.password = (*body).get("password", "").asString();

    app::LoggingScope _log("LoginUserCommand");
    app::validateOrThrow(cmd);

    auto& services = AppServices::instance();
    auto scope = services.makeScope(mapping::currentUserFrom(req));
    app::LoginUserCommandHandler handler(scope.users, services.passwordHasher(),
                                         services.tokenService());
    auto result = handler.handle(cmd);

    if (result.isFailure()) { callback(json(errorBody(result), k401Unauthorized)); return; }
    callback(json(mapping::toJson(*result.value()), k200OK));
}

}  // namespace ecf::api
