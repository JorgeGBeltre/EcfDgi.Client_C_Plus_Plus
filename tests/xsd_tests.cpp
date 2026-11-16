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
#include "Domain/Exceptions/EcfException.h"

static std::string resolveXsdDir() {
    std::vector<std::string> candidates = {
        "Documentación Técnica (XSD)",
        "../Documentación Técnica (XSD)",
        "../../Documentación Técnica (XSD)"
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

    {
        std::string xml = "<ECF><Encabezado><IdDoc><TipoeCF>31</TipoeCF></IdDoc></Encabezado></ECF>";
        std::string res = ecf::infra::EcfXsdFileNameResolver::resolve(xml);
        CHECK(res == "e-CF 31 v.1.0.xsd", "Resolve e-CF 31");
    }

    {
        std::string xml = "<ECF><Encabezado><IdDoc><TipoeCF>32</TipoeCF></IdDoc></Encabezado></ECF>";
        std::string res = ecf::infra::EcfXsdFileNameResolver::resolve(xml);
        CHECK(res == "e-CF 32 v.1.0.xsd", "Resolve e-CF 32");
    }

    {
        std::string xml = "<ECF><Encabezado><IdDoc><TipoeCF>34</TipoeCF></IdDoc></Encabezado></ECF>";
        std::string res = ecf::infra::EcfXsdFileNameResolver::resolve(xml);
        CHECK(res == "e-CF 34 v.1.0.xsd", "Resolve e-CF 34");
    }

    {
        std::string xml = "<ECF><Encabezado><IdDoc><TipoeCF>41</TipoeCF></IdDoc></Encabezado></ECF>";
        std::string res = ecf::infra::EcfXsdFileNameResolver::resolve(xml);
        CHECK(res == "e-CF 41 v.1.0.xsd", "Resolve e-CF 41");
    }

    {
        std::string xml = "<ECF><Encabezado><IdDoc><TipoeCF>47</TipoeCF></IdDoc></Encabezado></ECF>";
        std::string res = ecf::infra::EcfXsdFileNameResolver::resolve(xml);
        CHECK(res == "e-CF 47 v.1.0.xsd", "Resolve e-CF 47");
    }

    {
        std::string xml = "<ARECF><DetalleAcusedeRecibo></DetalleAcusedeRecibo></ARECF>";
        std::string res = ecf::infra::EcfXsdFileNameResolver::resolve(xml);
        CHECK(res == "ARECF v1.0.xsd", "Resolve ARECF");
    }

    {
        ecf::infra::EcfXmlSigner signer;
        CHECK(signer.usesFallbackCertificate(), "Fallback signer indicates usesFallbackCertificate is true");

        std::string sampleXml = "<?xml version=\"1.0\" encoding=\"utf-8\"?><ECF><Encabezado><IdDoc><TipoeCF>31</TipoeCF><eNCF>E310000000001</eNCF></IdDoc></Encabezado></ECF>";
        std::string signedXml = signer.signXml(sampleXml, "101000021");
        CHECK(signedXml.find("Signature") != std::string::npos, "Signed XML contains Signature");
        CHECK(signedXml.find("SignatureValue") != std::string::npos, "Signed XML contains SignatureValue");

        std::string sigVal = signer.extractSignatureValue(signedXml);
        std::string secCode = ecf::infra::EcfSecurityUtils::calcularCodigoSeguridad(signedXml);
        CHECK(secCode.length() == 6, "Security code length is 6 characters");
        CHECK(secCode == sigVal.substr(0, 6), "Security code matches exactly first 6 chars of SignatureValue");
    }

    {
        std::string xsdDir = resolveXsdDir();
        ecf::infra::EcfSchemaValidator validator(xsdDir);

        auto res = validator.validate("<InvalidDoc/>", xsdDir + "/e-CF 31 v.1.0.xsd");
        CHECK(!res.isValid, "Malformed document fails XSD validation");
        CHECK(!res.errors.empty(), "XSD validation failure provides error descriptions");
    }

    {
        ecf::infra::EcfSchemaValidator emptyValidator("");
        auto res1 = emptyValidator.validate("<ECF><Encabezado><IdDoc><TipoeCF>31</TipoeCF></IdDoc></Encabezado></ECF>");
        CHECK(!res1.isValid, "Schema validation without configured XSD directory must fail-closed");
        CHECK(!res1.errors.empty(), "Fail-closed validation provides error description");

        std::string xsdDir = resolveXsdDir();
        ecf::infra::EcfSchemaValidator validator(xsdDir);
        auto res2 = validator.validate("<UnknownRoot><Invalid/></UnknownRoot>");
        CHECK(!res2.isValid, "Unresolved XML schema file name must fail-closed");
        CHECK(!res2.errors.empty(), "Unresolved schema provides error description");
    }

    {
        ecf::domain::TimbreEcfRequest ecfReq;
        ecfReq.rncEmisor = "101000021";
        ecfReq.rncComprador = "101000001";
        ecfReq.eNcf = "E310000000001";
        ecfReq.fechaEmision = "10-10-2020";
        ecfReq.montoTotal = 100.50;
        ecfReq.fechaFirma = "10-10-2020 09:00:00";
        ecfReq.codigoSeguridad = "abcd12";

        ecf::domain::TimbreFcRequest fcReq;
        fcReq.rncEmisor = "101000021";
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

    {
        ecf::domain::Rfce rfce;
        rfce.encabezado.version = "1.0";
        rfce.encabezado.idDoc.tipoeCF = "32";
        rfce.encabezado.idDoc.eNcf = "E320000000001";
        rfce.encabezado.idDoc.tipoIngresos = 1;
        rfce.encabezado.idDoc.tipoPago = 1;
        rfce.encabezado.emisor.rncEmisor = "101000021";
        rfce.encabezado.emisor.razonSocialEmisor = "EMPRESA DEMO SRL";
        rfce.encabezado.emisor.fechaEmision = "10-10-2020";
        ecf::domain::RfceComprador comp;
        comp.rncComprador = "101000001";
        comp.razonSocialComprador = "Cliente Test";
        rfce.encabezado.comprador = comp;
        rfce.encabezado.totales.montoTotal = 100.50;
        rfce.encabezado.totales.totalITBIS = 18.00;
        rfce.encabezado.codigoSeguridadeCF = "ABCD12";

        ecf::infra::EcfXmlSerializer serializer;
        std::string xml = serializer.serialize(rfce);

        CHECK(xml.find("<Version>1.0</Version>") != std::string::npos, "RFCE XML has <Version>1.0</Version>");
        CHECK(xml.find("<eNCF>E320000000001</eNCF>") != std::string::npos, "RFCE XML has <eNCF>");
        CHECK(xml.find("<RNCEmisor>101000021</RNCEmisor>") != std::string::npos, "RFCE XML has <RNCEmisor>");
        CHECK(xml.find("<RNCComprador>101000001</RNCComprador>") != std::string::npos, "RFCE XML has <RNCComprador>");
        CHECK(xml.find("<CodigoSeguridadeCF>ABCD12</CodigoSeguridadeCF>") != std::string::npos, "RFCE XML has <CodigoSeguridadeCF>");

        auto idxSecCode = xml.find("<CodigoSeguridadeCF>");
        auto idxTotales = xml.find("</Totales>");
        auto idxEnc = xml.find("</Encabezado>");
        CHECK(idxTotales != std::string::npos && idxSecCode > idxTotales, "CodigoSeguridadeCF is placed after </Totales>");
        CHECK(idxEnc != std::string::npos && idxSecCode < idxEnc, "CodigoSeguridadeCF is placed before </Encabezado>");

        ecf::infra::EcfXmlSigner signer;
        std::string signedXml = signer.signXml(xml, "101000021");
        CHECK(signedXml.find("Signature") != std::string::npos, "Signed RFCE XML contains Signature");

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

    {
        ecf::infra::EcfXmlSigner signer;
        CHECK(signer.validateCertificateSn("101000021"), "Fallback cert passes self-signed validation");
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

    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E31";
        dto.header.rncEmisor = "101000001";
        dto.header.razonSocialEmisor = "Empresa Demo";
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

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E310000000801", "101000001", "Empresa Demo");
        CHECK(xml.find("<FechaEmision>06-08-2026</FechaEmision>") != std::string::npos, "Tipo 31 ISO date converted to dd-MM-yyyy");
        signAndCheck(xml, "101000001", "e-CF 31 v.1.0.xsd", "Tipo 31 generated XML is valid against e-CF 31 XSD");
    }

    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E31";
        dto.header.rncEmisor = "101000001";
        dto.header.razonSocialEmisor = "Empresa Demo";
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

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E310000000802", "101000001", "Empresa Demo");
        CHECK(xml.find("<MontoExento>50.00</MontoExento>") != std::string::npos, "Tipo 31 includes MontoExento");
        signAndCheck(xml, "101000001", "e-CF 31 v.1.0.xsd", "Tipo 31 with exempt lines is valid against e-CF 31 XSD");
    }

    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E32";
        dto.header.rncEmisor = "101000001";
        dto.header.razonSocialEmisor = "Empresa Demo";
        dto.header.rncComprador = "101000001";
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

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E320000000101", "101000001", "Empresa Demo");
        CHECK(xml.find("<TipoeCF>32</TipoeCF>") != std::string::npos, "Tipo 32 XML has TipoeCF 32");
        CHECK(xml.find("<FechaVencimientoSecuencia>") == std::string::npos, "Tipo 32 omits FechaVencimientoSecuencia");
        signAndCheck(xml, "101000001", "e-CF 32 v.1.0.xsd", "Tipo 32 is valid against e-CF 32 XSD");
    }

    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E32";
        dto.header.rncEmisor = "101000001";
        dto.header.razonSocialEmisor = "Empresa Demo";
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

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E320000000102", "101000001", "Empresa Demo");
        CHECK(xml.find("<RazonSocialComprador>Consumidor Final</RazonSocialComprador>") != std::string::npos, "Tipo 32 without buyer sets Consumidor Final");
        signAndCheck(xml, "101000001", "e-CF 32 v.1.0.xsd", "Tipo 32 with no buyer is valid against e-CF 32 XSD");
    }

    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E33";
        dto.header.rncEmisor = "101000001";
        dto.header.razonSocialEmisor = "Empresa Demo";
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

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E330000000801", "101000001", "Empresa Demo");
        CHECK(xml.find("<TipoeCF>33</TipoeCF>") != std::string::npos, "Tipo 33 has TipoeCF 33");
        CHECK(xml.find("<NCFModificado>E310000000801</NCFModificado>") != std::string::npos, "Tipo 33 has NCFModificado");
        signAndCheck(xml, "101000001", "e-CF 33 v.1.0.xsd", "Tipo 33 is valid against e-CF 33 XSD");
    }

    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E34";
        dto.header.rncEmisor = "101000001";
        dto.header.razonSocialEmisor = "Empresa Demo";
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

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E340000000801", "101000001", "Empresa Demo");
        CHECK(xml.find("<IndicadorNotaCredito>0</IndicadorNotaCredito>") != std::string::npos, "Tipo 34 has IndicadorNotaCredito");
        CHECK(xml.find("<NCFModificado>E310000000801</NCFModificado>") != std::string::npos, "Tipo 34 has NCFModificado");
        signAndCheck(xml, "101000001", "e-CF 34 v.1.0.xsd", "Tipo 34 is valid against e-CF 34 XSD");
    }

    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E41";
        dto.header.rncEmisor = "101000001";
        dto.header.razonSocialEmisor = "Empresa Demo";
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

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E410000000801", "101000001", "Empresa Demo");
        CHECK(xml.find("<TotalITBISRetenido>180.00</TotalITBISRetenido>") != std::string::npos, "Tipo 41 has TotalITBISRetenido");
        CHECK(xml.find("<TotalISRRetencion>100.00</TotalISRRetencion>") != std::string::npos, "Tipo 41 has TotalISRRetencion");
        signAndCheck(xml, "101000001", "e-CF 41 v.1.0.xsd", "Tipo 41 is valid against e-CF 41 XSD");
    }

    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E43";
        dto.header.rncEmisor = "101000001";
        dto.header.razonSocialEmisor = "Empresa Demo";
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

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E430000000801", "101000001", "Empresa Demo");
        CHECK(xml.find("<Comprador>") == std::string::npos, "Tipo 43 does not contain Comprador");
        CHECK(xml.find("<TipoIngresos>") == std::string::npos, "Tipo 43 does not contain TipoIngresos");
        CHECK(xml.find("<MontoExento>500.00</MontoExento>") != std::string::npos, "Tipo 43 has MontoExento");
        signAndCheck(xml, "101000001", "e-CF 43 v.1.0.xsd", "Tipo 43 is valid against e-CF 43 XSD");
    }

    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E44";
        dto.header.rncEmisor = "101000001";
        dto.header.razonSocialEmisor = "Empresa Demo";
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

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E440000000801", "101000001", "Empresa Demo");
        CHECK(xml.find("<Comprador>") != std::string::npos, "Tipo 44 contains Comprador");
        CHECK(xml.find("<MontoExento>1000.00</MontoExento>") != std::string::npos, "Tipo 44 has MontoExento");
        CHECK(xml.find("<TotalITBIS>") == std::string::npos, "Tipo 44 does not contain TotalITBIS");
        signAndCheck(xml, "101000001", "e-CF 44 v.1.0.xsd", "Tipo 44 is valid against e-CF 44 XSD");
    }

    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E45";
        dto.header.rncEmisor = "101000001";
        dto.header.razonSocialEmisor = "Empresa Demo";
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

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E450000000801", "101000001", "Empresa Demo");
        CHECK(xml.find("<RNCComprador>401007421</RNCComprador>") != std::string::npos, "Tipo 45 has RNCComprador");
        CHECK(xml.find("<MontoGravadoTotal>1000.00</MontoGravadoTotal>") != std::string::npos, "Tipo 45 has MontoGravadoTotal");
        CHECK(xml.find("<TotalITBIS>180.00</TotalITBIS>") != std::string::npos, "Tipo 45 has TotalITBIS");
        signAndCheck(xml, "101000001", "e-CF 45 v.1.0.xsd", "Tipo 45 is valid against e-CF 45 XSD");
    }

    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E46";
        dto.header.rncEmisor = "101000001";
        dto.header.razonSocialEmisor = "Empresa Demo";
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

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E460000000801", "101000001", "Empresa Demo");
        CHECK(xml.find("<IdentificadorExtranjero>US-987654321</IdentificadorExtranjero>") != std::string::npos, "Tipo 46 has IdentificadorExtranjero");
        CHECK(xml.find("<MontoGravadoI3>2500.00</MontoGravadoI3>") != std::string::npos, "Tipo 46 has MontoGravadoI3");
        CHECK(xml.find("<ITBIS3>0</ITBIS3>") != std::string::npos, "Tipo 46 has ITBIS3 0");
        signAndCheck(xml, "101000001", "e-CF 46 v.1.0.xsd", "Tipo 46 is valid against e-CF 46 XSD");
    }

    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E47";
        dto.header.rncEmisor = "101000001";
        dto.header.razonSocialEmisor = "Empresa Demo";
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

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E470000000801", "101000001", "Empresa Demo");
        CHECK(xml.find("<IdentificadorExtranjero>DE-123456789</IdentificadorExtranjero>") != std::string::npos, "Tipo 47 has IdentificadorExtranjero");
        CHECK(xml.find("<TotalISRRetencion>300.00</TotalISRRetencion>") != std::string::npos, "Tipo 47 has TotalISRRetencion");
        CHECK(xml.find("<MontoISRRetenido>300.00</MontoISRRetenido>") != std::string::npos, "Tipo 47 has MontoISRRetenido");
        CHECK(xml.find("<TotalITBIS>") == std::string::npos, "Tipo 47 does not have TotalITBIS");
        signAndCheck(xml, "101000001", "e-CF 47 v.1.0.xsd", "Tipo 47 is valid against e-CF 47 XSD");
    }

    {
        std::string unsignedArecf =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
            "<ARECF>\n"
            "  <DetalleAcusedeRecibo>\n"
            "    <Version>1.0</Version>\n"
            "    <RNCEmisor>101000001</RNCEmisor>\n"
            "    <RNCComprador>130000000</RNCComprador>\n"
            "    <eNCF>E310000000801</eNCF>\n"
            "    <Estado>0</Estado>\n"
            "    <FechaHoraAcuseRecibo>10-09-2026 21:00:00</FechaHoraAcuseRecibo>\n"
            "  </DetalleAcusedeRecibo>\n"
            "</ARECF>";

        signAndCheck(unsignedArecf, "130000000", "ARECF v1.0.xsd", "ARECF generated XML is valid against ARECF XSD");
    }

    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E31";
        dto.header.rncEmisor = "131888999";
        dto.header.razonSocialEmisor = "Tenant B Corp";
        dto.header.rncComprador = "101000001";
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

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E310000000999", "131888999", "Tenant B Corp");
        CHECK(xml.find("<RNCEmisor>131888999</RNCEmisor>") != std::string::npos, "Multi-tenant XML has Tenant RNCEmisor");
        CHECK(xml.find("<RazonSocialEmisor>Tenant B Corp</RazonSocialEmisor>") != std::string::npos, "Multi-tenant XML has Tenant RazonSocialEmisor");
        CHECK(xml.find("Empresa Demo") == std::string::npos, "Multi-tenant XML does not leak default host name");
        signAndCheck(xml, "131888999", "e-CF 31 v.1.0.xsd", "Multi-tenant isolated XML is valid against e-CF 31 XSD");
    }

    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E46";
        dto.header.rncEmisor = "101000001";
        dto.header.razonSocialEmisor = "Empresa Demo";
        dto.header.rncComprador = "";
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

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E460000000028", "101000001", "Empresa Demo");
        CHECK(xml.find("<IdentificadorExtranjero>EXTRANJERO</IdentificadorExtranjero>") != std::string::npos,
              "Tipo 46 without buyer RNC generates IdentificadorExtranjero EXTRANJERO");
        CHECK(xml.find("<RNCComprador>") == std::string::npos,
              "Tipo 46 without buyer RNC does not generate RNCComprador");
        signAndCheck(xml, "101000001", "e-CF 46 v.1.0.xsd", "Tipo 46 with EXTRANJERO fallback passes XSD");
    }

    {
        ecf::infra::EcfXmlSerializer serializer;
        bool threwRecepcion = false;
        try {

            std::string foreignXml = "<ProblemDetails><Status>502</Status><Title>Bad Gateway</Title></ProblemDetails>";
            serializer.deserializeEcfRecepcion(foreignXml);
        } catch (const ecf::domain::EcfException&) {
            threwRecepcion = true;
        }
        CHECK(threwRecepcion, "deserializeEcfRecepcion rejects well-formed XML lacking trackId and error");

        bool validRecepcion = false;
        try {
            std::string validXml = "<RespuestaRecepcion><trackId>test-track-1234</trackId></RespuestaRecepcion>";
            auto r = serializer.deserializeEcfRecepcion(validXml);
            validRecepcion = (r.trackId == "test-track-1234");
        } catch (...) {}
        CHECK(validRecepcion, "deserializeEcfRecepcion accepts XML with valid trackId");

        bool errorRecepcion = false;
        try {
            std::string errXml = "<RespuestaRecepcion><error>Certificado digital vencido</error></RespuestaRecepcion>";
            auto r = serializer.deserializeEcfRecepcion(errXml);
            errorRecepcion = (r.error.value_or("") == "Certificado digital vencido");
        } catch (...) {}
        CHECK(errorRecepcion, "deserializeEcfRecepcion accepts XML with error field");
    }

    {
        ecf::app::CanonicalCurrencyDto usd;
        usd.code = "US$";
        usd.exchangeRate = 60.50;

        {
            ecf::app::CanonicalDocumentDto dto;
            dto.tipoComprobante = "E31";
            dto.currency = usd;
            dto.header.rncEmisor = "101000001";
            dto.header.razonSocialEmisor = "Empresa Demo";
            dto.header.rncComprador = "130000000";
            dto.header.razonSocialComprador = "Cliente USD SRL";
            dto.totals.montoSubtotal = 100.0;
            dto.totals.montoGravadoTotal = 100.0;
            dto.totals.montoItbis = 18.0;
            dto.totals.montoTotal = 118.0;
            ecf::app::CanonicalLineDto l1;
            l1.lineNumber = 1;
            l1.itemName = "Consultoria Cloud USD";
            l1.quantity = 2.0;
            l1.unitPrice = 50.0;
            l1.amount = 100.0;
            dto.lines.push_back(l1);

            double totalDop = 0.0, itbisDop = 0.0;
            std::string xml = ecf::app::buildXmlFromCanonical(dto, "E310000000901", "101000001", "Empresa Demo", totalDop, itbisDop);
            CHECK(xml.find("<TipoMoneda>USD</TipoMoneda>") != std::string::npos, "E31 USD has TipoMoneda USD");
            CHECK(xml.find("<TipoCambio>60.50</TipoCambio>") != std::string::npos, "E31 USD has TipoCambio 60.50");
            CHECK(xml.find("<OtraMonedaDetalle>") != std::string::npos, "E31 USD has OtraMonedaDetalle");
            CHECK(xml.find("<PrecioOtraMoneda>50.00</PrecioOtraMoneda>") != std::string::npos, "E31 USD has PrecioOtraMoneda 50.00");
            CHECK(xml.find("<MontoItemOtraMoneda>100.00</MontoItemOtraMoneda>") != std::string::npos, "E31 USD has MontoItemOtraMoneda 100.00");
            CHECK(xml.find("<MontoTotalOtraMoneda>118.00</MontoTotalOtraMoneda>") != std::string::npos, "E31 USD has MontoTotalOtraMoneda 118.00");
            CHECK(totalDop == 7139.00 && itbisDop == 1089.00, "E31 USD computes exact DOP total (7139.00) and ITBIS (1089.00)");
            signAndCheck(xml, "101000001", "e-CF 31 v.1.0.xsd", "E31 USD passes DGII XSD validation");
        }

        {
            ecf::app::CanonicalDocumentDto dto32;
            dto32.tipoComprobante = "E32";
            dto32.currency = usd;
            dto32.totals.montoSubtotal = 50.0;
            dto32.totals.montoItbis = 9.0;
            dto32.totals.montoTotal = 59.0;
            ecf::app::CanonicalLineDto l;
            l.lineNumber = 1; l.itemName = "Prod USD"; l.quantity = 1.0; l.unitPrice = 50.0; l.amount = 50.0;
            dto32.lines.push_back(l);
            signAndCheck(ecf::app::buildXmlFromCanonical(dto32, "E320000000901", "101000001", "Empresa Demo"),
                         "101000001", "e-CF 32 v.1.0.xsd", "E32 USD passes DGII XSD validation");

            ecf::app::CanonicalDocumentDto dto34 = dto32;
            dto34.tipoComprobante = "E34";
            dto34.header.rncComprador = "130000000";
            dto34.header.razonSocialComprador = "Cliente USD";
            dto34.references.correctsENcf = "E310000000901";
            dto34.references.codigoModificacion = 1;
            dto34.references.fechaNcfModificado = "2026-08-06";
            signAndCheck(ecf::app::buildXmlFromCanonical(dto34, "E340000000901", "101000001", "Empresa Demo"),
                         "101000001", "e-CF 34 v.1.0.xsd", "E34 USD passes DGII XSD validation");

            ecf::app::CanonicalDocumentDto dto41 = dto32;
            dto41.tipoComprobante = "E41";
            dto41.header.rncComprador = "130000000";
            dto41.header.razonSocialComprador = "Proveedor USD";
            ecf::app::CanonicalRetentionDto ret41;
            ret41.indicadorAgenteRetencionoPercepcion = 1;
            ret41.montoItbisRetenido = 9.0;
            ret41.montoIsrRetenido = 5.0;
            dto41.retention = ret41;
            signAndCheck(ecf::app::buildXmlFromCanonical(dto41, "E410000000901", "101000001", "Empresa Demo"),
                         "101000001", "e-CF 41 v.1.0.xsd", "E41 USD passes DGII XSD validation");

            ecf::app::CanonicalDocumentDto dto43;
            dto43.tipoComprobante = "E43";
            dto43.currency = usd;
            dto43.totals.montoSubtotal = 20.0;
            dto43.totals.montoTotal = 20.0;
            dto43.lines.push_back(l);
            dto43.lines[0].unitPrice = 20.0;
            dto43.lines[0].amount = 20.0;
            signAndCheck(ecf::app::buildXmlFromCanonical(dto43, "E430000000901", "101000001", "Empresa Demo"),
                         "101000001", "e-CF 43 v.1.0.xsd", "E43 USD passes DGII XSD validation");

            ecf::app::CanonicalDocumentDto dto46;
            dto46.tipoComprobante = "E46";
            dto46.currency = usd;
            dto46.header.razonSocialComprador = "Foreign Buyer Inc";
            dto46.totals.montoSubtotal = 500.0;
            dto46.totals.montoTotal = 500.0;
            dto46.lines.push_back(l);
            dto46.lines[0].unitPrice = 500.0;
            dto46.lines[0].amount = 500.0;
            signAndCheck(ecf::app::buildXmlFromCanonical(dto46, "E460000000901", "101000001", "Empresa Demo"),
                         "101000001", "e-CF 46 v.1.0.xsd", "E46 USD passes DGII XSD validation");

            ecf::app::CanonicalDocumentDto dto47 = dto46;
            dto47.tipoComprobante = "E47";
            ecf::app::CanonicalRetentionDto ret47;
            ret47.indicadorAgenteRetencionoPercepcion = 1;
            ret47.montoIsrRetenido = 135.0;
            dto47.retention = ret47;
            signAndCheck(ecf::app::buildXmlFromCanonical(dto47, "E470000000901", "101000001", "Empresa Demo"),
                         "101000001", "e-CF 47 v.1.0.xsd", "E47 USD passes DGII XSD validation");
        }
    }

    {

        ecf::app::CanonicalDocumentDto dtoB13;
        dtoB13.tipoComprobante = "E34";
        dtoB13.totals.montoSubtotal = 300.0;
        dtoB13.totals.montoExento = 300.0;
        dtoB13.totals.montoItbis = 0.0;
        dtoB13.totals.montoTotal = 300.0;
        dtoB13.references.correctsENcf = "B1300000005";
        dtoB13.references.codigoModificacion = 1;
        dtoB13.references.fechaNcfModificado = "2026-08-01";
        ecf::app::CanonicalLineDto l;
        l.lineNumber = 1; l.itemName = "Anulacion gasto menor"; l.quantity = 1.0; l.unitPrice = 300.0; l.amount = 300.0;
        dtoB13.lines.push_back(l);

        std::string xmlB13 = ecf::app::buildXmlFromCanonical(dtoB13, "E340000000910", "101000001", "Empresa Demo");
        CHECK(xmlB13.find("<Comprador>") == std::string::npos, "E34 modifying B13 omits <Comprador>");
        CHECK(xmlB13.find("<TipoIngresos>") == std::string::npos, "E34 modifying B13 omits <TipoIngresos>");
        CHECK(xmlB13.find("<IndicadorMontoGravado>") == std::string::npos, "E34 modifying B13 omits <IndicadorMontoGravado>");
        CHECK(xmlB13.find("<MontoExento>300.00</MontoExento>") != std::string::npos, "E34 modifying B13 emits MontoExento");
        signAndCheck(xmlB13, "101000001", "e-CF 34 v.1.0.xsd", "E34 modifying B13 passes DGII XSD validation");

        ecf::app::CanonicalDocumentDto dtoE46;
        dtoE46.tipoComprobante = "E34";
        dtoE46.header.razonSocialComprador = "Foreign Export Client";
        dtoE46.totals.montoSubtotal = 1000.0;
        dtoE46.totals.montoTotal = 1000.0;
        dtoE46.references.correctsENcf = "E460000000801";
        dtoE46.references.codigoModificacion = 1;
        dtoE46.references.fechaNcfModificado = "2026-08-01";
        l.unitPrice = 1000.0; l.amount = 1000.0;
        dtoE46.lines.push_back(l);

        std::string xmlE46 = ecf::app::buildXmlFromCanonical(dtoE46, "E340000000911", "101000001", "Empresa Demo");
        CHECK(xmlE46.find("<IdentificadorExtranjero>EXTRANJERO</IdentificadorExtranjero>") != std::string::npos,
              "E34 modifying E46 emits EXTRANJERO fallback when foreign buyer has no RNC");
        CHECK(xmlE46.find("<IndicadorFacturacion>3</IndicadorFacturacion>") != std::string::npos,
              "E34 modifying E46 uses IndicadorFacturacion 3 (0% export)");
        signAndCheck(xmlE46, "101000001", "e-CF 34 v.1.0.xsd", "E34 modifying E46 passes DGII XSD validation");
    }

    {
        ecf::app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E31";
        dto.header.rncComprador = "130000000";
        dto.header.razonSocialComprador = "Cliente Descuento Plegado";
        dto.totals.montoSubtotal = 90.0;
        dto.totals.montoGravadoTotal = 90.0;
        dto.totals.montoItbis = 16.20;
        dto.totals.montoTotal = 106.20;

        dto.references.correctsENcf = "E310000000001";
        dto.references.codigoModificacion = 1;

        ecf::app::CanonicalLineDto l;
        l.lineNumber = 1;
        l.itemName = "Lote 3 unidades con descuento plegado";
        l.quantity = 3.0;
        l.unitPrice = 30.0;
        l.discountAmount = 10.0;
        l.amount = 90.0;
        dto.lines.push_back(l);

        std::string xml = ecf::app::buildXmlFromCanonical(dto, "E310000000920", "101000001", "Empresa Demo");
        CHECK(xml.find("<PrecioUnitarioItem>33.3333</PrecioUnitarioItem>") != std::string::npos,
              "Pre-folded line reconstructs 4-decimal gross PrecioUnitarioItem 33.3333");
        CHECK(xml.find("<DescuentoMonto>10.00</DescuentoMonto>") != std::string::npos,
              "Pre-folded line emits DescuentoMonto 10.00");
        CHECK(xml.find("<InformacionReferencia>") == std::string::npos,
              "Rule [613]: E31 with CodigoModificacion=1 strips InformacionReferencia");
        signAndCheck(xml, "101000001", "e-CF 31 v.1.0.xsd", "Pre-folded 4-decimal discount E31 passes DGII XSD validation");

        dto.references.correctsENcf = "0100000009";
        dto.references.codigoModificacion = 4;
        dto.references.fechaNcfModificado = "2026-08-05";
        std::string xmlContingency = ecf::app::buildXmlFromCanonical(dto, "E310000000921", "101000001", "Empresa Demo");
        CHECK(xmlContingency.find("<NCFModificado>B0100000009</NCFModificado>") != std::string::npos,
              "Rule [613]: E31 with CodigoModificacion=4 preserves normalized Serie B NCFModificado");
        signAndCheck(xmlContingency, "101000001", "e-CF 31 v.1.0.xsd", "Contingency replacement E31 passes DGII XSD validation");
    }

    if (failures == 0) {
        std::printf("All XSD, multi-tenant, and security tests passed successfully!\n");
    } else {
        std::printf("%d test(s) failed.\n", failures);
    }

    return failures ? 1 : 0;
}
