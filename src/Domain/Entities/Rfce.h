#pragma once
// Represents a "Resumen de Factura de Consumo Electrónica" (e-CF type 32).
// The XML element names/order are reproduced by EcfXmlSerializer.

#include <optional>
#include <string>
#include <vector>

namespace ecf::domain {

struct FormaDePagoItem {
    int formaPago = 0;
    double montoPago = 0;
};

struct RfceIdDoc {
    std::string tipoeCF = "32";
    std::string eNcf;
    int tipoIngresos = 0;
    int tipoPago = 0;
    std::vector<FormaDePagoItem> tablaFormasPago;
};

struct RfceEmisor {
    std::string rncEmisor;
    std::string razonSocialEmisor;
    std::string fechaEmision;
};

struct RfceComprador {
    std::optional<std::string> rncComprador;
    std::optional<std::string> identificadorExtranjero;
    std::optional<std::string> razonSocialComprador;
};

struct ImpuestoAdicionalItem {
    std::string tipoImpuesto;
    std::optional<double> montoImpuestoSelectivoConsumoEspecifico;
    std::optional<double> montoImpuestoSelectivoConsumoAdvalorem;
    std::optional<double> otrosImpuestosAdicionales;
};

struct RfceTotales {
    std::optional<double> montoGravadoTotal;
    std::optional<double> montoGravadoI1;
    std::optional<double> montoGravadoI2;
    std::optional<double> montoGravadoI3;
    std::optional<double> montoExento;
    std::optional<double> totalITBIS;
    std::optional<double> totalITBIS1;
    std::optional<double> totalITBIS2;
    std::optional<double> totalITBIS3;
    std::optional<double> montoImpuestoAdicional;
    std::vector<ImpuestoAdicionalItem> impuestosAdicionales;
    double montoTotal = 0;
    std::optional<double> montoNoFacturable;
    std::optional<double> montoPeriodo;
    std::optional<std::string> codigoSeguridadeCF;
};

struct RfceEncabezado {
    std::string version = "1.0";
    RfceIdDoc idDoc;
    RfceEmisor emisor;
    std::optional<RfceComprador> comprador;
    RfceTotales totales;
};

struct Rfce {
    RfceEncabezado encabezado;
    // The <Signature> block is appended by EcfXmlSigner after serialization.
};

}  // namespace ecf::domain
