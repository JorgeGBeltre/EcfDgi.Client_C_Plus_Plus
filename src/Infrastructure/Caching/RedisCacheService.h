#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

#include "Domain/Interfaces/ICacheService.h"

namespace ecf::infra {

class RedisCacheService : public domain::ICacheService {
public:
    explicit RedisCacheService(const std::string& connectionString = "");
    ~RedisCacheService() override;

    std::optional<std::string> get(const std::string& key) override;
    bool set(const std::string& key, const std::string& value,
             std::optional<std::chrono::seconds> expiration = std::nullopt) override;
    bool remove(const std::string& key) override;

    bool acquireLock(const std::string& lockKey, const std::string& lockValue,
                     std::chrono::seconds expiration) override;
    bool releaseLock(const std::string& lockKey, const std::string& lockValue) override;

private:
    struct CacheItem {
        std::string value;
        std::chrono::system_clock::time_point expireAt;
        bool hasExpiration{false};
    };

    std::string host_;
    int port_{6379};
    bool useRedis_{false};

    // Memory fallback store
    std::mutex memMutex_;
    std::unordered_map<std::string, CacheItem> memoryStore_;
    std::unordered_map<std::string, std::pair<std::string, std::chrono::system_clock::time_point>> locks_;

    void parseConnectionString(const std::string& connectionString);
    std::string sendRedisCommand(const std::string& cmd);
};

}  // namespace ecf::infra
