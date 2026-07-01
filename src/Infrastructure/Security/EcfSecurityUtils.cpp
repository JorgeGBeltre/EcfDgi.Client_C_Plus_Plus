#include "Infrastructure/Security/EcfSecurityUtils.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <openssl/evp.h>
#include <openssl/sha.h>

#include <array>
#include <cstdio>
#include <sstream>

#include "Domain/Exceptions/EcfException.h"

namespace ecf::infra::EcfSecurityUtils {

using domain::EcfException;

namespace {

std::string urlEncode(const std::string& value) {
    static const char hex[] = "0123456789ABCDEF";
    std::string out;
    out.reserve(value.size() * 3);
    for (unsigned char c : value) {
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 0x0F];
        }
    }
    return out;
}

std::string money(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.2f", v);
    return std::string(buf);
}

}  // namespace

std::string extractSignatureValue(const std::string& signedXml) {
    xmlDocPtr doc = xmlReadMemory(signedXml.c_str(),
                                  static_cast<int>(signedXml.size()),
                                  "signed.xml", nullptr, 0);
    if (!doc) throw EcfException("El XML firmado es inválido.");

    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    xmlXPathRegisterNs(ctx, reinterpret_cast<const xmlChar*>("ds"),
                       reinterpret_cast<const xmlChar*>("http://www.w3.org/2000/09/xmldsig#"));
    xmlXPathObjectPtr obj = xmlXPathEvalExpression(
        reinterpret_cast<const xmlChar*>("//ds:SignatureValue"), ctx);

    std::string value;
    if (obj && obj->nodesetval && obj->nodesetval->nodeNr > 0) {
        xmlChar* content = xmlNodeGetContent(obj->nodesetval->nodeTab[0]);
        if (content) {
            value = reinterpret_cast<const char*>(content);
            xmlFree(content);
        }
    }
    if (obj) xmlXPathFreeObject(obj);
    if (ctx) xmlXPathFreeContext(ctx);
    xmlFreeDoc(doc);

    size_t a = value.find_first_not_of(" \t\r\n");
    size_t b = value.find_last_not_of(" \t\r\n");
    if (a == std::string::npos)
        throw EcfException(
            "El XML no contiene un nodo SignatureValue. ¿Fue firmado correctamente?");
    return value.substr(a, b - a + 1);
}

std::string calcularCodigoSeguridad(const std::string& signedXml) {
    const std::string signatureValue = extractSignatureValue(signedXml);

    std::array<unsigned char, SHA256_DIGEST_LENGTH> hash{};
    SHA256(reinterpret_cast<const unsigned char*>(signatureValue.data()),
           signatureValue.size(), hash.data());

    static const char hex[] = "0123456789abcdef";
    std::string full;
    full.reserve(hash.size() * 2);
    for (unsigned char c : hash) {
        full += hex[c >> 4];
        full += hex[c & 0x0F];
    }
    return full.substr(0, 6);
}

std::string buildTimbreUrl(const std::string& baseUrl,
                           const domain::TimbreEcfRequest& req) {
    std::ostringstream os;
    // NOTE: the DGII query parameter is spelled "codigoseuridad" verbatim so the
    // request matches exactly what the endpoint expects.
    os << baseUrl
       << "?rncemisor=" << urlEncode(req.rncEmisor)
       << "&rnccomprador=" << urlEncode(req.rncComprador)
       << "&encf=" << urlEncode(req.eNcf)
       << "&fechaemision=" << urlEncode(req.fechaEmision)
       << "&montototal=" << urlEncode(money(req.montoTotal))
       << "&fechafirma=" << urlEncode(req.fechaFirma)
       << "&codigoseuridad=" << urlEncode(req.codigoSeguridad);
    return os.str();
}

std::string buildTimbreFcUrl(const std::string& baseUrl,
                             const domain::TimbreFcRequest& req) {
    std::ostringstream os;
    os << baseUrl
       << "?rncemisor=" << urlEncode(req.rncEmisor)
       << "&encf=" << urlEncode(req.eNcf)
       << "&montototal=" << urlEncode(money(req.montoTotal))
       << "&codigoseuridad=" << urlEncode(req.codigoSeguridad);
    return os.str();
}

}  // namespace ecf::infra::EcfSecurityUtils
