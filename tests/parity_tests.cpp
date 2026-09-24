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

    // Test 10: DGII Timbre URL for Facturas de Consumo uses fc.dgii.gov.do across environments
    {
        auto prodConfig = infra::EcfEnvironmentConfig::getConfig(domain::AmbienteEnum::Produccion);
        CHECK(prodConfig.timbreFcUrl.find("fc.dgii.gov.do") != std::string::npos,
              "Produccion timbreFcUrl uses fc.dgii.gov.do");
        CHECK(prodConfig.timbreFcUrl == "https://fc.dgii.gov.do/ecf/consultatimbrefc",
              "Produccion timbreFcUrl matches exact DGII URL");

        auto certConfig = infra::EcfEnvironmentConfig::getConfig(domain::AmbienteEnum::Certificacion);
        CHECK(certConfig.timbreFcUrl.find("fc.dgii.gov.do") != std::string::npos,
              "Certificacion timbreFcUrl uses fc.dgii.gov.do");
        CHECK(certConfig.timbreFcUrl == "https://fc.dgii.gov.do/certecf/consultatimbrefc",
              "Certificacion timbreFcUrl matches exact DGII URL");

        auto preCertConfig = infra::EcfEnvironmentConfig::getConfig(domain::AmbienteEnum::PreCertificacion);
        CHECK(preCertConfig.timbreFcUrl.find("fc.dgii.gov.do") != std::string::npos,
              "PreCertificacion timbreFcUrl uses fc.dgii.gov.do");
        CHECK(preCertConfig.timbreFcUrl == "https://fc.dgii.gov.do/testecf/consultatimbrefc",
              "PreCertificacion timbreFcUrl matches exact DGII URL");
    }

    // Test 11: buildRfceXml generates valid RFCE with ITBIS gravado and CodigoSeguridadeCF
    {
        domain::EcfDocument doc;
        doc.eNcf = "E320000000015";
        doc.rncEmisor = "101672919";
        doc.rncComprador = "40223456789";
        doc.totalAmount = 118.0;
        doc.itbisAmount = 18.0;
        doc.securityCode = "xYz123";

        app::CanonicalDocumentDto dto;
        dto.header.fechaEmision = "2026-09-22";
        dto.header.razonSocialComprador = "Juan Perez";

        std::string rfceXml = app::buildRfceXml(doc, &dto, "WILLY CHIC DOMINICANA SRL");

        CHECK(rfceXml.rfind("<?xml version=\"1.0\" encoding=\"utf-8\"?>", 0) == 0,
              "buildRfceXml starts with XML declaration");
        CHECK(rfceXml.find("<RFCE>") != std::string::npos, "RFCE root tag present");
        CHECK(rfceXml.find("<TipoeCF>32</TipoeCF>") != std::string::npos, "TipoeCF is 32");
        CHECK(rfceXml.find("<eNCF>E320000000015</eNCF>") != std::string::npos, "eNCF matches doc");
        CHECK(rfceXml.find("<RNCEmisor>101672919</RNCEmisor>") != std::string::npos, "RNCEmisor matches");
        CHECK(rfceXml.find("<RazonSocialEmisor>WILLY CHIC DOMINICANA SRL</RazonSocialEmisor>") != std::string::npos, "Emisor name present");
        CHECK(rfceXml.find("<FechaEmision>22-09-2026</FechaEmision>") != std::string::npos, "Normalized FechaEmision present");
        CHECK(rfceXml.find("<RNCComprador>40223456789</RNCComprador>") != std::string::npos, "RNCComprador present");
        CHECK(rfceXml.find("<RazonSocialComprador>Juan Perez</RazonSocialComprador>") != std::string::npos, "Comprador name present");
        CHECK(rfceXml.find("<MontoGravadoTotal>100.00</MontoGravadoTotal>") != std::string::npos, "MontoGravadoTotal = 100.00");
        CHECK(rfceXml.find("<MontoGravadoI1>100.00</MontoGravadoI1>") != std::string::npos, "MontoGravadoI1 = 100.00");
        CHECK(rfceXml.find("<TotalITBIS>18.00</TotalITBIS>") != std::string::npos, "TotalITBIS = 18.00");
        CHECK(rfceXml.find("<TotalITBIS1>18.00</TotalITBIS1>") != std::string::npos, "TotalITBIS1 = 18.00");
        CHECK(rfceXml.find("<MontoTotal>118.00</MontoTotal>") != std::string::npos, "MontoTotal = 118.00");
        CHECK(rfceXml.find("<CodigoSeguridadeCF>xYz123</CodigoSeguridadeCF>") != std::string::npos, "CodigoSeguridadeCF matches securityCode");
        CHECK(rfceXml.find("</RFCE>") != std::string::npos, "RFCE closing tag present");
    }

    // Test 12: buildRfceXml with exento (no ITBIS) produces MontoExento
    {
        domain::EcfDocument doc;
        doc.eNcf = "E320000000020";
        doc.rncEmisor = "101672919";
        doc.totalAmount = 500.0;
        doc.itbisAmount = 0.0;
        doc.securityCode = "SeCuRe";

        std::string rfceXml = app::buildRfceXml(doc, nullptr, "WILLY CHIC DOMINICANA SRL");

        CHECK(rfceXml.find("<MontoExento>500.00</MontoExento>") != std::string::npos, "MontoExento = 500.00");
        CHECK(rfceXml.find("<TotalITBIS>") == std::string::npos, "TotalITBIS absent when exento");
        CHECK(rfceXml.find("<MontoGravadoTotal>") == std::string::npos, "MontoGravadoTotal absent when exento");
        CHECK(rfceXml.find("<RazonSocialComprador>CONSUMIDOR FINAL</RazonSocialComprador>") != std::string::npos,
              "Defaults to CONSUMIDOR FINAL when dto is null");
    }

    // Test 13: RFCE vs ECF decision rule
    {
        auto isRfceDoc = [](const std::string& encf, double total) {
            return (encf.rfind("E32", 0) == 0 || encf.rfind("e32", 0) == 0) && total < 250000.0;
        };

        CHECK(isRfceDoc("E320000000001", 100.0) == true, "E32 below 250k is RFCE");
        CHECK(isRfceDoc("E320000000002", 249999.99) == true, "E32 at 249,999.99 is RFCE");
        CHECK(isRfceDoc("E320000000003", 250000.0) == false, "E32 at 250,000.0 is regular ECF");
        CHECK(isRfceDoc("E320000000004", 300000.0) == false, "E32 above 250k is regular ECF");
        CHECK(isRfceDoc("E310000000001", 50.0) == false, "E31 is never RFCE");
        CHECK(isRfceDoc("E340000000001", 50.0) == false, "E34 is never RFCE");
    }

    // Test 14: Environment alias resolution
    {
        auto resolveAlt = [](const std::string& tenantId) -> std::string {
            auto endsWithNoCase = [](const std::string& str, const std::string& suffix) {
                if (str.size() < suffix.size()) return false;
                return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin(),
                                  [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); });
            };
            const std::string sPre = ":PreCertificacion";
            const std::string sTest = ":TestEcf";
            const std::string sCert = ":Certificacion";
            const std::string sCertEcf = ":CertEcf";
            if (endsWithNoCase(tenantId, sPre)) return tenantId.substr(0, tenantId.size() - sPre.size()) + sTest;
            if (endsWithNoCase(tenantId, sTest)) return tenantId.substr(0, tenantId.size() - sTest.size()) + sPre;
            if (endsWithNoCase(tenantId, sCert)) return tenantId.substr(0, tenantId.size() - sCert.size()) + sCertEcf;
            if (endsWithNoCase(tenantId, sCertEcf)) return tenantId.substr(0, tenantId.size() - sCertEcf.size()) + sCert;
            return "";
        };

        CHECK(resolveAlt("t1:PreCertificacion") == "t1:TestEcf", ":PreCertificacion maps to :TestEcf");
        CHECK(resolveAlt("t1:TestEcf") == "t1:PreCertificacion", ":TestEcf maps to :PreCertificacion");
        CHECK(resolveAlt("t1:Certificacion") == "t1:CertEcf", ":Certificacion maps to :CertEcf");
        CHECK(resolveAlt("t1:CertEcf") == "t1:Certificacion", ":CertEcf maps to :CertEcf");
        CHECK(resolveAlt("t1:Produccion") == "", "Produccion has no alternate alias");
    }

    // Test 15: Item discount emits DescuentoMonto and TablaSubDescuento block
    {
        app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E31";
        dto.header.rncEmisor = "101672919";
        dto.header.razonSocialEmisor = "WILLY CHIC DOMINICANA SRL";
        dto.header.rncComprador = "130000000";
        dto.header.razonSocialComprador = "Cliente de Prueba";
        dto.totals.montoSubtotal = 100.0;
        dto.totals.montoItbis = 18.0;
        dto.totals.montoTotal = 118.0;

        app::CanonicalLineDto line1;
        line1.lineNumber = 1;
        line1.itemName = "Item Normal";
        line1.quantity = 2.0;
        line1.unitPrice = 100.0;
        line1.amount = 200.0;
        dto.lines.push_back(line1);

        app::CanonicalLineDto line2;
        line2.lineNumber = 2;
        line2.itemName = "Descuento 50%";
        line2.quantity = 1.0;
        line2.unitPrice = -100.0;
        line2.amount = -100.0;
        dto.lines.push_back(line2);

        std::string xml = app::buildXmlFromCanonical(dto, "E310000000001", "101672919", "WILLY CHIC DOMINICANA SRL");

        CHECK(xml.find("<DescuentoMonto>100.00</DescuentoMonto>") != std::string::npos,
              "DescuentoMonto tag emitted with 100.00");
        CHECK(xml.find("<TablaSubDescuento>") != std::string::npos,
              "TablaSubDescuento block emitted");
        CHECK(xml.find("<TipoSubDescuento>$</TipoSubDescuento>") != std::string::npos,
              "TipoSubDescuento is $");
        CHECK(xml.find("<MontoSubDescuento>100.00</MontoSubDescuento>") != std::string::npos,
              "MontoSubDescuento tag emitted with 100.00");
        CHECK(xml.find("<PrecioUnitarioItem>-") == std::string::npos,
              "Negative price item not emitted");
        CHECK(xml.find("<MontoItem>-") == std::string::npos,
              "Negative amount item not emitted");
    }

    // Test 16: Tipo 46 without buyer RNC emits IdentificadorExtranjero EXTRANJERO fallback
    {
        app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E46";
        dto.header.fechaEmision = "2026-09-24";
        dto.header.rncComprador = "";
        dto.header.razonSocialComprador = "Foreign Client Corp";
        dto.totals.montoTotal = 500.0;
        dto.totals.montoSubtotal = 500.0;

        std::string xml = app::buildXmlFromCanonical(dto, "E460000000028", "101889063", "Willy Chic");
        CHECK(xml.find("<IdentificadorExtranjero>EXTRANJERO</IdentificadorExtranjero>") != std::string::npos,
              "E46 without buyer RNC emits <IdentificadorExtranjero>EXTRANJERO</IdentificadorExtranjero>");
        CHECK(xml.find("<RNCComprador>") == std::string::npos,
              "E46 without buyer RNC does NOT emit <RNCComprador>");
    }

    // Test 17: Dynamic sequence expiry date in test/cert environment defaults to 31-12-2028
    {
        app::CanonicalDocumentDto dtoPreCert;
        dtoPreCert.tipoComprobante = "E31";
        dtoPreCert.environment = "PreCertificacion";
        dtoPreCert.header.fechaEmision = "2026-09-24";
        dtoPreCert.header.rncComprador = "131234567";
        dtoPreCert.totals.montoTotal = 100.0;

        std::string xmlPreCert = app::buildXmlFromCanonical(dtoPreCert, "E310000000001", "101672919", "WILLY CHIC DOMINICANA SRL");
        CHECK(xmlPreCert.find("<FechaVencimientoSecuencia>31-12-2028</FechaVencimientoSecuencia>") != std::string::npos,
              "PreCertificacion environment defaults sequence expiry to 31-12-2028");

        app::CanonicalDocumentDto dtoCert;
        dtoCert.tipoComprobante = "E31";
        dtoCert.environment = "Certificacion";
        dtoCert.header.fechaEmision = "2026-09-24";
        dtoCert.header.rncComprador = "131234567";
        dtoCert.totals.montoTotal = 100.0;

        std::string xmlCert = app::buildXmlFromCanonical(dtoCert, "E310000000001", "101672919", "WILLY CHIC DOMINICANA SRL");
        CHECK(xmlCert.find("<FechaVencimientoSecuencia>31-12-2028</FechaVencimientoSecuencia>") != std::string::npos,
              "Certificacion environment defaults sequence expiry to 31-12-2028");
    }

    std::printf("\nTotal failures: %d\n", failures);
    return failures > 0 ? 1 : 0;
}
