#include <cstdio>
#include <string>
#include <vector>
#include <filesystem>
#include <cassert>

#include "Infrastructure/Serialization/EcfXsdFileNameResolver.h"
#include "Infrastructure/Serialization/EcfSchemaValidator.h"
#include "Infrastructure/Serialization/EcfXmlSerializer.h"
#include "Infrastructure/Security/EcfXmlSigner.h"
#include "Infrastructure/Security/EcfSecurityUtils.h"
#include "Domain/Entities/Rfce.h"
#include "Domain/Entities/ResponseModels.h"

static std::string resolveXsdDir() {
    std::vector<std::string> candidates = {
        "Documentación Técnica (XSD)",
        "../Documentación Técnica (XSD)",
        "../../Documentación Técnica (XSD)",
        "C:/Users/Jorge/Pictures/DGII/EcfDgi.Client_C_Plus_Plus/Documentación Técnica (XSD)"
    };
    for (const auto& c : candidates) {
        auto p = std::filesystem::path(std::u8string_view(
            reinterpret_cast<const char8_t*>(c.data()), c.size()));
        if (std::filesystem::exists(p)) return c;
    }
    return "Documentación Técnica (XSD)";
}

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
        std::string xsdDir = resolveXsdDir();
        ecf::infra::EcfSchemaValidator validator(xsdDir);
        // Validating an empty XML against invalid schema returns invalid with clear errors
        auto res = validator.validate("<InvalidDoc/>", xsdDir + "/e-CF 31 v.1.0.xsd");
        CHECK(!res.isValid, "Malformed document fails XSD validation");
        CHECK(!res.errors.empty(), "XSD validation failure provides error descriptions");
    }

    // Test 10: Timbre URL Spelling Parity (codigoseguridad, not codigoseuridad)
    {
        ecf::domain::TimbreEcfRequest ecfReq;
        ecfReq.rncEmisor = "101672919";
        ecfReq.rncComprador = "101889063";
        ecfReq.eNcf = "E310000000001";
        ecfReq.fechaEmision = "10-10-2020";
        ecfReq.montoTotal = 100.50;
        ecfReq.fechaFirma = "10-10-2020 09:00:00";
        ecfReq.codigoSeguridad = "abcd12";

        ecf::domain::TimbreFcRequest fcReq;
        fcReq.rncEmisor = "101672919";
        fcReq.eNcf = "E320000000001";
        fcReq.montoTotal = 100.50;
        fcReq.codigoSeguridad = "abcd12";

        std::string ecfUrl = ecf::infra::EcfSecurityUtils::buildTimbreUrl("https://example.com/timbre", ecfReq);
        std::string fcUrl = ecf::infra::EcfSecurityUtils::buildTimbreFcUrl("https://example.com/timbrefc", fcReq);

        CHECK(ecfUrl.find("codigoseguridad=") != std::string::npos, "ecfUrl contains codigoseguridad=");
        CHECK(ecfUrl.find("codigoseuridad=") == std::string::npos, "ecfUrl does not contain codigoseuridad=");
        CHECK(fcUrl.find("codigoseguridad=") != std::string::npos, "fcUrl contains codigoseguridad=");
        CHECK(fcUrl.find("codigoseuridad=") == std::string::npos, "fcUrl does not contain codigoseuridad=");
    }

    // Test 11: RFCE Serialization, CodigoSeguridadeCF placement, and Schema Validation
    {
        ecf::domain::Rfce rfce;
        rfce.encabezado.version = "1.0";
        rfce.encabezado.idDoc.tipoeCF = "32";
        rfce.encabezado.idDoc.eNcf = "E320000000001";
        rfce.encabezado.idDoc.tipoIngresos = 1;
        rfce.encabezado.idDoc.tipoPago = 1;
        rfce.encabezado.emisor.rncEmisor = "101672919";
        rfce.encabezado.emisor.razonSocialEmisor = "WILLY CHIC DOMINICANA SRL";
        rfce.encabezado.emisor.fechaEmision = "10-10-2020";
        ecf::domain::RfceComprador comp;
        comp.rncComprador = "101889063";
        comp.razonSocialComprador = "Cliente Test";
        rfce.encabezado.comprador = comp;
        rfce.encabezado.totales.montoTotal = 100.50;
        rfce.encabezado.totales.totalITBIS = 18.00;
        rfce.encabezado.codigoSeguridadeCF = "ABCD12";

        ecf::infra::EcfXmlSerializer serializer;
        std::string xml = serializer.serialize(rfce);

        // Assert tag casing parity with C# RfceSerializationTests
        CHECK(xml.find("<Version>1.0</Version>") != std::string::npos, "RFCE XML has <Version>1.0</Version>");
        CHECK(xml.find("<eNCF>E320000000001</eNCF>") != std::string::npos, "RFCE XML has <eNCF>");
        CHECK(xml.find("<RNCEmisor>101672919</RNCEmisor>") != std::string::npos, "RFCE XML has <RNCEmisor>");
        CHECK(xml.find("<RNCComprador>101889063</RNCComprador>") != std::string::npos, "RFCE XML has <RNCComprador>");
        CHECK(xml.find("<CodigoSeguridadeCF>ABCD12</CodigoSeguridadeCF>") != std::string::npos, "RFCE XML has <CodigoSeguridadeCF>");

        // Verify CodigoSeguridadeCF is child of Encabezado, after </Totales>
        auto idxSecCode = xml.find("<CodigoSeguridadeCF>");
        auto idxTotales = xml.find("</Totales>");
        auto idxEnc = xml.find("</Encabezado>");
        CHECK(idxTotales != std::string::npos && idxSecCode > idxTotales, "CodigoSeguridadeCF is placed after </Totales>");
        CHECK(idxEnc != std::string::npos && idxSecCode < idxEnc, "CodigoSeguridadeCF is placed before </Encabezado>");

        // Sign XML with fallback signer
        ecf::infra::EcfXmlSigner signer;
        std::string signedXml = signer.signXml(xml, "101672919");
        CHECK(signedXml.find("Signature") != std::string::npos, "Signed RFCE XML contains Signature");

        // Validate against RFCE 32 v.1.0.xsd
        std::string xsdDir = resolveXsdDir();
        ecf::infra::EcfSchemaValidator validator(xsdDir);
        auto res = validator.validate(signedXml, xsdDir + "/RFCE 32 v.1.0.xsd");
        if (!res.isValid) {
            for (const auto& err : res.errors) {
                std::printf("  RFCE validation error: %s\n", err.c_str());
            }
        }
        CHECK(res.isValid, "Signed RFCE validates against RFCE 32 v.1.0.xsd");
    }

    // Test 12: Certificate validation with self-signed bypass and Dominican RNC
    {
        ecf::infra::EcfXmlSigner signer;
        CHECK(signer.validateCertificateSn("101672919"), "Fallback cert passes self-signed validation");
        CHECK(signer.validateCertificateSn("131-23456-7"), "Fallback cert passes formatted RNC");
    }

    if (failures == 0) {
        std::printf("All XSD and security tests passed successfully!\n");
    } else {
        std::printf("%d test(s) failed.\n", failures);
    }

    return failures ? 1 : 0;
}
