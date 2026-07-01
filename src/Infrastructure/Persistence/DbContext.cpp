#include "Infrastructure/Persistence/DbContext.h"

namespace ecf::infra {

DbContext::DbContext(const std::string& connectionString,
                     std::shared_ptr<domain::ICurrentUserService> currentUser)
    : conn_(connectionString), currentUser_(std::move(currentUser)) {}

std::string DbContext::auditUsername() const {
    if (currentUser_) {
        auto u = currentUser_->username();
        if (u && !u->empty()) return *u;
    }
    return "System";
}

int DbContext::saveChanges() {
    if (pending_.empty()) return 0;
    pqxx::work w(conn_);
    for (auto& op : pending_) op(w);
    w.commit();
    int count = static_cast<int>(pending_.size());
    pending_.clear();
    return count;
}

}  // namespace ecf::infra
