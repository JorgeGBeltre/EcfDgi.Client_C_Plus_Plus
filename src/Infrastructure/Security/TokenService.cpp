#include "Infrastructure/Security/TokenService.h"

#include <jwt-cpp/jwt.h>

#include <chrono>

namespace ecf::infra {

std::string TokenService::generateToken(const domain::User& user) {
    const auto now = std::chrono::system_clock::now();
    const auto exp = now + std::chrono::minutes(settings_.expirationMinutes);

    return jwt::create()
        .set_issuer(settings_.issuer)
        .set_audience(settings_.audience)
        .set_issued_at(now)
        .set_expires_at(exp)
        .set_payload_claim("nameid", jwt::claim(user.id))
        .set_payload_claim("name", jwt::claim(user.username))
        .set_payload_claim("email", jwt::claim(user.email))
        .set_payload_claim("role", jwt::claim(user.role))
        .sign(jwt::algorithm::hs256{settings_.secret});
}

}  // namespace ecf::infra
