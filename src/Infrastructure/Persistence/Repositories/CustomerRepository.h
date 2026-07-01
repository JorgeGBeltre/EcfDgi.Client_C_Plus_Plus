#pragma once

#include <memory>

#include "Domain/Interfaces/ICustomerRepository.h"
#include "Infrastructure/Persistence/DbContext.h"

namespace ecf::infra {

class CustomerRepository : public domain::ICustomerRepository {
public:
    explicit CustomerRepository(std::shared_ptr<DbContext> db) : db_(std::move(db)) {}

    std::optional<domain::Customer> getById(const std::string& id) override;
    std::vector<domain::Customer> getAll() override;
    void add(const domain::Customer& customer) override;
    void update(const domain::Customer& customer) override;
    void remove(const domain::Customer& customer) override;

private:
    std::shared_ptr<DbContext> db_;
};

}  // namespace ecf::infra
