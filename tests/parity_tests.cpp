// Comprehensive Parity Tests between C# and C++ implementations
#include <cstdio>
#include <string>
#include <iostream>

#include "Application/Ecf/CanonicalXmlBuilder.h"
#include "Application/Ecf/CanonicalDocumentDto.h"
#include "Domain/Entities/EcfDocument.h"
#include "Domain/Entities/EcfClientOptions.h"
#include "Infrastructure/Security/EcfSecurityUtils.h"
#include "Infrastructure/Dgii/EcfEnvironmentConfig.h"

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

int main() {
    std::cout << "--- Running Parity Tests ---\n";

    // Test 1: IndicadorMontoGravado and FechaVencimientoSecuencia for E31
    {
        app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E31";
        dto.header.fechaEmision = "2026-09-17";
        dto.header.rncComprador = "131234567";
        dto.header.razonSocialComprador = "Empresa Cliente SRL";
        dto.totals.montoTotal = 118.0;
        dto.totals.montoSubtotal = 100.0;
        dto.totals.montoItbis = 18.0;

        std::string xml = app::buildXmlFromCanonical(dto, "E310000000001", "101672919", "WILLY CHIC DOMINICANA SRL");
        
        CHECK(xml.find("<IndicadorMontoGravado>0</IndicadorMontoGravado>") != std::string::npos,
              "E31 XML includes <IndicadorMontoGravado>0</IndicadorMontoGravado>");
        CHECK(xml.find("<FechaVencimientoSecuencia>") != std::string::npos,
              "E31 XML includes default <FechaVencimientoSecuencia>");
    }

    // Test 2: Non-E31 (E32) must NOT have FechaVencimientoSecuencia
    {
        app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E32";
        dto.header.fechaEmision = "2026-09-17";
        dto.header.rncComprador = "131234567";
        dto.totals.montoTotal = 100.0;

        std::string xml = app::buildXmlFromCanonical(dto, "E320000000001", "101672919", "WILLY CHIC DOMINICANA SRL");
        
        CHECK(xml.find("<FechaVencimientoSecuencia>") == std::string::npos,
              "E32 XML excludes <FechaVencimientoSecuencia>");
    }

    // Test 3: IndicadorMontoGravado for E34
    {
        app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E34";
        dto.header.fechaEmision = "2026-09-17";
        dto.header.rncComprador = "131234567";
        dto.totals.montoTotal = 100.0;

        std::string xml = app::buildXmlFromCanonical(dto, "E340000000001", "101672919", "WILLY CHIC DOMINICANA SRL");
        
        CHECK(xml.find("<IndicadorMontoGravado>0</IndicadorMontoGravado>") != std::string::npos,
              "E34 XML includes <IndicadorMontoGravado>0</IndicadorMontoGravado>");
    }

    // Test 4: Explicit FechaVencimientoSecuencia passed in DTO
    {
        app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E31";
        dto.fechaVencimientoSecuencia = "31-12-2029";
        dto.header.fechaEmision = "2026-09-17";
        dto.header.rncComprador = "131234567";
        dto.totals.montoTotal = 100.0;

        std::string xml = app::buildXmlFromCanonical(dto, "E310000000001", "101672919", "WILLY CHIC DOMINICANA SRL");
        
        CHECK(xml.find("<FechaVencimientoSecuencia>31-12-2029</FechaVencimientoSecuencia>") != std::string::npos,
              "Explicit fechaVencimientoSecuencia is respected");
    }

    // Test 5: Multi-email in Comprador
    {
        app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E31";
        dto.header.fechaEmision = "2026-09-17";
        dto.header.rncComprador = "131234567";
        dto.header.correoComprador = "primary@client.com; secondary@client.com, third@client.com";
        dto.totals.montoTotal = 100.0;

        std::string xml = app::buildXmlFromCanonical(dto, "E310000000001", "101672919", "WILLY CHIC DOMINICANA SRL");
        
        CHECK(xml.find("<CorreoComprador>primary@client.com</CorreoComprador>") != std::string::npos,
              "Multi-email is trimmed to primary email in <CorreoComprador>");
        CHECK(xml.find("secondary@client.com") == std::string::npos,
              "Secondary emails are correctly excluded");
    }

    // Test 6: Empty line items fallback assigns defaultTotal
    {
        app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E32";
        dto.header.fechaEmision = "2026-09-17";
        dto.totals.montoTotal = 543.21;
        // lines are empty

        std::string xml = app::buildXmlFromCanonical(dto, "E320000000001", "101672919", "WILLY CHIC DOMINICANA SRL");
        
        CHECK(xml.find("<PrecioUnitarioItem>543.21</PrecioUnitarioItem>") != std::string::npos,
              "Fallback line item inherits total as unit price");
        CHECK(xml.find("<MontoItem>543.21</MontoItem>") != std::string::npos,
              "Fallback line item inherits total as amount");
    }

    // Test 7: Multi-environment field in EcfDocument
    {
        domain::EcfDocument doc;
        doc.ambiente = "PreCertificacion";
        CHECK(doc.ambiente.has_value() && *doc.ambiente == "PreCertificacion",
              "EcfDocument entity supports ambiente");
    }

    // Test 8: DGII CodigoSeguridad calculation (first 6 chars of Base64 signature, case-preserved)
    {
        std::string sampleSig = "wX9yZ123456789==";
        std::string code = infra::EcfSecurityUtils::calcularCodigoSeguridad(sampleSig);
        CHECK(code == "wX9yZ1", "calcularCodigoSeguridad extracts first 6 chars and preserves case");

        std::string xmlWithSig = "<Signature><SignatureValue>AbCdEf123456</SignatureValue></Signature>";
        std::string codeFromXml = infra::EcfSecurityUtils::calcularCodigoSeguridad(xmlWithSig);
        CHECK(codeFromXml == "AbCdEf", "calcularCodigoSeguridad extracts from XML SignatureValue correctly");
    }

    // Test 9: extractFechaHoraFirma extracts timestamp from XML or returns empty optional
    {
        std::string xml = "<Invoice><FechaHoraFirma>19-09-2026 15:30:00</FechaHoraFirma></Invoice>";
        auto dt = infra::EcfSecurityUtils::extractFechaHoraFirma(xml);
        CHECK(dt.has_value() && *dt == "19-09-2026 15:30:00", "extractFechaHoraFirma correctly parses <FechaHoraFirma>");

        std::string xmlWithout = "<Invoice><Detalle/></Invoice>";
        auto dtEmpty = infra::EcfSecurityUtils::extractFechaHoraFirma(xmlWithout);
        CHECK(!dtEmpty.has_value(), "extractFechaHoraFirma returns empty optional when tag is absent");
    }

    // Test 10: DGII Timbre URL uses ecf.dgii.gov.do host across environments
    {
        auto prodConfig = infra::EcfEnvironmentConfig::getConfig(domain::AmbienteEnum::Produccion);
        CHECK(prodConfig.timbreFcUrl.find("ecf.dgii.gov.do") != std::string::npos,
              "Produccion timbreFcUrl uses ecf.dgii.gov.do");
        CHECK(prodConfig.timbreFcUrl.find("fc.dgii.gov.do") == std::string::npos,
              "Produccion timbreFcUrl does not use deprecated fc.dgii.gov.do");

        auto certConfig = infra::EcfEnvironmentConfig::getConfig(domain::AmbienteEnum::Certificacion);
        CHECK(certConfig.timbreFcUrl.find("ecf.dgii.gov.do") != std::string::npos,
              "Certificacion timbreFcUrl uses ecf.dgii.gov.do");

        auto preCertConfig = infra::EcfEnvironmentConfig::getConfig(domain::AmbienteEnum::PreCertificacion);
        CHECK(preCertConfig.timbreFcUrl.find("ecf.dgii.gov.do") != std::string::npos,
              "PreCertificacion timbreFcUrl uses ecf.dgii.gov.do");
    }

    std::printf("\nTotal failures: %d\n", failures);
    return failures > 0 ? 1 : 0;
}
