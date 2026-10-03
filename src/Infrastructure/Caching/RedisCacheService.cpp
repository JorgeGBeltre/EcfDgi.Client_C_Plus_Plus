#include "Infrastructure/Caching/RedisCacheService.h"

#include <spdlog/spdlog.h>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include <iostream>
#include <sstream>

namespace ecf::infra {

RedisCacheService::RedisCacheService(const std::string& connectionString) {
#if defined(_WIN32)
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
    parseConnectionString(connectionString);
}

RedisCacheService::~RedisCacheService() {
#if defined(_WIN32)
    WSACleanup();
#endif
}

void RedisCacheService::parseConnectionString(const std::string& connectionString) {
    if (connectionString.empty()) {
        useRedis_ = false;
        spdlog::info("Redis connection string empty. Cache operating in in-memory fallback mode.");
        return;
    }

    std::string s = connectionString;

    // Check for password in connection string: e.g. "host:6379,password=secret,abortConnect=false"
    auto passPos = s.find("password=");
    if (passPos != std::string::npos) {
        auto passEnd = s.find_first_of(",;", passPos);
        password_ = s.substr(passPos + 9, passEnd == std::string::npos ? std::string::npos : passEnd - (passPos + 9));
    }
    if (const char* envPass = std::getenv("REDIS_PASSWORD")) {
        password_ = envPass;
    }

    size_t colon = s.find(':');
    if (colon != std::string::npos) {
        host_ = s.substr(0, colon);
        try {
            port_ = std::stoi(s.substr(colon + 1));
        } catch (...) {
            port_ = 6379;
        }
    } else {
        size_t comma = s.find(',');
        host_ = (comma != std::string::npos) ? s.substr(0, comma) : s;
        port_ = 6379;
    }

    // Test socket connection
    lastPingAttempt_ = std::chrono::system_clock::now();
    std::string pingResp = sendRedisCommand("*1\r\n$4\r\nPING\r\n");
    if (!pingResp.empty() && pingResp.find("+PONG") != std::string::npos) {
        useRedis_ = true;
        spdlog::info("Connected to Redis server at {}:{}", host_, port_);
    } else {
        useRedis_ = false;
        spdlog::warn("Could not connect to Redis server at {}:{}. Cache operating in in-memory fallback mode.", host_, port_);
    }
}

bool RedisCacheService::checkRedisAvailability() {
    if (host_.empty()) return false;
    if (useRedis_) return true;

    auto now = std::chrono::system_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now - lastPingAttempt_).count() < 30) {
        return false;
    }
    lastPingAttempt_ = now;

    std::string pingResp = sendRedisCommand("*1\r\n$4\r\nPING\r\n");
    if (!pingResp.empty() && pingResp.find("+PONG") != std::string::npos) {
        useRedis_ = true;
        spdlog::info("Reconnected to Redis server at {}:{}", host_, port_);
        return true;
    }
    return false;
}

std::string RedisCacheService::sendRedisCommand(const std::string& cmd) {
    if (host_.empty()) return "";

#if defined(_WIN32)
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return "";
    DWORD timeout = 2500; // 2.5s socket timeout
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&timeout, sizeof(timeout));
#else
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return "";
    struct timeval tv;
    tv.tv_sec = 2;
    tv.tv_usec = 500000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));
#endif

    struct addrinfo hints{}, *res = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    std::string portStr = std::to_string(port_);
    if (getaddrinfo(host_.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) {
#if defined(_WIN32)
        closesocket(sock);
#else
        close(sock);
#endif
        return "";
    }

    if (connect(sock, res->ai_addr, (int)res->ai_addrlen) != 0) {
        freeaddrinfo(res);
#if defined(_WIN32)
        closesocket(sock);
#else
        close(sock);
#endif
        return "";
    }
    freeaddrinfo(res);

    if (!password_.empty()) {
        std::string authCmd = "*2\r\n$4\r\nAUTH\r\n$" + std::to_string(password_.length()) + "\r\n" + password_ + "\r\n";
        send(sock, authCmd.c_str(), (int)authCmd.length(), 0);
        char authBuf[256];
        int authBytes = recv(sock, authBuf, sizeof(authBuf) - 1, 0);
        if (authBytes <= 0 || (authBytes > 0 && authBuf[0] == '-')) {
            spdlog::error("Redis AUTH failed for host {}:{}", host_, port_);
#if defined(_WIN32)
            closesocket(sock);
#else
            close(sock);
#endif
            return "";
        }
    }

    if (send(sock, cmd.c_str(), (int)cmd.length(), 0) <= 0) {
#if defined(_WIN32)
        closesocket(sock);
#else
        close(sock);
#endif
        return "";
    }

    std::string response;
    char buffer[4096];
    int bytesRead = 0;
    while ((bytesRead = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        response.append(buffer, bytesRead);

        if (!response.empty()) {
            if (response[0] == '+' || response[0] == '-' || response[0] == ':') {
                if (response.size() >= 2 && response.substr(response.size() - 2) == "\r\n") break;
            } else if (response[0] == '$') {
                if (response.rfind("$-1\r\n", 0) == 0) break;
                size_t crlf = response.find("\r\n");
                if (crlf != std::string::npos) {
                    try {
                        long long len = std::stoll(response.substr(1, crlf - 1));
                        if (len >= 0 && response.size() >= crlf + 2 + len + 2) break;
                    } catch (...) {
                        break;
                    }
                }
            } else {
                break;
            }
        }
    }

#if defined(_WIN32)
    closesocket(sock);
#else
    close(sock);
#endif

    return response;
}

std::optional<std::string> RedisCacheService::get(const std::string& key) {
    if (checkRedisAvailability()) {
        std::ostringstream ss;
        ss << "*2\r\n$3\r\nGET\r\n$" << key.length() << "\r\n" << key << "\r\n";
        std::string resp = sendRedisCommand(ss.str());

        if (resp.rfind("$", 0) == 0) {
            if (resp.find("$-1") == 0) return std::nullopt; // Key not found
            size_t crlf = resp.find("\r\n");
            if (crlf != std::string::npos) {
                try {
                    int len = std::stoi(resp.substr(1, crlf - 1));
                    if (len > 0 && crlf + 2 + len <= resp.length()) {
                        return resp.substr(crlf + 2, len);
                    }
                } catch (...) { }
            }
        }
    }

    // In-memory fallback
    std::lock_guard<std::mutex> lock(memMutex_);
    auto it = memoryStore_.find(key);
    if (it != memoryStore_.end()) {
        if (it->second.hasExpiration && std::chrono::system_clock::now() > it->second.expireAt) {
            memoryStore_.erase(it);
            return std::nullopt;
        }
        return it->second.value;
    }
    return std::nullopt;
}

bool RedisCacheService::set(const std::string& key, const std::string& value,
                            std::optional<std::chrono::seconds> expiration) {
    if (checkRedisAvailability()) {
        std::ostringstream ss;
        if (expiration.has_value()) {
            ss << "*4\r\n$3\r\nSET\r\n$" << key.length() << "\r\n" << key
               << "\r\n$" << value.length() << "\r\n" << value
               << "\r\n$2\r\nEX\r\n$" << std::to_string(expiration->count()).length()
               << "\r\n" << expiration->count() << "\r\n";
        } else {
            ss << "*3\r\n$3\r\nSET\r\n$" << key.length() << "\r\n" << key
               << "\r\n$" << value.length() << "\r\n" << value << "\r\n";
        }

        std::string resp = sendRedisCommand(ss.str());
        if (resp.find("+OK") != std::string::npos) return true;
    }

    // In-memory fallback
    std::lock_guard<std::mutex> lock(memMutex_);
    CacheItem item;
    item.value = value;
    if (expiration.has_value()) {
        item.hasExpiration = true;
        item.expireAt = std::chrono::system_clock::now() + *expiration;
    } else {
        item.hasExpiration = false;
    }
    memoryStore_[key] = item;
    return true;
}

bool RedisCacheService::remove(const std::string& key) {
    if (checkRedisAvailability()) {
        std::ostringstream ss;
        ss << "*2\r\n$3\r\nDEL\r\n$" << key.length() << "\r\n" << key << "\r\n";
        sendRedisCommand(ss.str());
    }

    std::lock_guard<std::mutex> lock(memMutex_);
    memoryStore_.erase(key);
    return true;
}

bool RedisCacheService::acquireLock(const std::string& lockKey, const std::string& lockValue,
                                    std::chrono::seconds expiration) {
    if (checkRedisAvailability()) {
        std::ostringstream ss;
        ss << "*6\r\n$3\r\nSET\r\n$" << lockKey.length() << "\r\n" << lockKey
           << "\r\n$" << lockValue.length() << "\r\n" << lockValue
           << "\r\n$2\r\nNX\r\n$2\r\nEX\r\n$" << std::to_string(expiration.count()).length()
           << "\r\n" << expiration.count() << "\r\n";
        std::string resp = sendRedisCommand(ss.str());
        if (resp.find("+OK") != std::string::npos) return true;
        
        // If Redis responded with null bulk string ("$-1\r\n") or error, the lock is held by another instance.
        // Returning false ensures true distributed exclusion and prevents split-brain.
        return false;
    }

    // In-memory fallback lock only when Redis is not available
    std::lock_guard<std::mutex> lock(memMutex_);
    auto now = std::chrono::system_clock::now();
    auto it = locks_.find(lockKey);
    if (it != locks_.end()) {
        if (now < it->second.second) {
            return false; // Lock taken and not expired
        }
    }
    locks_[lockKey] = {lockValue, now + expiration};
    return true;
}

bool RedisCacheService::releaseLock(const std::string& lockKey, const std::string& lockValue) {
    if (checkRedisAvailability()) {
        // Atomic compare-and-delete via EVAL Lua script
        const std::string script = "if redis.call('get', KEYS[1]) == ARGV[1] then return redis.call('del', KEYS[1]) else return 0 end";
        std::ostringstream ss;
        ss << "*5\r\n$4\r\nEVAL\r\n$" << script.length() << "\r\n" << script
           << "\r\n$1\r\n1\r\n$" << lockKey.length() << "\r\n" << lockKey
           << "\r\n$" << lockValue.length() << "\r\n" << lockValue << "\r\n";
        std::string resp = sendRedisCommand(ss.str());
        return resp.find(":1") != std::string::npos;
    }

    std::lock_guard<std::mutex> lock(memMutex_);
    auto it = locks_.find(lockKey);
    if (it != locks_.end()) {
        if (it->second.first == lockValue) {
            locks_.erase(it);
            return true;
        }
    }
    return false;
}

}  // namespace ecf::infra
