#include "Api/Security/NonceCache.h"
#include <chrono>

namespace ecf::api {

namespace {
double currentTimestamp() {
    auto now = std::chrono::system_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() / 1000.0;
}
} // namespace

bool NonceCache::tryAddNonce(const std::string& keyId, const std::string& nonce, double ttlSeconds) {
    if (keyId.empty() || nonce.empty()) {
        return false;
    }

    std::string cacheKey = "nonce:" + keyId + ":" + nonce;
    double now = currentTimestamp();

    std::lock_guard<std::mutex> lock(mutex_);

    // Prune expired nonces to prevent indefinite growth
    for (auto it = cache_.begin(); it != cache_.end();) {
        if (it->second < now) {
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }

    auto it = cache_.find(cacheKey);
    if (it != cache_.end()) {
        if (it->second >= now) {
            return false; // Nonce already active -> replay attack
        }
    }

    cache_[cacheKey] = now + ttlSeconds;
    return true;
}

} // namespace ecf::api
