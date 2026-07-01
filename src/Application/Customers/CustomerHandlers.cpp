#include "Application/Customers/CustomerHandlers.h"

#include "Domain/Entities/Customer.h"
#include "Shared/Common/Sys.h"

namespace ecf::app {

using shared::Result;
using shared::ResultT;

namespace {
CustomerDto toDto(const domain::Customer& c) {
    return CustomerDto{c.id, c.name, c.email, c.rnc};
}
}  // namespace

ResultT<std::string> CreateCustomerCommandHandler::handle(const CreateCustomerCommand& cmd) {
    domain::Customer customer;
    // Generate the id up front so the repository persists it and we can return it.
    customer.id = sys::newUuid();
    customer.name = cmd.name;
    customer.email = cmd.email;
    customer.rnc = cmd.rnc;

    repo_->add(customer);
    uow_->saveChanges();

    return ResultT<std::string>::Success(customer.id);
}

Result UpdateCustomerCommandHandler::handle(const UpdateCustomerCommand& cmd) {
    auto existing = repo_->getById(cmd.id);
    if (!existing.has_value()) return Result::Failure("Customer not found.");

    existing->name = cmd.name;
    existing->email = cmd.email;
    existing->rnc = cmd.rnc;

    repo_->update(*existing);
    uow_->saveChanges();
    return Result::Success();
}

Result DeleteCustomerCommandHandler::handle(const DeleteCustomerCommand& cmd) {
    auto existing = repo_->getById(cmd.id);
    if (!existing.has_value()) return Result::Failure("Customer not found.");

    repo_->remove(*existing);
    uow_->saveChanges();
    return Result::Success();
}

ResultT<std::vector<CustomerDto>> GetAllCustomersQueryHandler::handle(
    const GetAllCustomersQuery&) {
    std::vector<CustomerDto> dtos;
    for (const auto& c : repo_->getAll()) dtos.push_back(toDto(c));
    return ResultT<std::vector<CustomerDto>>::Success(std::move(dtos));
}

ResultT<CustomerDto> GetCustomerByIdQueryHandler::handle(const GetCustomerByIdQuery& q) {
    auto c = repo_->getById(q.id);
    if (!c.has_value()) return ResultT<CustomerDto>::Failure("Customer not found.");
    return ResultT<CustomerDto>::Success(toDto(*c));
}

}  // namespace ecf::app
