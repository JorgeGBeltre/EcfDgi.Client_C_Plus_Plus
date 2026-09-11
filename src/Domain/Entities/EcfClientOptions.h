#pragma once

#include <optional>
#include <string>

namespace ecf::domain {

enum class EcfEnvironment { Test, Cert, Prod };

enum class IntegrationMode { DgiiDirect };

// DGII environment identifier used when selecting endpoints.
enum class AmbienteEnum { PreCertificacion = 1, Certificacion = 2, Produccion = 3 };

struct EcfClientOptions {
    std::optional<std::string> apiKey;
    std::optional<std::string> baseUrl;
    EcfEnvironment environment = EcfEnvironment::Test;
    IntegrationMode mode = IntegrationMode::DgiiDirect;
    std::optional<std::string> rncEmisor;
    std::optional<std::string> certificatePath;
    std::optional<std::string> certificatePassword;
    bool autoRetryOnReuseableSequence = true;
    std::optional<std::string> xsdDirectoryPath;
    bool validateSchemasLocal = true;
};

struct EcfEmisorOptions {
    std::string rnc;
    std::string razonSocial;
};

struct EcfStatusPollingOptions {
    int pollingIntervalMinutes = 15;
    int minDocumentAgeMinutes = 2;
    int maxPollingWindowHours = 72;
};

struct PollingOptions {
    int initialDelayMs = 1000;
    int maxDelayMs = 30000;
    int maxRetries = 60;
    double backoffMultiplier = 2;
    std::optional<int> timeoutMs;
};

}  // namespace ecf::domain
