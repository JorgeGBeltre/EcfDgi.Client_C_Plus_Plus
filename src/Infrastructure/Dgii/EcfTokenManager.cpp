#include "Infrastructure/Dgii/EcfTokenManager.h"

#include <cpr/cpr.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <cstdio>
#include <ctime>

#include "Domain/Exceptions/EcfException.h"

namespace ecf::infra {

using domain::EcfException;

namespace {

std::string childText(xmlNode* parent, const char* name) {
    if (!parent) return {};
    for (xmlNode* n = parent->children; n; n = n->next) {
        if (n->type == XML_ELEMENT_NODE &&
            xmlStrcasecmp(n->name, reinterpret_cast<const xmlChar*>(name)) == 0) {
            xmlChar* c = xmlNodeGetContent(n);
            std::string s = c ? reinterpret_cast<const char*>(c) : "";
            if (c) xmlFree(c);
            return s;
        }
    }
    return {};
}

// Parses "yyyy-MM-ddTHH:mm:ssZ" into a UTC time_point.
bool parseExpiry(const std::string& s, std::chrono::system_clock::time_point& out) {
    std::tm tm{};
    int y, mo, d, h, mi, se;
    if (std::sscanf(s.c_str(), "%d-%d-%dT%d:%d:%dZ", &y, &mo, &d, &h, &mi, &se) != 6)
        return false;
    tm.tm_year = y - 1900;
    tm.tm_mon = mo - 1;
    tm.tm_mday = d;
    tm.tm_hour = h;
    tm.tm_min = mi;
    tm.tm_sec = se;
#if defined(_WIN32)
    std::time_t tt = _mkgmtime(&tm);
#else
    std::time_t tt = timegm(&tm);
#endif
    if (tt == static_cast<std::time_t>(-1)) return false;
    out = std::chrono::system_clock::from_time_t(tt);
    return true;
}

}  // namespace

EcfTokenManager::EcfTokenManager(std::shared_ptr<EcfXmlSigner> signer,
                                 EcfEnvironmentConfig config, std::string rncEmisor)
    : signer_(std::move(signer)),
      config_(std::move(config)),
      rncEmisor_(std::move(rncEmisor)) {
    if (!signer_) throw std::invalid_argument("signer");
    if (rncEmisor_.empty()) throw std::invalid_argument("rncEmisor");
}

std::string EcfTokenManager::getToken() {
    using namespace std::chrono;
    auto valid = [&] {
        return !cachedToken_.empty() &&
               duration_cast<minutes>(tokenExpiry_ - system_clock::now()).count() > 5;
    };

    if (valid()) return cachedToken_;

    std::lock_guard<std::mutex> lock(renewMutex_);
    if (valid()) return cachedToken_;

    renewToken();
    return cachedToken_;
}

void EcfTokenManager::renewToken() {
    // 1. Request the seed.
    auto seedResp = cpr::Get(cpr::Url{config_.autenticacionUrl + "/api/autenticacion/semilla"});
    if (seedResp.status_code == 0)
        throw EcfException("No se pudo obtener la semilla: " + seedResp.error.message);
    const std::string semillaXml = seedResp.text;

    // 2. Sign the seed.
    const std::string semillaFirmada = signer_->signXml(semillaXml, rncEmisor_);

    // 3. Validate the signed seed (multipart form-data, field "xml").
    auto validateResp = cpr::Post(
        cpr::Url{config_.autenticacionUrl + "/api/autenticacion/validarsemilla"},
        cpr::Multipart{{"xml", cpr::Buffer{semillaFirmada.begin(), semillaFirmada.end(),
                                           "semilla.xml"}}});
    if (validateResp.status_code < 200 || validateResp.status_code >= 300)
        throw EcfException("Fallo la validación de la semilla (HTTP " +
                           std::to_string(validateResp.status_code) + ").");

    // 4. Extract token + expiry.
    xmlDocPtr doc = xmlReadMemory(validateResp.text.c_str(),
                                  static_cast<int>(validateResp.text.size()),
                                  "auth.xml", nullptr, 0);
    if (!doc) throw EcfException("Respuesta de autenticación inválida.");
    xmlNode* root = xmlDocGetRootElement(doc);
    std::string token = childText(root, "token");
    std::string expira = childText(root, "expira");
    xmlFreeDoc(doc);

    if (token.empty())
        throw EcfException(
            "Respuesta de autenticación inválida: falta token o fecha de expiración.");

    cachedToken_ = token;
    if (!parseExpiry(expira, tokenExpiry_)) {
        tokenExpiry_ = std::chrono::system_clock::now() + std::chrono::hours(1);
    }
}

}  // namespace ecf::infra
