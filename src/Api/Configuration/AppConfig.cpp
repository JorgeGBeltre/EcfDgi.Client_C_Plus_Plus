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
        if (s.contains("AutoRetryOnReuseableSequence"))
            o.autoRetryOnReuseableSequence = s["AutoRetryOnReuseableSequence"].get<bool>();
    }

    // Parse Worker configurations
    cfg.workerKeyId = getStr(j, "WorkerKeyId", cfg.workerKeyId);
    cfg.workerSecretKey = getStr(j, "WorkerSecretKey", cfg.workerSecretKey);
    if (j.contains("WorkerApiKey")) {
        cfg.workerSecretKey = getStr(j, "WorkerApiKey");
    }
    cfg.workerTenantId = getStr(j, "WorkerTenantId", cfg.workerTenantId);
    cfg.workerAllowedRncs = getStr(j, "WorkerAllowedRncs", cfg.workerAllowedRncs);

    // Support env overrides
    if (const char* envKeyId = std::getenv("WORKER_KEY_ID")) cfg.workerKeyId = envKeyId;
    if (const char* envSecret = std::getenv("WORKER_SECRET_KEY")) cfg.workerSecretKey = envSecret;
    if (const char* envTenant = std::getenv("WORKER_TENANT_ID")) cfg.workerTenantId = envTenant;
    if (const char* envRncs = std::getenv("WORKER_ALLOWED_RNCS")) cfg.workerAllowedRncs = envRncs;

    return cfg;
}

}  // namespace ecf::api
