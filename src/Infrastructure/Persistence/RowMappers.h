#pragma once
// Helpers to map pqxx rows to domain entities (read side).

#include <optional>
#include <string>

#include <pqxx/pqxx>

#include "Domain/Entities/Customer.h"
#include "Domain/Entities/EcfDocument.h"
#include "Domain/Entities/User.h"

namespace ecf::infra {

inline std::optional<std::string> optStr(const pqxx::field& f) {
    if (f.is_null()) return std::nullopt;
    return f.as<std::string>();
}

inline void mapAuditable(const pqxx::row& r, domain::AuditableEntity& e) {
    e.id = r["id"].as<std::string>();
    e.createdAt = r["created_at"].is_null() ? "" : r["created_at"].as<std::string>();
    e.createdBy = optStr(r["created_by"]);
    e.updatedAt = optStr(r["updated_at"]);
    e.updatedBy = optStr(r["updated_by"]);
    e.deletedAt = optStr(r["deleted_at"]);
    e.deletedBy = optStr(r["deleted_by"]);
    e.isDeleted = r["is_deleted"].as<bool>();
}

inline domain::User mapUser(const pqxx::row& r) {
    domain::User u;
    mapAuditable(r, u);
    u.username = r["username"].as<std::string>();
    u.email = r["email"].as<std::string>();
    u.passwordHash = r["password_hash"].as<std::string>();
    u.role = r["role"].as<std::string>();
    return u;
}

inline domain::Customer mapCustomer(const pqxx::row& r) {
    domain::Customer c;
    mapAuditable(r, c);
    c.name = r["name"].as<std::string>();
    c.email = r["email"].as<std::string>();
    c.rnc = r["rnc"].as<std::string>();
    return c;
}

inline domain::EcfDocument mapEcfDocument(const pqxx::row& r) {
    domain::EcfDocument d;
    mapAuditable(r, d);
    d.eNcf = r["e_ncf"].as<std::string>();
    d.rncEmisor = r["rnc_emisor"].as<std::string>();
    d.rncComprador = optStr(r["rnc_comprador"]);
    d.trackId = optStr(r["track_id"]);
    d.state = r["state"].as<std::string>();
    d.totalAmount = r["total_amount"].as<double>();
    d.itbisAmount = r["itbis_amount"].as<double>();
    d.securityCode = optStr(r["security_code"]);
    d.xmlContent = r["xml_content"].as<std::string>();
    d.receiptDate = optStr(r["receipt_date"]);
    return d;
}

// Common SELECT column lists (timestamps cast to text for stable ISO strings).
inline const char* userColumns() {
    return "id::text, username, email, password_hash, role, "
           "created_at::text, created_by, updated_at::text, updated_by, "
           "deleted_at::text, deleted_by, is_deleted";
}
inline const char* customerColumns() {
    return "id::text, name, email, rnc, "
           "created_at::text, created_by, updated_at::text, updated_by, "
           "deleted_at::text, deleted_by, is_deleted";
}
inline const char* ecfDocumentColumns() {
    return "id::text, e_ncf, rnc_emisor, rnc_comprador, track_id, state, "
           "total_amount, itbis_amount, security_code, xml_content, "
           "receipt_date::text, created_at::text, created_by, updated_at::text, "
           "updated_by, deleted_at::text, deleted_by, is_deleted";
}

}  // namespace ecf::infra
