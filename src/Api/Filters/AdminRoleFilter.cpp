#include "Api/Filters/AdminRoleFilter.h"

using namespace drogon;

namespace ecf::api {

void AdminRoleFilter::doFilter(const HttpRequestPtr& req, FilterCallback&& fcb,
                               FilterChainCallback&& fccb) {
    std::string role;
    if (auto attr = req->attributes(); attr->find("role")) {
        role = attr->get<std::string>("role");
    }

    if (role != "Admin") {
        Json::Value body;
        body["title"] = "Forbidden";
        body["status"] = 403;
        body["detail"] = "Admin role required.";
        auto resp = HttpResponse::newHttpJsonResponse(body);
        resp->setStatusCode(k403Forbidden);
        resp->setContentTypeString("application/problem+json");
        fcb(resp);
        return;
    }

    fccb();
}

}  // namespace ecf::api
