#pragma once
// Ports of the Auth commands + AuthResponseDto.

#include <string>

namespace ecf::app {

struct AuthResponseDto {
    std::string username;
    std::string token;
    std::string role;
};

struct RegisterUserCommand {
    std::string username;
    std::string email;
    std::string password;
    std::string role;
};

struct LoginUserCommand {
    std::string username;
    std::string password;
};

}  // namespace ecf::app
