#include "Infrastructure/Security/CanonicalRequestHelper.h"
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/crypto.h>
#include <openssl/bio.h>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <stdexcept>

namespace ecf::infra {

std::string CanonicalRequestHelper::buildCanonicalString(
    const std::string& method,
    const std::string& pathAndQuery,
    const std::string& timestamp,
    const std::string& nonce,
    const std::string& body) {

    std::string formattedMethod = method;
    std::transform(formattedMethod.begin(), formattedMethod.end(), formattedMethod.begin(), ::toupper);

    std::string formattedPath = pathAndQuery.empty() ? "/" : pathAndQuery;
    std::string bodyHash = computeSha256Hex(body);

    std::ostringstream ss;
    ss << formattedMethod << "\n"
       << formattedPath << "\n"
       << timestamp << "\n"
       << nonce << "\n"
       << bodyHash;

    return ss.str();
}

std::string CanonicalRequestHelper::computeHmacSha256(
    const std::string& secretKey,
    const std::string& canonicalString) {

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int length = 0;

    HMAC(EVP_sha256(), secretKey.c_str(), static_cast<int>(secretKey.size()),
         reinterpret_cast<const unsigned char*>(canonicalString.c_str()), canonicalString.size(),
         hash, &length);

    // Encode to Base64 using OpenSSL BIO
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO* mem = BIO_new(BIO_s_mem());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO_push(b64, mem);
    BIO_write(b64, hash, length);
    (void)BIO_flush(b64);

    char* base64Data = nullptr;
    long base64Length = BIO_get_mem_data(mem, &base64Data);
    std::string result(base64Data, base64Length);
    BIO_free_all(b64);

    return result;
}

std::string CanonicalRequestHelper::computeSha256Hex(
    const std::string& input) {

    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int length = 0;

    EVP_MD_CTX* context = EVP_MD_CTX_new();
    if (!context) {
        throw std::runtime_error("Fallo al crear contexto SHA256.");
    }
    
    if (EVP_DigestInit_ex(context, EVP_sha256(), nullptr) <= 0 ||
        EVP_DigestUpdate(context, input.c_str(), input.size()) <= 0 ||
        EVP_DigestFinal_ex(context, hash, &length) <= 0) {
        EVP_MD_CTX_free(context);
        throw std::runtime_error("Fallo al calcular SHA256.");
    }
    EVP_MD_CTX_free(context);

    std::ostringstream ss;
    ss << std::hex << std::setfill('0');
    for (unsigned int i = 0; i < length; ++i) {
        ss << std::setw(2) << static_cast<int>(hash[i]);
    }
    return ss.str();
}

bool CanonicalRequestHelper::safeCompare(
    const std::string& a,
    const std::string& b) {

    if (a.size() != b.size()) return false;
    return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}

} // namespace ecf::infra
