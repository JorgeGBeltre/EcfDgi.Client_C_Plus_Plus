#pragma once
// Hashes and verifies passwords using libsodium Argon2id (crypto_pwhash_str):
// a modern, salted, adaptive password hash stored in a self-describing format.

#include <string>

#include "Domain/Interfaces/ISecurity.h"

namespace ecf::infra {

class PasswordHasher : public domain::IPasswordHasher {
public:
    PasswordHasher();
    std::string hashPassword(const std::string& password) override;
    bool verifyPassword(const std::string& password,
                        const std::string& hashedPassword) override;
};

}  // namespace ecf::infra
