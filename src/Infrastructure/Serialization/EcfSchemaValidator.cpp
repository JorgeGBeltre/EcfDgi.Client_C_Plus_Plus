#include "Infrastructure/Serialization/EcfSchemaValidator.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <filesystem>
#include <sstream>

namespace ecf::infra {

namespace {

void schemaErrorCallback(void* ctx, const char* msg, ...) {
    auto* errors = static_cast<std::vector<std::string>*>(ctx);
    char buf[1024];
    va_list args;
    va_start(args, msg);
    vsnprintf(buf, sizeof(buf), msg, args);
    va_end(args);

    std::string errStr(buf);
    // Remove trailing newline if present
    while (!errStr.empty() && (errStr.back() == '\n' || errStr.back() == '\r')) {
        errStr.pop_back();
    }
    if (!errStr.empty()) {
        errors->push_back(errStr);
    }
}

}  // namespace

EcfSchemaValidator::EcfSchemaValidator() = default;

EcfSchemaValidator::~EcfSchemaValidator() {
    std::unique_lock lock(cacheMutex_);
    for (auto& [path, schema] : schemaCache_) {
        if (schema) {
            xmlSchemaFree(schema);
        }
    }
    schemaCache_.clear();
}

xmlSchemaPtr EcfSchemaValidator::getOrCompileSchema(const std::string& xsdPath) {
    {
        std::shared_lock readLock(cacheMutex_);
        auto it = schemaCache_.find(xsdPath);
        if (it != schemaCache_.end()) {
            return it->second;
        }
    }

    std::unique_lock writeLock(cacheMutex_);
    // Double-check pattern
    auto it = schemaCache_.find(xsdPath);
    if (it != schemaCache_.end()) {
        return it->second;
    }

    xmlSchemaParserCtxtPtr parserCtxt = xmlSchemaNewParserCtxt(xsdPath.c_str());
    if (!parserCtxt) {
        return nullptr;
    }

    xmlSchemaPtr schema = xmlSchemaParse(parserCtxt);
    xmlSchemaFreeParserCtxt(parserCtxt);

    if (schema) {
        schemaCache_[xsdPath] = schema;
    }
    return schema;
}

domain::SchemaValidationResult EcfSchemaValidator::validate(const std::string& xmlContent,
                                                           const std::string& xsdPath) {
    domain::SchemaValidationResult result;

    if (xmlContent.empty()) {
        result.addError("XML content is empty.");
        return result;
    }

    if (xsdPath.empty() || !std::filesystem::exists(xsdPath)) {
        result.addError("XSD schema file not found at: " + xsdPath);
        return result;
    }

    xmlSchemaPtr schema = getOrCompileSchema(xsdPath);
    if (!schema) {
        result.addError("Failed to parse XSD schema at: " + xsdPath);
        return result;
    }

    xmlDocPtr doc = xmlReadMemory(xmlContent.c_str(), static_cast<int>(xmlContent.size()),
                                  "doc.xml", nullptr, 0);
    if (!doc) {
        result.addError("XML document is structurally malformed.");
        return result;
    }

    xmlSchemaValidCtxtPtr validCtxt = xmlSchemaNewValidCtxt(schema);
    if (!validCtxt) {
        xmlFreeDoc(doc);
        result.addError("Failed to create XSD validation context.");
        return result;
    }

    std::vector<std::string> validationErrors;
    xmlSchemaSetValidErrors(validCtxt, schemaErrorCallback, schemaErrorCallback, &validationErrors);

    int status = xmlSchemaValidateDoc(validCtxt, doc);

    xmlSchemaFreeValidCtxt(validCtxt);
    xmlFreeDoc(doc);

    if (status != 0 || !validationErrors.empty()) {
        result.isValid = false;
        if (validationErrors.empty()) {
            result.errors.push_back("Schema validation failed with exit code: " + std::to_string(status));
        } else {
            result.errors = std::move(validationErrors);
        }
    }

    return result;
}

}  // namespace ecf::infra
