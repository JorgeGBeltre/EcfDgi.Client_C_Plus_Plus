#pragma once

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

}
