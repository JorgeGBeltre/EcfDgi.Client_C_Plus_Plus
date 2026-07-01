#pragma once

#include <drogon/HttpController.h>

namespace ecf::api {

class CustomersController : public drogon::HttpController<CustomersController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(CustomersController::getAll, "/api/customers", drogon::Get,
                  "ecf::api::JwtAuthFilter");
    ADD_METHOD_TO(CustomersController::getById, "/api/customers/{1}", drogon::Get,
                  "ecf::api::JwtAuthFilter");
    ADD_METHOD_TO(CustomersController::create, "/api/customers", drogon::Post,
                  "ecf::api::JwtAuthFilter");
    ADD_METHOD_TO(CustomersController::update, "/api/customers/{1}", drogon::Put,
                  "ecf::api::JwtAuthFilter");
    ADD_METHOD_TO(CustomersController::remove, "/api/customers/{1}", drogon::Delete,
                  "ecf::api::JwtAuthFilter", "ecf::api::AdminRoleFilter");
    METHOD_LIST_END

    void getAll(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void getById(const drogon::HttpRequestPtr& req,
                 std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                 std::string id);
    void create(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void update(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                std::string id);
    void remove(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                std::string id);
};

}  // namespace ecf::api
