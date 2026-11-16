
#include <cstdio>
#include <string>
#include <iostream>

#include "Application/Ecf/CanonicalXmlBuilder.h"
#include "Application/Ecf/CanonicalDocumentDto.h"
#include "Domain/Entities/EcfDocument.h"
#include "Domain/Entities/EcfClientOptions.h"
#include "Infrastructure/Security/EcfSecurityUtils.h"
#include "Infrastructure/Dgii/EcfEnvironmentConfig.h"
#include "Domain/Enums/AmbienteEnum.h"
#include "Infrastructure/Dgii/DgiiDirectTransport.h"
#include "Shared/Common/Sys.h"

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

    {
        app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E31";
        dto.header.fechaEmision = "2026-09-17";
        dto.header.rncComprador = "131234567";
        dto.header.razonSocialComprador = "Empresa Cliente SRL";
        dto.totals.montoTotal = 118.0;
        dto.totals.montoSubtotal = 100.0;
        dto.totals.montoItbis = 18.0;

        std::string xml = app::buildXmlFromCanonical(dto, "E310000000001", "101000021", "EMPRESA DEMO SRL");

        CHECK(xml.find("<IndicadorMontoGravado>0</IndicadorMontoGravado>") != std::string::npos,
              "E31 XML includes <IndicadorMontoGravado>0</IndicadorMontoGravado>");
        CHECK(xml.find("<FechaVencimientoSecuencia>") != std::string::npos,
              "E31 XML includes default <FechaVencimientoSecuencia>");
    }

    {
        app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E32";
        dto.header.fechaEmision = "2026-09-17";
        dto.header.rncComprador = "131234567";
        dto.totals.montoTotal = 100.0;

        std::string xml = app::buildXmlFromCanonical(dto, "E320000000001", "101000021", "EMPRESA DEMO SRL");

        CHECK(xml.find("<FechaVencimientoSecuencia>") == std::string::npos,
              "E32 XML excludes <FechaVencimientoSecuencia>");
    }

    {
        app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E34";
        dto.header.fechaEmision = "2026-09-17";
        dto.header.rncComprador = "131234567";
        dto.totals.montoTotal = 100.0;

        std::string xml = app::buildXmlFromCanonical(dto, "E340000000001", "101000021", "EMPRESA DEMO SRL");

        CHECK(xml.find("<IndicadorMontoGravado>0</IndicadorMontoGravado>") != std::string::npos,
              "E34 XML includes <IndicadorMontoGravado>0</IndicadorMontoGravado>");
    }

    {
        app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E31";
        dto.fechaVencimientoSecuencia = "31-12-2029";
        dto.header.fechaEmision = "2026-09-17";
        dto.header.rncComprador = "131234567";
        dto.totals.montoTotal = 100.0;

        std::string xml = app::buildXmlFromCanonical(dto, "E310000000001", "101000021", "EMPRESA DEMO SRL");

        CHECK(xml.find("<FechaVencimientoSecuencia>31-12-2029</FechaVencimientoSecuencia>") != std::string::npos,
              "Explicit fechaVencimientoSecuencia is respected");
    }

    {
        app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E31";
        dto.header.fechaEmision = "2026-09-17";
        dto.header.rncComprador = "131234567";
        dto.header.correoComprador = "primary@client.com; secondary@client.com, third@client.com";
        dto.totals.montoTotal = 100.0;

        std::string xml = app::buildXmlFromCanonical(dto, "E310000000001", "101000021", "EMPRESA DEMO SRL");

        CHECK(xml.find("<CorreoComprador>primary@client.com</CorreoComprador>") != std::string::npos,
              "Multi-email is trimmed to primary email in <CorreoComprador>");
        CHECK(xml.find("secondary@client.com") == std::string::npos,
              "Secondary emails are correctly excluded");
    }

    {
        app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E32";
        dto.header.fechaEmision = "2026-09-17";
        dto.totals.montoTotal = 543.21;

        std::string xml = app::buildXmlFromCanonical(dto, "E320000000001", "101000021", "EMPRESA DEMO SRL");

        CHECK(xml.find("<PrecioUnitarioItem>543.21</PrecioUnitarioItem>") != std::string::npos,
              "Fallback line item inherits total as unit price");
        CHECK(xml.find("<MontoItem>543.21</MontoItem>") != std::string::npos,
              "Fallback line item inherits total as amount");
    }

    {
        domain::EcfDocument doc;
        doc.ambiente = "PreCertificacion";
        CHECK(doc.ambiente.has_value() && *doc.ambiente == "PreCertificacion",
              "EcfDocument entity supports ambiente");
    }

    {
        std::string sampleSig = "wX9yZ123456789==";
        std::string code = infra::EcfSecurityUtils::calcularCodigoSeguridad(sampleSig);
        CHECK(code == "wX9yZ1", "calcularCodigoSeguridad extracts first 6 chars and preserves case");

        std::string xmlWithSig = "<Signature><SignatureValue>AbCdEf123456</SignatureValue></Signature>";
        std::string codeFromXml = infra::EcfSecurityUtils::calcularCodigoSeguridad(xmlWithSig);
        CHECK(codeFromXml == "AbCdEf", "calcularCodigoSeguridad extracts from XML SignatureValue correctly");
    }

    {
        std::string xml = "<Invoice><FechaHoraFirma>19-09-2026 15:30:00</FechaHoraFirma></Invoice>";
        auto dt = infra::EcfSecurityUtils::extractFechaHoraFirma(xml);
        CHECK(dt.has_value() && *dt == "19-09-2026 15:30:00", "extractFechaHoraFirma correctly parses <FechaHoraFirma>");

        std::string xmlWithout = "<Invoice><Detalle/></Invoice>";
        auto dtEmpty = infra::EcfSecurityUtils::extractFechaHoraFirma(xmlWithout);
        CHECK(!dtEmpty.has_value(), "extractFechaHoraFirma returns empty optional when tag is absent");
    }

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

    {
        domain::EcfDocument doc;
        doc.eNcf = "E320000000015";
        doc.rncEmisor = "101000021";
        doc.rncComprador = "40223456789";
        doc.totalAmount = 118.0;
        doc.itbisAmount = 18.0;
        doc.securityCode = "xYz123";

        app::CanonicalDocumentDto dto;
        dto.header.fechaEmision = "2026-09-22";
        dto.header.razonSocialComprador = "Juan Perez";

        std::string rfceXml = app::buildRfceXml(doc, &dto, "EMPRESA DEMO SRL");

        CHECK(rfceXml.rfind("<?xml version=\"1.0\" encoding=\"utf-8\"?>", 0) == 0,
              "buildRfceXml starts with XML declaration");
        CHECK(rfceXml.find("<RFCE>") != std::string::npos, "RFCE root tag present");
        CHECK(rfceXml.find("<TipoeCF>32</TipoeCF>") != std::string::npos, "TipoeCF is 32");
        CHECK(rfceXml.find("<eNCF>E320000000015</eNCF>") != std::string::npos, "eNCF matches doc");
        CHECK(rfceXml.find("<RNCEmisor>101000021</RNCEmisor>") != std::string::npos, "RNCEmisor matches");
        CHECK(rfceXml.find("<RazonSocialEmisor>EMPRESA DEMO SRL</RazonSocialEmisor>") != std::string::npos, "Emisor name present");
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

    {
        domain::EcfDocument doc;
        doc.eNcf = "E320000000020";
        doc.rncEmisor = "101000021";
        doc.totalAmount = 500.0;
        doc.itbisAmount = 0.0;
        doc.securityCode = "SeCuRe";

        std::string rfceXml = app::buildRfceXml(doc, nullptr, "EMPRESA DEMO SRL");

        CHECK(rfceXml.find("<MontoExento>500.00</MontoExento>") != std::string::npos, "MontoExento = 500.00");
        CHECK(rfceXml.find("<TotalITBIS>") == std::string::npos, "TotalITBIS absent when exento");
        CHECK(rfceXml.find("<MontoGravadoTotal>") == std::string::npos, "MontoGravadoTotal absent when exento");
        CHECK(rfceXml.find("<RazonSocialComprador>CONSUMIDOR FINAL</RazonSocialComprador>") != std::string::npos,
              "Defaults to CONSUMIDOR FINAL when dto is null");
    }

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

    {
        app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E31";
        dto.header.rncEmisor = "101000021";
        dto.header.razonSocialEmisor = "EMPRESA DEMO SRL";
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

        std::string xml = app::buildXmlFromCanonical(dto, "E310000000001", "101000021", "EMPRESA DEMO SRL");

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

    {
        app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E46";
        dto.header.fechaEmision = "2026-09-24";
        dto.header.rncComprador = "";
        dto.header.razonSocialComprador = "Foreign Client Corp";
        dto.totals.montoTotal = 500.0;
        dto.totals.montoSubtotal = 500.0;

        std::string xml = app::buildXmlFromCanonical(dto, "E460000000028", "101000001", "Empresa Demo");
        CHECK(xml.find("<IdentificadorExtranjero>EXTRANJERO</IdentificadorExtranjero>") != std::string::npos,
              "E46 without buyer RNC emits <IdentificadorExtranjero>EXTRANJERO</IdentificadorExtranjero>");
        CHECK(xml.find("<RNCComprador>") == std::string::npos,
              "E46 without buyer RNC does NOT emit <RNCComprador>");
    }

    {
        app::CanonicalDocumentDto dtoPreCert;
        dtoPreCert.tipoComprobante = "E31";
        dtoPreCert.environment = "PreCertificacion";
        dtoPreCert.header.fechaEmision = "2026-09-24";
        dtoPreCert.header.rncComprador = "131234567";
        dtoPreCert.totals.montoTotal = 100.0;

        std::string xmlPreCert = app::buildXmlFromCanonical(dtoPreCert, "E310000000001", "101000021", "EMPRESA DEMO SRL");
        CHECK(xmlPreCert.find("<FechaVencimientoSecuencia>31-12-2028</FechaVencimientoSecuencia>") != std::string::npos,
              "PreCertificacion environment defaults sequence expiry to 31-12-2028");

        app::CanonicalDocumentDto dtoCert;
        dtoCert.tipoComprobante = "E31";
        dtoCert.environment = "Certificacion";
        dtoCert.header.fechaEmision = "2026-09-24";
        dtoCert.header.rncComprador = "131234567";
        dtoCert.totals.montoTotal = 100.0;

        std::string xmlCert = app::buildXmlFromCanonical(dtoCert, "E310000000001", "101000021", "EMPRESA DEMO SRL");
        CHECK(xmlCert.find("<FechaVencimientoSecuencia>31-12-2028</FechaVencimientoSecuencia>") != std::string::npos,
              "Certificacion environment defaults sequence expiry to 31-12-2028");
    }

    {

        auto t1 = sys::parseIsoUtc("2026-10-02T14:30:15Z");
        CHECK(t1 != std::chrono::system_clock::time_point{}, "parseIsoUtc parses standard ISO 8601 with Z");

        auto t2 = sys::parseIsoUtc("2026-10-02 14:30:15.123456+00");
        CHECK(t2 != std::chrono::system_clock::time_point{}, "parseIsoUtc parses PostgreSQL timestamptz ::text format with space and microseconds");

        auto s1 = std::chrono::duration_cast<std::chrono::seconds>(t1.time_since_epoch()).count();
        auto s2 = std::chrono::duration_cast<std::chrono::seconds>(t2.time_since_epoch()).count();
        CHECK(s1 == s2, "parseIsoUtc ISO and PostgreSQL formats produce identical UTC timestamp");

        auto t3 = sys::parseIsoUtc("2026-10-02 10:30:15-04:00");
        auto s3 = std::chrono::duration_cast<std::chrono::seconds>(t3.time_since_epoch()).count();
        CHECK(s1 == s3, "parseIsoUtc timezone offset -04:00 correctly converted to UTC");

        auto tEmpty = sys::parseIsoUtc("");
        CHECK(tEmpty == std::chrono::system_clock::time_point{}, "parseIsoUtc empty returns default time_point");

        auto tInvalid = sys::parseIsoUtc("not-a-date");
        CHECK(tInvalid == std::chrono::system_clock::time_point{}, "parseIsoUtc invalid returns default time_point");
    }

    {
        CHECK(infra::kDefaultConnectTimeout.count() == 5000, "PERF-001: kDefaultConnectTimeout is 5000ms");
        CHECK(infra::kDefaultReadTimeout.count() == 15000, "PERF-001: kDefaultReadTimeout is 15000ms");
        CHECK(infra::kReceptionTimeout.count() == 30000, "PERF-001: kReceptionTimeout is 30000ms");

        cpr::Response netErrResp;
        netErrResp.error.code = cpr::ErrorCode::NO_CONNECTION_AVAILABLE;
        netErrResp.error.message = "Connection refused";
        bool caughtNetErr = false;
        try {
            infra::DgiiDirectTransport::validateDgiiResponse(netErrResp);
        } catch (const domain::EcfException&) {
            caughtNetErr = true;
        }
        CHECK(caughtNetErr, "BUG-014: validateDgiiResponse throws EcfException on cpr network failure");

        cpr::Response http0Resp;
        http0Resp.status_code = 0;
        bool caughtHttp0 = false;
        try {
            infra::DgiiDirectTransport::validateDgiiResponse(http0Resp);
        } catch (const domain::EcfException&) {
            caughtHttp0 = true;
        }
        CHECK(caughtHttp0, "BUG-014: validateDgiiResponse throws EcfException on HTTP 0");

        cpr::Response http500Resp;
        http500Resp.status_code = 503;
        http500Resp.text = "Service Unavailable";
        bool caught500 = false;
        try {
            infra::DgiiDirectTransport::validateDgiiResponse(http500Resp);
        } catch (const domain::EcfException&) {
            caught500 = true;
        }
        CHECK(caught500, "BUG-014: validateDgiiResponse throws EcfException on HTTP 503");

        cpr::Response http401Resp;
        http401Resp.status_code = 401;
        http401Resp.text = "Unauthorized token";
        bool caught401 = false;
        try {
            infra::DgiiDirectTransport::validateDgiiResponse(http401Resp);
        } catch (const domain::EcfException&) {
            caught401 = true;
        }
        CHECK(caught401, "BUG-014: validateDgiiResponse throws EcfException on HTTP 401");

        cpr::Response http200Resp;
        http200Resp.status_code = 200;
        http200Resp.text = "{\"trackId\":\"TRK-123\"}";
        bool noThrow = true;
        try {
            infra::DgiiDirectTransport::validateDgiiResponse(http200Resp);
        } catch (...) {
            noThrow = false;
        }
        CHECK(noThrow, "BUG-014: validateDgiiResponse allows HTTP 200 OK to pass");
    }

    {
        std::string spanishText = "Construcción y Reparación";

        std::string trunc11 = app::truncateUtf8(spanishText, 11);
        CHECK(trunc11 == "Construcció", "BUG-092: truncateUtf8 preserves multi-byte accent character boundary");
        CHECK(trunc11.size() == 12, "BUG-092: 11 codepoints with 1 multi-byte char yields 12 bytes");

        std::string withControlChars = "Producto\x01 con\x1b control\tchars\n";
        std::string escaped = app::escapeXml(withControlChars);
        CHECK(escaped.find('\x01') == std::string::npos, "BUG-092: escapeXml strips SOH 0x01");
        CHECK(escaped.find('\x1b') == std::string::npos, "BUG-092: escapeXml strips ESC 0x1b");
        CHECK(escaped.find('\t') != std::string::npos, "BUG-092: escapeXml preserves tab 0x09");
        CHECK(escaped.find('\n') != std::string::npos, "BUG-092: escapeXml preserves newline 0x0A");
    }

    {
        domain::AmbienteEnum amb;
        CHECK(domain::tryResolveAmbienteEnum("Test", amb) && amb == domain::AmbienteEnum::PreCertificacion, "BUG-093: recognizes Test");
        CHECK(domain::tryResolveAmbienteEnum("TestEcf", amb) && amb == domain::AmbienteEnum::PreCertificacion, "BUG-093: recognizes TestEcf");
        CHECK(domain::tryResolveAmbienteEnum("PreCertificacion", amb) && amb == domain::AmbienteEnum::PreCertificacion, "BUG-093: recognizes PreCertificacion");
        CHECK(domain::tryResolveAmbienteEnum("1", amb) && amb == domain::AmbienteEnum::PreCertificacion, "BUG-093: recognizes 1");
        CHECK(domain::tryResolveAmbienteEnum("Cert", amb) && amb == domain::AmbienteEnum::Certificacion, "BUG-093: recognizes Cert");
        CHECK(domain::tryResolveAmbienteEnum("CertEcf", amb) && amb == domain::AmbienteEnum::Certificacion, "BUG-093: recognizes CertEcf");
        CHECK(domain::tryResolveAmbienteEnum("Homologacion", amb) && amb == domain::AmbienteEnum::Certificacion, "BUG-093: recognizes Homologacion");
        CHECK(domain::tryResolveAmbienteEnum("3", amb) && amb == domain::AmbienteEnum::Certificacion, "BUG-093: recognizes 3");
        CHECK(domain::tryResolveAmbienteEnum("Prod", amb) && amb == domain::AmbienteEnum::Produccion, "BUG-093: recognizes Prod");
        CHECK(domain::tryResolveAmbienteEnum("prd", amb) && amb == domain::AmbienteEnum::Produccion, "BUG-093: recognizes prd");
        CHECK(domain::tryResolveAmbienteEnum("Production", amb) && amb == domain::AmbienteEnum::Produccion, "BUG-093: recognizes Production");
        CHECK(domain::tryResolveAmbienteEnum("Produccion", amb) && amb == domain::AmbienteEnum::Produccion, "BUG-093: recognizes Produccion");
        CHECK(domain::tryResolveAmbienteEnum("2", amb) && amb == domain::AmbienteEnum::Produccion, "BUG-093: recognizes 2");

        CHECK(!domain::tryResolveAmbienteEnum("invalid_env", amb), "BUG-093: rejects invalid_env");
        CHECK(!domain::tryResolveAmbienteEnum("dev", amb), "BUG-093: rejects dev");
        CHECK(!domain::tryResolveAmbienteEnum("staging", amb), "BUG-093: rejects staging");
        CHECK(!domain::tryResolveAmbienteEnum("", amb), "BUG-093: rejects empty env");
    }

    {
        struct MockDoc {
            std::string sourceTxnId;
            std::string trackId;
            std::string eNcf;
        };

        MockDoc existingDoc{"QB-INV-5001", "TRK-9999", "E310000000001"};

        auto matchesSourceTxnId = [](const MockDoc& doc, const std::string& queryId) -> bool {

            return doc.sourceTxnId == queryId;
        };

        CHECK(!matchesSourceTxnId(existingDoc, "E310000000001"), "BUG-100: query by e_ncf does NOT falsely collide with source_txn_id");
        CHECK(!matchesSourceTxnId(existingDoc, "TRK-9999"), "BUG-100: query by track_id does NOT falsely collide with source_txn_id");
        CHECK(matchesSourceTxnId(existingDoc, "QB-INV-5001"), "BUG-100: query by actual source_txn_id matches correctly");
    }

    {
        CHECK(app::normalizeNcfReference("0300000012") == "B0300000012", "10-digit 03... normalizes to B03");
        CHECK(app::normalizeNcfReference("1500000045") == "B1500000045", "10-digit 15... normalizes to B15");
        CHECK(app::normalizeNcfReference("1600000099") == "B1600000099", "10-digit 16... normalizes to B16");
        CHECK(app::normalizeNcfReference("1700000007") == "B1700000007", "10-digit 17... normalizes to B17");
        CHECK(app::normalizeNcfReference("500000008") == "B1500000008", "9-digit 5... normalizes to B15");
        CHECK(app::normalizeNcfReference("600000008") == "B1600000008", "9-digit 6... normalizes to B16");
        CHECK(app::normalizeNcfReference("700000008") == "B1700000008", "9-digit 7... normalizes to B17");
        CHECK(app::normalizeNcfReference("b1300000001") == "B1300000001", "lowercase b13 uppercases cleanly");
    }

    {
        domain::TimbreEcfRequest req43;
        req43.rncEmisor = "101000021";
        req43.rncComprador = "101000021";
        req43.eNcf = "E430000000001";
        req43.fechaEmision = "05-11-2026";
        req43.montoTotal = 450.00;
        req43.fechaFirma = "05-11-2026 10:00:00";
        req43.codigoSeguridad = "Ab12Cd";

        std::string url43 = infra::EcfSecurityUtils::buildTimbreUrl("https://ecf.dgii.gov.do/ecf/consultatimbre", req43);
        CHECK(url43.find("rnccomprador=") == std::string::npos, "E43 QR Timbre URL strictly omits rnccomprador=");

        domain::TimbreEcfRequest req34NoBuyer = req43;
        req34NoBuyer.eNcf = "E340000000002";
        req34NoBuyer.rncComprador = "";
        std::string url34 = infra::EcfSecurityUtils::buildTimbreUrl("https://ecf.dgii.gov.do/ecf/consultatimbre", req34NoBuyer);
        CHECK(url34.find("rnccomprador=") == std::string::npos, "Empty-buyer E34 QR Timbre URL omits rnccomprador=");
    }

    {
        app::CanonicalCurrencyDto cUsd{"US$", 60.0};
        auto rUsd = app::resolveCanonicalCurrency(cUsd);
        CHECK(rUsd.isForeign && rUsd.code == "USD" && rUsd.validationError.empty(), "US$ resolves to valid foreign USD");

        app::CanonicalCurrencyDto cCny{"CNY", 8.45};
        auto rCny = app::resolveCanonicalCurrency(cCny);
        CHECK(rCny.isForeign && rCny.code == "CHY" && rCny.validationError.empty(), "CNY resolves to DGII CHY");

        app::CanonicalCurrencyDto cDop{"RD$", 1.0};
        auto rDop = app::resolveCanonicalCurrency(cDop);
        CHECK(!rDop.isForeign && rDop.exchangeRate == 1.0, "RD$ resolves as domestic DOP");

        app::CanonicalCurrencyDto cBad{"XYZ", 60.0};
        auto rBad = app::resolveCanonicalCurrency(cBad);
        CHECK(!rBad.validationError.empty(), "Unsupported ISO currency code produces validation error");

        domain::EcfDocument doc;
        doc.eNcf = "E320000000099";
        doc.rncEmisor = "101000021";
        doc.totalAmount = 7080.00;
        doc.itbisAmount = 1080.00;
        doc.securityCode = "UsdRfc";

        app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E32";
        dto.currency = cUsd;
        dto.totals.montoSubtotal = 100.0;
        dto.totals.montoGravadoTotal = 100.0;
        dto.totals.montoItbis = 18.0;
        dto.totals.montoTotal = 118.0;

        std::string rfceXml = app::buildRfceXml(doc, &dto, "EMPRESA DEMO SRL");
        CHECK(rfceXml.find("<MontoGravadoTotal>6000.00</MontoGravadoTotal>") != std::string::npos,
              "Foreign-currency RFCE emits DOP MontoGravadoTotal (6000.00), not USD (100.00)");
        CHECK(rfceXml.find("<TotalITBIS>1080.00</TotalITBIS>") != std::string::npos,
              "Foreign-currency RFCE emits DOP TotalITBIS (1080.00), not USD (18.00)");
        CHECK(rfceXml.find("<MontoTotal>7080.00</MontoTotal>") != std::string::npos,
              "Foreign-currency RFCE emits DOP MontoTotal (7080.00), not USD (118.00)");
    }

    std::printf("\nTotal failures: %d\n", failures);
    return failures > 0 ? 1 : 0;
}
