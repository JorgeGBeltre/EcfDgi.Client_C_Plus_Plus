#pragma once

#include <memory>
#include <string>

#include "Domain/Entities/EcfClientOptions.h"
#include "Domain/Interfaces/IEcfClient.h"
#include "Domain/Interfaces/IEcfDocumentRepository.h"
#include "Domain/Interfaces/ITenantSignerResolver.h"
#include "Domain/Interfaces/IUnitOfWork.h"

namespace ecf::app {

class EcfStatusReconciler {
public:
    EcfStatusReconciler(std::shared_ptr<domain::IEcfDocumentRepository> docsRepo,
                        std::shared_ptr<domain::IUnitOfWork> uow,
                        std::shared_ptr<domain::IEcfClient> ecfClient,
                        domain::EcfStatusPollingOptions options,
                        std::shared_ptr<domain::ITenantSignerResolver> signerResolver = nullptr);

    int reconcile();

private:
    std::shared_ptr<domain::IEcfDocumentRepository> docsRepo_;
    std::shared_ptr<domain::IUnitOfWork> uow_;
    std::shared_ptr<domain::IEcfClient> ecfClient_;
    domain::EcfStatusPollingOptions options_;
    std::shared_ptr<domain::ITenantSignerResolver> signerResolver_;
};

}  // namespace ecf::app
