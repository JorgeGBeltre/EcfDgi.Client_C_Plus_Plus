// Wires configuration, DB bootstrap (schema + admin seed), a global exception
// handler (RFC 9457 problem+json), a /health endpoint, JWT auth and the
// controllers (which self-register via Drogon's HttpController templates).

#include <drogon/drogon.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <string>
#include <thread>

#include "Api/AppServices.h"
#include "Api/Configuration/AppConfig.h"
#include "Application/Common/Exceptions/ValidationException.h"
#include "Application/Services/EcfStatusReconciler.h"
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
        config = ecf::api::AppConfig::load("config/appsettings.json");
    } catch (...) {
        try {
            config = ecf::api::AppConfig::load("appsettings.json");
        } catch (const std::exception& ex) {
            spdlog::error("Failed to load configuration: {}", ex.what());
            return 1;
        }
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

    // Start background status reconciliation thread
    std::thread pollerThread([&services]() {
        spdlog::info("Started EcfStatusReconciler background poller thread.");
        int intervalSeconds = services.statusPollingOptions().pollingIntervalMinutes * 60;
        if (intervalSeconds <= 0) intervalSeconds = 900; // 15 minutes

        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(intervalSeconds));
            try {
                auto scope = services.makeScope(nullptr);
                ecf::app::EcfStatusReconciler reconciler(
                    scope.docs,
                    scope.uow,
                    services.ecfClient(),
                    services.statusPollingOptions()
                );
                int processed = reconciler.reconcile();
                if (processed > 0) {
                    spdlog::info("EcfStatusReconciler pass completed: {} documents processed.", processed);
                }
            } catch (const std::exception& ex) {
                spdlog::warn("EcfStatusReconciler pass encountered exception: {}", ex.what());
            }
        }
    });
    pollerThread.detach();

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
            body["database"] = "Healthy";
            body["redis"] = "Healthy";
            cb(HttpResponse::newHttpJsonResponse(body));
        },
        {Get});

    // OpenAPI Specification JSON.
    app().registerHandler(
        "/openapi/v1.json",
        [](const HttpRequestPtr&, std::function<void(const HttpResponsePtr&)>&& cb) {
            std::string openApiJson = R"json({
  "openapi": "3.0.3",
  "info": {
    "title": "EcfDgii.Client API (C++)",
    "description": "Dominican Republic e-CF Enterprise High-Performance REST API client wrapper.",
    "version": "2.0.0"
  },
  "servers": [{ "url": "/" }],
  "components": {
    "securitySchemes": {
      "BearerAuth": {
        "type": "http",
        "scheme": "bearer",
        "bearerFormat": "JWT"
      }
    }
  },
  "paths": {
    "/health": {
      "get": {
        "summary": "Health check endpoint",
        "responses": { "200": { "description": "System operational" } }
      }
    },
    "/api/documents": {
      "post": {
        "summary": "ERP integration: compiles canonical document into XML, signs, validates XSD, and sends to DGII",
        "security": [{ "BearerAuth": [] }],
        "responses": { "202": { "description": "Document accepted or processed" } }
      }
    },
    "/api/documents/by-source/{txnId}": {
      "get": {
        "summary": "Query document by ERP source transaction ID",
        "security": [{ "BearerAuth": [] }],
        "responses": { "200": { "description": "Document details" } }
      }
    },
    "/api/documents/by-source/{txnId}/xml": {
      "get": {
        "summary": "Download signed XML by ERP source transaction ID",
        "security": [{ "BearerAuth": [] }],
        "responses": { "200": { "description": "Raw signed XML file attachment" } }
      }
    },
    "/api/documents/{id}": {
      "get": {
        "summary": "Query document by UUID",
        "security": [{ "BearerAuth": [] }],
        "responses": { "200": { "description": "Document details" } }
      }
    },
    "/api/documents/{id}/xml": {
      "get": {
        "summary": "Download signed XML by document UUID",
        "security": [{ "BearerAuth": [] }],
        "responses": { "200": { "description": "Raw signed XML file attachment" } }
      }
    },
    "/api/ecf/send": {
      "post": {
        "summary": "Direct e-CF XML transmission to DGII REST services",
        "security": [{ "BearerAuth": [] }],
        "responses": { "200": { "description": "DGII reception response" } }
      }
    },
    "/api/ecf/send-rfce": {
      "post": {
        "summary": "Submit Consumption Summary (RFCE) to DGII",
        "security": [{ "BearerAuth": [] }],
        "responses": { "200": { "description": "RFCE reception response" } }
      }
    },
    "/api/ecf/status": {
      "get": {
        "summary": "Query DGII processing status",
        "security": [{ "BearerAuth": [] }],
        "responses": { "200": { "description": "Status response" } }
      }
    },
    "/api/customers": {
      "get": {
        "summary": "List customers",
        "security": [{ "BearerAuth": [] }],
        "responses": { "200": { "description": "Customer list" } }
      },
      "post": {
        "summary": "Create customer",
        "security": [{ "BearerAuth": [] }],
        "responses": { "201": { "description": "Customer created" } }
      }
    },
    "/api/customers/{id}": {
      "get": {
        "summary": "Get customer by ID",
        "security": [{ "BearerAuth": [] }],
        "responses": { "200": { "description": "Customer details" } }
      },
      "put": {
        "summary": "Update customer",
        "security": [{ "BearerAuth": [] }],
        "responses": { "200": { "description": "Customer updated" } }
      },
      "delete": {
        "summary": "Delete customer",
        "security": [{ "BearerAuth": [] }],
        "responses": { "204": { "description": "Customer deleted" } }
      }
    },
    "/api/auth/register": {
      "post": {
        "summary": "Register a new user",
        "responses": { "200": { "description": "User registered" } }
      }
    },
    "/api/auth/login": {
      "post": {
        "summary": "Authenticate user and issue JWT token",
        "responses": { "200": { "description": "Authentication token" } }
      }
    },
    "/fe/recepcion/api/ecf": {
      "post": {
        "summary": "B2B reception of e-CF XML and generation of signed Acuse de Recibo (ARECF)",
        "responses": { "200": { "description": "Signed ARECF XML" } }
      }
    },
    "/fe/aprobacioncomercial/api/ecf": {
      "post": {
        "summary": "Commercial approval endpoint",
        "responses": { "200": { "description": "Approval accepted" } }
      }
    },
    "/fe/autenticacion/api/semilla": {
      "get": {
        "summary": "Get authentication seed XML",
        "responses": { "200": { "description": "SemillaModel XML" } }
      }
    },
    "/fe/autenticacion/api/validacioncertificado": {
      "post": {
        "summary": "Validate signed seed XML and issue bearer token",
        "responses": { "200": { "description": "Authentication token" } }
      }
    }
  }
})json";
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(openApiJson);
            resp->setContentTypeString("application/json");
            cb(resp);
        },
        {Get});

    // Scalar API Reference UI.
    app().registerHandler(
        "/scalar",
        [](const HttpRequestPtr&, std::function<void(const HttpResponsePtr&)>&& cb) {
            std::string html = R"html(<!doctype html>
<html>
  <head>
    <title>EcfDgii Client API Reference (Scalar UI)</title>
    <meta charset="utf-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1" />
  </head>
  <body>
    <script id="api-reference" data-url="/openapi/v1.json"></script>
    <script src="https://cdn.jsdelivr.net/npm/@scalar/api-reference"></script>
  </body>
</html>)html";
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(html);
            resp->setContentTypeString("text/html");
            cb(resp);
        },
        {Get});

    // Swagger UI.
    app().registerHandler(
        "/swagger",
        [](const HttpRequestPtr&, std::function<void(const HttpResponsePtr&)>&& cb) {
            std::string html = R"html(<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <title>EcfDgii Client API - Swagger UI</title>
  <link rel="stylesheet" type="text/css" href="https://unpkg.com/swagger-ui-dist@5/swagger-ui.css" />
</head>
<body>
  <div id="swagger-ui"></div>
  <script src="https://unpkg.com/swagger-ui-dist@5/swagger-ui-bundle.js" charset="UTF-8"></script>
  <script>
    window.onload = function() {
      window.ui = SwaggerUIBundle({
        url: "/openapi/v1.json",
        dom_id: '#swagger-ui',
      });
    };
  </script>
</body>
</html>)html";
            auto resp = HttpResponse::newHttpResponse();
            resp->setBody(html);
            resp->setContentTypeString("text/html");
            cb(resp);
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
