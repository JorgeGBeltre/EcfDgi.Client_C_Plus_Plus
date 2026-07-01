cmake_minimum_required(VERSION 3.20)
project(ecfdgii-client-cpp CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find dependencies (normally resolved via vcpkg toolchain)
find_package(Drogon CONFIG REQUIRED)
find_package(cpr CONFIG REQUIRED)
find_package(LibXml2 REQUIRED)
find_package(unofficial-xmlsec CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(jwt-cpp CONFIG REQUIRED)
find_package(libpqxx CONFIG REQUIRED)
find_package(unofficial-sodium CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)

# Core Library Target
add_library(ecfdgii_core STATIC
    # Shared
    src/Shared/Common/Sys.cpp
    src/Shared/Common/Sys.h
    src/Shared/Common/Result.h

    # Domain
    src/Domain/Common/AuditableEntity.h
    src/Domain/Entities/Customer.h
    src/Domain/Entities/EcfClientOptions.h
    src/Domain/Entities/EcfDocument.h
    src/Domain/Entities/ResponseModels.h
    src/Domain/Entities/Rfce.h
    src/Domain/Entities/User.h
    src/Domain/Exceptions/EcfException.h
    src/Domain/Interfaces/ICustomerRepository.h
    src/Domain/Interfaces/IEcfClient.h
    src/Domain/Interfaces/IEcfDocumentRepository.h
    src/Domain/Interfaces/IEcfSequenceProvider.h
    src/Domain/Interfaces/IEcfTransport.h
    src/Domain/Interfaces/IEcfXmlSerializer.h
    src/Domain/Interfaces/ISecurity.h
    src/Domain/Interfaces/IUnitOfWork.h
    src/Domain/Interfaces/IUserRepository.h

    # Application
    src/Application/Auth/AuthDtos.h
    src/Application/Auth/AuthHandlers.cpp
    src/Application/Auth/AuthHandlers.h
    src/Application/Common/Behaviors/Behaviors.h
    src/Application/Common/Exceptions/ValidationException.h
    src/Application/Common/RequestValidators.cpp
    src/Application/Common/RequestValidators.h
    src/Application/Customers/CustomerDtos.h
    src/Application/Customers/CustomerHandlers.cpp
    src/Application/Customers/CustomerHandlers.h
    src/Application/Ecf/EcfDtos.h
    src/Application/Ecf/EcfHandlers.cpp
    src/Application/Ecf/EcfHandlers.h
    src/Application/Services/EcfValidator.cpp
    src/Application/Services/EcfValidator.h
    src/Application/Services/PollingHelper.h

    # Infrastructure
    src/Infrastructure/Dgii/DgiiDirectTransport.cpp
    src/Infrastructure/Dgii/DgiiDirectTransport.h
    src/Infrastructure/Dgii/EcfEnvironmentConfig.cpp
    src/Infrastructure/Dgii/EcfEnvironmentConfig.h
    src/Infrastructure/Dgii/EcfTokenManager.cpp
    src/Infrastructure/Dgii/EcfTokenManager.h
    src/Infrastructure/EcfClient.cpp
    src/Infrastructure/EcfClient.h
    src/Infrastructure/EcfFrontendClient.cpp
    src/Infrastructure/EcfFrontendClient.h
    src/Infrastructure/Persistence/DbContext.cpp
    src/Infrastructure/Persistence/DbContext.h
    src/Infrastructure/Persistence/DbInitializer.cpp
    src/Infrastructure/Persistence/DbInitializer.h
    src/Infrastructure/Persistence/MemorySequenceProvider.cpp
    src/Infrastructure/Persistence/MemorySequenceProvider.h
    src/Infrastructure/Persistence/Repositories/CustomerRepository.cpp
    src/Infrastructure/Persistence/Repositories/CustomerRepository.h
    src/Infrastructure/Persistence/Repositories/EcfDocumentRepository.cpp
    src/Infrastructure/Persistence/Repositories/EcfDocumentRepository.h
    src/Infrastructure/Persistence/Repositories/UserRepository.cpp
    src/Infrastructure/Persistence/Repositories/UserRepository.h
    src/Infrastructure/Persistence/RowMappers.h
    src/Infrastructure/Persistence/UnitOfWork.h
    src/Infrastructure/Security/EcfSecurityUtils.cpp
    src/Infrastructure/Security/EcfSecurityUtils.h
    src/Infrastructure/Security/EcfXmlSigner.cpp
    src/Infrastructure/Security/EcfXmlSigner.h
    src/Infrastructure/Security/JwtSettings.h
    src/Infrastructure/Security/PasswordHasher.cpp
    src/Infrastructure/Security/PasswordHasher.h
    src/Infrastructure/Security/TokenService.cpp
    src/Infrastructure/Security/TokenService.h
    src/Infrastructure/Serialization/EcfXmlSerializer.cpp
    src/Infrastructure/Serialization/EcfXmlSerializer.h
)

target_include_directories(ecfdgii_core PUBLIC src)

target_link_libraries(ecfdgii_core PUBLIC
    Drogon::Drogon
    cpr::cpr
    LibXml2::LibXml2
    unofficial::xmlsec::xmlsec1-openssl
    OpenSSL::SSL
    OpenSSL::Crypto
    nlohmann_json::nlohmann_json
    jwt-cpp::jwt-cpp
    libpqxx::pqxx
    unofficial-sodium::sodium
    spdlog::spdlog
)

# API Host Executable Target
add_executable(ecfdgii_api
    src/Api/main.cpp
    src/Api/AppServices.cpp
    src/Api/AppServices.h
    src/Api/Configuration/AppConfig.cpp
    src/Api/Configuration/AppConfig.h
    src/Api/Controllers/AuthController.cpp
    src/Api/Controllers/AuthController.h
    src/Api/Controllers/CustomersController.cpp
    src/Api/Controllers/CustomersController.h
    src/Api/Controllers/EcfController.cpp
    src/Api/Controllers/EcfController.h
    src/Api/Filters/AdminRoleFilter.cpp
    src/Api/Filters/AdminRoleFilter.h
    src/Api/Filters/JwtAuthFilter.cpp
    src/Api/Filters/JwtAuthFilter.h
    src/Api/JsonMapping.cpp
    src/Api/JsonMapping.h
    src/Api/Services/CurrentUserService.h
)

target_link_libraries(ecfdgii_api PRIVATE ecfdgii_core)

# Tests Subdirectory
option(ECF_BUILD_TESTS "Build tests" ON)
if(ECF_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
