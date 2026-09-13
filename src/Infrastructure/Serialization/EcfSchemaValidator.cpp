#include "Infrastructure/Serialization/EcfSchemaValidator.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <cstdarg>
#include <filesystem>
#include <fstream>
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

    auto p = std::filesystem::path(std::u8string_view(
        reinterpret_cast<const char8_t*>(xsdPath.data()), xsdPath.size()));
    std::ifstream file(p, std::ios::binary);
    if (!file) {
        return nullptr;
    }
    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    // Sanitize DGII non-standard regex syntax for libxml2 W3C compliance
    // 1. Replace (?: with (
    size_t pos = 0;
    while ((pos = content.find("(?:", pos)) != std::string::npos) {
        content.replace(pos, 3, "(");
        pos += 1;
    }
    // 2. Replace \- with -
    pos = 0;
    while ((pos = content.find("\\-", pos)) != std::string::npos) {
        content.replace(pos, 2, "-");
        pos += 1;
    }
    // 3. Fix missing IndicadorServicioTodoIncluidoType in DGII drafts if present
    if (content.find("IndicadorServicioTodoIncluidoType") != std::string::npos &&
        content.find("name=\"IndicadorServicioTodoIncluidoType\"") == std::string::npos) {
        const std::string missingType =
            "\n<xs:simpleType name=\"IndicadorServicioTodoIncluidoType\"><xs:restriction base=\"xs:integer\"><xs:enumeration value=\"1\"/><xs:enumeration value=\"2\"/></xs:restriction></xs:simpleType>\n";
        auto endTag = content.rfind("</xs:schema>");
        if (endTag != std::string::npos) {
            content.insert(endTag, missingType);
        }
    }

    xmlSchemaParserCtxtPtr parserCtxt =
        xmlSchemaNewMemParserCtxt(content.c_str(), static_cast<int>(content.size()));
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

    auto p = std::filesystem::path(std::u8string_view(
        reinterpret_cast<const char8_t*>(xsdPath.data()), xsdPath.size()));
    if (xsdPath.empty() || !std::filesystem::exists(p)) {
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
