#pragma once

#include <string>
#include <unordered_map>
#include <mutex>

namespace ecf::api {

class NonceCache {
public:

    bool tryAddNonce(const std::string& keyId, const std::string& nonce, double ttlSeconds);

private:
    std::unordered_map<std::string, double> cache_;
    std::mutex mutex_;
};

}
