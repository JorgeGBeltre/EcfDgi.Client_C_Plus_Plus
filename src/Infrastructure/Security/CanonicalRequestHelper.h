#pragma once

#include <string>

namespace ecf::infra {

class CanonicalRequestHelper {
public:
    static std::string buildCanonicalString(
        const std::string& method,
        const std::string& pathAndQuery,
        const std::string& timestamp,
        const std::string& nonce,
        const std::string& body);

    static std::string computeHmacSha256(
        const std::string& secretKey,
        const std::string& canonicalString);

    static std::string computeSha256Hex(
        const std::string& input);

    static bool safeCompare(
        const std::string& a,
        const std::string& b);
};

} // namespace ecf::infra
