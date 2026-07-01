#pragma once
// Helpers to map pqxx rows to domain entities (read side).
//
// The mappers are templated on the row/field type on purpose: across libpqxx
// versions result indexing and iteration yield different-but-compatible types
// (pqxx::row vs pqxx::row_ref, pqxx::field vs pqxx::field_ref). Templating lets
// the same code bind to whichever the installed libpqxx provides. GCC requires
// the `.template` disambiguator on the dependent `.as<T>()` member calls.

#include <optional>
#include <string>

#include <pqxx/pqxx>

#include "Domain/Entities/Customer.h"
#include "Domain/Entities/EcfDocument.h"
#include "Domain/Entities/User.h"

namespace ecf::infra {

// Accepts any pqxx field-like type (pqxx::field or pqxx::field_ref); both
// expose is_null()/as<T>().
template <class Field>
inline std::optional<std::string> optStr(const Field& f) {
    if (f.is_null()) return std::nullopt;
    return f.template as<std::string>();
}

// Accepts any pqxx row-like type (pqxx::row from iteration, pqxx::row_ref from
// result::operator[] in newer libpqxx); both expose the same field accessors.
template <class Row>
inline void mapAuditable(const Row& r, domain::AuditableEntity& e) {
    e.id = r["id"].template as<std::string>();
    e.createdAt =
        r["created_at"].is_null() ? "" : r["created_at"].template as<std::string>();
    e.createdBy = optStr(r["created_by"]);
    e.updatedAt = optStr(r["updated_at"]);
    e.updatedBy = optStr(r["updated_by"]);
    e.deletedAt = optStr(r["deleted_at"]);
    e.deletedBy = optStr(r["deleted_by"]);
    e.isDeleted = r["is_deleted"].template as<bool>();
}

template <class Row>
inline domain::User mapUser(const Row& r) {
    domain::User u;
    mapAuditable(r, u);
    u.username = r["username"].template as<std::string>();
    u.email = r["email"].template as<std::string>();
    u.passwordHash = r["password_hash"].template as<std::string>();
    u.role = r["role"].template as<std::string>();
    return u;
}

template <class Row>
inline domain::Customer mapCustomer(const Row& r) {
    domain::Customer c;
    mapAuditable(r, c);
    c.name = r["name"].template as<std::string>();
    c.email = r["email"].template as<std::string>();
    c.rnc = r["rnc"].template as<std::string>();
    return c;
}

template <class Row>
inline domain::EcfDocument mapEcfDocument(const Row& r) {
    domain::EcfDocument d;
    mapAuditable(r, d);
    d.eNcf = r["e_ncf"].template as<std::string>();
    d.rncEmisor = r["rnc_emisor"].template as<std::string>();
    d.rncComprador = optStr(r["rnc_comprador"]);
    d.trackId = optStr(r["track_id"]);
    d.state = r["state"].template as<std::string>();
    d.totalAmount = r["total_amount"].template as<double>();
    d.itbisAmount = r["itbis_amount"].template as<double>();
    d.securityCode = optStr(r["security_code"]);
    d.xmlContent = r["xml_content"].template as<std::string>();
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
