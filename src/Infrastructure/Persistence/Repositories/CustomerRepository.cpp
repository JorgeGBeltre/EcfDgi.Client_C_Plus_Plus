#include "Infrastructure/Persistence/Repositories/CustomerRepository.h"

#include "Infrastructure/Persistence/RowMappers.h"
#include "Shared/Common/Sys.h"

namespace ecf::infra {

using domain::Customer;

std::optional<Customer> CustomerRepository::getById(const std::string& id) {
    pqxx::nontransaction n(db_->connection());
    pqxx::result r = n.exec_params(
        std::string("SELECT ") + customerColumns() +
            " FROM customers WHERE id = $1 AND is_deleted = false",
        id);
    if (r.empty()) return std::nullopt;
    return mapCustomer(r[0]);
}

std::vector<Customer> CustomerRepository::getAll() {
    pqxx::nontransaction n(db_->connection());
    pqxx::result r = n.exec(
        std::string("SELECT ") + customerColumns() +
        " FROM customers WHERE is_deleted = false ORDER BY created_at");
    std::vector<Customer> out;
    out.reserve(r.size());
    for (const auto& row : r) out.push_back(mapCustomer(row));
    return out;
}

void CustomerRepository::add(const Customer& customer) {
    Customer e = customer;
    if (e.id.empty()) e.id = sys::newUuid();
    e.createdAt = sys::utcNowIso();
    const std::string by = db_->auditUsername();
    e.isDeleted = false;
    db_->stage([e, by](pqxx::work& w) {
        w.exec_params(
            "INSERT INTO customers (id, name, email, rnc, created_at, created_by, is_deleted) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7)",
            e.id, e.name, e.email, e.rnc, e.createdAt, by, e.isDeleted);
    });
}

void CustomerRepository::update(const Customer& customer) {
    Customer e = customer;
    const std::string at = sys::utcNowIso();
    const std::string by = db_->auditUsername();
    db_->stage([e, at, by](pqxx::work& w) {
        w.exec_params(
            "UPDATE customers SET name = $2, email = $3, rnc = $4, "
            "updated_at = $5, updated_by = $6 WHERE id = $1",
            e.id, e.name, e.email, e.rnc, at, by);
    });
}

void CustomerRepository::remove(const Customer& customer) {
    // Soft delete: mark the row deleted instead of removing it.
    const std::string id = customer.id;
    const std::string at = sys::utcNowIso();
    const std::string by = db_->auditUsername();
    db_->stage([id, at, by](pqxx::work& w) {
        w.exec_params(
            "UPDATE customers SET is_deleted = true, deleted_at = $2, deleted_by = $3 "
            "WHERE id = $1",
            id, at, by);
    });
}

}  // namespace ecf::infra
