#pragma once
// Request validators (one per command). Each returns the list of failures;
// validateOrThrow turns a non-empty list into a ValidationException.

#include <vector>

#include "Application/Auth/AuthDtos.h"
#include "Application/Common/Exceptions/ValidationException.h"
#include "Application/Customers/CustomerDtos.h"
#include "Application/Ecf/EcfDtos.h"

namespace ecf::app::validators {

std::vector<ValidationFailure> validate(const RegisterUserCommand& c);
std::vector<ValidationFailure> validate(const LoginUserCommand& c);
std::vector<ValidationFailure> validate(const CreateCustomerCommand& c);
std::vector<ValidationFailure> validate(const UpdateCustomerCommand& c);
std::vector<ValidationFailure> validate(const SendEcfCommand& c);
std::vector<ValidationFailure> validate(const SendRfceCommand& c);

}  // namespace ecf::app::validators
