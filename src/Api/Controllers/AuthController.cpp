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

void AuthController::login(const HttpRequestPtr& req,
                           std::function<void(const HttpResponsePtr&)>&& callback) {
    (void)req;
    // ARC-021: Interactive end-user authentication is disabled on the C++ fiscal engine.
    // End users authenticate exclusively through SaaS-Ecf-Back (/api/v1/auth/login).
    // The C++ engine operates as an internal subsystem using worker tokens and mutual HMAC.
    Json::Value resp;
    resp["error"] = "Interactive end-user login is disabled on the C++ fiscal engine. Please authenticate through SaaS-Ecf-Back at /api/v1/auth/login.";
    resp["status"] = 403;
    callback(json(resp, k403Forbidden));
}

}  // namespace ecf::api
