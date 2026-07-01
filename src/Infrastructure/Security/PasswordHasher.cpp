#include "Infrastructure/Security/PasswordHasher.h"

#include <sodium.h>

#include <stdexcept>

namespace ecf::infra {

PasswordHasher::PasswordHasher() {
    if (sodium_init() < 0) {
        throw std::runtime_error("No se pudo inicializar libsodium.");
    }
}

std::string PasswordHasher::hashPassword(const std::string& password) {
    char hashed[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(hashed, password.c_str(), password.size(),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        throw std::runtime_error("No se pudo generar el hash de la contraseña (out of memory).");
    }
    return std::string(hashed);
}

bool PasswordHasher::verifyPassword(const std::string& password,
                                    const std::string& hashedPassword) {
    return crypto_pwhash_str_verify(hashedPassword.c_str(), password.c_str(),
                                    password.size()) == 0;
}

}  // namespace ecf::infra
