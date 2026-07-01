#include "Api/JsonMapping.h"

#include "Api/Services/CurrentUserService.h"

namespace ecf::api::mapping {

using namespace ecf::domain;

namespace {

std::string jstr(const Json::Value& j, const char* key, const std::string& def = "") {
    return (j.isObject() && j.isMember(key) && j[key].isString()) ? j[key].asString() : def;
}
double jdbl(const Json::Value& j, const char* key, double def = 0) {
    return (j.isObject() && j.isMember(key) && j[key].isNumeric()) ? j[key].asDouble() : def;
}
int jint(const Json::Value& j, const char* key, int def = 0) {
    return (j.isObject() && j.isMember(key) && j[key].isNumeric()) ? j[key].asInt() : def;
}
bool has(const Json::Value& j, const char* key) {
    return j.isObject() && j.isMember(key) && !j[key].isNull();
}
std::optional<std::string> optStr(const Json::Value& j, const char* key) {
    if (has(j, key) && j[key].isString()) return j[key].asString();
    return std::nullopt;
}
std::optional<double> optDbl(const Json::Value& j, const char* key) {
    if (has(j, key) && j[key].isNumeric()) return j[key].asDouble();
    return std::nullopt;
}

}  // namespace

Json::Value toJson(const EcfRecepcionResponse& r) {
    Json::Value v;
    v["trackId"] = r.trackId;
    v["error"] = r.error.has_value() ? Json::Value(*r.error) : Json::Value();
    v["mensaje"] = r.mensaje.has_value() ? Json::Value(*r.mensaje) : Json::Value();
    return v;
}

Json::Value toJson(const RfceRecepcionResponse& r) {
    Json::Value v;
    v["codigo"] = r.codigo;
    v["estado"] = r.estado;
    Json::Value msgs(Json::arrayValue);
    for (const auto& m : r.mensajes) {
        Json::Value mv;
        mv["codigo"] = m.codigo;
        mv["valor"] = m.valor;
        msgs.append(mv);
    }
    v["mensajes"] = msgs;
    v["encf"] = r.eNcf;
    v["secuenciaUtilizada"] = r.secuenciaUtilizada;
    return v;
}

Json::Value toJson(const ConsultaEstadoResponse& r) {
    Json::Value v;
    v["codigo"] = r.codigo;
    v["estado"] = r.estado;
    v["rncEmisor"] = r.rncEmisor;
    v["ncfElectronico"] = r.ncfElectronico;
    v["montoTotal"] = r.montoTotal;
    v["totalITBIS"] = r.totalITBIS;
    v["fechaEmision"] = r.fechaEmision;
    v["fechaFirma"] = r.fechaFirma;
    v["rncComprador"] = r.rncComprador;
    v["codigoSeguridad"] = r.codigoSeguridad;
    v["idExtranjero"] = r.idExtranjero;
    return v;
}

Json::Value toJson(const app::AuthResponseDto& d) {
    Json::Value v;
    v["username"] = d.username;
    v["token"] = d.token;
    v["role"] = d.role;
    return v;
}

Json::Value toJson(const app::CustomerDto& d) {
    Json::Value v;
    v["id"] = d.id;
    v["name"] = d.name;
    v["email"] = d.email;
    v["rnc"] = d.rnc;
    return v;
}

Rfce rfceFromJson(const Json::Value& j) {
    Rfce rfce;
    const Json::Value& enc = j["encabezado"];
    auto& e = rfce.encabezado;
    e.version = jstr(enc, "version", "1.0");

    const Json::Value& idDoc = enc["idDoc"];
    e.idDoc.tipoeCF = jstr(idDoc, "tipoeCF", "32");
    e.idDoc.eNcf = jstr(idDoc, "eNcf");
    e.idDoc.tipoIngresos = jint(idDoc, "tipoIngresos");
    e.idDoc.tipoPago = jint(idDoc, "tipoPago");
    if (idDoc.isMember("tablaFormasPago") && idDoc["tablaFormasPago"].isArray()) {
        for (const auto& fp : idDoc["tablaFormasPago"]) {
            FormaDePagoItem item;
            item.formaPago = jint(fp, "formaPago");
            item.montoPago = jdbl(fp, "montoPago");
            e.idDoc.tablaFormasPago.push_back(item);
        }
    }

    const Json::Value& em = enc["emisor"];
    e.emisor.rncEmisor = jstr(em, "rncEmisor");
    e.emisor.razonSocialEmisor = jstr(em, "razonSocialEmisor");
    e.emisor.fechaEmision = jstr(em, "fechaEmision");

    if (has(enc, "comprador")) {
        const Json::Value& co = enc["comprador"];
        RfceComprador comp;
        comp.rncComprador = optStr(co, "rncComprador");
        comp.identificadorExtranjero = optStr(co, "identificadorExtranjero");
        comp.razonSocialComprador = optStr(co, "razonSocialComprador");
        e.comprador = comp;
    }

    const Json::Value& t = enc["totales"];
    auto& to = e.totales;
    to.montoGravadoTotal = optDbl(t, "montoGravadoTotal");
    to.montoGravadoI1 = optDbl(t, "montoGravadoI1");
    to.montoGravadoI2 = optDbl(t, "montoGravadoI2");
    to.montoGravadoI3 = optDbl(t, "montoGravadoI3");
    to.montoExento = optDbl(t, "montoExento");
    to.totalITBIS = optDbl(t, "totalITBIS");
    to.totalITBIS1 = optDbl(t, "totalITBIS1");
    to.totalITBIS2 = optDbl(t, "totalITBIS2");
    to.totalITBIS3 = optDbl(t, "totalITBIS3");
    to.montoImpuestoAdicional = optDbl(t, "montoImpuestoAdicional");
    if (t.isMember("impuestosAdicionales") && t["impuestosAdicionales"].isArray()) {
        for (const auto& ia : t["impuestosAdicionales"]) {
            ImpuestoAdicionalItem item;
            item.tipoImpuesto = jstr(ia, "tipoImpuesto");
            item.montoImpuestoSelectivoConsumoEspecifico =
                optDbl(ia, "montoImpuestoSelectivoConsumoEspecifico");
            item.montoImpuestoSelectivoConsumoAdvalorem =
                optDbl(ia, "montoImpuestoSelectivoConsumoAdvalorem");
            item.otrosImpuestosAdicionales = optDbl(ia, "otrosImpuestosAdicionales");
            to.impuestosAdicionales.push_back(item);
        }
    }
    to.montoTotal = jdbl(t, "montoTotal");
    to.montoNoFacturable = optDbl(t, "montoNoFacturable");
    to.montoPeriodo = optDbl(t, "montoPeriodo");
    to.codigoSeguridadeCF = optStr(t, "codigoSeguridadeCF");

    return rfce;
}

std::shared_ptr<domain::ICurrentUserService> currentUserFrom(
    const drogon::HttpRequestPtr& req) {
    std::optional<std::string> userId, username;
    auto attr = req->attributes();
    if (attr->find("userId")) userId = attr->get<std::string>("userId");
    if (attr->find("username")) username = attr->get<std::string>("username");
    return std::make_shared<CurrentUserService>(std::move(userId), std::move(username));
}

}  // namespace ecf::api::mapping
