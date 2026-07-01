#include "Api/Filters/JwtAuthFilter.h"

#include <jwt-cpp/jwt.h>

#include <string>

#include "Api/AppServices.h"

using namespace drogon;

namespace ecf::api {

namespace {
HttpResponsePtr unauthorized(const std::string& detail) {
    Json::Value body;
    body["title"] = "Unauthorized";
    body["status"] = 401;
    body["detail"] = detail;
    auto resp = HttpResponse::newHttpJsonResponse(body);
    resp->setStatusCode(k401Unauthorized);
    resp->setContentTypeString("application/problem+json");
    return resp;
}
}  // namespace

void JwtAuthFilter::doFilter(const HttpRequestPtr& req, FilterCallback&& fcb,
                             FilterChainCallback&& fccb) {
    const std::string& auth = req->getHeader("authorization");
    const std::string prefix = "Bearer ";
    if (auth.size() <= prefix.size() || auth.compare(0, prefix.size(), prefix) != 0) {
        fcb(unauthorized("Missing or malformed Authorization header."));
        return;
    }

    const std::string token = auth.substr(prefix.size());
    const auto& jwtCfg = AppServices::instance().config().jwt;

    try {
        auto decoded = jwt::decode(token);
        auto verifier = jwt::verify()
                            .allow_algorithm(jwt::algorithm::hs256{jwtCfg.secret})
                            .with_issuer(jwtCfg.issuer)
                            .with_audience(jwtCfg.audience);
        verifier.verify(decoded);

        auto getClaim = [&](const char* name) -> std::string {
            return decoded.has_payload_claim(name)
                       ? decoded.get_payload_claim(name).as_string()
                       : std::string{};
        };

        req->attributes()->insert("userId", getClaim("nameid"));
        req->attributes()->insert("username", getClaim("name"));
        req->attributes()->insert("role", getClaim("role"));
    } catch (const std::exception& ex) {
        fcb(unauthorized(std::string("Invalid token: ") + ex.what()));
        return;
    }

    fccb();
}

}  // namespace ecf::api
