#pragma once
// Loads appsettings.json into strongly-typed configuration options.

#include <string>

#include "Domain/Entities/EcfClientOptions.h"
#include "Infrastructure/Security/JwtSettings.h"

namespace ecf::api {

struct AppConfig {
    std::string serverHost = "0.0.0.0";
    int serverPort = 8080;
    int threads = 0;  // 0 => hardware_concurrency

    std::string connectionString;
    std::string schemaPath = "db/schema.sql";

    infra::JwtSettings jwt;
    domain::EcfClientOptions ecfOptions;

    // Loads from a JSON file; throws std::runtime_error on failure.
    static AppConfig load(const std::string& path);
};

}  // namespace ecf::api
