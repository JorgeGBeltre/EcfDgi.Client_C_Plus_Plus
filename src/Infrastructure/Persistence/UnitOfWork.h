#pragma once

#include <memory>

#include "Domain/Interfaces/IUnitOfWork.h"
#include "Infrastructure/Persistence/DbContext.h"

namespace ecf::infra {

class UnitOfWork : public domain::IUnitOfWork {
public:
    explicit UnitOfWork(std::shared_ptr<DbContext> db) : db_(std::move(db)) {}
    int saveChanges() override { return db_->saveChanges(); }

private:
    std::shared_ptr<DbContext> db_;
};

}  // namespace ecf::infra
