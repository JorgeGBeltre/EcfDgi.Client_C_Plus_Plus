// Minimal tests for EcfValidator.
// No external test framework — returns non-zero on the first failure.

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <openssl/evp.h>
#include <openssl/kdf.h>
#include <openssl/pkcs12.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <drogon/utils/Utilities.h>

#include "Application/Services/EcfValidator.h"
#include "Domain/Entities/Rfce.h"
#include "Infrastructure/Security/CertificateExpiryPolicy.h"
#include "Infrastructure/Security/TenantSignerResolver.h"
#include "Infrastructure/Security/EcfXmlSigner.h"

using namespace ecf;

static int failures = 0;
#define CHECK(cond, msg)                                        \
    do {                                                        \
        if (!(cond)) {                                          \
            std::printf("FAIL: %s\n", msg);                     \
            ++failures;                                         \
        } else {                                                \
            std::printf("ok:   %s\n", msg);                     \
        }                                                       \
    } while (0)

static domain::Rfce makeValidRfce() {
    domain::Rfce r;
    r.encabezado.idDoc.tipoeCF = "32";
    r.encabezado.idDoc.eNcf = "E320000000001";  // 13 chars
    r.encabezado.emisor.rncEmisor = "101672919";  // 9 digits
    r.encabezado.emisor.razonSocialEmisor = "ACME SRL";
    r.encabezado.emisor.fechaEmision = "30-06-2026";  // dd-MM-yyyy
    r.encabezado.totales.montoGravadoTotal = 1000.00;
    r.encabezado.totales.montoTotal = 1180.00;
    r.encabezado.totales.totalITBIS = 180.00;
    return r;
}

int main() {
    app::EcfValidator validator;

    {
        auto rfce = makeValidRfce();
        auto result = validator.validateRfce(rfce);
        CHECK(result.isValid(), "valid RFCE passes validation");
    }

    {
        auto rfce = makeValidRfce();
        rfce.encabezado.totales.montoTotal = 2000.00;  // 1000 + 180 != 2000 (BUG-111)
        auto result = validator.validateRfce(rfce);
        CHECK(!result.isValid(), "inconsistent MontoTotal is rejected");
    }

    {
        auto rfce = makeValidRfce();
        rfce.encabezado.emisor.fechaEmision = "31-02-2026";  // invalid calendar date (BUG-111)
        auto result = validator.validateRfce(rfce);
        CHECK(!result.isValid(), "invalid calendar date (31-02-2026) is rejected");
    }

    {
        auto rfce = makeValidRfce();
        rfce.encabezado.emisor.rncEmisor = "12A45";  // invalid
        auto result = validator.validateRfce(rfce);
        CHECK(!result.isValid(), "invalid RNC is rejected");
    }

    {
        auto rfce = makeValidRfce();
        rfce.encabezado.idDoc.eNcf = "SHORT";  // not 13 chars
        auto result = validator.validateRfce(rfce);
        CHECK(!result.isValid(), "wrong-length eNCF is rejected");
    }

    {
        auto rfce = makeValidRfce();
        rfce.encabezado.totales.montoTotal = 300000.0;  // >= 250k threshold
        auto result = validator.validateRfce(rfce);
        CHECK(!result.isValid(), "amount over RFCE threshold is rejected");
    }

    // CertificateExpiryPolicy tests
    {
        auto now = std::chrono::system_clock::now();
        auto okDate = now + std::chrono::hours(35 * 24);
        auto warningDate = now + std::chrono::hours(20 * 24);
        auto criticalDate = now + std::chrono::hours(3 * 24);

        CHECK(infra::CertificateExpiryPolicy::classify(now, okDate) ==
              infra::CertificateExpiryPolicy::ExpiryUrgency::Ok,
              "Certificate expiry > 30 days is Ok");

        CHECK(infra::CertificateExpiryPolicy::classify(now, warningDate) ==
              infra::CertificateExpiryPolicy::ExpiryUrgency::Warning,
              "Certificate expiry <= 30 days is Warning");

        CHECK(infra::CertificateExpiryPolicy::classify(now, criticalDate) ==
              infra::CertificateExpiryPolicy::ExpiryUrgency::Critical,
              "Certificate expiry <= 7 days is Critical");
    }

    // TenantSignerResolver tests
    {
        auto defaultSigner = std::make_shared<infra::EcfXmlSigner>();
        infra::TenantSignerResolver resolver("", defaultSigner);

        CHECK(resolver.resolveSigner("") == defaultSigner,
              "TenantSignerResolver falls back to default signer on empty RNC");

        CHECK(resolver.resolveSigner("101672919") == defaultSigner,
              "TenantSignerResolver falls back to default signer when RNC not found");

        CHECK(resolver.resolveTenantSignerOnly("") == nullptr,
              "TenantSignerResolver resolveTenantSignerOnly returns nullptr on empty RNC");

        CHECK(resolver.resolveTenantSignerOnly("101672919") == nullptr,
              "TenantSignerResolver resolveTenantSignerOnly returns nullptr when RNC not found");
    }

    // TenantSignerResolver decryptPasswordIfEncrypted tests
    {
        // 1. Plaintext returns unchanged
        CHECK(infra::TenantSignerResolver::decryptPasswordIfEncrypted("PlainTextPass123") == "PlainTextPass123",
              "decryptPasswordIfEncrypted preserves plaintext passwords");

        // 2. Empty string returns empty
        CHECK(infra::TenantSignerResolver::decryptPasswordIfEncrypted("") == "",
              "decryptPasswordIfEncrypted handles empty string");

        // 3. Corrupted enc:v1: does not crash and safely returns empty string
        CHECK(infra::TenantSignerResolver::decryptPasswordIfEncrypted("enc:v1:corrupt:bad") == "",
              "decryptPasswordIfEncrypted safely handles corrupted enc:v1 prefix by returning empty string without throwing");

        // 4. enc:v3: HKDF-SHA256 + AES-256-GCM + AAD round-trip and tenant tamper rejection
#if defined(_WIN32)
        _putenv_s("JWT_SECRET_KEY", "test-master-secret-key-for-hkdf-v3-validation-012345");
#else
        setenv("JWT_SECRET_KEY", "test-master-secret-key-for-hkdf-v3-validation-012345", 1);
#endif
        {
            const std::string masterSecret = "test-master-secret-key-for-hkdf-v3-validation-012345";
            const std::string info = "ecf:certificate-password:v1";
            unsigned char derivedKey[32] = {};
            EVP_PKEY_CTX* pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, nullptr);
            EVP_PKEY_derive_init(pctx);
            EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha256());
            EVP_PKEY_CTX_set1_hkdf_key(pctx, reinterpret_cast<const unsigned char*>(masterSecret.data()), static_cast<int>(masterSecret.size()));
            EVP_PKEY_CTX_add1_hkdf_info(pctx, reinterpret_cast<const unsigned char*>(info.data()), static_cast<int>(info.size()));
            size_t keyLen = 32;
            EVP_PKEY_derive(pctx, derivedKey, &keyLen);
            EVP_PKEY_CTX_free(pctx);

            const std::string plainPwd = "CertPfxSecret#2026!";
            const std::string aad = "tenant:acme-srl|field:CertificatePassword|v3";
            const unsigned char nonce[12] = {1,2,3,4,5,6,7,8,9,10,11,12};
            std::vector<unsigned char> cipher(plainPwd.size());
            unsigned char tag[16] = {};

            EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
            int len = 0;
            EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr);
            EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, 12, nullptr);
            EVP_EncryptInit_ex(ctx, nullptr, nullptr, derivedKey, nonce);
            EVP_EncryptUpdate(ctx, nullptr, &len, reinterpret_cast<const unsigned char*>(aad.data()), static_cast<int>(aad.size()));
            EVP_EncryptUpdate(ctx, cipher.data(), &len, reinterpret_cast<const unsigned char*>(plainPwd.data()), static_cast<int>(plainPwd.size()));
            int finalLen = 0;
            EVP_EncryptFinal_ex(ctx, cipher.data() + len, &finalLen);
            EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag);
            EVP_CIPHER_CTX_free(ctx);

            std::vector<unsigned char> combined;
            combined.insert(combined.end(), nonce, nonce + 12);
            combined.insert(combined.end(), tag, tag + 16);
            combined.insert(combined.end(), cipher.begin(), cipher.end());
            std::string encV3 = "enc:v3:" + drogon::utils::base64Encode(combined.data(), combined.size());

            CHECK(infra::TenantSignerResolver::decryptPasswordIfEncrypted(encV3, masterSecret, "acme-srl") == plainPwd,
                  "decryptPasswordIfEncrypted decrypts enc:v3: HKDF+AAD payload with matching tenantTag");
            CHECK(infra::TenantSignerResolver::decryptPasswordIfEncrypted(encV3, "", "acme-srl") == plainPwd,
                  "decryptPasswordIfEncrypted resolves JWT_SECRET_KEY fallback for enc:v3: payload");
            CHECK(infra::TenantSignerResolver::decryptPasswordIfEncrypted(encV3, masterSecret, "other-tenant") == "",
                  "decryptPasswordIfEncrypted rejects enc:v3: payload when tenantTag AAD differs");
        }
    }

    // Dominican CA personal Tax-Procedures vs Corporate RNC certificate SN validation tests
    {
        auto buildTestPkcs12 = [](const std::string& issuerOrg,
                                  const std::string& subjectOu,
                                  const std::string& subjectSerial,
                                  bool selfSigned) -> std::vector<unsigned char> {
            EVP_PKEY_CTX* kctx = EVP_PKEY_CTX_new_id(EVP_PKEY_RSA, nullptr);
            EVP_PKEY* pkey = nullptr;
            EVP_PKEY_keygen_init(kctx);
            EVP_PKEY_CTX_set_rsa_keygen_bits(kctx, 2048);
            EVP_PKEY_keygen(kctx, &pkey);
            EVP_PKEY_CTX_free(kctx);

            X509* x509 = X509_new();
            ASN1_INTEGER_set(X509_get_serialNumber(x509), 1);
            X509_gmtime_adj(X509_get_notBefore(x509), -60);
            X509_gmtime_adj(X509_get_notAfter(x509), 365 * 86400);
            X509_set_pubkey(x509, pkey);

            X509_NAME* subj = X509_get_subject_name(x509);
            X509_NAME_add_entry_by_txt(subj, "C", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("DO"), -1, -1, 0);
            X509_NAME_add_entry_by_txt(subj, "OU", MBSTRING_ASC, reinterpret_cast<const unsigned char*>(subjectOu.c_str()), -1, -1, 0);
            X509_NAME_add_entry_by_txt(subj, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("JUAN PEREZ REPRESENTANTE"), -1, -1, 0);
            X509_NAME_add_entry_by_txt(subj, "serialNumber", MBSTRING_ASC, reinterpret_cast<const unsigned char*>(subjectSerial.c_str()), -1, -1, 0);

            if (selfSigned) {
                X509_set_issuer_name(x509, subj);
            } else {
                X509_NAME* iss = X509_NAME_new();
                X509_NAME_add_entry_by_txt(iss, "C", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("DO"), -1, -1, 0);
                X509_NAME_add_entry_by_txt(iss, "O", MBSTRING_ASC, reinterpret_cast<const unsigned char*>(issuerOrg.c_str()), -1, -1, 0);
                X509_NAME_add_entry_by_txt(iss, "CN", MBSTRING_ASC, reinterpret_cast<const unsigned char*>("CA DOMINICANA"), -1, -1, 0);
                X509_set_issuer_name(x509, iss);
                X509_NAME_free(iss);
            }

            X509_sign(x509, pkey, EVP_sha256());

            PKCS12* p12 = PKCS12_create("testpwd", "TestCert", pkey, x509, nullptr, 0, 0, 0, 0, 0);
            int len = i2d_PKCS12(p12, nullptr);
            std::vector<unsigned char> der(len);
            unsigned char* p = der.data();
            i2d_PKCS12(p12, &p);

            PKCS12_free(p12);
            X509_free(x509);
            EVP_PKEY_free(pkey);
            return der;
        };

        // Case A: Viafirma / Camara Santo Domingo TAX PROCEDURES personal cedula (11 digits) signing for 9-digit company RNC -> ALLOWED
        auto personalTaxPfx = buildTestPkcs12(
            "CAMARA DE COMERCIO Y PRODUCCION DE SANTO DOMINGO",
            "TAX PROCEDURES",
            "IDCDO-00112345678",
            false);
        infra::EcfXmlSigner repSigner(personalTaxPfx, "testpwd");
        CHECK(repSigner.validateCertificateSn("131234567"),
              "Dominican CA personal TAX PROCEDURES certificate (11-digit cedula) is authorized to sign for 9-digit corporate RNC");

        // Case B: Corporate certificate (RNC-101999888) signing for different corporate RNC (131234567) -> REJECTED
        auto corpPfx = buildTestPkcs12(
            "CAMARA DE COMERCIO Y PRODUCCION DE SANTO DOMINGO",
            "PERSONA JURIDICA",
            "RNC-101999888",
            false);
        infra::EcfXmlSigner corpSigner(corpPfx, "testpwd");
        CHECK(!corpSigner.validateCertificateSn("131234567"),
              "Corporate certificate for RNC 101999888 is strictly rejected when signing for RNC 131234567");
        CHECK(corpSigner.validateCertificateSn("101999888"),
              "Corporate certificate for RNC 101999888 is accepted when signing for matching RNC 101999888");

        // Case C: Non-Dominican CA certificate with 11-digit serial signing for unrelated 9-digit RNC -> REJECTED
        auto untrustedCaPfx = buildTestPkcs12(
            "UNTRUSTED FOREIGN CA INC",
            "TAX PROCEDURES",
            "IDCDO-00112345678",
            false);
        infra::EcfXmlSigner untrustedSigner(untrustedCaPfx, "testpwd");
        CHECK(!untrustedSigner.validateCertificateSn("131234567"),
              "Non-Dominican CA personal certificate is rejected when signing for unrelated corporate RNC");
    }

    std::printf("\n%s (%d failure(s))\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED",
                failures);
    return failures == 0 ? 0 : 1;
}
