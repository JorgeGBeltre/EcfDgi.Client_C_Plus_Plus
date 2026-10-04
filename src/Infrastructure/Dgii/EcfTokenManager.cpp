#include "Infrastructure/Dgii/EcfTokenManager.h"

#include <cpr/cpr.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <cstdio>
#include <ctime>
#include <random>
#include <sstream>
#include <thread>
#include <nlohmann/json.hpp>

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

// Parses ISO timestamps with optional fractional seconds / milliseconds and timezone offset into a UTC time_point.
bool parseExpiry(const std::string& s, std::chrono::system_clock::time_point& out) {
    if (s.empty()) return false;
    std::tm tm{};
    int y = 0, mo = 0, d = 0, h = 0, mi = 0, se = 0;
    char sep = 'T';
    int n = std::sscanf(s.c_str(), "%d-%d-%d%c%d:%d:%d", &y, &mo, &d, &sep, &h, &mi, &se);
    if (n < 7) return false;

    int ms = 0;
    size_t dotPos = s.find('.', 19);
    size_t tzPos = std::string::npos;
    if (dotPos != std::string::npos) {
        size_t endDigits = dotPos + 1;
        while (endDigits < s.size() && std::isdigit(static_cast<unsigned char>(s[endDigits]))) {
            ++endDigits;
        }
        std::string fracStr = s.substr(dotPos + 1, endDigits - (dotPos + 1));
        if (!fracStr.empty()) {
            while (fracStr.size() < 3) fracStr += '0';
            ms = std::stoi(fracStr.substr(0, 3));
        }
        tzPos = endDigits;
    } else {
        tzPos = 19;
    }

    int tzOffsetSec = 0;
    if (tzPos < s.size()) {
        std::string tzPart = s.substr(tzPos);
        if (!tzPart.empty()) {
            if (tzPart[0] == 'Z' || tzPart[0] == 'z') {
                tzOffsetSec = 0;
            } else if (tzPart[0] == '+' || tzPart[0] == '-') {
                int sign = (tzPart[0] == '-') ? -1 : 1;
                int tzH = 0, tzM = 0;
                if (std::sscanf(tzPart.c_str() + 1, "%d:%d", &tzH, &tzM) >= 1) {
                    tzOffsetSec = sign * (tzH * 3600 + tzM * 60);
                }
            }
        }
    }

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

    tt -= tzOffsetSec;
    out = std::chrono::system_clock::from_time_t(tt) + std::chrono::milliseconds(ms);
    return true;
}

}  // namespace

EcfTokenManager::EcfTokenManager(std::shared_ptr<domain::IEcfXmlSigner> signer,
                                 EcfEnvironmentConfig config, std::string rncEmisor,
                                 std::shared_ptr<domain::ICacheService> cacheService)
    : signer_(std::move(signer)),
      config_(std::move(config)),
      rncEmisor_(std::move(rncEmisor)),
      cacheService_(std::move(cacheService)) {
    if (!signer_) throw std::invalid_argument("signer");
    if (rncEmisor_.empty()) throw std::invalid_argument("rncEmisor");
}

std::string EcfTokenManager::getToken() {
    using namespace std::chrono;
    std::string cacheKey = "ecf:tokens:" + rncEmisor_ + ":" + std::to_string(static_cast<int>(config_.ambiente));

    // RAII guard for distributed lock
    struct DistLockGuard {
        std::shared_ptr<domain::ICacheService> cache;
        std::string key;
        std::string val;
        bool acquired = false;

        ~DistLockGuard() {
            release();
        }

        void release() {
            if (acquired && cache) {
                try {
                    cache->releaseLock(key, val);
                } catch (...) {}
                acquired = false;
            }
        }
    };

    // 1. Check Distributed Cache
    if (cacheService_) {
        if (auto tokenOpt = cacheService_->get(cacheKey)) {
            if (!tokenOpt->empty()) {
                std::lock_guard<std::mutex> tlock(tokenMutex_);
                cachedToken_ = *tokenOpt;
                tokenExpiry_ = system_clock::now() + std::chrono::minutes(50);
                return *tokenOpt;
            }
        }
    }

    // 2. Check Memory Cache (synchronized under tokenMutex_)
    auto validMemoryToken = [&]() -> std::optional<std::string> {
        std::lock_guard<std::mutex> tlock(tokenMutex_);
        if (!cachedToken_.empty() &&
            duration_cast<minutes>(tokenExpiry_ - system_clock::now()).count() > 5) {
            return cachedToken_;
        }
        return std::nullopt;
    };

    if (auto t = validMemoryToken()) return *t;

    // 3. Acquire Distributed / Local Lock
    std::string lockKey = "ecf:tokens:lock:" + rncEmisor_ + ":" + std::to_string(static_cast<int>(config_.ambiente));
    static std::random_device rd;
    static std::mt19937_64 gen(rd());
    static std::uniform_int_distribution<uint64_t> dis;
    std::ostringstream ssLock;
    ssLock << "lock_" << duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count() << "_" << std::hex << dis(gen);
    std::string lockValue = ssLock.str();
    bool acquiredDistLock = false;

    if (cacheService_) {
        acquiredDistLock = cacheService_->acquireLock(lockKey, lockValue, std::chrono::seconds(30));
        if (acquiredDistLock) {
            if (auto tokenOpt = cacheService_->get(cacheKey)) {
                if (!tokenOpt->empty()) {
                    cacheService_->releaseLock(lockKey, lockValue);
                    std::lock_guard<std::mutex> tlock(tokenMutex_);
                    cachedToken_ = *tokenOpt;
                    tokenExpiry_ = system_clock::now() + std::chrono::minutes(50);
                    return *tokenOpt;
                }
            }
        } else {
            // Another instance holds the lock and is renewing the token.
            // Wait and poll the cache instead of renewing simultaneously (BUG-052).
            for (int attempt = 0; attempt < 12; ++attempt) {
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
                if (auto tokenOpt = cacheService_->get(cacheKey)) {
                    if (!tokenOpt->empty()) {
                        std::lock_guard<std::mutex> tlock(tokenMutex_);
                        cachedToken_ = *tokenOpt;
                        tokenExpiry_ = system_clock::now() + std::chrono::minutes(50);
                        return *tokenOpt;
                    }
                }
            }
            // If still not available after 3s, attempt to acquire lock one more time
            acquiredDistLock = cacheService_->acquireLock(lockKey, lockValue, std::chrono::seconds(30));
            if (!acquiredDistLock) {
                if (auto tokenOpt = cacheService_->get(cacheKey)) {
                    if (!tokenOpt->empty()) {
                        std::lock_guard<std::mutex> tlock(tokenMutex_);
                        cachedToken_ = *tokenOpt;
                        tokenExpiry_ = system_clock::now() + std::chrono::minutes(50);
                        return *tokenOpt;
                    }
                }
                throw EcfException("No se pudo obtener el token DGII: bloqueo distribuido retenido por otra instancia.");
            }
        }
    }

    DistLockGuard distGuard{cacheService_, lockKey, lockValue, acquiredDistLock};

    std::lock_guard<std::mutex> lock(renewMutex_);
    if (auto t = validMemoryToken()) {
        distGuard.release();
        return *t;
    }

    renewToken();

    std::string tokenResult;
    std::chrono::seconds ttlSeconds{0};
    {
        std::lock_guard<std::mutex> tlock(tokenMutex_);
        tokenResult = cachedToken_;
        ttlSeconds = duration_cast<seconds>(tokenExpiry_ - system_clock::now());
    }

    if (cacheService_ && !tokenResult.empty() && ttlSeconds.count() > 0) {
        cacheService_->set(cacheKey, tokenResult, ttlSeconds);
    }
    distGuard.release();

    return tokenResult;
}

void EcfTokenManager::invalidate() {
    {
        std::lock_guard<std::mutex> lock(renewMutex_);
        std::lock_guard<std::mutex> tlock(tokenMutex_);
        cachedToken_.clear();
        tokenExpiry_ = {};
    }
    if (cacheService_) {
        std::string cacheKey = "ecf:tokens:" + rncEmisor_ + ":" + std::to_string(static_cast<int>(config_.ambiente));
        cacheService_->remove(cacheKey);
    }
}

void EcfTokenManager::renewToken() {
    // 1. Request the seed.
    auto seedResp = cpr::Get(
        cpr::Url{config_.autenticacionUrl + "/api/autenticacion/semilla"},
        cpr::Timeout{15000},
        cpr::ConnectTimeout{5000});
    if (seedResp.error.code != cpr::ErrorCode::OK || seedResp.status_code == 0)
        throw EcfException("No se pudo obtener la semilla: " + (seedResp.error.message.empty() ? "HTTP 0" : seedResp.error.message));
    const std::string semillaXml = seedResp.text;

    // 2. Sign the seed.
    const std::string semillaFirmada = signer_->signXml(semillaXml, rncEmisor_);

    // 3. Validate the signed seed (multipart form-data, field "xml").
    auto validateResp = cpr::Post(
        cpr::Url{config_.autenticacionUrl + "/api/autenticacion/validarsemilla"},
        cpr::Multipart{{"xml", cpr::Buffer{semillaFirmada.begin(), semillaFirmada.end(),
                                           "semilla.xml"}}},
        cpr::Timeout{15000},
        cpr::ConnectTimeout{5000});
    if (validateResp.error.code != cpr::ErrorCode::OK || validateResp.status_code < 200 || validateResp.status_code >= 300)
        throw EcfException("Fallo la validación de la semilla (HTTP " +
                           std::to_string(validateResp.status_code) + "): " +
                           (validateResp.error.message.empty() ? validateResp.text : validateResp.error.message));

    // 4. Extract token + expiry (supports both XML and JSON formats from DGII / mock endpoints).
    std::string token;
    std::string expira;

    std::string trimmed = validateResp.text;
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front()))) trimmed.erase(trimmed.begin());

    if (!trimmed.empty() && trimmed.front() == '<') {
        xmlDocPtr doc = xmlReadMemory(validateResp.text.c_str(),
                                      static_cast<int>(validateResp.text.size()),
                                      "auth.xml", nullptr, XML_PARSE_NONET);
        if (!doc) throw EcfException("Respuesta de autenticación inválida: XML malformado.");
        xmlNode* root = xmlDocGetRootElement(doc);
        token = childText(root, "token");
        expira = childText(root, "expira");
        xmlFreeDoc(doc);
    } else {
        try {
            auto j = nlohmann::json::parse(validateResp.text);
            if (j.contains("token") && j["token"].is_string()) {
                token = j["token"].get<std::string>();
            }
            if (j.contains("expira") && j["expira"].is_string()) {
                expira = j["expira"].get<std::string>();
            }
        } catch (const std::exception& ex) {
            throw EcfException(std::string("Respuesta de autenticación inválida (JSON/XML): ") + ex.what());
        }
    }

    if (token.empty())
        throw EcfException(
            "Respuesta de autenticación inválida de DGII: falta token o fecha de expiración.");

    {
        std::lock_guard<std::mutex> tlock(tokenMutex_);
        cachedToken_ = token;
        if (!parseExpiry(expira, tokenExpiry_)) {
            tokenExpiry_ = std::chrono::system_clock::now() + std::chrono::hours(1);
        }
    }
}

}  // namespace ecf::infra
