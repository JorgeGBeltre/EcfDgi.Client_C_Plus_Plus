#include "Api/Filters/UserOrWorkerFilter.h"
#include <jwt-cpp/jwt.h>
#include <string>
#include <chrono>
#include <cmath>
#include "Api/AppServices.h"
#include "Infrastructure/Security/CanonicalRequestHelper.h"

using namespace drogon;

namespace ecf::api {

namespace {
HttpResponsePtr unauthorized(const std::string& detail) {
    Json::Value body;
    body["title"] = "Unauthorized";
    body["status"] = 401;
    body["detail"] = detail;
    auto resp = HttpResponse::newHttpJsonResponse(body);
    resp->setStatusCode(k401Unauthorized);
    resp->setContentTypeString("application/problem+json");
    return resp;
}

long long currentUnixTimestamp() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
}
}  // namespace

void UserOrWorkerFilter::doFilter(const HttpRequestPtr& req, FilterCallback&& fcb,
                                  FilterChainCallback&& fccb) {
    const std::string& auth = req->getHeader("authorization");
    const std::string prefix = "Bearer ";

    // 1. Try JWT Bearer Authentication first
    if (auth.size() > prefix.size() && auth.compare(0, prefix.size(), prefix) == 0) {
        const std::string token = auth.substr(prefix.size());
        const auto& jwtCfg = AppServices::instance().config().jwt;

        try {
            auto decoded = jwt::decode(token);
            auto verifier = jwt::verify()
                                .allow_algorithm(jwt::algorithm::hs256{jwtCfg.secret})
                                .with_issuer(jwtCfg.issuer)
                                .with_audience(jwtCfg.audience);
            verifier.verify(decoded);

            auto getClaim = [&](const char* name) -> std::string {
                return decoded.has_payload_claim(name)
                           ? decoded.get_payload_claim(name).as_string()
                           : std::string{};
            };

            req->attributes()->insert("userId", getClaim("nameid"));
            req->attributes()->insert("username", getClaim("name"));
            req->attributes()->insert("role", getClaim("role"));
            req->attributes()->insert("tenantId", getClaim("tenant_id").empty() ? "default-tenant" : getClaim("tenant_id"));
            req->attributes()->insert("clientType", std::string("user"));
            fccb();
            return;
        } catch (const std::exception& ex) {
            fcb(unauthorized(std::string("Invalid token: ") + ex.what()));
            return;
        }
    }

    // 2. Try Worker HMAC Authentication
    const std::string& keyId = req->getHeader("x-worker-key-id");
    const std::string& timestampStr = req->getHeader("x-request-timestamp");
    const std::string& nonce = req->getHeader("x-request-nonce");
    const std::string& signature = req->getHeader("x-request-signature");

    if (!keyId.empty() || !timestampStr.empty() || !nonce.empty() || !signature.empty()) {
        if (keyId.empty() || timestampStr.empty() || nonce.empty() || signature.empty()) {
            fcb(unauthorized("Missing required worker authentication headers (X-Worker-Key-Id, X-Request-Timestamp, X-Request-Nonce, X-Request-Signature)."));
            return;
        }

        // Verify Timestamp & Clock Drift
        long long clientTs = 0;
        try {
            clientTs = std::stoll(timestampStr);
        } catch (...) {
            fcb(unauthorized("Invalid timestamp format."));
            return;
        }

        long long serverTs = currentUnixTimestamp();
        long long driftSeconds = std::abs(serverTs - clientTs);
        if (driftSeconds > 300) {
            fcb(unauthorized("Request expired due to clock drift (drift: " + std::to_string(driftSeconds) + "s, max allowed: 300s). Code: timestamp_drift."));
            return;
        }

        // Resolve Worker Key
        const auto& config = AppServices::instance().config();
        
        if (config.workerKeyId.empty()) {
            fcb(unauthorized("Worker authentication is not configured."));
            return;
        }

        if (keyId != config.workerKeyId) {
            fcb(unauthorized("Unknown worker key ID. Code: unknown_key_id."));
            return;
        }

        // Read Request Body & Verify HMAC Signature
        std::string body(req->bodyData(), req->bodyLength());
        std::string pathAndQuery = req->path();
        if (!req->query().empty()) {
            pathAndQuery += "?" + req->query();
        }

        std::string canonicalString = infra::CanonicalRequestHelper::buildCanonicalString(
            req->methodString(), pathAndQuery, timestampStr, nonce, body
        );
        std::string computedSignature = infra::CanonicalRequestHelper::computeHmacSha256(
            config.workerSecretKey, canonicalString
        );

        if (!infra::CanonicalRequestHelper::safeCompare(computedSignature, signature)) {
            fcb(unauthorized("Invalid signature. Code: bad_signature."));
            return;
        }

        // Verify Anti-Replay Nonce
        auto nonceCache = AppServices::instance().nonceCache();
        if (!nonceCache->tryAddNonce(keyId, nonce, 300.0)) {
            fcb(unauthorized("Replayed nonce detected. Code: nonce_replayed."));
            return;
        }

        // Build Authenticated Claims
        req->attributes()->insert("userId", keyId);
        req->attributes()->insert("username", std::string("Worker:") + keyId);
        req->attributes()->insert("role", std::string("Worker"));
        req->attributes()->insert("tenantId", config.workerTenantId);
        req->attributes()->insert("workerKeyId", keyId);
        req->attributes()->insert("clientType", std::string("worker"));
        
        fccb();
        return;
    }

    fcb(unauthorized("Missing or malformed Authorization header or Worker credentials."));
}

} // namespace ecf::api
