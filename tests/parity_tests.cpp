// Comprehensive Parity Tests between C# and C++ implementations
#include <cstdio>
#include <string>
#include <iostream>

#include "Application/Ecf/CanonicalXmlBuilder.h"
#include "Application/Ecf/CanonicalDocumentDto.h"
#include "Domain/Entities/EcfDocument.h"

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
        
        CHECK(xml.find("<FechaVencimientoSecuencia>31-12-") != std::string::npos,
              "E31 XML includes default sequence expiration ending in 31-12-YYYY");
    }

    // Test 2: E32 must have IndicadorMontoGravado, but NO FechaVencimientoSecuencia
    {
        app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E32";
        dto.header.fechaEmision = "2026-09-17";
        dto.totals.montoTotal = 1000.0;

        std::string xml = app::buildXmlFromCanonical(dto, "E320000000001", "101672919", "WILLY CHIC DOMINICANA SRL");
        
        CHECK(xml.find("<IndicadorMontoGravado>0</IndicadorMontoGravado>") != std::string::npos,
              "E32 XML includes <IndicadorMontoGravado>0</IndicadorMontoGravado>");
        
        CHECK(xml.find("<FechaVencimientoSecuencia>") == std::string::npos,
              "E32 XML does NOT include <FechaVencimientoSecuencia>");
    }

    // Test 3: E34 must have IndicadorNotaCredito and IndicadorMontoGravado, and NO FechaVencimientoSecuencia (per DGII XSD)
    {
        app::CanonicalDocumentDto dto;
        dto.tipoComprobante = "E34";
        dto.header.fechaEmision = "2026-09-17";
        dto.references.correctsENcf = "E310000000001";
        dto.references.codigoModificacion = 1;
        dto.totals.montoTotal = 118.0;

        std::string xml = app::buildXmlFromCanonical(dto, "E340000000001", "101672919", "WILLY CHIC DOMINICANA SRL");
        
        CHECK(xml.find("<IndicadorNotaCredito>0</IndicadorNotaCredito>") != std::string::npos,
              "E34 XML includes <IndicadorNotaCredito>0</IndicadorNotaCredito>");
        
        CHECK(xml.find("<FechaVencimientoSecuencia>") == std::string::npos,
              "E34 XML does NOT include <FechaVencimientoSecuencia>");
        
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

    std::printf("\nTotal failures: %d\n", failures);
    return failures > 0 ? 1 : 0;
}
