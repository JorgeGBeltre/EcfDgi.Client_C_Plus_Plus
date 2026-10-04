#include "Api/Controllers/CustomersController.h"

#include "Api/AppServices.h"
#include "Api/JsonMapping.h"
#include "Application/Common/Behaviors/Behaviors.h"
#include "Application/Customers/CustomerHandlers.h"

using namespace drogon;

namespace ecf::api {

namespace {
HttpResponsePtr json(const Json::Value& body, HttpStatusCode code) {
    auto resp = HttpResponse::newHttpJsonResponse(body);
    resp->setStatusCode(code);
    return resp;
}
HttpResponsePtr noContent() {
    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k204NoContent);
    return resp;
}
Json::Value err(const std::string& message) {
    Json::Value v;
    v["error"] = message;
    return v;
}

bool isValidUuid(const std::string& s) {
    if (s.size() != 36) return false;
    for (size_t i = 0; i < 36; ++i) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (s[i] != '-') return false;
        } else {
            if (!std::isxdigit(static_cast<unsigned char>(s[i]))) return false;
        }
    }
    return true;
}

}  // namespace

void CustomersController::getAll(const HttpRequestPtr& req,
                                 std::function<void(const HttpResponsePtr&)>&& callback) {
    auto scope = AppServices::instance().makeScope(mapping::currentUserFrom(req));
    app::GetAllCustomersQueryHandler handler(scope.customers);
    auto result = handler.handle(app::GetAllCustomersQuery{});

    Json::Value arr(Json::arrayValue);
    for (const auto& dto : *result.value()) arr.append(mapping::toJson(dto));
    callback(json(arr, k200OK));
}

void CustomersController::getById(const HttpRequestPtr& req,
                                  std::function<void(const HttpResponsePtr&)>&& callback,
                                  std::string id) {
    if (!isValidUuid(id)) {
        callback(json(err("El ID proporcionado no es un UUID válido."), k400BadRequest));
        return;
    }
    auto scope = AppServices::instance().makeScope(mapping::currentUserFrom(req));
    app::GetCustomerByIdQueryHandler handler(scope.customers);
    auto result = handler.handle(app::GetCustomerByIdQuery{id});

    if (result.isFailure()) { callback(json(err(*result.error()), k404NotFound)); return; }
    callback(json(mapping::toJson(*result.value()), k200OK));
}

void CustomersController::create(const HttpRequestPtr& req,
                                 std::function<void(const HttpResponsePtr&)>&& callback) {
    auto body = req->getJsonObject();
    if (!body || !body->isObject()) { callback(json(err("Invalid JSON body."), k400BadRequest)); return; }

    if (body->isMember("name") && !(*body)["name"].isString()) {
        callback(json(err("Field 'name' must be a string."), k400BadRequest));
        return;
    }
    if (body->isMember("email") && !(*body)["email"].isString()) {
        callback(json(err("Field 'email' must be a string."), k400BadRequest));
        return;
    }
    if (body->isMember("rnc") && !(*body)["rnc"].isString()) {
        callback(json(err("Field 'rnc' must be a string."), k400BadRequest));
        return;
    }

    app::CreateCustomerCommand cmd;
    cmd.name = body->isMember("name") ? (*body)["name"].asString() : "";
    cmd.email = body->isMember("email") ? (*body)["email"].asString() : "";
    cmd.rnc = body->isMember("rnc") ? (*body)["rnc"].asString() : "";

    app::LoggingScope _log("CreateCustomerCommand");
    app::validateOrThrow(cmd);

    auto scope = AppServices::instance().makeScope(mapping::currentUserFrom(req));
    app::CreateCustomerCommandHandler handler(scope.customers, scope.uow);
    auto result = handler.handle(cmd);

    if (result.isFailure()) { callback(json(err(*result.error()), k400BadRequest)); return; }
    Json::Value v = *result.value();  // the new id
    auto resp = json(v, k201Created);
    resp->addHeader("Location", "/api/customers/" + *result.value());
    callback(resp);
}

void CustomersController::update(const HttpRequestPtr& req,
                                 std::function<void(const HttpResponsePtr&)>&& callback,
                                 std::string id) {
    if (!isValidUuid(id)) {
        callback(json(err("El ID proporcionado no es un UUID válido."), k400BadRequest));
        return;
    }
    auto body = req->getJsonObject();
    if (!body || !body->isObject()) { callback(json(err("Invalid JSON body."), k400BadRequest)); return; }

    if (body->isMember("id")) {
        if (!(*body)["id"].isString()) {
            callback(json(err("Field 'id' must be a string."), k400BadRequest));
            return;
        }
        if ((*body)["id"].asString() != id) {
            callback(json(err("Mismatched route ID and request body ID."), k400BadRequest));
            return;
        }
    }

    if (body->isMember("name") && !(*body)["name"].isString()) {
        callback(json(err("Field 'name' must be a string."), k400BadRequest));
        return;
    }
    if (body->isMember("email") && !(*body)["email"].isString()) {
        callback(json(err("Field 'email' must be a string."), k400BadRequest));
        return;
    }
    if (body->isMember("rnc") && !(*body)["rnc"].isString()) {
        callback(json(err("Field 'rnc' must be a string."), k400BadRequest));
        return;
    }

    app::UpdateCustomerCommand cmd;
    cmd.id = id;
    cmd.name = body->isMember("name") ? (*body)["name"].asString() : "";
    cmd.email = body->isMember("email") ? (*body)["email"].asString() : "";
    cmd.rnc = body->isMember("rnc") ? (*body)["rnc"].asString() : "";

    app::LoggingScope _log("UpdateCustomerCommand");
    app::validateOrThrow(cmd);

    auto scope = AppServices::instance().makeScope(mapping::currentUserFrom(req));
    app::UpdateCustomerCommandHandler handler(scope.customers, scope.uow);
    auto result = handler.handle(cmd);

    if (result.isFailure()) { callback(json(err(*result.error()), k404NotFound)); return; }
    callback(noContent());
}

void CustomersController::remove(const HttpRequestPtr& req,
                                 std::function<void(const HttpResponsePtr&)>&& callback,
                                 std::string id) {
    if (!isValidUuid(id)) {
        callback(json(err("El ID proporcionado no es un UUID válido."), k400BadRequest));
        return;
    }
    auto scope = AppServices::instance().makeScope(mapping::currentUserFrom(req));
    app::DeleteCustomerCommandHandler handler(scope.customers, scope.uow);
    auto result = handler.handle(app::DeleteCustomerCommand{id});

    if (result.isFailure()) { callback(json(err(*result.error()), k404NotFound)); return; }
    callback(noContent());
}

}  // namespace ecf::api
