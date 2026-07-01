#pragma once
// DTOs for DGII responses and request payloads. Field names match the JSON
// property names used by the DGII endpoints.

#include <optional>
#include <string>
#include <vector>

namespace ecf::domain {

struct MensajeCodigo {
    std::string codigo;
    std::string valor;
};

struct EcfRecepcionResponse {
    std::string trackId;
    std::optional<std::string> error;
    std::optional<std::string> mensaje;
};

struct RfceRecepcionResponse {
    int codigo = 0;
    std::string estado;
    std::vector<MensajeCodigo> mensajes;
    std::string eNcf;
    bool secuenciaUtilizada = false;
};

struct ConsultaResultadoResponse {
    std::string trackId;
    int codigo = 0;
    std::string estado;
    std::string rnc;
    std::string eNcf;
    bool secuenciaUtilizada = false;
    std::string fechaRecepcion;
    std::vector<MensajeCodigo> mensajes;
};

struct ConsultaEstadoResponse {
    int codigo = 0;
    std::string estado;
    std::string rncEmisor;
    std::string ncfElectronico;
    double montoTotal = 0;
    double totalITBIS = 0;
    std::string fechaEmision;
    std::string fechaFirma;
    std::string rncComprador;
    std::string codigoSeguridad;
    std::string idExtranjero;
};

struct RfceConsultaResponse {
    std::string rnc;
    std::string eNcf;
    bool secuenciaUtilizada = false;
    std::string codigo;
    std::string estado;
    std::vector<MensajeCodigo> mensajes;
};

struct TimbreResponse {
    std::string rncEmisor;
    std::string razonSocialEmisor;
    std::string rncComprador;
    std::string razonSocialComprador;
    std::string eNcf;
    std::string fechaEmision;
    double totalITBIS = 0;
    double montoTotal = 0;
    std::string estado;
};

struct TimbreFcResponse {
    std::string rncEmisor;
    std::string razonSocialEmisor;
    std::string eNcf;
    std::string estado;
};

struct DirectorioContribuyente {
    std::string nombre;
    std::string rnc;
    std::string urlRecepcion;
    std::string urlAceptacion;
    std::string urlOpcional;
};

struct EstatusServicio {
    std::string servicio;
    std::string estatus;
    std::string ambiente;
};

struct TrackIdDetalle {
    std::string trackId;
    std::string estado;
    std::string fechaRecepcion;
};

struct VentanaMantenimiento {
    std::string inicio;
    std::string fin;
    std::string descripcion;
};

struct AprobacionComercialResponse {
    std::string codigo;
    std::string estado;
    std::vector<std::string> mensaje;
};

struct AnulacionResponse {
    std::string rnc;
    std::string codigo;
    std::string nombre;
    std::vector<std::string> mensajes;
};

struct TimbreEcfRequest {
    std::string rncEmisor;
    std::string rncComprador;
    std::string eNcf;
    std::string fechaEmision;
    double montoTotal = 0;
    std::string fechaFirma;
    std::string codigoSeguridad;
};

struct TimbreFcRequest {
    std::string rncEmisor;
    std::string eNcf;
    double montoTotal = 0;
    std::string codigoSeguridad;
};

struct ConsultaEstadoRequest {
    std::string rncEmisor;
    std::string eNcf;
    std::optional<std::string> rncComprador;
    std::optional<std::string> codigoSeguridad;

    ConsultaEstadoRequest() = default;
    ConsultaEstadoRequest(std::string rnc, std::string encf,
                          std::optional<std::string> rncComp = std::nullopt,
                          std::optional<std::string> codSeg = std::nullopt)
        : rncEmisor(std::move(rnc)),
          eNcf(std::move(encf)),
          rncComprador(std::move(rncComp)),
          codigoSeguridad(std::move(codSeg)) {}
};

}  // namespace ecf::domain
