#include "Infrastructure/Serialization/EcfXmlSerializer.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#include <cstdio>
#include <sstream>
#include <string>

#include "Domain/Exceptions/EcfException.h"

namespace ecf::infra {

using namespace ecf::domain;

namespace {

std::string xmlEscape(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out += c;
        }
    }
    return out;
}

std::string money(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.2f", v);
    return std::string(buf);
}

void el(std::ostringstream& os, const std::string& tag, const std::string& value) {
    os << '<' << tag << '>' << xmlEscape(value) << "</" << tag << '>';
}

void elOpt(std::ostringstream& os, const std::string& tag,
           const std::optional<std::string>& value) {
    if (value.has_value()) el(os, tag, *value);
}

void elMoneyOpt(std::ostringstream& os, const std::string& tag,
                const std::optional<double>& value) {
    if (value.has_value()) el(os, tag, money(*value));
}

// --- libxml2 read helpers --------------------------------------------------

std::string nodeText(xmlNode* node) {
    if (!node) return {};
    xmlChar* content = xmlNodeGetContent(node);
    std::string s = content ? reinterpret_cast<const char*>(content) : "";
    if (content) xmlFree(content);
    // trim
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");
    return (a == std::string::npos) ? "" : s.substr(a, b - a + 1);
}

xmlNode* firstChildNamed(xmlNode* parent, const char* name) {
    if (!parent) return nullptr;
    for (xmlNode* n = parent->children; n; n = n->next) {
        if (n->type == XML_ELEMENT_NODE &&
            xmlStrcasecmp(n->name, reinterpret_cast<const xmlChar*>(name)) == 0) {
            return n;
        }
    }
    return nullptr;
}

std::string childText(xmlNode* parent, const char* name) {
    return nodeText(firstChildNamed(parent, name));
}

bool childBool(xmlNode* parent, const char* name) {
    std::string s = childText(parent, name);
    return s == "true" || s == "True" || s == "1";
}

int childInt(xmlNode* parent, const char* name) {
    std::string s = childText(parent, name);
    try {
        return s.empty() ? 0 : std::stoi(s);
    } catch (...) {
        return 0;
    }
}

std::vector<MensajeCodigo> parseMensajes(xmlNode* parent) {
    std::vector<MensajeCodigo> out;
    xmlNode* wrapper = firstChildNamed(parent, "mensajes");
    if (!wrapper) return out;
    for (xmlNode* m = wrapper->children; m; m = m->next) {
        if (m->type != XML_ELEMENT_NODE) continue;
        MensajeCodigo mc;
        mc.codigo = childText(m, "Codigo");
        mc.valor = childText(m, "Valor");
        out.push_back(std::move(mc));
    }
    return out;
}

struct XmlDocGuard {
    xmlDocPtr doc;
    explicit XmlDocGuard(const std::string& xml)
        : doc(xmlReadMemory(xml.c_str(), static_cast<int>(xml.size()), "resp.xml",
                            nullptr, XML_PARSE_NOERROR | XML_PARSE_NOWARNING)) {}
    ~XmlDocGuard() { if (doc) xmlFreeDoc(doc); }
};

}  // namespace

std::string EcfXmlSerializer::serialize(const Rfce& model) {
    const auto& e = model.encabezado;
    std::ostringstream os;
    os << "<?xml version=\"1.0\" encoding=\"utf-8\"?>";
    os << "<RFCE>";
    os << "<Encabezado>";

    el(os, "VERSI\xC3\x93N", e.version);  // "VERSIÓN" in UTF-8

    // IdDoc
    os << "<IdDoc>";
    el(os, "TipoeCF", e.idDoc.tipoeCF);
    el(os, "ENcf", e.idDoc.eNcf);
    el(os, "TipoIngresos", std::to_string(e.idDoc.tipoIngresos));
    el(os, "TipoPago", std::to_string(e.idDoc.tipoPago));
    os << "<TablaFormasPago>";
    for (const auto& fp : e.idDoc.tablaFormasPago) {
        os << "<FormaDePago>";
        el(os, "FormaPago", std::to_string(fp.formaPago));
        el(os, "MontoPago", money(fp.montoPago));
        os << "</FormaDePago>";
    }
    os << "</TablaFormasPago>";
    os << "</IdDoc>";

    // Emisor
    os << "<Emisor>";
    el(os, "RncEmisor", e.emisor.rncEmisor);
    el(os, "RazonSocialEmisor", e.emisor.razonSocialEmisor);
    el(os, "FechaEmision", e.emisor.fechaEmision);
    os << "</Emisor>";

    // Comprador (optional)
    if (e.comprador.has_value()) {
        os << "<Comprador>";
        elOpt(os, "RncComprador", e.comprador->rncComprador);
        elOpt(os, "IdentificadorExtranjero", e.comprador->identificadorExtranjero);
        elOpt(os, "RazonSocialComprador", e.comprador->razonSocialComprador);
        os << "</Comprador>";
    }

    // Totales
    const auto& t = e.totales;
    os << "<Totales>";
    elMoneyOpt(os, "MontoGravadoTotal", t.montoGravadoTotal);
    elMoneyOpt(os, "MontoGravadoI1", t.montoGravadoI1);
    elMoneyOpt(os, "MontoGravadoI2", t.montoGravadoI2);
    elMoneyOpt(os, "MontoGravadoI3", t.montoGravadoI3);
    elMoneyOpt(os, "MontoExento", t.montoExento);
    elMoneyOpt(os, "TotalITBIS", t.totalITBIS);
    elMoneyOpt(os, "TotalITBIS1", t.totalITBIS1);
    elMoneyOpt(os, "TotalITBIS2", t.totalITBIS2);
    elMoneyOpt(os, "TotalITBIS3", t.totalITBIS3);
    elMoneyOpt(os, "MontoImpuestoAdicional", t.montoImpuestoAdicional);
    if (!t.impuestosAdicionales.empty()) {
        os << "<ImpuestosAdicionales>";
        for (const auto& ia : t.impuestosAdicionales) {
            os << "<ImpuestoAdicional>";
            el(os, "TipoImpuesto", ia.tipoImpuesto);
            elMoneyOpt(os, "MontoImpuestoSelectivoConsumoEspecifico",
                       ia.montoImpuestoSelectivoConsumoEspecifico);
            elMoneyOpt(os, "MontoImpuestoSelectivoConsumoAdvalorem",
                       ia.montoImpuestoSelectivoConsumoAdvalorem);
            elMoneyOpt(os, "OtrosImpuestosAdicionales", ia.otrosImpuestosAdicionales);
            os << "</ImpuestoAdicional>";
        }
        os << "</ImpuestosAdicionales>";
    }
    el(os, "MontoTotal", money(t.montoTotal));
    elMoneyOpt(os, "MontoNoFacturable", t.montoNoFacturable);
    elMoneyOpt(os, "MontoPeriodo", t.montoPeriodo);
    elOpt(os, "CodigoSeguridadeCF", t.codigoSeguridadeCF);
    os << "</Totales>";

    os << "</Encabezado>";
    os << "</RFCE>";
    return os.str();
}

EcfRecepcionResponse EcfXmlSerializer::deserializeEcfRecepcion(const std::string& xml) {
    XmlDocGuard g(xml);
    if (!g.doc) throw EcfException("XML de respuesta inválido (RespuestaRecepcion).");
    xmlNode* root = xmlDocGetRootElement(g.doc);
    EcfRecepcionResponse r;
    r.trackId = childText(root, "trackId");
    std::string err = childText(root, "error");
    if (!err.empty()) r.error = err;
    std::string msg = childText(root, "mensaje");
    if (!msg.empty()) r.mensaje = msg;
    return r;
}

RfceRecepcionResponse EcfXmlSerializer::deserializeRfceRecepcion(const std::string& xml) {
    XmlDocGuard g(xml);
    if (!g.doc) throw EcfException("XML de respuesta inválido (Respuesta).");
    xmlNode* root = xmlDocGetRootElement(g.doc);
    RfceRecepcionResponse r;
    r.codigo = childInt(root, "codigo");
    r.estado = childText(root, "estado");
    r.mensajes = parseMensajes(root);
    r.eNcf = childText(root, "encf");
    r.secuenciaUtilizada = childBool(root, "secuenciaUtilizada");
    return r;
}

ConsultaResultadoResponse EcfXmlSerializer::deserializeConsultaResultado(
    const std::string& xml) {
    XmlDocGuard g(xml);
    if (!g.doc) throw EcfException("XML de respuesta inválido (RespuestaConsultaTrackId).");
    xmlNode* root = xmlDocGetRootElement(g.doc);
    ConsultaResultadoResponse r;
    r.trackId = childText(root, "trackId");
    r.codigo = childInt(root, "codigo");
    r.estado = childText(root, "estado");
    r.rnc = childText(root, "rnc");
    r.eNcf = childText(root, "encf");
    r.secuenciaUtilizada = childBool(root, "secuenciaUtilizada");
    r.fechaRecepcion = childText(root, "fechaRecepcion");
    r.mensajes = parseMensajes(root);
    return r;
}

std::string EcfXmlSerializer::getFileName(const std::string& rncEmisor,
                                          const std::string& eNcf) {
    return rncEmisor + eNcf + ".xml";
}

std::string EcfXmlSerializer::escapeAlfanum(const std::string& value) {
    return xmlEscape(value);
}

}  // namespace ecf::infra
