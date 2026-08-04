#include <cstdio>
#include <fstream>
#include <string>
#include <nlohmann/json.hpp>
#include "Infrastructure/Security/CanonicalRequestHelper.h"

using namespace ecf::infra;
using json = nlohmann::json;

static int failures = 0;

#define CHECK_EQUAL(actual, expected, msg) \
    do { \
        if ((actual) != (expected)) { \
            std::printf("FAIL: %s\n  Actual:   '%s'\n  Expected: '%s'\n", msg, (actual).c_str(), (expected).c_str()); \
            ++failures; \
        } else { \
            std::printf("ok:   %s\n", msg); \
        } \
    } while (0)

int main() {
    std::string path = "tests/hmac_test_vectors.json";
    std::ifstream f(path);
    if (!f) {
        f.open("../tests/hmac_test_vectors.json");
    }
    if (!f) {
        f.open("../../tests/hmac_test_vectors.json");
    }
    if (!f) {
        std::printf("FAIL: Could not open hmac_test_vectors.json\n");
        return 1;
    }

    json data;
    try {
        f >> data;
    } catch (const std::exception& ex) {
        std::printf("FAIL: Parsing JSON failed: %s\n", ex.what());
        return 1;
    }

    std::string secret = data["secret"].get<std::string>();
    auto vectors = data["vectors"];

    for (const auto& v : vectors) {
        std::string id = v["id"].get<std::string>();
        std::string method = v["method"].get<std::string>();
        std::string rawTarget = v["rawTarget"].get<std::string>();
        std::string timestamp = v["timestamp"].get<std::string>();
        std::string nonce = v["nonce"].get<std::string>();
        std::string body = v["body"].get<std::string>();
        std::string expectedBodyHashHex = v["expectedBodyHashHex"].get<std::string>();
        std::string expectedCanonicalString = v["expectedCanonicalString"].get<std::string>();
        std::string expectedSignature = v["expectedSignature"].get<std::string>();

        std::printf("\n--- Running Vector %s ---\n", id.c_str());

        // 1. Verify Body SHA256 Hash
        std::string actualBodyHashHex = CanonicalRequestHelper::computeSha256Hex(body);
        CHECK_EQUAL(actualBodyHashHex, expectedBodyHashHex, (id + " - Body Hash").c_str());

        // 2. Verify Canonical String construction
        std::string actualCanonicalString = CanonicalRequestHelper::buildCanonicalString(
            method, rawTarget, timestamp, nonce, body
        );
        CHECK_EQUAL(actualCanonicalString, expectedCanonicalString, (id + " - Canonical String").c_str());

        // 3. Verify HMAC SHA256 Signature
        std::string actualSignature = CanonicalRequestHelper::computeHmacSha256(secret, actualCanonicalString);
        CHECK_EQUAL(actualSignature, expectedSignature, (id + " - Signature").c_str());
    }

    std::printf("\n%s (%d failure(s))\n", failures ? "HMAC TESTS FAILED" : "ALL HMAC TESTS PASSED", failures);
    return failures == 0 ? 0 : 1;
}
