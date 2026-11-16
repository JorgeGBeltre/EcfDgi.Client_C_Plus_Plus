#pragma once

#include <string>

#include "Domain/Entities/EcfClientOptions.h"
#include "Infrastructure/Security/JwtSettings.h"

namespace ecf::api {

struct AppConfig {
    std::string serverHost = "0.0.0.0";
    int serverPort = 8080;
    int threads = 0;

    std::string connectionString;
    std::string redisConnectionString;
    std::string schemaPath = "db/schema.sql";

    std::string certEncryptionKey;
    infra::JwtSettings jwt;
    domain::EcfClientOptions ecfOptions;
    domain::EcfEmisorOptions emisorOptions;
    domain::EcfStatusPollingOptions statusPollingOptions;

    std::string workerKeyId = "default-worker-id";
    std::string workerSecretKey = "WorkerSecretKey";
    std::string workerTenantId = "default-tenant";
    std::string workerAllowedRncs = "*";

    static AppConfig load(const std::string& path);
};

}
