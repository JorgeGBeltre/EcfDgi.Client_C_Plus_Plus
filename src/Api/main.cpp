// Wires configuration, DB bootstrap (schema + admin seed), a global exception
// handler (RFC 9457 problem+json), a /health endpoint, JWT auth and the
// controllers (which self-register via Drogon's HttpController templates).

#include <drogon/drogon.h>
#include <spdlog/spdlog.h>

#include <cstdint>
#include <exception>
#include <string>
#include <thread>

#include "Api/AppServices.h"
#include "Api/Configuration/AppConfig.h"
#include "Application/Common/Exceptions/ValidationException.h"
#include "Infrastructure/Persistence/DbInitializer.h"

using namespace drogon;

namespace {

HttpResponsePtr problemDetails(HttpStatusCode code, const std::string& title,
                               const std::string& detail, const std::string& type) {
    Json::Value body;
    body["title"] = title;
    body["status"] = static_cast<int>(code);
    body["detail"] = detail;
    body["type"] = type;
    auto resp = HttpResponse::newHttpJsonResponse(body);
    resp->setStatusCode(code);
    resp->setContentTypeString("application/problem+json");
    return resp;
}

}  // namespace

int main() {
    spdlog::info("Starting web host (EcfDgii.Client C++)");

    ecf::api::AppConfig config;
    try {
        config = ecf::api::AppConfig::load("appsettings.json");
    } catch (const std::exception& ex) {
        spdlog::error("Failed to load configuration: {}", ex.what());
        return 1;
    }

    auto& services = ecf::api::AppServices::instance();
    services.configure(config);

    // Auto-apply schema and seed the default admin user on startup.
    try {
        ecf::infra::DbInitializer::initialize(config.connectionString, config.schemaPath,
                                              *services.passwordHasher());
        spdlog::info("Database schema applied and admin user ensured.");
    } catch (const std::exception& ex) {
        spdlog::warn("Database initialization skipped/failed: {}", ex.what());
    }

    // Global exception handler -> RFC 9457 ProblemDetails: a validation error
    // yields 400 with the field errors; any other error yields 500.
    app().setExceptionHandler([](const std::exception& e, const HttpRequestPtr& req,
                                 std::function<void(const HttpResponsePtr&)>&& cb) {
        if (const auto* ve = dynamic_cast<const ecf::app::ValidationException*>(&e)) {
            Json::Value body;
            body["title"] = "Validation Error";
            body["status"] = static_cast<int>(k400BadRequest);
            body["detail"] = ve->what();
            body["type"] = "https://tools.ietf.org/html/rfc9457#section-6.1";
            body["instance"] = req->getPath();
            Json::Value errors;
            for (const auto& [prop, msgs] : ve->errors()) {
                Json::Value arr(Json::arrayValue);
                for (const auto& m : msgs) arr.append(m);
                errors[prop] = arr;
            }
            body["errors"] = errors;
            auto out = HttpResponse::newHttpJsonResponse(body);
            out->setStatusCode(k400BadRequest);
            out->setContentTypeString("application/problem+json");
            cb(out);
            return;
        }
        spdlog::error("Unhandled exception: {}", e.what());
        cb(problemDetails(k500InternalServerError, "Internal Server Error", e.what(),
                          "https://tools.ietf.org/html/rfc9457#section-6.6"));
    });

    // Health check.
    app().registerHandler(
        "/health",
        [](const HttpRequestPtr&, std::function<void(const HttpResponsePtr&)>&& cb) {
            Json::Value body;
            body["status"] = "Healthy";
            cb(HttpResponse::newHttpJsonResponse(body));
        },
        {Get});

    const int threads =
        config.threads > 0 ? config.threads
                           : static_cast<int>(std::thread::hardware_concurrency());

    spdlog::info("Listening on {}:{}", config.serverHost, config.serverPort);

    app()
        .addListener(config.serverHost, static_cast<uint16_t>(config.serverPort))
        .setThreadNum(static_cast<size_t>(threads > 0 ? threads : 1))
        .setDocumentRoot("./")
        .run();

    return 0;
}
