#include "Application/Common/RequestValidators.h"

#include <regex>
#include <string>

namespace ecf::app::validators {

namespace {

const std::regex kRncRegex(R"(^\d{9}$|^\d{11}$)");
// Pragmatic e-mail check.
const std::regex kEmailRegex(R"(^[^@\s]+@[^@\s]+\.[^@\s]+$)");

bool empty(const std::string& s) { return s.empty(); }

void notEmpty(std::vector<ValidationFailure>& f, const std::string& prop,
              const std::string& value, const std::string& msg) {
    if (empty(value)) f.push_back({prop, msg});
}

void maxLength(std::vector<ValidationFailure>& f, const std::string& prop,
               const std::string& value, size_t max, const std::string& msg) {
    if (value.size() > max) f.push_back({prop, msg});
}

}  // namespace

std::vector<ValidationFailure> validate(const RegisterUserCommand& c) {
    std::vector<ValidationFailure> f;
    notEmpty(f, "Username", c.username, "Username is required.");
    maxLength(f, "Username", c.username, 100, "Username must not exceed 100 characters.");
    notEmpty(f, "Email", c.email, "Email is required.");
    if (!c.email.empty() && !std::regex_match(c.email, kEmailRegex))
        f.push_back({"Email", "Email must be a valid email address."});
    maxLength(f, "Email", c.email, 150, "Email must not exceed 150 characters.");
    notEmpty(f, "Password", c.password, "Password is required.");
    if (c.password.size() < 6)
        f.push_back({"Password", "Password must be at least 6 characters long."});
    notEmpty(f, "Role", c.role, "Role is required.");
    if (c.role != "Admin" && c.role != "User")
        f.push_back({"Role", "Role must be either 'Admin' or 'User'."});
    return f;
}

std::vector<ValidationFailure> validate(const LoginUserCommand& c) {
    std::vector<ValidationFailure> f;
    notEmpty(f, "Username", c.username, "Username is required.");
    notEmpty(f, "Password", c.password, "Password is required.");
    return f;
}

std::vector<ValidationFailure> validate(const CreateCustomerCommand& c) {
    std::vector<ValidationFailure> f;
    notEmpty(f, "Name", c.name, "Name is required.");
    maxLength(f, "Name", c.name, 200, "Name must not exceed 200 characters.");
    notEmpty(f, "Email", c.email, "Email is required.");
    if (!c.email.empty() && !std::regex_match(c.email, kEmailRegex))
        f.push_back({"Email", "Email must be a valid email address."});
    maxLength(f, "Email", c.email, 150, "Email must not exceed 150 characters.");
    notEmpty(f, "Rnc", c.rnc, "RNC is required.");
    if (!c.rnc.empty() && !std::regex_match(c.rnc, kRncRegex))
        f.push_back({"Rnc", "RNC must be exactly 9 or 11 numeric digits."});
    return f;
}

std::vector<ValidationFailure> validate(const UpdateCustomerCommand& c) {
    std::vector<ValidationFailure> f;
    notEmpty(f, "Id", c.id, "Customer ID is required.");
    notEmpty(f, "Name", c.name, "Name is required.");
    maxLength(f, "Name", c.name, 200, "Name must not exceed 200 characters.");
    notEmpty(f, "Email", c.email, "Email is required.");
    if (!c.email.empty() && !std::regex_match(c.email, kEmailRegex))
        f.push_back({"Email", "Email must be a valid email address."});
    maxLength(f, "Email", c.email, 150, "Email must not exceed 150 characters.");
    notEmpty(f, "Rnc", c.rnc, "RNC is required.");
    if (!c.rnc.empty() && !std::regex_match(c.rnc, kRncRegex))
        f.push_back({"Rnc", "RNC must be exactly 9 or 11 numeric digits."});
    return f;
}

std::vector<ValidationFailure> validate(const SendEcfCommand& c) {
    std::vector<ValidationFailure> f;
    notEmpty(f, "XmlContent", c.xmlContent, "XML Content is required.");
    notEmpty(f, "FileName", c.fileName, "File name is required.");
    maxLength(f, "FileName", c.fileName, 150, "File name must not exceed 150 characters.");
    notEmpty(f, "RncEmisor", c.rncEmisor, "RNC Emisor is required.");
    if (!c.rncEmisor.empty() && !std::regex_match(c.rncEmisor, kRncRegex))
        f.push_back({"RncEmisor", "RNC Emisor must be exactly 9 or 11 numeric digits."});
    notEmpty(f, "ENcf", c.eNcf, "e-NCF is required.");
    maxLength(f, "ENcf", c.eNcf, 20, "e-NCF must not exceed 20 characters.");
    if (c.totalAmount < 0)
        f.push_back({"TotalAmount", "Total amount must be greater than or equal to 0."});
    if (c.itbisAmount < 0)
        f.push_back({"ItbisAmount", "ITBIS amount must be greater than or equal to 0."});
    return f;
}

std::vector<ValidationFailure> validate(const SendRfceCommand& c) {
    std::vector<ValidationFailure> f;
    const auto& enc = c.rfceModel.encabezado;
    notEmpty(f, "RncEmisor", enc.emisor.rncEmisor, "RncEmisor is required.");
    if (!enc.emisor.rncEmisor.empty() && !std::regex_match(enc.emisor.rncEmisor, kRncRegex))
        f.push_back({"RncEmisor", "RNC Emisor must be exactly 9 or 11 numeric digits."});
    notEmpty(f, "ENcf", enc.idDoc.eNcf, "e-NCF is required.");
    maxLength(f, "ENcf", enc.idDoc.eNcf, 20, "e-NCF must not exceed 20 characters.");
    return f;
}

}  // namespace ecf::app::validators
