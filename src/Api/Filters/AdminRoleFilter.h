#pragma once
// Drogon filter equivalent to [Authorize(Roles = "Admin")]. Runs after
// JwtAuthFilter and checks the "role" claim it stashed.

#include <drogon/HttpFilter.h>

namespace ecf::api {

class AdminRoleFilter : public drogon::HttpFilter<AdminRoleFilter> {
public:
    void doFilter(const drogon::HttpRequestPtr& req,
                  drogon::FilterCallback&& fcb,
                  drogon::FilterChainCallback&& fccb) override;
};

}  // namespace ecf::api
