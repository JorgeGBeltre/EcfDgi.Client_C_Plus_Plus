#pragma once

#include <drogon/HttpController.h>

namespace ecf::api {

class DocumentsController : public drogon::HttpController<DocumentsController> {
public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(DocumentsController::submit, "/api/documents", drogon::Post,
                  "ecf::api::UserOrWorkerFilter");
    ADD_METHOD_TO(DocumentsController::getBySourceTxnId, "/api/documents/by-source/{txnId}", drogon::Get,
                  "ecf::api::UserOrWorkerFilter");
    METHOD_LIST_END

    void submit(const drogon::HttpRequestPtr& req,
                std::function<void(const drogon::HttpResponsePtr&)>&& callback);
                
    void getBySourceTxnId(const drogon::HttpRequestPtr& req,
                          std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                          std::string txnId);
};

} // namespace ecf::api
