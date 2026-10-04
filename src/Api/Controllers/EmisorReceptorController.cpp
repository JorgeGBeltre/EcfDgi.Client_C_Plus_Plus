#include "Api/Controllers/EmisorReceptorController.h"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <algorithm>
#include <cctype>

#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <drogon/MultiPart.h>
#include <spdlog/spdlog.h>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/x509.h>

#include "Api/AppServices.h"
#include "Shared/Common/Sys.h"

using namespace drogon;

namespace ecf::api {

namespace {

struct IssuedSeedInfo {
    std::chrono::system_clock::time_point expiresAt;
};

std::mutex g_seedMutex;
std::unordered_map<std::string, IssuedSeedInfo> g_issuedSeeds;

struct ActiveTokenInfo {
    std::chrono::system_clock::time_point expiresAt;
    std::string certSubject;
};

std::mutex g_tokenMutex;
std::unordered_map<std::string, ActiveTokenInfo> g_activeTokens;

void pruneExpiredSeeds() {
    auto now = std::chrono::system_clock::now();
    for (auto it = g_issuedSeeds.begin(); it != g_issuedSeeds.end(); ) {
        if (it->second.expiresAt < now) {
            it = g_issuedSeeds.erase(it);
        } else {
            ++it;
        }
    }
}

void pruneExpiredTokens() {
    auto now = std::chrono::system_clock::now();
    for (auto it = g_activeTokens.begin(); it != g_activeTokens.end(); ) {
        if (it->second.expiresAt < now) {
            it = g_activeTokens.erase(it);
        } else {
            ++it;
        }
    }
}

bool isAuthorizedB2B(const HttpRequestPtr& req, std::string* outSubject = nullptr) {
    std::string auth = req->getHeader("Authorization");
    std::string token;
    if (auth.rfind("Bearer ", 0) == 0 || auth.rfind("bearer ", 0) == 0) {
        token = auth.substr(7);
    } else {
        token = req->getHeader("token");
    }
    while (!token.empty() && std::isspace(static_cast<unsigned char>(token.front()))) token.erase(token.begin());
    while (!token.empty() && std::isspace(static_cast<unsigned char>(token.back()))) token.pop_back();

    if (token.empty()) return false;

    std::lock_guard<std::mutex> lock(g_tokenMutex);
    auto it = g_activeTokens.find(token);
    if (it == g_activeTokens.end()) return false;
    if (std::chrono::system_clock::now() > it->second.expiresAt) {
        g_activeTokens.erase(it);
        return false;
    }
    if (outSubject) {
        *outSubject = it->second.certSubject;
    }
    return true;
}

bool validateX509Certificate(const std::string& certBase64, std::string& subjectOut, std::string& errorMsg) {
    std::string cleanB64;
    cleanB64.reserve(certBase64.size());
    for (char c : certBase64) {
        if (!std::isspace(static_cast<unsigned char>(c))) {
            cleanB64.push_back(c);
        }
    }
    if (cleanB64.empty()) {
        errorMsg = "El certificado X.509 está vacío.";
        return false;
    }

    BIO* b64 = BIO_new(BIO_f_base64());
    if (!b64) {
        errorMsg = "Error al inicializar decodificador Base64.";
        return false;
    }
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    BIO* mem = BIO_new_mem_buf(cleanB64.data(), static_cast<int>(cleanB64.size()));
    if (!mem) {
        BIO_free(b64);
        errorMsg = "Error al asignar buffer de memoria para el certificado.";
        return false;
    }
    BIO* bio = BIO_push(b64, mem);

    X509* cert = d2i_X509_bio(bio, nullptr);
    BIO_free_all(bio);

    if (!cert) {
        errorMsg = "El certificado X.509 no pudo ser decodificado o tiene formato inválido.";
        return false;
    }

    char subjBuf[512] = {0};
    if (X509_NAME_oneline(X509_get_subject_name(cert), subjBuf, sizeof(subjBuf))) {
        subjectOut = subjBuf;
    }

    int notBeforeCmp = X509_cmp_current_time(X509_get0_notBefore(cert));
    int notAfterCmp = X509_cmp_current_time(X509_get0_notAfter(cert));
    X509_free(cert);

    if (notBeforeCmp > 0) {
        errorMsg = "El certificado digital provisto aún no ha entrado en vigencia.";
        return false;
    }
    if (notAfterCmp < 0) {
        errorMsg = "El certificado digital provisto ha expirado.";
        return false;
    }

    return true;
}

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
    if (!isAuthorizedB2B(req)) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k401Unauthorized);
        resp->setBody("Acceso no autorizado: Token de autenticación B2B ausente, inválido o expirado.");
        callback(resp);
        return;
    }

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
    std::string callerSubject;
    if (!isAuthorizedB2B(req, &callerSubject)) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k401Unauthorized);
        resp->setBody("Acceso no autorizado: Token de autenticación B2B ausente, inválido o expirado.");
        callback(resp);
        return;
    }

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

    xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
    xmlXPathRegisterNs(xpathCtx, reinterpret_cast<const xmlChar*>("ds"), reinterpret_cast<const xmlChar*>("http://www.w3.org/2000/09/xmldsig#"));
    std::string rncEmisor = xpathValue(xpathCtx, "//*[local-name()='RNCEmisor']");
    std::string rncComprador = xpathValue(xpathCtx, "//*[local-name()='RNCComprador']");
    std::string encf = xpathValue(xpathCtx, "//*[local-name()='eNCF']");
    std::string estado = xpathValue(xpathCtx, "//*[local-name()='Estado']");
    if (estado.empty()) {
        estado = xpathValue(xpathCtx, "//*[local-name()='EstadoAprobacion']");
    }
    std::string sigValue = xpathValue(xpathCtx, "//ds:SignatureValue");
    std::string motivoRechazo = xpathValue(xpathCtx, "//*[local-name()='DetalleMotivoRechazo']");
    xmlXPathFreeContext(xpathCtx);
    xmlFreeDoc(doc);

    if (rncEmisor.empty() || rncComprador.empty() || encf.empty() || estado.empty()) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("El XML de aprobación comercial no contiene las etiquetas obligatorias (RNCEmisor, RNCComprador, eNCF, Estado).");
        callback(resp);
        return;
    }

    if (sigValue.empty()) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("El XML de aprobación comercial no contiene una firma digital válida (ds:SignatureValue).");
        callback(resp);
        return;
    }

    if (!isValidRncOrCedula(rncEmisor) || !isValidRncOrCedula(rncComprador) || !isValidEncf(encf)) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Valores fiscales con formato inválido en XML de aprobación comercial (RNCEmisor, RNCComprador o eNCF).");
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

    spdlog::info("Aprobación comercial procesada para eNCF: {}, RNCEmisor: {}, RNCComprador: {}, Estado: {}, Caller: {}",
                 encf, rncEmisor, rncComprador, estado, callerSubject);

    std::string accept = req->getHeader("Accept");
    if (accept.find("application/json") != std::string::npos) {
        Json::Value j;
        j["codigo"] = "1";
        j["estado"] = "Aceptado";
        j["mensaje"] = "Aprobación comercial procesada correctamente.";
        auto resp = HttpResponse::newHttpJsonResponse(j);
        resp->setStatusCode(k200OK);
        callback(resp);
        return;
    }

    std::ostringstream ss;
    ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
       << "<RespuestaAprobacionComercial>\n"
       << "  <codigo>1</codigo>\n"
       << "  <estado>Aceptado</estado>\n"
       << "  <mensaje>Aprobación comercial procesada correctamente.</mensaje>\n"
       << "</RespuestaAprobacionComercial>";

    auto resp = HttpResponse::newHttpResponse();
    resp->setStatusCode(k200OK);
    resp->setContentTypeCode(CT_APPLICATION_XML);
    resp->setBody(ss.str());
    callback(resp);
}

void EmisorReceptorController::getSemilla(const HttpRequestPtr&,
                                         std::function<void(const HttpResponsePtr&)>&& callback) {
    std::string seedVal = sys::newUuid();
    auto now = std::chrono::system_clock::now();
    std::string fecha = formatIsoUtc(now);

    {
        std::lock_guard<std::mutex> lock(g_seedMutex);
        pruneExpiredSeeds();
        g_issuedSeeds[seedVal] = { now + std::chrono::minutes(10) };
    }

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
        resp->setBody("La semilla provista no contiene una firma digital válida (ds:SignatureValue), certificado X.509 o valor de semilla.");
        callback(resp);
        return;
    }

    {
        std::lock_guard<std::mutex> lock(g_seedMutex);
        pruneExpiredSeeds();
        auto it = g_issuedSeeds.find(valor);
        if (it == g_issuedSeeds.end()) {
            auto resp = HttpResponse::newHttpResponse();
            resp->setStatusCode(k400BadRequest);
            resp->setBody("La semilla provista no fue emitida por este servidor o ya ha expirado.");
            callback(resp);
            return;
        }
        g_issuedSeeds.erase(it); // Consume seed
    }

    std::string subject;
    std::string certErr;
    if (!validateX509Certificate(certValue, subject, certErr)) {
        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k400BadRequest);
        resp->setBody("Error de validación de certificado digital X.509: " + certErr);
        callback(resp);
        return;
    }

    std::string token = stripHyphens(sys::newUuid());
    auto now = std::chrono::system_clock::now();
    std::string expedido = formatIsoUtc(now);
    auto expTime = now + std::chrono::hours(1);
    std::string expira = formatIsoUtc(expTime);

    {
        std::lock_guard<std::mutex> lock(g_tokenMutex);
        pruneExpiredTokens();
        g_activeTokens[token] = { expTime, subject };
    }

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

