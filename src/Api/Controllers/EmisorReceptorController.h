#pragma once

#include <drogon/HttpController.h>

namespace ecf::api {

class EmisorReceptorController : public drogon::HttpController<EmisorReceptorController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(EmisorReceptorController::recepcioneCF, "/fe/recepcion/api/ecf", drogon::Post);
    ADD_METHOD_TO(EmisorReceptorController::aprobacionComercial, "/fe/aprobacioncomercial/api/ecf", drogon::Post);
    ADD_METHOD_TO(EmisorReceptorController::getSemilla, "/fe/autenticacion/api/semilla", drogon::Get);
    ADD_METHOD_TO(EmisorReceptorController::validarSemilla, "/fe/autenticacion/api/validacioncertificado", drogon::Post);
    METHOD_LIST_END

    void recepcioneCF(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void aprobacionComercial(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void getSemilla(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void validarSemilla(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);
};

}  // namespace ecf::api
