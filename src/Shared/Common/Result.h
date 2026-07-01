#pragma once
// A lightweight success/failure wrapper used by the Application handlers.

#include <optional>
#include <string>
#include <vector>

namespace ecf::shared {

class Result {
public:
    bool isSuccess() const { return success_; }
    bool isFailure() const { return !success_; }
    const std::optional<std::string>& error() const { return error_; }
    const std::vector<std::string>& errors() const { return errors_; }

    static Result Success() { return Result(true, std::nullopt, {}); }
    static Result Failure(std::string error) {
        return Result(false, std::move(error), {});
    }
    static Result Failure(std::vector<std::string> errors) {
        return Result(false, std::nullopt, std::move(errors));
    }

protected:
    Result(bool success, std::optional<std::string> error,
           std::vector<std::string> errors)
        : success_(success), error_(std::move(error)), errors_(std::move(errors)) {
        if (errors_.empty() && error_.has_value() && !error_->empty()) {
            errors_.push_back(*error_);
        }
    }

    bool success_;
    std::optional<std::string> error_;
    std::vector<std::string> errors_;
};

template <typename T>
class ResultT : public Result {
public:
    const std::optional<T>& value() const { return value_; }

    static ResultT<T> Success(T value) {
        return ResultT<T>(true, std::move(value), std::nullopt, {});
    }
    static ResultT<T> Failure(std::string error) {
        return ResultT<T>(false, std::nullopt, std::move(error), {});
    }
    static ResultT<T> Failure(std::vector<std::string> errors) {
        return ResultT<T>(false, std::nullopt, std::nullopt, std::move(errors));
    }

private:
    ResultT(bool success, std::optional<T> value, std::optional<std::string> error,
            std::vector<std::string> errors)
        : Result(success, std::move(error), std::move(errors)),
          value_(std::move(value)) {}

    std::optional<T> value_;
};

}  // namespace ecf::shared
