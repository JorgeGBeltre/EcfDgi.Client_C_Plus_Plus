#include <cstdio>
#include <string>
#include <cassert>

#include "Infrastructure/Serialization/EcfXsdFileNameResolver.h"
#include "Infrastructure/Serialization/EcfSchemaValidator.h"
#include "Infrastructure/Security/EcfXmlSigner.h"
#include "Infrastructure/Security/EcfSecurityUtils.h"

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

int main() {
    std::printf("Running XSD resolver and validation tests...\n");

    // Test 1: Resolver for e-CF 31
    {
        std::string xml = "<ECF><Encabezado><IdDoc><TipoeCF>31</TipoeCF></IdDoc></Encabezado></ECF>";
        std::string res = ecf::infra::EcfXsdFileNameResolver::resolve(xml);
        CHECK(res == "e-CF 31 v.1.0.xsd", "Resolve e-CF 31");
    }

    // Test 2: Resolver for e-CF 32
    {
        std::string xml = "<ECF><Encabezado><IdDoc><TipoeCF>32</TipoeCF></IdDoc></Encabezado></ECF>";
        std::string res = ecf::infra::EcfXsdFileNameResolver::resolve(xml);
        CHECK(res == "e-CF 32 v.1.0.xsd", "Resolve e-CF 32");
    }

    // Test 3: Resolver for e-CF 34
    {
        std::string xml = "<ECF><Encabezado><IdDoc><TipoeCF>34</TipoeCF></IdDoc></Encabezado></ECF>";
        std::string res = ecf::infra::EcfXsdFileNameResolver::resolve(xml);
        CHECK(res == "e-CF 34 v.1.0.xsd", "Resolve e-CF 34");
    }

    // Test 4: Resolver for e-CF 41
    {
        std::string xml = "<ECF><Encabezado><IdDoc><TipoeCF>41</TipoeCF></IdDoc></Encabezado></ECF>";
        std::string res = ecf::infra::EcfXsdFileNameResolver::resolve(xml);
        CHECK(res == "e-CF 41 v.1.0.xsd", "Resolve e-CF 41");
    }

    // Test 5: Resolver for e-CF 47
    {
        std::string xml = "<ECF><Encabezado><IdDoc><TipoeCF>47</TipoeCF></IdDoc></Encabezado></ECF>";
        std::string res = ecf::infra::EcfXsdFileNameResolver::resolve(xml);
        CHECK(res == "e-CF 47 v.1.0.xsd", "Resolve e-CF 47");
    }

    // Test 6: Resolver for ARECF
    {
        std::string xml = "<ARECF><DetalleAcusedeRecibo></DetalleAcusedeRecibo></ARECF>";
        std::string res = ecf::infra::EcfXsdFileNameResolver::resolve(xml);
        CHECK(res == "ARECF v1.0.xsd", "Resolve ARECF");
    }

    // Test 7: Fallback XML Signer generates valid XMLDSig
    {
        ecf::infra::EcfXmlSigner signer; // Uses fallback cert in memory
        CHECK(signer.usesFallbackCertificate(), "Fallback signer indicates usesFallbackCertificate is true");

        std::string sampleXml = "<?xml version=\"1.0\" encoding=\"utf-8\"?><ECF><Encabezado><IdDoc><TipoeCF>31</TipoeCF><eNCF>E310000000001</eNCF></IdDoc></Encabezado></ECF>";
        std::string signedXml = signer.signXml(sampleXml, "101672919");
        CHECK(signedXml.find("Signature") != std::string::npos, "Signed XML contains Signature");
        CHECK(signedXml.find("SignatureValue") != std::string::npos, "Signed XML contains SignatureValue");

        // Test 8: Security Code extraction and hashing
        std::string secCode = ecf::infra::EcfSecurityUtils::calcularCodigoSeguridad(signedXml);
        CHECK(secCode.length() == 6, "Security code length is 6 hex characters");
    }

    // Test 9: Schema Validator caching
    {
        ecf::infra::EcfSchemaValidator validator("Documentación Técnica (XSD)");
        // Validating an empty XML against invalid schema returns invalid with clear errors
        auto res = validator.validate("<InvalidDoc/>", "Documentación Técnica (XSD)/e-CF 31 v.1.0.xsd");
        CHECK(!res.isValid, "Malformed document fails XSD validation");
        CHECK(!res.errors.empty(), "XSD validation failure provides error descriptions");
    }

    if (failures == 0) {
        std::printf("All XSD and security tests passed successfully!\n");
    } else {
        std::printf("%d test(s) failed.\n", failures);
    }

    return failures ? 1 : 0;
}
