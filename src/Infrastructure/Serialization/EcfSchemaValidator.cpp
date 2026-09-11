#include "Infrastructure/Serialization/EcfSchemaValidator.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <cstdarg>
#include <filesystem>
#include <sstream>

#include "Infrastructure/Serialization/EcfXsdFileNameResolver.h"

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

EcfSchemaValidator::EcfSchemaValidator(const std::optional<std::string>& xsdDirectoryPath)
    : xsdDirectoryPath_(xsdDirectoryPath) {}

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
        result.addError("Failed to create schema validation context.");
        return result;
    }

    xmlSchemaSetValidErrors(validCtxt, schemaErrorCallback, nullptr, &result.errors);

    int val = xmlSchemaValidateDoc(validCtxt, doc);
    if (val != 0) {
        result.isValid = false;
        if (result.errors.empty()) {
            result.errors.push_back("Schema validation failed with code: " + std::to_string(val));
        }
    } else {
        result.isValid = true;
    }

    xmlSchemaFreeValidCtxt(validCtxt);
    xmlFreeDoc(doc);

    return result;
}

domain::SchemaValidationResult EcfSchemaValidator::validate(const std::string& xmlContent) {
    if (!xsdDirectoryPath_.has_value() || xsdDirectoryPath_->empty()) {
        domain::SchemaValidationResult r;
        return r;
    }
    std::string xsdFileName = EcfXsdFileNameResolver::resolve(xmlContent);
    if (xsdFileName.empty()) {
        domain::SchemaValidationResult r;
        return r;
    }
    std::string xsdPath = *xsdDirectoryPath_ + "/" + xsdFileName;
    return validate(xmlContent, xsdPath);
}

}  // namespace ecf::infra
