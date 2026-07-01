#include "Infrastructure/Persistence/Repositories/UserRepository.h"

#include "Infrastructure/Persistence/RowMappers.h"
#include "Shared/Common/Sys.h"

namespace ecf::infra {

using domain::User;

namespace {
std::optional<User> queryOne(DbContext& db, const std::string& where,
                             const std::string& arg) {
    pqxx::nontransaction n(db.connection());
    pqxx::result r = n.exec_params(
        std::string("SELECT ") + userColumns() + " FROM users WHERE " + where +
            " AND is_deleted = false",
        arg);
    if (r.empty()) return std::nullopt;
    return mapUser(r[0]);
}
}  // namespace

std::optional<User> UserRepository::getById(const std::string& id) {
    return queryOne(*db_, "id = $1", id);
}

std::optional<User> UserRepository::getByUsername(const std::string& username) {
    return queryOne(*db_, "username = $1", username);
}

std::optional<User> UserRepository::getByEmail(const std::string& email) {
    return queryOne(*db_, "email = $1", email);
}

void UserRepository::add(const User& user) {
    User e = user;
    if (e.id.empty()) e.id = sys::newUuid();
    e.createdAt = sys::utcNowIso();
    const std::string by = db_->auditUsername();
    e.isDeleted = false;
    db_->stage([e, by](pqxx::work& w) {
        w.exec_params(
            "INSERT INTO users (id, username, email, password_hash, role, "
            "created_at, created_by, is_deleted) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8)",
            e.id, e.username, e.email, e.passwordHash, e.role, e.createdAt, by,
            e.isDeleted);
    });
}

void UserRepository::update(const User& user) {
    User e = user;
    const std::string at = sys::utcNowIso();
    const std::string by = db_->auditUsername();
    db_->stage([e, at, by](pqxx::work& w) {
        w.exec_params(
            "UPDATE users SET username = $2, email = $3, password_hash = $4, "
            "role = $5, updated_at = $6, updated_by = $7 WHERE id = $1",
            e.id, e.username, e.email, e.passwordHash, e.role, at, by);
    });
}

}  // namespace ecf::infra
