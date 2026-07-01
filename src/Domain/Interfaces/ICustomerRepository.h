#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Domain/Entities/Customer.h"

namespace ecf::domain {

class ICustomerRepository {
public:
    virtual ~ICustomerRepository() = default;

    virtual std::optional<Customer> getById(const std::string& id) = 0;
    virtual std::vector<Customer> getAll() = 0;
    virtual void add(const Customer& customer) = 0;
    virtual void update(const Customer& customer) = 0;
    virtual void remove(const Customer& customer) = 0;  // soft delete
};

}  // namespace ecf::domain
