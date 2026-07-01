#pragma once
// Ports of SendEcfCommandHandler, SendRfceCommandHandler, GetEcfStatusQueryHandler.

#include <memory>

#include "Application/Ecf/EcfDtos.h"
#include "Domain/Entities/ResponseModels.h"
#include "Domain/Interfaces/IEcfClient.h"
#include "Domain/Interfaces/IEcfDocumentRepository.h"
#include "Domain/Interfaces/IEcfXmlSerializer.h"
#include "Domain/Interfaces/IUnitOfWork.h"
#include "Shared/Common/Result.h"

namespace ecf::app {

class SendEcfCommandHandler {
public:
    SendEcfCommandHandler(std::shared_ptr<domain::IEcfClient> client,
                          std::shared_ptr<domain::IEcfDocumentRepository> docRepo,
                          std::shared_ptr<domain::IUnitOfWork> uow)
        : client_(std::move(client)), docRepo_(std::move(docRepo)), uow_(std::move(uow)) {}
    shared::ResultT<domain::EcfRecepcionResponse> handle(const SendEcfCommand& cmd);

private:
    std::shared_ptr<domain::IEcfClient> client_;
    std::shared_ptr<domain::IEcfDocumentRepository> docRepo_;
    std::shared_ptr<domain::IUnitOfWork> uow_;
};

class SendRfceCommandHandler {
public:
    SendRfceCommandHandler(std::shared_ptr<domain::IEcfClient> client,
                           std::shared_ptr<domain::IEcfXmlSerializer> serializer,
                           std::shared_ptr<domain::IEcfDocumentRepository> docRepo,
                           std::shared_ptr<domain::IUnitOfWork> uow)
        : client_(std::move(client)),
          serializer_(std::move(serializer)),
          docRepo_(std::move(docRepo)),
          uow_(std::move(uow)) {}
    shared::ResultT<domain::RfceRecepcionResponse> handle(SendRfceCommand cmd);

private:
    std::shared_ptr<domain::IEcfClient> client_;
    std::shared_ptr<domain::IEcfXmlSerializer> serializer_;
    std::shared_ptr<domain::IEcfDocumentRepository> docRepo_;
    std::shared_ptr<domain::IUnitOfWork> uow_;
};

class GetEcfStatusQueryHandler {
public:
    GetEcfStatusQueryHandler(std::shared_ptr<domain::IEcfClient> client,
                             std::shared_ptr<domain::IEcfDocumentRepository> docRepo,
                             std::shared_ptr<domain::IUnitOfWork> uow)
        : client_(std::move(client)), docRepo_(std::move(docRepo)), uow_(std::move(uow)) {}
    shared::ResultT<domain::ConsultaEstadoResponse> handle(const GetEcfStatusQuery& q);

private:
    std::shared_ptr<domain::IEcfClient> client_;
    std::shared_ptr<domain::IEcfDocumentRepository> docRepo_;
    std::shared_ptr<domain::IUnitOfWork> uow_;
};

}  // namespace ecf::app
