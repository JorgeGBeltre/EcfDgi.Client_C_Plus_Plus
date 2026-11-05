#include "Infrastructure/Persistence/DbIdempotencyStore.h"
#include <json/json.h>
#include <pqxx/pqxx>
#include <sstream>
#include <stdexcept>
#include "Infrastructure/Persistence/RowMappers.h"

namespace ecf::infra {

using domain::IdempotencyReservationResult;
using domain::IdempotencyReservationStatus;
using domain::IdempotentResult;

namespace {

bool isUnderlyingDocumentRejected(pqxx::work& w, const std::string& responseBody) {
    if (responseBody.empty() || responseBody.find("\"documentId\"") == std::string::npos) {
        return false;
    }
    try {
        Json::CharReaderBuilder builder;
        Json::Value root;
        std::string errs;
        std::istringstream iss(responseBody);
        if (Json::parseFromStream(builder, iss, &root, &errs) &&
            root.isObject() && root.isMember("documentId") && root["documentId"].isString()) {
            std::string docId = root["documentId"].asString();
            if (!docId.empty()) {
                pqxx::result docRes = w.exec_params(
                    "SELECT state FROM ecf_documents WHERE id = $1 LIMIT 1",
                    docId);
                if (!docRes.empty() && !docRes[0]["state"].is_null()) {
                    std::string state = docRes[0]["state"].as<std::string>();
                    return (state == "RejectedByDgii" || state == "SchemaInvalid" || state == "SigningFailed");
                }
            }
        }
    } catch (...) {}
    return false;
}

} // namespace

IdempotencyReservationResult DbIdempotencyStore::reserveOrGet(
    const std::string& key, 
    const std::string& payloadHash, 
    const std::string& workerKeyId) {
    
    pqxx::connection conn(connectionString_);
    pqxx::work w(conn);

    // 1. Fetch existing record
    pqxx::result r = w.exec_params(
        "SELECT key, created_by_worker_key_id, payload_hash, status, status_code, "
        "       content_type, response_body, created_at::text, updated_at::text, expires_at::text, "
        "       (created_at < NOW() - INTERVAL '5 minutes' OR (expires_at IS NOT NULL AND expires_at < NOW())) as is_lease_expired "
        "FROM ecf_idempotency_records WHERE key = $1",
        key
    );

    if (!r.empty()) {
        const auto& row = r[0];
        std::string existingHash = row["payload_hash"].template as<std::string>();
        std::string statusStr = row["status"].template as<std::string>();
        int statusCode = row["status_code"].template as<int>();
        std::string responseBody = row["response_body"].template as<std::string>();

        if (existingHash != payloadHash) {
            // Allow retry with corrected payload if previous attempt failed (>= 400) or was rejected by DGII
            if (statusCode >= 400 || isUnderlyingDocumentRejected(w, responseBody)) {
                w.exec_params(
                    "UPDATE ecf_idempotency_records "
                    "SET payload_hash = $2, status = 'Processing', status_code = 0, response_body = '', "
                    "    created_at = NOW(), updated_at = NOW(), created_by_worker_key_id = $3 "
                    "WHERE key = $1",
                    key, payloadHash, workerKeyId
                );
                w.commit();
                return IdempotencyReservationResult{IdempotencyReservationStatus::Reserved, std::nullopt};
            }
            return IdempotencyReservationResult{IdempotencyReservationStatus::PayloadMismatch, std::nullopt};
        }

        if (statusStr == "Completed") {
            if (statusCode >= 200 && statusCode < 300 && !isUnderlyingDocumentRejected(w, responseBody)) {
                IdempotentResult res;
                res.statusCode = statusCode;
                res.contentType = row["content_type"].template as<std::string>();
                res.body = responseBody;
                return IdempotencyReservationResult{IdempotencyReservationStatus::AlreadyCompleted, res};
            }

            // Previous completion failed (>= 400) or was rejected by DGII: allow retry
            w.exec_params(
                "UPDATE ecf_idempotency_records "
                "SET status = 'Processing', status_code = 0, response_body = '', "
                "    created_at = NOW(), updated_at = NOW(), created_by_worker_key_id = $2 "
                "WHERE key = $1",
                key, workerKeyId
            );
            w.commit();
            return IdempotencyReservationResult{IdempotencyReservationStatus::Reserved, std::nullopt};
        }

        bool isLeaseExpired = row["is_lease_expired"].template as<bool>();
        if (statusStr == "Processing" && isLeaseExpired) {
            // Reclaim lease atomically
            pqxx::result upR = w.exec_params(
                "UPDATE ecf_idempotency_records "
                "SET created_at = NOW(), "
                "    created_by_worker_key_id = $2, "
                "    updated_at = NOW() "
                "WHERE key = $1 AND status = 'Processing' AND created_at < NOW() - INTERVAL '5 minutes'",
                key, workerKeyId
            );

            if (upR.affected_rows() > 0) {
                w.commit();
                return IdempotencyReservationResult{IdempotencyReservationStatus::Reserved, std::nullopt};
            }
        }

        return IdempotencyReservationResult{IdempotencyReservationStatus::AlreadyProcessing, std::nullopt};
    }

    // 2. Try to insert new record
    try {
        w.exec_params(
            "INSERT INTO ecf_idempotency_records (key, created_by_worker_key_id, payload_hash, status, "
            "       status_code, content_type, response_body, created_at, updated_at, expires_at) "
            "VALUES ($1, $2, $3, 'Processing', 0, 'application/json', '', NOW(), NULL, NOW() + INTERVAL '30 days')",
            key, workerKeyId, payloadHash
        );
        w.commit();
        return IdempotencyReservationResult{IdempotencyReservationStatus::Reserved, std::nullopt};
    } catch (const pqxx::sql_error& ex) {
        // Safe check for unique violation
        // Code 23505 is PostgreSQL's unique_violation code
        if (std::string(ex.sqlstate()) == "23505") {
            return IdempotencyReservationResult{IdempotencyReservationStatus::AlreadyProcessing, std::nullopt};
        }
        throw;
    }
}

void DbIdempotencyStore::complete(
    const std::string& key, 
    const IdempotentResult& result) {
    
    pqxx::connection conn(connectionString_);
    pqxx::work w(conn);

    w.exec_params(
        "UPDATE ecf_idempotency_records "
        "SET status = 'Completed', "
        "    status_code = $2, "
        "    content_type = $3, "
        "    response_body = $4, "
        "    updated_at = NOW() "
        "WHERE key = $1 AND status = 'Processing'",
        key, result.statusCode, result.contentType, result.body
    );
    w.commit();
}

void DbIdempotencyStore::release(const std::string& key) {
    try {
        pqxx::connection conn(connectionString_);
        pqxx::work w(conn);
        w.exec_params(
            "DELETE FROM ecf_idempotency_records WHERE key = $1 AND status = 'Processing'",
            key
        );
        w.commit();
    } catch (...) {}
}

} // namespace ecf::infra
