#pragma once
// Ports of RegisterUserCommandHandler and LoginUserCommandHandler.

#include <memory>

#include "Application/Auth/AuthDtos.h"
#include "Domain/Interfaces/ISecurity.h"
#include "Domain/Interfaces/IUnitOfWork.h"
#include "Domain/Interfaces/IUserRepository.h"
#include "Shared/Common/Result.h"

namespace ecf::app {

class RegisterUserCommandHandler {
public:
    RegisterUserCommandHandler(std::shared_ptr<domain::IUserRepository> userRepo,
                               std::shared_ptr<domain::IPasswordHasher> hasher,
                               std::shared_ptr<domain::ITokenService> tokenService,
                               std::shared_ptr<domain::IUnitOfWork> uow)
        : userRepo_(std::move(userRepo)),
          hasher_(std::move(hasher)),
          tokenService_(std::move(tokenService)),
          uow_(std::move(uow)) {}

    shared::ResultT<AuthResponseDto> handle(const RegisterUserCommand& cmd);

private:
    std::shared_ptr<domain::IUserRepository> userRepo_;
    std::shared_ptr<domain::IPasswordHasher> hasher_;
    std::shared_ptr<domain::ITokenService> tokenService_;
    std::shared_ptr<domain::IUnitOfWork> uow_;
};

class LoginUserCommandHandler {
public:
    LoginUserCommandHandler(std::shared_ptr<domain::IUserRepository> userRepo,
                            std::shared_ptr<domain::IPasswordHasher> hasher,
                            std::shared_ptr<domain::ITokenService> tokenService)
        : userRepo_(std::move(userRepo)),
          hasher_(std::move(hasher)),
          tokenService_(std::move(tokenService)) {}

    shared::ResultT<AuthResponseDto> handle(const LoginUserCommand& cmd);

private:
    std::shared_ptr<domain::IUserRepository> userRepo_;
    std::shared_ptr<domain::IPasswordHasher> hasher_;
    std::shared_ptr<domain::ITokenService> tokenService_;
};

}  // namespace ecf::app
