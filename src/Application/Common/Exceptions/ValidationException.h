#pragma once
// Aggregates request validation failures grouped by property name.

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace ecf::app {

struct ValidationFailure {
    std::string propertyName;
    std::string errorMessage;
};

class ValidationException : public std::runtime_error {
public:
    ValidationException()
        : std::runtime_error("One or more validation failures have occurred.") {}

    explicit ValidationException(const std::vector<ValidationFailure>& failures)
        : ValidationException() {
        for (const auto& f : failures) errors_[f.propertyName].push_back(f.errorMessage);
    }

    // property name -> list of error messages
    const std::map<std::string, std::vector<std::string>>& errors() const { return errors_; }

private:
    std::map<std::string, std::vector<std::string>> errors_;
};

}  // namespace ecf::app
