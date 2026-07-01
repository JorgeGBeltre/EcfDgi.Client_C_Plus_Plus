#include "Infrastructure/Dgii/DgiiDirectTransport.h"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>

#include "Domain/Exceptions/EcfException.h"
#include "Infrastructure/Security/EcfSecurityUtils.h"

namespace ecf::infra {

using namespace ecf::domain;
using json = nlohmann::json;

namespace {

// Case-insensitive JSON field lookup for tolerant matching.
const json* find(const json& j, const std::string& key) {
    if (!j.is_object()) return nullptr;
    for (auto it = j.begin(); it != j.end(); ++it) {
        std::string a = it.key(), b = key;
        std::transform(a.begin(), a.end(), a.begin(), ::tolower);
        std::transform(b.begin(), b.end(), b.begin(), ::tolower);
        if (a == b) return &it.value();
    }
    return nullptr;
}

std::string jstr(const json& j, const std::string& key) {
    const json* v = find(j, key);
    if (!v || v->is_null()) return {};
    if (v->is_string()) return v->get<std::string>();
    return v->dump();
}

int jint(const json& j, const std::string& key) {
    const json* v = find(j, key);
    if (!v || v->is_null()) return 0;
    if (v->is_number()) return v->get<int>();
    if (v->is_string()) { try { return std::stoi(v->get<std::string>()); } catch (...) {} }
    return 0;
}

double jdbl(const json& j, const std::string& key) {
    const json* v = find(j, key);
    if (!v || v->is_null()) return 0;
    if (v->is_number()) return v->get<double>();
    if (v->is_string()) { try { return std::stod(v->get<std::string>()); } catch (...) {} }
    return 0;
}

bool jbool(const json& j, const std::string& key) {
    const json* v = find(j, key);
    if (!v || v->is_null()) return false;
    if (v->is_boolean()) return v->get<bool>();
    if (v->is_string()) { std::string s = v->get<std::string>(); return s == "true" || s == "1"; }
    return false;
}

std::vector<MensajeCodigo> jmensajes(const json& j) {
    std::vector<MensajeCodigo> out;
    const json* v = find(j, "mensajes");
    if (v && v->is_array()) {
        for (const auto& m : *v) {
            MensajeCodigo mc;
            mc.codigo = jstr(m, "codigo");
            mc.valor = jstr(m, "valor");
            out.push_back(std::move(mc));
        }
    }
    return out;
}

json parseJson(const std::string& body) {
    return json::parse(body, nullptr, /*allow_exceptions=*/false);
}

bool looksLikeXml(const std::string& body, const std::string& contentType) {
    if (contentType.find("application/xml") != std::string::npos) return true;
    size_t a = body.find_first_not_of(" \t\r\n");
    return a != std::string::npos && body[a] == '<';
}

std::string contentTypeOf(const cpr::Response& r) {
    auto it = r.header.find("Content-Type");
    return it != r.header.end() ? it->second : "";
}

cpr::Multipart xmlPart(const std::string& xml, const std::string& fileName) {
    return cpr::Multipart{{"xml", cpr::Buffer{xml.begin(), xml.end(), fileName}}};
}

}  // namespace

DgiiDirectTransport::DgiiDirectTransport(std::shared_ptr<EcfTokenManager> tokenManager,
                                         EcfEnvironmentConfig config)
    : tokenManager_(std::move(tokenManager)), config_(std::move(config)) {}

std::string DgiiDirectTransport::getAuthToken() {
    if (!tokenManager_)
        throw EcfException(
            "No hay administrador de token configurado (falta certificado de firma).");
    return tokenManager_->getToken();
}

EcfRecepcionResponse DgiiDirectTransport::sendEcf(const std::string& xmlContent,
                                                  const std::string& fileName) {
    auto token = getAuthToken();
    auto resp = cpr::Post(cpr::Url{config_.recepcionUrl + "/api/facturaselectronicas"},
                          cpr::Bearer{token}, xmlPart(xmlContent, fileName));
    if (looksLikeXml(resp.text, contentTypeOf(resp)))
        return xmlSerializer_.deserializeEcfRecepcion(resp.text);

    json j = parseJson(resp.text);
    EcfRecepcionResponse r;
    r.trackId = jstr(j, "trackId");
    std::string err = jstr(j, "error");
    if (!err.empty()) r.error = err;
    std::string msg = jstr(j, "mensaje");
    if (!msg.empty()) r.mensaje = msg;
    return r;
}

RfceRecepcionResponse DgiiDirectTransport::sendRfce(const std::string& xmlContent,
                                                    const std::string& fileName) {
    auto token = getAuthToken();
    auto resp = cpr::Post(cpr::Url{config_.recepcionFcUrl + "/api/recepcion/ecf"},
                          cpr::Bearer{token}, xmlPart(xmlContent, fileName));
    if (looksLikeXml(resp.text, contentTypeOf(resp)))
        return xmlSerializer_.deserializeRfceRecepcion(resp.text);

    json j = parseJson(resp.text);
    RfceRecepcionResponse r;
    r.codigo = jint(j, "codigo");
    r.estado = jstr(j, "estado");
    r.mensajes = jmensajes(j);
    r.eNcf = jstr(j, "encf");
    r.secuenciaUtilizada = jbool(j, "secuenciaUtilizada");
    return r;
}

ConsultaResultadoResponse DgiiDirectTransport::consultarResultado(const std::string& trackId) {
    auto token = getAuthToken();
    auto resp = cpr::Get(
        cpr::Url{config_.consultaResultadoUrl + "/api/consultas/estado?trackid=" + trackId},
        cpr::Bearer{token}, cpr::Header{{"Accept", "application/json"}});
    json j = parseJson(resp.text);
    ConsultaResultadoResponse r;
    r.trackId = jstr(j, "trackId");
    r.codigo = jint(j, "codigo");
    r.estado = jstr(j, "estado");
    r.rnc = jstr(j, "rnc");
    r.eNcf = jstr(j, "encf");
    r.secuenciaUtilizada = jbool(j, "secuenciaUtilizada");
    r.fechaRecepcion = jstr(j, "fechaRecepcion");
    r.mensajes = jmensajes(j);
    return r;
}

ConsultaEstadoResponse DgiiDirectTransport::consultarEstado(const ConsultaEstadoRequest& req) {
    auto token = getAuthToken();
    std::string url = config_.consultaEstadoUrl +
                      "/api/consultas/estado?rncemisor=" + req.rncEmisor +
                      "&ncfelectronico=" + req.eNcf;
    if (req.rncComprador && !req.rncComprador->empty())
        url += "&rnccomprador=" + *req.rncComprador;
    if (req.codigoSeguridad && !req.codigoSeguridad->empty())
        url += "&codigoseguridad=" + *req.codigoSeguridad;

    auto resp = cpr::Get(cpr::Url{url}, cpr::Bearer{token},
                         cpr::Header{{"Accept", "application/json"}});
    json j = parseJson(resp.text);
    ConsultaEstadoResponse r;
    r.codigo = jint(j, "codigo");
    r.estado = jstr(j, "estado");
    r.rncEmisor = jstr(j, "rncEmisor");
    r.ncfElectronico = jstr(j, "ncfElectronico");
    r.montoTotal = jdbl(j, "montoTotal");
    r.totalITBIS = jdbl(j, "totalITBIS");
    r.fechaEmision = jstr(j, "fechaEmision");
    r.fechaFirma = jstr(j, "fechaFirma");
    r.rncComprador = jstr(j, "rncComprador");
    r.codigoSeguridad = jstr(j, "codigoSeguridad");
    r.idExtranjero = jstr(j, "idExtranjero");
    return r;
}

std::vector<TrackIdDetalle> DgiiDirectTransport::consultarTrackIds(const std::string& rncEmisor,
                                                                   const std::string& eNcf) {
    auto token = getAuthToken();
    auto resp = cpr::Get(cpr::Url{config_.consultaTrackIdsUrl +
                                  "/api/trackids/consulta?rncemisor=" + rncEmisor +
                                  "&encf=" + eNcf},
                         cpr::Bearer{token}, cpr::Header{{"Accept", "application/json"}});
    json j = parseJson(resp.text);
    std::vector<TrackIdDetalle> out;
    if (j.is_array()) {
        for (const auto& it : j) {
            TrackIdDetalle d;
            d.trackId = jstr(it, "trackId");
            d.estado = jstr(it, "estado");
            d.fechaRecepcion = jstr(it, "fechaRecepcion");
            out.push_back(std::move(d));
        }
    }
    return out;
}

RfceConsultaResponse DgiiDirectTransport::consultarRfce(const std::string& rncEmisor,
                                                        const std::string& eNcf,
                                                        const std::string& codigoSeguridad) {
    auto token = getAuthToken();
    auto resp = cpr::Get(cpr::Url{config_.consultaRfceUrl +
                                  "/api/Consultas/Consulta?RNC_Emisor=" + rncEmisor +
                                  "&ENCF=" + eNcf + "&Cod_Seguridad_eCF=" + codigoSeguridad},
                         cpr::Bearer{token}, cpr::Header{{"Accept", "application/json"}});
    json j = parseJson(resp.text);
    RfceConsultaResponse r;
    r.rnc = jstr(j, "rnc");
    r.eNcf = jstr(j, "encf");
    r.secuenciaUtilizada = jbool(j, "secuenciaUtilizada");
    r.codigo = jstr(j, "codigo");
    r.estado = jstr(j, "estado");
    r.mensajes = jmensajes(j);
    return r;
}

AprobacionComercialResponse DgiiDirectTransport::sendAprobacionComercial(
    const std::string& xmlContent, const std::string& fileName) {
    auto token = getAuthToken();
    auto resp = cpr::Post(cpr::Url{config_.aprobacionComercialUrl + "/api/aprobacioncomercial"},
                          cpr::Bearer{token}, xmlPart(xmlContent, fileName));
    json j = parseJson(resp.text);
    AprobacionComercialResponse r;
    r.codigo = jstr(j, "codigo");
    r.estado = jstr(j, "estado");
    const json* m = find(j, "mensaje");
    if (m && m->is_array())
        for (const auto& s : *m) r.mensaje.push_back(s.is_string() ? s.get<std::string>() : s.dump());
    return r;
}

AnulacionResponse DgiiDirectTransport::anularRangos(const std::string& xmlContent) {
    auto token = getAuthToken();
    auto resp = cpr::Post(cpr::Url{config_.anulacionRangosUrl + "/api/operaciones/anularrango"},
                          cpr::Bearer{token},
                          cpr::Header{{"Content-Type", "text/xml; charset=utf-8"}},
                          cpr::Body{xmlContent});
    json j = parseJson(resp.text);
    AnulacionResponse r;
    r.rnc = jstr(j, "rnc");
    r.codigo = jstr(j, "codigo");
    r.nombre = jstr(j, "nombre");
    const json* m = find(j, "mensajes");
    if (m && m->is_array())
        for (const auto& s : *m) r.mensajes.push_back(s.is_string() ? s.get<std::string>() : s.dump());
    return r;
}

std::vector<DirectorioContribuyente> DgiiDirectTransport::consultarDirectorio() {
    auto resp = cpr::Get(cpr::Url{config_.directorioUrl + "/api/consultas/listado"},
                         cpr::Header{{"Accept", "application/json"}});
    json j = parseJson(resp.text);
    std::vector<DirectorioContribuyente> out;
    if (j.is_array()) {
        for (const auto& it : j) {
            DirectorioContribuyente d;
            d.nombre = jstr(it, "nombre");
            d.rnc = jstr(it, "rnc");
            d.urlRecepcion = jstr(it, "urlRecepcion");
            d.urlAceptacion = jstr(it, "urlAceptacion");
            d.urlOpcional = jstr(it, "urlOpcional");
            out.push_back(std::move(d));
        }
    }
    return out;
}

DirectorioContribuyente DgiiDirectTransport::consultarDirectorioPorRnc(const std::string& rnc) {
    auto resp = cpr::Get(cpr::Url{config_.directorioUrl +
                                  "/api/consultas/obtenerdirectorioporrnc?RNC=" + rnc},
                         cpr::Header{{"Accept", "application/json"}});
    json j = parseJson(resp.text);
    DirectorioContribuyente d;
    d.nombre = jstr(j, "nombre");
    d.rnc = jstr(j, "rnc");
    d.urlRecepcion = jstr(j, "urlRecepcion");
    d.urlAceptacion = jstr(j, "urlAceptacion");
    d.urlOpcional = jstr(j, "urlOpcional");
    return d;
}

TimbreResponse DgiiDirectTransport::consultarTimbre(const TimbreEcfRequest& req) {
    std::string url = EcfSecurityUtils::buildTimbreUrl(config_.timbreUrl, req);
    auto resp = cpr::Get(cpr::Url{url}, cpr::Header{{"Accept", "application/json"}});
    json j = parseJson(resp.text);
    TimbreResponse r;
    r.rncEmisor = jstr(j, "rncEmisor");
    r.razonSocialEmisor = jstr(j, "razonSocialEmisor");
    r.rncComprador = jstr(j, "rncComprador");
    r.razonSocialComprador = jstr(j, "razonSocialComprador");
    r.eNcf = jstr(j, "eNcf");
    r.fechaEmision = jstr(j, "fechaEmision");
    r.totalITBIS = jdbl(j, "totalITBIS");
    r.montoTotal = jdbl(j, "montoTotal");
    r.estado = jstr(j, "estado");
    return r;
}

TimbreFcResponse DgiiDirectTransport::consultarTimbreFc(const TimbreFcRequest& req) {
    std::string url = EcfSecurityUtils::buildTimbreFcUrl(config_.timbreFcUrl, req);
    auto resp = cpr::Get(cpr::Url{url}, cpr::Header{{"Accept", "application/json"}});
    json j = parseJson(resp.text);
    TimbreFcResponse r;
    r.rncEmisor = jstr(j, "rncEmisor");
    r.razonSocialEmisor = jstr(j, "razonSocialEmisor");
    r.eNcf = jstr(j, "eNcf");
    r.estado = jstr(j, "estado");
    return r;
}

std::vector<EstatusServicio> DgiiDirectTransport::consultarEstatusServicios() {
    auto resp = cpr::Get(cpr::Url{config_.estatusServiciosUrl +
                                  "/api/estatusservicios/obtenerestatus"},
                         cpr::Header{{"Accept", "application/json"}});
    json j = parseJson(resp.text);
    std::vector<EstatusServicio> out;
    if (j.is_array()) {
        for (const auto& it : j) {
            EstatusServicio e;
            e.servicio = jstr(it, "servicio");
            e.estatus = jstr(it, "estatus");
            e.ambiente = jstr(it, "ambiente");
            out.push_back(std::move(e));
        }
    }
    return out;
}

std::vector<VentanaMantenimiento> DgiiDirectTransport::consultarVentanasMantenimiento() {
    auto resp = cpr::Get(cpr::Url{config_.estatusServiciosUrl +
                                  "/api/estatusservicios/obtenerventanasmantenimiento"},
                         cpr::Header{{"Accept", "application/json"}});
    json j = parseJson(resp.text);
    std::vector<VentanaMantenimiento> out;
    if (j.is_array()) {
        for (const auto& it : j) {
            VentanaMantenimiento v;
            v.inicio = jstr(it, "inicio");
            v.fin = jstr(it, "fin");
            v.descripcion = jstr(it, "descripcion");
            out.push_back(std::move(v));
        }
    }
    return out;
}

std::string DgiiDirectTransport::verificarEstadoAmbiente(AmbienteEnum ambiente) {
    int ambienteId = 1;
    switch (ambiente) {
        case AmbienteEnum::PreCertificacion: ambienteId = 1; break;
        case AmbienteEnum::Produccion: ambienteId = 2; break;
        case AmbienteEnum::Certificacion: ambienteId = 3; break;
    }
    auto resp = cpr::Get(cpr::Url{config_.estatusServiciosUrl +
                                  "/api/estatusservicios/verificarestado?ambiente=" +
                                  std::to_string(ambienteId)});
    return resp.text;
}

}  // namespace ecf::infra
