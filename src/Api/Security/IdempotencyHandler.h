#pragma once

#include <string>
#include <functional>
#include <drogon/HttpTypes.h>
#include <drogon/HttpResponse.h>
#include <json/json.h>
#include "Domain/Interfaces/IIdempotencyStore.h"
#include "Infrastructure/Security/CanonicalRequestHelper.h"

namespace ecf::api {

class IdempotencyHandler {
public:
    static void handle(
        const std::shared_ptr<domain::IIdempotencyStore>& store,
        const drogon::HttpRequestPtr& req,
        const std::string& tenantId,
        const std::string& workerKeyId,
        std::function<void(const drogon::HttpResponsePtr&)>&& callback,
        std::function<void(std::function<void(const drogon::HttpResponsePtr&)>&&)>&& next) {

        const std::string& idempotencyKey = req->getHeader("x-idempotency-key");
        if (idempotencyKey.empty() || 
            (req->method() != drogon::Post && req->method() != drogon::Put && req->method() != drogon::Patch)) {
            next(std::move(callback));
            return;
        }

        std::string scopedKey = tenantId + ":" + idempotencyKey;
        std::string body(req->bodyData(), req->bodyLength());
        std::string payloadHash = infra::CanonicalRequestHelper::computeSha256Hex(body);

        auto reservation = store->reserveOrGet(scopedKey, payloadHash, workerKeyId);

        if (reservation.status == domain::IdempotencyReservationStatus::AlreadyCompleted && reservation.completedResult) {
            auto res = drogon::HttpResponse::newHttpResponse();
            res->setStatusCode(static_cast<drogon::HttpStatusCode>(reservation.completedResult->statusCode));
            res->setContentTypeString(reservation.completedResult->contentType);
            res->setBody(reservation.completedResult->body);
            res->addHeader("X-Cache-Lookup", "Hit-Idempotent");
            callback(res);
            return;
        }

        if (reservation.status == domain::IdempotencyReservationStatus::AlreadyProcessing) {
            Json::Value err;
            err["error"] = "Request with this Idempotency-Key is currently being processed.";
            auto res = drogon::HttpResponse::newHttpJsonResponse(err);
            res->setStatusCode(drogon::k409Conflict);
            callback(res);
            return;
        }

        if (reservation.status == domain::IdempotencyReservationStatus::PayloadMismatch) {
            Json::Value err;
            err["error"] = "Idempotency key payload mismatch. The same key was used with a different request body.";
            auto res = drogon::HttpResponse::newHttpJsonResponse(err);
            res->setStatusCode(drogon::k422UnprocessableEntity);
            callback(res);
            return;
        }

        // Status == Reserved: Execute handler and capture the response
        next([store, scopedKey, callback = std::move(callback)](const drogon::HttpResponsePtr& resp) {
            int sc = resp->statusCode();
            // Selective caching: Cache business domain results (2xx, 400, 404, 422).
            bool shouldCache = sc >= 200 && sc < 500 &&
                               sc != 401 && sc != 403 && sc != 408 && sc != 409 && sc != 429;

            if (shouldCache) {
                domain::IdempotentResult result;
                result.statusCode = sc;
                result.contentType = resp->contentTypeString();
                result.body = std::string(resp->body().data(), resp->body().size());
                store->complete(scopedKey, result);
            }
            callback(resp);
        });
    }
};

} // namespace ecf::api
