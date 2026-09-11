#pragma once

#include <string>

namespace ecf::infra {

class EcfXsdFileNameResolver {
public:
    static std::string resolve(const std::string& xmlContent);
};

}  // namespace ecf::infra
