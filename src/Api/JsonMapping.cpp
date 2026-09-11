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
std::optional<int> optInt(const Json::Value& j, const char* key) {
    if (has(j, key) && j[key].isNumeric()) return j[key].asInt();
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

Json::Value toJson(const AprobacionComercialResponse& r) {
    Json::Value v;
    v["codigo"] = r.codigo;
    v["estado"] = r.estado;
    Json::Value msgs(Json::arrayValue);
    for (const auto& m : r.mensaje) {
        msgs.append(m);
    }
    v["mensaje"] = msgs;
    return v;
}

Json::Value toJson(const DirectorioContribuyente& d) {
    Json::Value v;
    v["nombre"] = d.nombre;
    v["rnc"] = d.rnc;
    v["urlRecepcion"] = d.urlRecepcion;
    v["urlAceptacion"] = d.urlAceptacion;
    v["urlOpcional"] = d.urlOpcional;
    return v;
}

Json::Value toJson(const app::AuthResponseDto& d) {
    Json::Value v;
    v["token"] = d.token;
    v["role"] = d.role;
    v["username"] = d.username;
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

Json::Value toJson(const app::CanonicalDocumentDto& d) {
    Json::Value j;
    if (d.ncf.has_value()) j["ncf"] = *d.ncf;
    j["documentKind"] = d.documentKind;
    j["tipoComprobante"] = d.tipoComprobante;

    Json::Value sr;
    sr["provider"] = d.sourceReference.provider;
    sr["txnId"] = d.sourceReference.txnId;
    sr["editSequence"] = d.sourceReference.editSequence;
    j["sourceReference"] = sr;

    Json::Value h;
    h["rncEmisor"] = d.header.rncEmisor;
    h["razonSocialEmisor"] = d.header.razonSocialEmisor;
    h["rncComprador"] = d.header.rncComprador;
    h["razonSocialComprador"] = d.header.razonSocialComprador;
    if (d.header.correoComprador.has_value()) h["correoComprador"] = *d.header.correoComprador;
    h["fechaEmision"] = d.header.fechaEmision;
    j["header"] = h;

    Json::Value lines(Json::arrayValue);
    for (const auto& l : d.lines) {
        Json::Value line;
        line["lineNumber"] = l.lineNumber;
        line["itemName"] = l.itemName;
        line["quantity"] = l.quantity;
        line["unitPrice"] = l.unitPrice;
        line["amount"] = l.amount;
        lines.append(line);
    }
    j["lines"] = lines;

    Json::Value t;
    t["montoSubtotal"] = d.totals.montoSubtotal;
    if (d.totals.montoGravadoTotal.has_value()) t["montoGravadoTotal"] = *d.totals.montoGravadoTotal;
    if (d.totals.montoExento.has_value()) t["montoExento"] = *d.totals.montoExento;
    if (!d.totals.taxBuckets.empty()) {
        Json::Value buckets(Json::arrayValue);
        for (const auto& b : d.totals.taxBuckets) {
            Json::Value bv;
            bv["rate"] = b.rate;
            bv["base"] = b.base;
            bv["tax"] = b.tax;
            buckets.append(bv);
        }
        t["taxBuckets"] = buckets;
    }
    t["montoItbis"] = d.totals.montoItbis;
    t["montoTotal"] = d.totals.montoTotal;
    j["totals"] = t;

    Json::Value r;
    r["correctsTxnId"] = d.references.correctsTxnId;
    r["correctsENcf"] = d.references.correctsENcf;
    if (d.references.codigoModificacion.has_value()) r["codigoModificacion"] = *d.references.codigoModificacion;
    if (d.references.razonModificacion.has_value()) r["razonModificacion"] = *d.references.razonModificacion;
    if (d.references.fechaNcfModificado.has_value()) r["fechaNcfModificado"] = *d.references.fechaNcfModificado;
    if (d.references.rncOtroContribuyente.has_value()) r["rncOtroContribuyente"] = *d.references.rncOtroContribuyente;
    j["references"] = r;

    if (d.retention.has_value()) {
        Json::Value ret;
        ret["indicadorAgenteRetencionoPercepcion"] = d.retention->indicadorAgenteRetencionoPercepcion;
        ret["montoItbisRetenido"] = d.retention->montoItbisRetenido;
        if (d.retention->montoIsrRetenido.has_value()) {
            ret["montoIsrRetenido"] = *d.retention->montoIsrRetenido;
        }
        j["retention"] = ret;
    }

    return j;
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

app::CanonicalDocumentDto canonicalDocumentFromJson(const Json::Value& j) {
    app::CanonicalDocumentDto d;
    d.ncf = optStr(j, "ncf");
    d.documentKind = jstr(j, "documentKind", "Invoice");
    d.tipoComprobante = jstr(j, "tipoComprobante", "E31");

    if (has(j, "sourceReference")) {
        const auto& sr = j["sourceReference"];
        d.sourceReference.provider = jstr(sr, "provider", "QuickBooksDesktop");
        d.sourceReference.txnId = jstr(sr, "txnId");
        d.sourceReference.editSequence = jstr(sr, "editSequence");
    }

    if (has(j, "header")) {
        const auto& h = j["header"];
        d.header.rncEmisor = jstr(h, "rncEmisor");
        d.header.razonSocialEmisor = jstr(h, "razonSocialEmisor");
        d.header.rncComprador = jstr(h, "rncComprador");
        d.header.razonSocialComprador = jstr(h, "razonSocialComprador");
        d.header.correoComprador = optStr(h, "correoComprador");
        d.header.fechaEmision = jstr(h, "fechaEmision");
    }

    if (j.isMember("lines") && j["lines"].isArray()) {
        for (const auto& l : j["lines"]) {
            app::CanonicalLineDto line;
            line.lineNumber = jint(l, "lineNumber");
            line.itemName = jstr(l, "itemName");
            line.quantity = jdbl(l, "quantity");
            line.unitPrice = jdbl(l, "unitPrice");
            line.amount = jdbl(l, "amount");
            d.lines.push_back(line);
        }
    }

    if (has(j, "totals")) {
        const auto& t = j["totals"];
        d.totals.montoSubtotal = jdbl(t, "montoSubtotal");
        d.totals.montoGravadoTotal = optDbl(t, "montoGravadoTotal");
        d.totals.montoExento = optDbl(t, "montoExento");
        if (t.isMember("taxBuckets") && t["taxBuckets"].isArray()) {
            for (const auto& tb : t["taxBuckets"]) {
                app::CanonicalTaxBucketDto b;
                b.rate = jint(tb, "rate", 18);
                b.base = jdbl(tb, "base");
                b.tax = jdbl(tb, "tax");
                d.totals.taxBuckets.push_back(b);
            }
        }
        d.totals.montoItbis = jdbl(t, "montoItbis");
        d.totals.montoTotal = jdbl(t, "montoTotal");
    }

    if (has(j, "references")) {
        const auto& r = j["references"];
        d.references.correctsTxnId = jstr(r, "correctsTxnId");
        d.references.correctsENcf = jstr(r, "correctsENcf");
        d.references.codigoModificacion = optInt(r, "codigoModificacion");
        d.references.razonModificacion = optStr(r, "razonModificacion");
        d.references.fechaNcfModificado = optStr(r, "fechaNcfModificado");
        d.references.rncOtroContribuyente = optStr(r, "rncOtroContribuyente");
    }

    if (has(j, "retention")) {
        const auto& ret = j["retention"];
        app::CanonicalRetentionDto retention;
        retention.indicadorAgenteRetencionoPercepcion = jint(ret, "indicadorAgenteRetencionoPercepcion", 1);
        retention.montoItbisRetenido = jdbl(ret, "montoItbisRetenido");
        retention.montoIsrRetenido = optDbl(ret, "montoIsrRetenido");
        d.retention = retention;
    }

    return d;
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
