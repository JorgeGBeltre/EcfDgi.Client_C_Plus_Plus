#pragma once
// Ports of the Customer command/query handlers.

#include <memory>
#include <vector>

#include "Application/Customers/CustomerDtos.h"
#include "Domain/Interfaces/ICustomerRepository.h"
#include "Domain/Interfaces/IUnitOfWork.h"
#include "Shared/Common/Result.h"

namespace ecf::app {

class CreateCustomerCommandHandler {
public:
    CreateCustomerCommandHandler(std::shared_ptr<domain::ICustomerRepository> repo,
                                 std::shared_ptr<domain::IUnitOfWork> uow)
        : repo_(std::move(repo)), uow_(std::move(uow)) {}
    shared::ResultT<std::string> handle(const CreateCustomerCommand& cmd);

private:
    std::shared_ptr<domain::ICustomerRepository> repo_;
    std::shared_ptr<domain::IUnitOfWork> uow_;
};

class UpdateCustomerCommandHandler {
public:
    UpdateCustomerCommandHandler(std::shared_ptr<domain::ICustomerRepository> repo,
                                 std::shared_ptr<domain::IUnitOfWork> uow)
        : repo_(std::move(repo)), uow_(std::move(uow)) {}
    shared::Result handle(const UpdateCustomerCommand& cmd);

private:
    std::shared_ptr<domain::ICustomerRepository> repo_;
    std::shared_ptr<domain::IUnitOfWork> uow_;
};

class DeleteCustomerCommandHandler {
public:
    DeleteCustomerCommandHandler(std::shared_ptr<domain::ICustomerRepository> repo,
                                 std::shared_ptr<domain::IUnitOfWork> uow)
        : repo_(std::move(repo)), uow_(std::move(uow)) {}
    shared::Result handle(const DeleteCustomerCommand& cmd);

private:
    std::shared_ptr<domain::ICustomerRepository> repo_;
    std::shared_ptr<domain::IUnitOfWork> uow_;
};

class GetAllCustomersQueryHandler {
public:
    explicit GetAllCustomersQueryHandler(std::shared_ptr<domain::ICustomerRepository> repo)
        : repo_(std::move(repo)) {}
    shared::ResultT<std::vector<CustomerDto>> handle(const GetAllCustomersQuery& q);

private:
    std::shared_ptr<domain::ICustomerRepository> repo_;
};

class GetCustomerByIdQueryHandler {
public:
    explicit GetCustomerByIdQueryHandler(std::shared_ptr<domain::ICustomerRepository> repo)
        : repo_(std::move(repo)) {}
    shared::ResultT<CustomerDto> handle(const GetCustomerByIdQuery& q);

private:
    std::shared_ptr<domain::ICustomerRepository> repo_;
};

}  // namespace ecf::app
