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
    size_t colon = s.find(':');
    if (colon != std::string::npos) {
        host_ = s.substr(0, colon);
        try {
            port_ = std::stoi(s.substr(colon + 1));
        } catch (...) {
            port_ = 6379;
        }
    } else {
        host_ = s;
        port_ = 6379;
    }

    // Test socket connection
    std::string pingResp = sendRedisCommand("*1\r\n$4\r\nPING\r\n");
    if (!pingResp.empty() && pingResp.find("+PONG") != std::string::npos) {
        useRedis_ = true;
        spdlog::info("Connected to Redis server at {}:{}", host_, port_);
    } else {
        useRedis_ = false;
        spdlog::warn("Could not connect to Redis server at {}:{}. Cache operating in in-memory fallback mode.", host_, port_);
    }
}

std::string RedisCacheService::sendRedisCommand(const std::string& cmd) {
    if (host_.empty()) return "";

#if defined(_WIN32)
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) return "";
#else
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return "";
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

    send(sock, cmd.c_str(), (int)cmd.length(), 0);

    char buffer[4096];
    int bytesRead = recv(sock, buffer, sizeof(buffer) - 1, 0);
    std::string response;
    if (bytesRead > 0) {
        buffer[bytesRead] = '\0';
        response.assign(buffer, bytesRead);
    }

#if defined(_WIN32)
    closesocket(sock);
#else
    close(sock);
#endif

    return response;
}

std::optional<std::string> RedisCacheService::get(const std::string& key) {
    if (useRedis_) {
        std::ostringstream ss;
        ss << "*2\r\n$3\r\nGET\r\n$" << key.length() << "\r\n" << key << "\r\n";
        std::string resp = sendRedisCommand(ss.str());

        if (resp.rfind("$", 0) == 0) {
            if (resp.find("$-1") == 0) return std::nullopt; // Key not found
            size_t crlf = resp.find("\r\n");
            if (crlf != std::string::npos) {
                int len = std::stoi(resp.substr(1, crlf - 1));
                if (len > 0 && crlf + 2 + len <= resp.length()) {
                    return resp.substr(crlf + 2, len);
                }
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
    if (useRedis_) {
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
    if (useRedis_) {
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
    if (useRedis_) {
        std::ostringstream ss;
        ss << "*6\r\n$3\r\nSET\r\n$" << lockKey.length() << "\r\n" << lockKey
           << "\r\n$" << lockValue.length() << "\r\n" << lockValue
           << "\r\n$2\r\nNX\r\n$2\r\nEX\r\n$" << std::to_string(expiration.count()).length()
           << "\r\n" << expiration.count() << "\r\n";
        std::string resp = sendRedisCommand(ss.str());
        if (resp.find("+OK") != std::string::npos) return true;
    }

    // In-memory fallback lock
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
    if (useRedis_) {
        // Safe lock release script or DEL key if value matches
        std::optional<std::string> currentVal = get(lockKey);
        if (currentVal && *currentVal == lockValue) {
            return remove(lockKey);
        }
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
