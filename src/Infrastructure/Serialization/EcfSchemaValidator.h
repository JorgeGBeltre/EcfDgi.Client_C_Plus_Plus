#pragma once

#include <libxml/xmlschemas.h>
#include <memory>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

#include "Domain/Interfaces/IEcfSchemaValidator.h"

namespace ecf::infra {

class EcfSchemaValidator : public domain::IEcfSchemaValidator {
public:
    explicit EcfSchemaValidator(const std::optional<std::string>& xsdDirectoryPath = std::nullopt);
    ~EcfSchemaValidator() override;

    domain::SchemaValidationResult validate(const std::string& xmlContent,
                                            const std::string& xsdPath) override;
    domain::SchemaValidationResult validate(const std::string& xmlContent) override;

private:
    xmlSchemaPtr getOrCompileSchema(const std::string& xsdPath);

    std::optional<std::string> xsdDirectoryPath_;
    std::shared_mutex cacheMutex_;
    std::unordered_map<std::string, xmlSchemaPtr> schemaCache_;
};

}  // namespace ecf::infra
