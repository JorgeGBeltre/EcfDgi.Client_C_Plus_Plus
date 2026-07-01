#include "Application/Auth/AuthHandlers.h"

#include "Domain/Entities/User.h"

namespace ecf::app {

using shared::ResultT;

ResultT<AuthResponseDto> RegisterUserCommandHandler::handle(const RegisterUserCommand& cmd) {
    if (userRepo_->getByUsername(cmd.username).has_value())
        return ResultT<AuthResponseDto>::Failure("Username is already taken.");

    if (userRepo_->getByEmail(cmd.email).has_value())
        return ResultT<AuthResponseDto>::Failure("Email is already registered.");

    domain::User user;
    user.username = cmd.username;
    user.email = cmd.email;
    user.passwordHash = hasher_->hashPassword(cmd.password);
    user.role = cmd.role;

    userRepo_->add(user);
    uow_->saveChanges();

    // Re-read to obtain the persisted id for the token subject.
    auto persisted = userRepo_->getByUsername(cmd.username).value_or(user);

    AuthResponseDto dto;
    dto.username = persisted.username;
    dto.role = persisted.role;
    dto.token = tokenService_->generateToken(persisted);
    return ResultT<AuthResponseDto>::Success(std::move(dto));
}

ResultT<AuthResponseDto> LoginUserCommandHandler::handle(const LoginUserCommand& cmd) {
    auto user = userRepo_->getByUsername(cmd.username);
    if (!user.has_value())
        return ResultT<AuthResponseDto>::Failure("Invalid username or password.");

    if (!hasher_->verifyPassword(cmd.password, user->passwordHash))
        return ResultT<AuthResponseDto>::Failure("Invalid username or password.");

    AuthResponseDto dto;
    dto.username = user->username;
    dto.role = user->role;
    dto.token = tokenService_->generateToken(*user);
    return ResultT<AuthResponseDto>::Success(std::move(dto));
}

}  // namespace ecf::app
