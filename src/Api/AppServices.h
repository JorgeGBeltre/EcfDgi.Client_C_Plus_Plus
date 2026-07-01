#pragma once
// Composition root: wires the application and infrastructure services. Holds
// singletons and builds a per-request scope (DbContext + repositories + unit of
// work).

#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "Api/Configuration/AppConfig.h"
#include "Domain/Interfaces/ICustomerRepository.h"
#include "Domain/Interfaces/IEcfClient.h"
#include "Domain/Interfaces/IEcfDocumentRepository.h"
#include "Domain/Interfaces/IEcfXmlSerializer.h"
#include "Domain/Interfaces/ISecurity.h"
#include "Domain/Interfaces/IUnitOfWork.h"
#include "Domain/Interfaces/IUserRepository.h"
#include "Infrastructure/Persistence/DbContext.h"

namespace ecf::api {

class AppServices {
public:
    struct Scope {
        std::shared_ptr<infra::DbContext> db;
        std::shared_ptr<domain::ICustomerRepository> customers;
        std::shared_ptr<domain::IUserRepository> users;
        std::shared_ptr<domain::IEcfDocumentRepository> docs;
        std::shared_ptr<domain::IUnitOfWork> uow;
    };

    static AppServices& instance();

    void configure(AppConfig config);
    const AppConfig& config() const { return config_; }

    // Singletons.
    std::shared_ptr<domain::IPasswordHasher> passwordHasher() const { return passwordHasher_; }
    std::shared_ptr<domain::ITokenService> tokenService() const { return tokenService_; }
    std::shared_ptr<domain::IEcfXmlSerializer> serializer() const { return serializer_; }
    std::shared_ptr<domain::IEcfClient> ecfClient();  // lazily built (needs certificate)

    // Per-request scope (new DB connection + repositories bound to currentUser).
    Scope makeScope(std::shared_ptr<domain::ICurrentUserService> currentUser);

private:
    AppConfig config_;
    std::shared_ptr<domain::IPasswordHasher> passwordHasher_;
    std::shared_ptr<domain::ITokenService> tokenService_;
    std::shared_ptr<domain::IEcfXmlSerializer> serializer_;

    std::shared_ptr<domain::IEcfClient> ecfClient_;
    std::once_flag ecfClientFlag_;
    std::exception_ptr ecfClientError_;
};

}  // namespace ecf::api
