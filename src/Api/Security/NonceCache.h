#pragma once

#include <string>
#include <unordered_map>
#include <mutex>

namespace ecf::api {

class NonceCache {
public:
    // Returns true if the nonce is successfully added (not seen before).
    // Returns false if the nonce is already in the cache (replay attack).
    bool tryAddNonce(const std::string& keyId, const std::string& nonce, double ttlSeconds);

private:
    std::unordered_map<std::string, double> cache_;
    std::mutex mutex_;
};

} // namespace ecf::api
