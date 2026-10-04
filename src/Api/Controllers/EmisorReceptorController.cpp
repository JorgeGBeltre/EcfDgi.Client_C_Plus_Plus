#include "Api/Controllers/EmisorReceptorController.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <unordered_set>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <drogon/MultiPart.h>

#include "Api/AppServices.h"
#include "Shared/Common/Sys.h"

using namespace drogon;

namespace ecf::api {

namespace {

std::string extractXmlContent(const HttpRequestPtr& req) {
    MultiPartParser parser;
    if (parser.parse(req) == 0) {
        if (!parser.getFiles().empty()) {
            const auto& files = parser.getFiles();
            for (const auto& f : files) {
                if (f.getItemName() == "xml" || files.size() == 1) {
                    return std::string(f.fileData(), f.fileLength());
                }
            }
        }
        const auto& params = parser.getParameters();
        auto it = params.find("xml");
        if (it != params.end() && !it->second.empty()) {
            return it->second;
        }
    }
    return std::string(req->getBody());
}

std::string xpathValue(xmlXPathContextPtr ctx, const char* expr) {
    xmlXPathObjectPtr obj = xmlXPathEvalExpression(reinterpret_cast<const xmlChar*>(expr), ctx);
    if (!obj) return "";
    std::string val;
    if (obj->nodesetval && obj->nodesetval->nodeNr > 0) {
        xmlNodePtr node = obj->nodesetval->nodeTab[0];
        xmlChar* content = xmlNodeGetContent(node);
        if (content) {
            val = reinterpret_cast<char*>(content);
            xmlFree(content);
        }
    }
    xmlXPathFreeObject(obj);
    while (!val.empty() && std::isspace(static_cast<unsigned char>(val.front()))) val.erase(val.begin());
    while (!val.empty() && std::isspace(static_cast<unsigned char>(val.back()))) val.pop_back();
    return val;
}

std::string stripHyphens(std::string s) {
    s.erase(std::remove(s.begin(), s.end(), '-'), s.end());
    return s;
}

std::string formatIsoUtc(std::chrono::system_clock::time_point tp) {
    std::time_t tt = std::chrono::system_clock::to_time_t(tp);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm);
    return std::string(buf);
}

std::string escapeXml(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (char c : input) {
        switch (c) {
            case '&': out.append("&amp;"); break;
            case '<': out.append("&lt;"); break;
            case '>': out.append("&gt;"); break;
            case '"': out.append("&quot;"); break;
            case '\'': out.append("&apos;"); break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

bool isValidRncOrCedula(const std::string& s) {
    if (s.size() != 9 && s.size() != 11) return false;
    for (char c : s) {
        if (!std::isdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

bool isValidEncf(const std::string& s) {
    // Un e-NCF de la DGII consta exactamente de 13 caracteres:
    // 'E' + 2 dígitos de tipo e-CF (31, 32, 33, 34, 41, 43, 44, 45, 46, 47) + 10 dígitos de secuencia
    if (s.size() != 13 || s[0] != 'E') return false;

    std::string type = s.substr(1, 2);
    static const std::unordered_set<std::string> validTypes = {
        "31", "32", "33", "34", "41", "43", "44", "45", "46", "47"
    };
    if (validTypes.find(type) == validTypes.end()) {
        return false;
    }

    for (size_t i = 3; i < s.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    }
    return true;
}

}  // namespace

void EmisorReceptorController::recepcioneCF(const HttpRequestPtr& req,
                                            std::function<void(const HttpResponsePtr&)>&& callback) {
    std::string xmlContent = extractXmlContent(req);
    if (xmlContent.empty()) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Archivo XML no provisto o vacío.");
        callback(resp);
        return;
    }

    xmlDocPtr doc = xmlReadMemory(xmlContent.c_str(), static_cast<int>(xmlContent.size()), "ecf.xml", nullptr, XML_PARSE_NOBLANKS | XML_PARSE_NONET);
    if (!doc) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("El archivo enviado no es un XML válido.");
        callback(resp);
        return;
    }

    xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
    std::string rncEmisor = xpathValue(xpathCtx, "//*[local-name()='RNCEmisor']");
    std::string rncComprador = xpathValue(xpathCtx, "//*[local-name()='RNCComprador']");
    std::string encf = xpathValue(xpathCtx, "//*[local-name()='eNCF']");
    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(doc);

    if (rncEmisor.empty() || encf.empty() || rncComprador.empty()) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("El XML de e-CF provisto no contiene las etiquetas obligatorias RNCEmisor, RNCComprador o eNCF.");
        callback(resp);
        return;
    }

    if (!isValidRncOrCedula(rncEmisor) || !isValidRncOrCedula(rncComprador) || !isValidEncf(encf)) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Valores fiscales con formato inválido en XML (RNCEmisor, RNCComprador o eNCF).");
        callback(resp);
        return;
    }

    auto resolver = AppServices::instance().tenantSignerResolver();
    auto signer = resolver ? resolver->resolveTenantSignerOnly(rncComprador) : nullptr;
    if (!signer || signer == AppServices::instance().signer() || signer->usesFallbackCertificate()) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k404NotFound);
        resp->setBody("El RNCComprador especificado no corresponde a ninguna empresa receptora registrada o con certificado activo en esta plataforma.");
        callback(resp);
        return;
    }

    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::time_t dom_tt = tt - (4 * 3600);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &dom_tt);
#else
    gmtime_r(&dom_tt, &tm);
#endif
    char fechaHora[64];
    std::strftime(fechaHora, sizeof(fechaHora), "%d-%m-%Y %H:%M:%S", &tm);

    std::ostringstream ss;
    ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
       << "<ARECF>\n"
       << "  <DetalleAcusedeRecibo>\n"
       << "    <Version>1.0</Version>\n"
       << "    <RNCEmisor>" << escapeXml(rncEmisor) << "</RNCEmisor>\n"
       << "    <RNCComprador>" << escapeXml(rncComprador) << "</RNCComprador>\n"
       << "    <eNCF>" << escapeXml(encf) << "</eNCF>\n"
       << "    <Estado>0</Estado>\n"
       << "    <FechaHoraAcuseRecibo>" << fechaHora << "</FechaHoraAcuseRecibo>\n"
       << "  </DetalleAcusedeRecibo>\n"
       << "</ARECF>\n";

    std::string unsignedArecf = ss.str();
    try {
        std::string signedArecf = signer->signXml(unsignedArecf, rncComprador);
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k200OK);
        resp->setContentTypeCode(CT_APPLICATION_XML);
        resp->setBody(signedArecf);
        callback(resp);
    } catch (const std::exception& ex) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody(std::string("Error al procesar la recepción de e-CF: ") + ex.what());
        callback(resp);
    }
}

void EmisorReceptorController::aprobacionComercial(const HttpRequestPtr& req,
                                                  std::function<void(const HttpResponsePtr&)>&& callback) {
    std::string xmlContent = extractXmlContent(req);
    if (xmlContent.empty()) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Archivo XML de aprobación comercial no provisto o vacío.");
        callback(resp);
        return;
    }

    xmlDocPtr doc = xmlReadMemory(xmlContent.c_str(), static_cast<int>(xmlContent.size()), "aprobacion.xml", nullptr, XML_PARSE_NOBLANKS | XML_PARSE_NONET);
    if (!doc) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Error al procesar la aprobación comercial: XML inválido.");
        callback(resp);
        return;
    }
    xmlFreeDoc(doc);

    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    callback(resp);
}

void EmisorReceptorController::getSemilla(const HttpRequestPtr&,
                                         std::function<void(const HttpResponsePtr&)>&& callback) {
    std::string seedVal = sys::newUuid();
    auto now = std::chrono::system_clock::now();
    std::string fecha = formatIsoUtc(now);

    std::ostringstream ss;
    ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
       << "<SemillaModel xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xmlns:xsd=\"http://www.w3.org/2001/XMLSchema\">\n"
       << "  <valor>" << seedVal << "</valor>\n"
       << "  <fecha>" << fecha << "</fecha>\n"
       << "</SemillaModel>";

    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->setContentTypeCode(CT_APPLICATION_XML);
    resp->setBody(ss.str());
    callback(resp);
}

void EmisorReceptorController::validarSemilla(const HttpRequestPtr& req,
                                             std::function<void(const HttpResponsePtr&)>&& callback) {
    std::string xmlContent = extractXmlContent(req);
    if (xmlContent.empty()) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Archivo XML de semilla firmada no provisto.");
        callback(resp);
        return;
    }

    xmlDocPtr doc = xmlReadMemory(xmlContent.c_str(), static_cast<int>(xmlContent.size()), "semilla.xml", nullptr, XML_PARSE_NOBLANKS | XML_PARSE_NONET);
    if (!doc) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Error al validar semilla: XML malformado.");
        callback(resp);
        return;
    }

    xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
    xmlXPathRegisterNs(xpathCtx, reinterpret_cast<const xmlChar*>("ds"), reinterpret_cast<const xmlChar*>("http://www.w3.org/2000/09/xmldsig#"));
    std::string sigValue = xpathValue(xpathCtx, "//ds:SignatureValue");
    std::string certValue = xpathValue(xpathCtx, "//ds:X509Certificate");
    std::string valor = xpathValue(xpathCtx, "//*[local-name()='valor']");
    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(doc);

    if (sigValue.empty() || certValue.empty() || valor.empty()) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("La semilla provista no contiene una firma digital válida (ds:SignatureValue) o certificado X.509.");
        callback(resp);
        return;
    }

    std::string token = stripHyphens(sys::newUuid());
    auto now = std::chrono::system_clock::now();
    std::string expedido = formatIsoUtc(now);
    std::string expira = formatIsoUtc(now + std::chrono::hours(1));

    std::string accept = req->getHeader("Accept");
    if (accept.find("application/json") != std::string::npos) {
        Json::Value j;
        j["token"] = token;
        j["expira"] = expira;
        j["expedido"] = expedido;
        auto resp = HttpResponse::newHttpJsonResponse(j);
        resp->setStatusCode(k200OK);
        callback(resp);
        return;
    }

    std::ostringstream ss;
    ss << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
       << "<RespuestaAutenticacion>\n"
       << "  <token>" << token << "</token>\n"
       << "  <expira>" << expira << "</expira>\n"
       << "  <expedido>" << expedido << "</expedido>\n"
       << "</RespuestaAutenticacion>";

    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->setContentTypeCode(CT_APPLICATION_XML);
    resp->setBody(ss.str());
    callback(resp);
}

}  // namespace ecf::api
