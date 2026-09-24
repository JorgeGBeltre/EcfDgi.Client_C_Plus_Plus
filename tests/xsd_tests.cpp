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
#include "Application/Ecf/CanonicalDocumentDto.h"
#include "Application/Ecf/CanonicalXmlBuilder.h"

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

        // Test 8: Security Code extraction (first 6 chars of Base64 SignatureValue per DGII standard)
        std::string sigVal = signer.extractSignatureValue(signedXml);
        std::string secCode = ecf::infra::EcfSecurityUtils::calcularCodigoSeguridad(signedXml);
        CHECK(secCode.length() == 6, "Security code length is 6 characters");
        CHECK(secCode == sigVal.substr(0, 6), "Security code matches exactly first 6 chars of SignatureValue");
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

    std::string xsdDir = resolveXsdDir();
    ecf::infra::EcfSchemaValidator schemaVal(xsdDir);
    ecf::infra::EcfXmlSigner signer;

    auto signAndCheck = [&](const std::string& xml, const std::string& rnc, const std::string& xsdName, const char* desc) {
        std::string signedXml = signer.signXml(xml, rnc);
        auto res = schemaVal.validate(signedXml, xsdDir + "/" + xsdName);
        if (!res.isValid) {
            std::printf("  Validation failed for %s (%s):\n", xsdName.c_str(), desc);
            for (const auto& err : res.errors) {
                std::printf("    %s\n", err.c_str());
            }
        }
        CHECK(res.isValid, desc);
        return res.isValid;
    };

    // Test 13: Tipo 31 (Factura de Crédito Fiscal) - Normalization of ISO Date & XSD validation
    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E31";
        dto.header.rncEmisor = "101889063";
        dto.header.razonSocialEmisor = "Willy Chic";
        dto.header.rncComprador = "130000000";
        dto.header.razonSocialComprador = "Cliente de Prueba";
        dto.header.fechaEmision = "2026-08-06";
        dto.totals.montoSubtotal = 100.0;
        dto.totals.montoItbis = 18.0;
        dto.totals.montoTotal = 118.0;
        ecf::app::CanonicalLineDto line;
        line.lineNumber = 1;
        line.itemName = "Articulo Gravado 18%";
        line.quantity = 1.0;
        line.unitPrice = 100.0;
        line.amount = 100.0;
        dto.lines.push_back(line);

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E310000000801", "101889063", "Willy Chic");
        CHECK(xml.find("<FechaEmision>06-08-2026</FechaEmision>") != std::string::npos, "Tipo 31 ISO date converted to dd-MM-yyyy");
        signAndCheck(xml, "101889063", "e-CF 31 v.1.0.xsd", "Tipo 31 generated XML is valid against e-CF 31 XSD");
    }

    // Test 14: Tipo 31 with Exempt and Taxed lines
    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E31";
        dto.header.rncEmisor = "101889063";
        dto.header.razonSocialEmisor = "Willy Chic";
        dto.header.rncComprador = "130000000";
        dto.header.razonSocialComprador = "Cliente de Prueba";
        dto.totals.montoSubtotal = 100.0;
        dto.totals.montoGravadoTotal = 50.0;
        dto.totals.montoExento = 50.0;
        dto.totals.montoItbis = 9.0;
        dto.totals.montoTotal = 109.0;
        ecf::app::CanonicalTaxBucketDto b1;
        b1.rate = 18;
        b1.base = 50.0;
        b1.tax = 9.0;
        dto.totals.taxBuckets.push_back(b1);

        ecf::app::CanonicalLineDto l1;
        l1.lineNumber = 1;
        l1.itemName = "Item Gravado";
        l1.quantity = 1.0;
        l1.unitPrice = 50.0;
        l1.amount = 50.0;
        dto.lines.push_back(l1);

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E310000000802", "101889063", "Willy Chic");
        CHECK(xml.find("<MontoExento>50.00</MontoExento>") != std::string::npos, "Tipo 31 includes MontoExento");
        signAndCheck(xml, "101889063", "e-CF 31 v.1.0.xsd", "Tipo 31 with exempt lines is valid against e-CF 31 XSD");
    }

    // Test 15: Tipo 32 (Factura de Consumo) - Buyer present
    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E32";
        dto.header.rncEmisor = "101889063";
        dto.header.razonSocialEmisor = "Willy Chic";
        dto.header.rncComprador = "101889063";
        dto.header.razonSocialComprador = "Juan Pérez Consumidor";
        dto.totals.montoSubtotal = 500.0;
        dto.totals.montoGravadoTotal = 500.0;
        dto.totals.montoItbis = 90.0;
        dto.totals.montoTotal = 590.0;
        ecf::app::CanonicalTaxBucketDto b1;
        b1.rate = 18;
        b1.base = 500.0;
        b1.tax = 90.0;
        dto.totals.taxBuckets.push_back(b1);

        ecf::app::CanonicalLineDto l1;
        l1.lineNumber = 1;
        l1.itemName = "Producto Consumo";
        l1.quantity = 1.0;
        l1.unitPrice = 500.0;
        l1.amount = 500.0;
        dto.lines.push_back(l1);

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E320000000101", "101889063", "Willy Chic");
        CHECK(xml.find("<TipoeCF>32</TipoeCF>") != std::string::npos, "Tipo 32 XML has TipoeCF 32");
        CHECK(xml.find("<FechaVencimientoSecuencia>") == std::string::npos, "Tipo 32 omits FechaVencimientoSecuencia");
        signAndCheck(xml, "101889063", "e-CF 32 v.1.0.xsd", "Tipo 32 is valid against e-CF 32 XSD");
    }

    // Test 16: Tipo 32 with No Buyer Data At All
    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E32";
        dto.header.rncEmisor = "101889063";
        dto.header.razonSocialEmisor = "Willy Chic";
        dto.totals.montoSubtotal = 100.0;
        dto.totals.montoGravadoTotal = 100.0;
        dto.totals.montoItbis = 18.0;
        dto.totals.montoTotal = 118.0;
        ecf::app::CanonicalLineDto l1;
        l1.lineNumber = 1;
        l1.itemName = "Item Sin Comprador";
        l1.quantity = 1.0;
        l1.unitPrice = 100.0;
        l1.amount = 100.0;
        dto.lines.push_back(l1);

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E320000000102", "101889063", "Willy Chic");
        CHECK(xml.find("<RazonSocialComprador>Consumidor Final</RazonSocialComprador>") != std::string::npos, "Tipo 32 without buyer sets Consumidor Final");
        signAndCheck(xml, "101889063", "e-CF 32 v.1.0.xsd", "Tipo 32 with no buyer is valid against e-CF 32 XSD");
    }

    // Test 17: Tipo 33 (Nota de Débito)
    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E33";
        dto.header.rncEmisor = "101889063";
        dto.header.razonSocialEmisor = "Willy Chic";
        dto.header.rncComprador = "130000000";
        dto.header.razonSocialComprador = "Cliente de Prueba";
        dto.totals.montoSubtotal = 100.0;
        dto.totals.montoItbis = 18.0;
        dto.totals.montoTotal = 118.0;
        dto.references.correctsENcf = "E310000000801";
        dto.references.codigoModificacion = 1;
        dto.references.razonModificacion = "Cargo adicional por mora";
        dto.references.fechaNcfModificado = "2026-08-06";
        ecf::app::CanonicalLineDto l1;
        l1.lineNumber = 1;
        l1.itemName = "Cargo por Mora";
        l1.quantity = 1.0;
        l1.unitPrice = 100.0;
        l1.amount = 100.0;
        dto.lines.push_back(l1);

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E330000000801", "101889063", "Willy Chic");
        CHECK(xml.find("<TipoeCF>33</TipoeCF>") != std::string::npos, "Tipo 33 has TipoeCF 33");
        CHECK(xml.find("<NCFModificado>E310000000801</NCFModificado>") != std::string::npos, "Tipo 33 has NCFModificado");
        signAndCheck(xml, "101889063", "e-CF 33 v.1.0.xsd", "Tipo 33 is valid against e-CF 33 XSD");
    }

    // Test 18: Tipo 34 (Nota de Crédito)
    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E34";
        dto.header.rncEmisor = "101889063";
        dto.header.razonSocialEmisor = "Willy Chic";
        dto.header.rncComprador = "130000000";
        dto.header.razonSocialComprador = "Cliente de Prueba";
        dto.totals.montoSubtotal = 100.0;
        dto.totals.montoItbis = 18.0;
        dto.totals.montoTotal = 118.0;
        dto.references.correctsENcf = "E310000000801";
        dto.references.codigoModificacion = 1;
        dto.references.razonModificacion = "Descuento concedido";
        dto.references.fechaNcfModificado = "2026-08-06";
        ecf::app::CanonicalLineDto l1;
        l1.lineNumber = 1;
        l1.itemName = "Descuento en Factura";
        l1.quantity = 1.0;
        l1.unitPrice = 100.0;
        l1.amount = 100.0;
        dto.lines.push_back(l1);

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E340000000801", "101889063", "Willy Chic");
        CHECK(xml.find("<IndicadorNotaCredito>0</IndicadorNotaCredito>") != std::string::npos, "Tipo 34 has IndicadorNotaCredito");
        CHECK(xml.find("<NCFModificado>E310000000801</NCFModificado>") != std::string::npos, "Tipo 34 has NCFModificado");
        signAndCheck(xml, "101889063", "e-CF 34 v.1.0.xsd", "Tipo 34 is valid against e-CF 34 XSD");
    }

    // Test 19: Tipo 41 (Compras con Retención)
    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E41";
        dto.header.rncEmisor = "101889063";
        dto.header.razonSocialEmisor = "Willy Chic";
        dto.header.rncComprador = "130000000";
        dto.header.razonSocialComprador = "Proveedor Informal";
        dto.totals.montoSubtotal = 1000.0;
        dto.totals.montoItbis = 180.0;
        dto.totals.montoTotal = 1180.0;
        ecf::app::CanonicalRetentionDto ret;
        ret.indicadorAgenteRetencionoPercepcion = 1;
        ret.montoItbisRetenido = 180.0;
        ret.montoIsrRetenido = 100.0;
        dto.retention = ret;

        ecf::app::CanonicalLineDto l1;
        l1.lineNumber = 1;
        l1.itemName = "Servicio Profesional Informal";
        l1.quantity = 1.0;
        l1.unitPrice = 1000.0;
        l1.amount = 1000.0;
        dto.lines.push_back(l1);

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E410000000801", "101889063", "Willy Chic");
        CHECK(xml.find("<TotalITBISRetenido>180.00</TotalITBISRetenido>") != std::string::npos, "Tipo 41 has TotalITBISRetenido");
        CHECK(xml.find("<TotalISRRetencion>100.00</TotalISRRetencion>") != std::string::npos, "Tipo 41 has TotalISRRetencion");
        signAndCheck(xml, "101889063", "e-CF 41 v.1.0.xsd", "Tipo 41 is valid against e-CF 41 XSD");
    }

    // Test 20: Tipo 43 (Gastos Menores)
    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E43";
        dto.header.rncEmisor = "101889063";
        dto.header.razonSocialEmisor = "Willy Chic";
        dto.totals.montoSubtotal = 500.0;
        dto.totals.montoItbis = 0.0;
        dto.totals.montoTotal = 500.0;
        ecf::app::CanonicalLineDto l1;
        l1.lineNumber = 1;
        l1.itemName = "Café y azúcar para oficina";
        l1.quantity = 1.0;
        l1.unitPrice = 500.0;
        l1.amount = 500.0;
        dto.lines.push_back(l1);

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E430000000801", "101889063", "Willy Chic");
        CHECK(xml.find("<Comprador>") == std::string::npos, "Tipo 43 does not contain Comprador");
        CHECK(xml.find("<TipoIngresos>") == std::string::npos, "Tipo 43 does not contain TipoIngresos");
        CHECK(xml.find("<MontoExento>500.00</MontoExento>") != std::string::npos, "Tipo 43 has MontoExento");
        signAndCheck(xml, "101889063", "e-CF 43 v.1.0.xsd", "Tipo 43 is valid against e-CF 43 XSD");
    }

    // Test 21: Tipo 44 (Regímenes Especiales)
    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E44";
        dto.header.rncEmisor = "101889063";
        dto.header.razonSocialEmisor = "Willy Chic";
        dto.header.rncComprador = "130000000";
        dto.header.razonSocialComprador = "Zona Franca del Norte SAS";
        dto.totals.montoSubtotal = 1000.0;
        dto.totals.montoItbis = 0.0;
        dto.totals.montoTotal = 1000.0;
        ecf::app::CanonicalLineDto l1;
        l1.lineNumber = 1;
        l1.itemName = "Prendas de vestir zona franca";
        l1.quantity = 10.0;
        l1.unitPrice = 100.0;
        l1.amount = 1000.0;
        dto.lines.push_back(l1);

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E440000000801", "101889063", "Willy Chic");
        CHECK(xml.find("<Comprador>") != std::string::npos, "Tipo 44 contains Comprador");
        CHECK(xml.find("<MontoExento>1000.00</MontoExento>") != std::string::npos, "Tipo 44 has MontoExento");
        CHECK(xml.find("<TotalITBIS>") == std::string::npos, "Tipo 44 does not contain TotalITBIS");
        signAndCheck(xml, "101889063", "e-CF 44 v.1.0.xsd", "Tipo 44 is valid against e-CF 44 XSD");
    }

    // Test 22: Tipo 45 (Gubernamental)
    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E45";
        dto.header.rncEmisor = "101889063";
        dto.header.razonSocialEmisor = "Willy Chic";
        dto.header.rncComprador = "401007421";
        dto.header.razonSocialComprador = "Ministerio de Hacienda";
        dto.totals.montoSubtotal = 1000.0;
        dto.totals.montoItbis = 180.0;
        dto.totals.montoTotal = 1180.0;
        ecf::app::CanonicalLineDto l1;
        l1.lineNumber = 1;
        l1.itemName = "Servicio Institucional";
        l1.quantity = 1.0;
        l1.unitPrice = 1000.0;
        l1.amount = 1000.0;
        dto.lines.push_back(l1);

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E450000000801", "101889063", "Willy Chic");
        CHECK(xml.find("<RNCComprador>401007421</RNCComprador>") != std::string::npos, "Tipo 45 has RNCComprador");
        CHECK(xml.find("<MontoGravadoTotal>1000.00</MontoGravadoTotal>") != std::string::npos, "Tipo 45 has MontoGravadoTotal");
        CHECK(xml.find("<TotalITBIS>180.00</TotalITBIS>") != std::string::npos, "Tipo 45 has TotalITBIS");
        signAndCheck(xml, "101889063", "e-CF 45 v.1.0.xsd", "Tipo 45 is valid against e-CF 45 XSD");
    }

    // Test 23: Tipo 46 (Exportaciones)
    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E46";
        dto.header.rncEmisor = "101889063";
        dto.header.razonSocialEmisor = "Willy Chic";
        dto.header.rncComprador = "US-987654321";
        dto.header.razonSocialComprador = "Miami Apparel Imports LLC";
        dto.totals.montoSubtotal = 2500.0;
        dto.totals.montoItbis = 0.0;
        dto.totals.montoTotal = 2500.0;
        ecf::app::CanonicalLineDto l1;
        l1.lineNumber = 1;
        l1.itemName = "Exportación Lote Textiles";
        l1.quantity = 25.0;
        l1.unitPrice = 100.0;
        l1.amount = 2500.0;
        dto.lines.push_back(l1);

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E460000000801", "101889063", "Willy Chic");
        CHECK(xml.find("<IdentificadorExtranjero>US-987654321</IdentificadorExtranjero>") != std::string::npos, "Tipo 46 has IdentificadorExtranjero");
        CHECK(xml.find("<MontoGravadoI3>2500.00</MontoGravadoI3>") != std::string::npos, "Tipo 46 has MontoGravadoI3");
        CHECK(xml.find("<ITBIS3>0</ITBIS3>") != std::string::npos, "Tipo 46 has ITBIS3 0");
        signAndCheck(xml, "101889063", "e-CF 46 v.1.0.xsd", "Tipo 46 is valid against e-CF 46 XSD");
    }

    // Test 24: Tipo 47 (Pagos al Exterior)
    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E47";
        dto.header.rncEmisor = "101889063";
        dto.header.razonSocialEmisor = "Willy Chic";
        dto.header.rncComprador = "DE-123456789";
        dto.header.razonSocialComprador = "Software Cloud GmbH";
        dto.totals.montoSubtotal = 3000.0;
        dto.totals.montoItbis = 0.0;
        dto.totals.montoTotal = 3000.0;
        ecf::app::CanonicalRetentionDto ret;
        ret.indicadorAgenteRetencionoPercepcion = 1;
        ret.montoIsrRetenido = 300.0;
        dto.retention = ret;

        ecf::app::CanonicalLineDto l1;
        l1.lineNumber = 1;
        l1.itemName = "Licencia de Software Extranjero";
        l1.quantity = 1.0;
        l1.unitPrice = 3000.0;
        l1.amount = 3000.0;
        dto.lines.push_back(l1);

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E470000000801", "101889063", "Willy Chic");
        CHECK(xml.find("<IdentificadorExtranjero>DE-123456789</IdentificadorExtranjero>") != std::string::npos, "Tipo 47 has IdentificadorExtranjero");
        CHECK(xml.find("<TotalISRRetencion>300.00</TotalISRRetencion>") != std::string::npos, "Tipo 47 has TotalISRRetencion");
        CHECK(xml.find("<MontoISRRetenido>300.00</MontoISRRetenido>") != std::string::npos, "Tipo 47 has MontoISRRetenido");
        CHECK(xml.find("<TotalITBIS>") == std::string::npos, "Tipo 47 does not have TotalITBIS");
        signAndCheck(xml, "101889063", "e-CF 47 v.1.0.xsd", "Tipo 47 is valid against e-CF 47 XSD");
    }

    // Test 25: ARECF (Acuse de Recibo) Schema Validation
    {
        std::string unsignedArecf =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<ARECF>\n"
            "  <DetalleAcusedeRecibo>\n"
            "    <Version>1.0</Version>\n"
            "    <RNCEmisor>101889063</RNCEmisor>\n"
            "    <RNCComprador>130000000</RNCComprador>\n"
            "    <eNCF>E310000000801</eNCF>\n"
            "    <Estado>0</Estado>\n"
            "    <FechaHoraAcuseRecibo>10-09-2026 21:00:00</FechaHoraAcuseRecibo>\n"
            "  </DetalleAcusedeRecibo>\n"
            "</ARECF>";

        signAndCheck(unsignedArecf, "130000000", "ARECF v1.0.xsd", "ARECF generated XML is valid against ARECF XSD");
    }

    // Test 26: Multi-Tenant RNC and Razon Social Isolation
    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E31";
        dto.header.rncEmisor = "131888999";
        dto.header.razonSocialEmisor = "Tenant B Corp";
        dto.header.rncComprador = "101889063";
        dto.header.razonSocialComprador = "Cliente Tenant";
        dto.totals.montoSubtotal = 200.0;
        dto.totals.montoItbis = 36.0;
        dto.totals.montoTotal = 236.0;

        ecf::app::CanonicalLineDto l1;
        l1.lineNumber = 1;
        l1.itemName = "Servicio Tenant";
        l1.quantity = 1.0;
        l1.unitPrice = 200.0;
        l1.amount = 200.0;
        dto.lines.push_back(l1);

        // Effective RNC and RazonSocial must match the tenant, NOT the default host
        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E310000000999", "131888999", "Tenant B Corp");
        CHECK(xml.find("<RNCEmisor>131888999</RNCEmisor>") != std::string::npos, "Multi-tenant XML has Tenant RNCEmisor");
        CHECK(xml.find("<RazonSocialEmisor>Tenant B Corp</RazonSocialEmisor>") != std::string::npos, "Multi-tenant XML has Tenant RazonSocialEmisor");
        CHECK(xml.find("Willy Chic") == std::string::npos, "Multi-tenant XML does not leak default host name");
        signAndCheck(xml, "131888999", "e-CF 31 v.1.0.xsd", "Multi-tenant isolated XML is valid against e-CF 31 XSD");
    }

    // Test 27: Tipo 46 without buyer RNC passes XSD validation with EXTRANJERO fallback
    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E46";
        dto.header.rncEmisor = "101889063";
        dto.header.razonSocialEmisor = "Willy Chic";
        dto.header.rncComprador = ""; // No Dominican RNC
        dto.header.razonSocialComprador = "Foreign Client Corp";
        dto.totals.montoSubtotal = 500.0;
        dto.totals.montoItbis = 0.0;
        dto.totals.montoTotal = 500.0;
        ecf::app::CanonicalLineDto l1;
        l1.lineNumber = 1;
        l1.itemName = "Export Item";
        l1.quantity = 1.0;
        l1.unitPrice = 500.0;
        l1.amount = 500.0;
        dto.lines.push_back(l1);

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E460000000028", "101889063", "Willy Chic");
        CHECK(xml.find("<IdentificadorExtranjero>EXTRANJERO</IdentificadorExtranjero>") != std::string::npos,
              "Tipo 46 without buyer RNC generates IdentificadorExtranjero EXTRANJERO");
        CHECK(xml.find("<RNCComprador>") == std::string::npos,
              "Tipo 46 without buyer RNC does not generate RNCComprador");
        signAndCheck(xml, "101889063", "e-CF 46 v.1.0.xsd", "Tipo 46 with EXTRANJERO fallback passes XSD");
    }

    if (failures == 0) {
        std::printf("All XSD, multi-tenant, and security tests passed successfully!\n");
    } else {
        std::printf("%d test(s) failed.\n", failures);
    }

    return failures ? 1 : 0;
}
