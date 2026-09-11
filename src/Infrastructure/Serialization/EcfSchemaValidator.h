#pragma once

#include <libxml/xmlschemas.h>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "Domain/Interfaces/IEcfSchemaValidator.h"

namespace ecf::infra {

class EcfSchemaValidator : public domain::IEcfSchemaValidator {
public:
    EcfSchemaValidator();
    ~EcfSchemaValidator() override;

    domain::SchemaValidationResult validate(const std::string& xmlContent,
                                            const std::string& xsdPath) override;

private:
    xmlSchemaPtr getOrCompileSchema(const std::string& xsdPath);

    std::shared_mutex cacheMutex_;
    std::unordered_map<std::string, xmlSchemaPtr> schemaCache_;
};

}  // namespace ecf::infra
