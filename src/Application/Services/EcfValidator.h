#pragma once

#include <string>
#include <vector>

#include "Domain/Entities/Rfce.h"

namespace ecf::app {

class ValidationResult {
public:
    explicit ValidationResult(std::vector<std::string> errors)
        : errors_(std::move(errors)) {}
    bool isValid() const { return errors_.empty(); }
    const std::vector<std::string>& errors() const { return errors_; }

private:
    std::vector<std::string> errors_;
};

class EcfValidator {
public:
    ValidationResult validateRfce(const domain::Rfce& rfce);

private:
    static constexpr double kRfceThreshold = 250000.00;
    void validateTotalesConsistency(const domain::RfceTotales& t,
                                    std::vector<std::string>& errors);
    static bool isValidRnc(const std::string& rnc);
};

}  // namespace ecf::app
