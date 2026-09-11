#pragma once

#include <string>
#include <vector>

namespace ecf::domain {

struct SchemaValidationResult {
    bool isValid = true;
    std::vector<std::string> errors;

    void addError(const std::string& err) {
        isValid = false;
        errors.push_back(err);
    }
};

class IEcfSchemaValidator {
public:
    virtual ~IEcfSchemaValidator() = default;

    virtual SchemaValidationResult validate(const std::string& xmlContent,
                                            const std::string& xsdPath) = 0;
};

}  // namespace ecf::domain
