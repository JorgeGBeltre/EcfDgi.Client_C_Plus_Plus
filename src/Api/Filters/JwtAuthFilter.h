#pragma once
// Drogon filter equivalent to [Authorize]: validates the Bearer JWT and stashes
// the user claims (userId/username/role) on the request for downstream use.

#include <drogon/HttpFilter.h>

namespace ecf::api {

class JwtAuthFilter : public drogon::HttpFilter<JwtAuthFilter> {
public:
    void doFilter(const drogon::HttpRequestPtr& req,
                  drogon::FilterCallback&& fcb,
                  drogon::FilterChainCallback&& fccb) override;
};

}  // namespace ecf::api
