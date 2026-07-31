#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <nlohmann/json.hpp>

namespace ecf::domain {

class ICacheService {
public:
    virtual ~ICacheService() = default;

    virtual std::optional<std::string> get(const std::string& key) = 0;
    virtual bool set(const std::string& key, const std::string& value,
                     std::optional<std::chrono::seconds> expiration = std::nullopt) = 0;
    virtual bool remove(const std::string& key) = 0;

    virtual bool acquireLock(const std::string& lockKey, const std::string& lockValue,
                             std::chrono::seconds expiration) = 0;
    virtual bool releaseLock(const std::string& lockKey, const std::string& lockValue) = 0;

    template <typename T>
    std::optional<T> getObject(const std::string& key) {
        auto val = get(key);
        if (!val) return std::nullopt;
        try {
            auto j = nlohmann::json::parse(*val);
            return j.get<T>();
        } catch (...) {
            return std::nullopt;
        }
    }

    template <typename T>
    bool setObject(const std::string& key, const T& value,
                   std::optional<std::chrono::seconds> expiration = std::nullopt) {
        try {
            nlohmann::json j = value;
            return set(key, j.dump(), expiration);
        } catch (...) {
            return false;
        }
    }
};

}  // namespace ecf::domain
