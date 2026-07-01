#pragma once

#include <drogon/HttpController.h>

namespace ecf::api {

class EcfController : public drogon::HttpController<EcfController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(EcfController::send, "/api/ecf/send", drogon::Post,
                  "ecf::api::JwtAuthFilter");
    ADD_METHOD_TO(EcfController::sendRfce, "/api/ecf/send-rfce", drogon::Post,
                  "ecf::api::JwtAuthFilter");
    ADD_METHOD_TO(EcfController::status, "/api/ecf/status", drogon::Get,
                  "ecf::api::JwtAuthFilter");
    METHOD_LIST_END

    void send(const drogon::HttpRequestPtr& req,
              std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void sendRfce(const drogon::HttpRequestPtr& req,
                  std::function<void(const drogon::HttpResponsePtr&)>&& callback);
    void status(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

}  // namespace ecf::api
