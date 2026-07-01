#include "Api/AppServices.h"

#include "Infrastructure/EcfClient.h"
#include "Infrastructure/Persistence/Repositories/CustomerRepository.h"
#include "Infrastructure/Persistence/Repositories/EcfDocumentRepository.h"
#include "Infrastructure/Persistence/Repositories/UserRepository.h"
#include "Infrastructure/Persistence/UnitOfWork.h"
#include "Infrastructure/Security/PasswordHasher.h"
#include "Infrastructure/Security/TokenService.h"
#include "Infrastructure/Serialization/EcfXmlSerializer.h"

namespace ecf::api {

AppServices& AppServices::instance() {
    static AppServices s;
    return s;
}

void AppServices::configure(AppConfig config) {
    config_ = std::move(config);
    passwordHasher_ = std::make_shared<infra::PasswordHasher>();
    tokenService_ = std::make_shared<infra::TokenService>(config_.jwt);
    serializer_ = std::make_shared<infra::EcfXmlSerializer>();
}

std::shared_ptr<domain::IEcfClient> AppServices::ecfClient() {
    // Built on first use: constructing DgiiDirectTransport loads the signing
    // certificate, so failures surface at call time.
    std::call_once(ecfClientFlag_, [this] {
        try {
            ecfClient_ = std::make_shared<infra::EcfClient>(config_.ecfOptions);
        } catch (...) {
            ecfClientError_ = std::current_exception();
        }
    });
    if (ecfClientError_) std::rethrow_exception(ecfClientError_);
    return ecfClient_;
}

AppServices::Scope AppServices::makeScope(
    std::shared_ptr<domain::ICurrentUserService> currentUser) {
    auto db = std::make_shared<infra::DbContext>(config_.connectionString,
                                                 std::move(currentUser));
    Scope scope;
    scope.db = db;
    scope.customers = std::make_shared<infra::CustomerRepository>(db);
    scope.users = std::make_shared<infra::UserRepository>(db);
    scope.docs = std::make_shared<infra::EcfDocumentRepository>(db);
    scope.uow = std::make_shared<infra::UnitOfWork>(db);
    return scope;
}

}  // namespace ecf::api
