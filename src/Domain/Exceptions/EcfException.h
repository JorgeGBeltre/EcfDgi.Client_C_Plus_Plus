#pragma once

#include <stdexcept>
#include <string>
#include <vector>

namespace ecf::domain {

class EcfException : public std::runtime_error {
public:
    explicit EcfException(const std::string& message) : std::runtime_error(message) {}
};

class EcfSigningException : public EcfException {
public:
    explicit EcfSigningException(const std::string& message) : EcfException(message) {}
};

// Raised by EcfValidator/EcfClient when an RFCE fails validation.
class EcfValidationException : public EcfException {
public:
    explicit EcfValidationException(std::vector<std::string> errors)
        : EcfException(buildMessage(errors)), errors_(std::move(errors)) {}

    const std::vector<std::string>& errors() const { return errors_; }

private:
    static std::string buildMessage(const std::vector<std::string>& errors) {
        std::string msg = "Errores de validación: ";
        for (size_t i = 0; i < errors.size(); ++i) {
            if (i) msg += ", ";
            msg += errors[i];
        }
        return msg;
    }
    std::vector<std::string> errors_;
};

class PollingMaxRetriesException : public std::runtime_error {
public:
    explicit PollingMaxRetriesException(int retries)
        : std::runtime_error("Polling exceeded maximum retries (" +
                             std::to_string(retries) + ")"),
          retries_(retries) {}
    int retries() const { return retries_; }

private:
    int retries_;
};

class PollingTimeoutException : public std::runtime_error {
public:
    explicit PollingTimeoutException(const std::string& message = "Polling timed out")
        : std::runtime_error(message) {}
};

}  // namespace ecf::domain
