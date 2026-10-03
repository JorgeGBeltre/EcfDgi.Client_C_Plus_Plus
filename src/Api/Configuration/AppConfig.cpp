#include "Api/Configuration/AppConfig.h"

#include <nlohmann/json.hpp>

#include <cstdlib>
#include <fstream>
#include <stdexcept>

namespace ecf::api {

using json = nlohmann::json;
using domain::EcfEnvironment;
using domain::IntegrationMode;

namespace {

std::string getStr(const json& j, const char* key, const std::string& def = "") {
    return (j.contains(key) && !j[key].is_null()) ? j[key].get<std::string>() : def;
}

EcfEnvironment parseEnv(const std::string& s) {
    if (s == "Cert") return EcfEnvironment::Cert;
    if (s == "Prod") return EcfEnvironment::Prod;
    return EcfEnvironment::Test;
}

static std::string normalizePostgresConnectionString(const std::string& input) {
    if (input.empty() || input.find(';') == std::string::npos) {
        return input;
    }
    std::string out;
    std::stringstream ss(input);
    std::string item;
    while (std::getline(ss, item, ';')) {
        auto eq = item.find('=');
        if (eq == std::string::npos) continue;
        std::string key = item.substr(0, eq);
        std::string val = item.substr(eq + 1);
        while (!key.empty() && std::isspace(static_cast<unsigned char>(key.front()))) key.erase(key.begin());
        while (!key.empty() && std::isspace(static_cast<unsigned char>(key.back()))) key.pop_back();
        std::string lowerKey = key;
        for (char& c : lowerKey) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        if (lowerKey == "host" || lowerKey == "server") key = "host";
        else if (lowerKey == "port") key = "port";
        else if (lowerKey == "database" || lowerKey == "initial catalog") key = "dbname";
        else if (lowerKey == "username" || lowerKey == "user id" || lowerKey == "user") key = "user";
        else if (lowerKey == "password") key = "password";
        else continue;

        if (!out.empty()) out += " ";
        out += key + "=" + val;
    }
    return out.empty() ? input : out;
}

}  // namespace

AppConfig AppConfig::load(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("No se pudo abrir la configuración: " + path);

    json j;
    f >> j;

    AppConfig cfg;

    if (j.contains("Server")) {
        const auto& s = j["Server"];
        cfg.serverHost = getStr(s, "Host", cfg.serverHost);
        if (s.contains("Port")) cfg.serverPort = s["Port"].get<int>();
        if (s.contains("Threads")) cfg.threads = s["Threads"].get<int>();
    }

    if (j.contains("ConnectionStrings")) {
        cfg.connectionString = getStr(j["ConnectionStrings"], "DefaultConnection");
        cfg.redisConnectionString = getStr(j["ConnectionStrings"], "Redis");
    }

    // Allow the connection strings to be overridden via environment variables.
    if (const char* env = std::getenv("ConnectionStrings__DefaultConnection"))
        cfg.connectionString = env;
    else if (const char* envDb = std::getenv("DATABASE_URL"))
        cfg.connectionString = envDb;

    cfg.connectionString = normalizePostgresConnectionString(cfg.connectionString);

    if (const char* redisEnv = std::getenv("ConnectionStrings__Redis"))
        cfg.redisConnectionString = redisEnv;
    else if (const char* redisUrlEnv = std::getenv("REDIS_URL"))
        cfg.redisConnectionString = redisUrlEnv;

    if (j.contains("JwtSettings")) {
        const auto& s = j["JwtSettings"];
        cfg.jwt.secret = getStr(s, "Secret");
        if (s.contains("ExpirationMinutes")) cfg.jwt.expirationMinutes = s["ExpirationMinutes"].get<int>();
        cfg.jwt.issuer = getStr(s, "Issuer");
        cfg.jwt.audience = getStr(s, "Audience");
    }
    if (const char* envJwt = std::getenv("JwtSettings__Secret")) cfg.jwt.secret = envJwt;
    else if (const char* envJwt2 = std::getenv("JWT_SECRET")) cfg.jwt.secret = envJwt2;

    if (cfg.jwt.secret.empty()) {
        const char* aspEnv = std::getenv("ASPNETCORE_ENVIRONMENT");
        if (aspEnv && std::string(aspEnv) == "Production") {
            throw std::runtime_error("Seguridad: JwtSettings:Secret es obligatorio y no puede estar vacío en ambiente de producción.");
        }
        cfg.jwt.secret = "e_CF_Dominican_Tax_Authority_Secure_JWT_Secret_Token_2026_Key_Length_Minimum_32_Bytes!";
    }

    if (const char* envCert = std::getenv("CERT_ENCRYPTION_KEY")) cfg.certEncryptionKey = envCert;
    else if (const char* envCert2 = std::getenv("Security__CertificateEncryptionKey")) cfg.certEncryptionKey = envCert2;

    if (cfg.jwt.secret.length() < 32) {
        throw std::runtime_error("Seguridad: JwtSettings:Secret debe tener al menos 32 caracteres (256 bits) para firma HMAC-SHA256.");
    }

    if (j.contains("EcfClientOptions")) {
        const auto& s = j["EcfClientOptions"];
        auto& o = cfg.ecfOptions;
        o.apiKey = getStr(s, "ApiKey");
        o.baseUrl = getStr(s, "BaseUrl");
        o.environment = parseEnv(getStr(s, "Environment", "Test"));
        o.mode = IntegrationMode::DgiiDirect;
        o.rncEmisor = getStr(s, "RncEmisor");
        o.certificatePath = getStr(s, "CertificatePath");
        o.certificatePassword = getStr(s, "CertificatePassword");
        if (const char* envCertPass = std::getenv("ECF_CERTIFICATE_PASSWORD")) o.certificatePassword = envCertPass;
        else if (const char* envCertPass2 = std::getenv("EcfClientOptions__CertificatePassword")) o.certificatePassword = envCertPass2;
        if (s.contains("AutoRetryOnReuseableSequence"))
            o.autoRetryOnReuseableSequence = s["AutoRetryOnReuseableSequence"].get<bool>();
        if (s.contains("ValidateSchemasLocal"))
            o.validateSchemasLocal = s["ValidateSchemasLocal"].get<bool>();
        if (s.contains("XsdDirectoryPath"))
            o.xsdDirectoryPath = getStr(s, "XsdDirectoryPath");
    }

    if (const char* envEcfEnv = std::getenv("ECF_ENVIRONMENT")) cfg.ecfOptions.environment = parseEnv(envEcfEnv);
    else if (const char* envEcfEnv2 = std::getenv("EcfClientOptions__Environment")) cfg.ecfOptions.environment = parseEnv(envEcfEnv2);

    if (const char* envBaseUrl = std::getenv("ECF_BASE_URL")) cfg.ecfOptions.baseUrl = envBaseUrl;
    else if (const char* envBaseUrl2 = std::getenv("EcfClientOptions__BaseUrl")) cfg.ecfOptions.baseUrl = envBaseUrl2;

    if (j.contains("EcfEmisor")) {
        const auto& s = j["EcfEmisor"];
        cfg.emisorOptions.rnc = getStr(s, "Rnc");
        cfg.emisorOptions.razonSocial = getStr(s, "RazonSocial");
    } else if (cfg.ecfOptions.rncEmisor.has_value() && !cfg.ecfOptions.rncEmisor->empty()) {
        cfg.emisorOptions.rnc = *cfg.ecfOptions.rncEmisor;
        cfg.emisorOptions.razonSocial = "WILLY CHIC DOMINICANA SRL";
    }

    if (j.contains("EcfStatusPolling")) {
        const auto& s = j["EcfStatusPolling"];
        if (s.contains("PollingIntervalMinutes"))
            cfg.statusPollingOptions.pollingIntervalMinutes = s["PollingIntervalMinutes"].get<int>();
        if (s.contains("MinDocumentAgeMinutes"))
            cfg.statusPollingOptions.minDocumentAgeMinutes = s["MinDocumentAgeMinutes"].get<int>();
        if (s.contains("MaxPollingWindowHours"))
            cfg.statusPollingOptions.maxPollingWindowHours = s["MaxPollingWindowHours"].get<int>();
    }

    // Support env overrides
    if (const char* envRnc = std::getenv("ECF_EMISOR_RNC")) {
        cfg.emisorOptions.rnc = envRnc;
        cfg.ecfOptions.rncEmisor = envRnc;
    } else if (const char* envRnc2 = std::getenv("EcfClientOptions__RncEmisor")) {
        cfg.emisorOptions.rnc = envRnc2;
        cfg.ecfOptions.rncEmisor = envRnc2;
    }
    if (const char* envRazon = std::getenv("ECF_EMISOR_RAZON_SOCIAL")) cfg.emisorOptions.razonSocial = envRazon;
    if (const char* envXsd = std::getenv("ECF_XSD_DIR")) cfg.ecfOptions.xsdDirectoryPath = envXsd;
    if (const char* envSchema = std::getenv("SCHEMA_PATH")) cfg.schemaPath = envSchema;

    // Parse Worker configurations
    cfg.workerKeyId = getStr(j, "WorkerKeyId", cfg.workerKeyId);
    cfg.workerSecretKey = getStr(j, "WorkerSecretKey", cfg.workerSecretKey);
    if (j.contains("WorkerApiKey")) {
        cfg.workerSecretKey = getStr(j, "WorkerApiKey");
    }
    cfg.workerTenantId = getStr(j, "WorkerTenantId", cfg.workerTenantId);
    cfg.workerAllowedRncs = getStr(j, "WorkerAllowedRncs", cfg.workerAllowedRncs);

    if (const char* envKeyId = std::getenv("WORKER_KEY_ID")) cfg.workerKeyId = envKeyId;
    if (const char* envSecret = std::getenv("WORKER_SECRET_KEY")) cfg.workerSecretKey = envSecret;
    if (const char* envTenant = std::getenv("WORKER_TENANT_ID")) cfg.workerTenantId = envTenant;
    if (const char* envRncs = std::getenv("WORKER_ALLOWED_RNCS")) cfg.workerAllowedRncs = envRncs;

    return cfg;
}

}  // namespace ecf::api
