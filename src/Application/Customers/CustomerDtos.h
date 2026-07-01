#pragma once
// Ports of the Customer commands/queries + CustomerDto.

#include <string>

namespace ecf::app {

struct CustomerDto {
    std::string id;
    std::string name;
    std::string email;
    std::string rnc;
};

struct CreateCustomerCommand {
    std::string name;
    std::string email;
    std::string rnc;
};

struct UpdateCustomerCommand {
    std::string id;
    std::string name;
    std::string email;
    std::string rnc;
};

struct DeleteCustomerCommand {
    std::string id;
};

struct GetCustomerByIdQuery {
    std::string id;
};

struct GetAllCustomersQuery {};

}  // namespace ecf::app
