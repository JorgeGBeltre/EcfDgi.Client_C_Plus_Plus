#pragma once

#include <drogon/HttpFilter.h>

namespace ecf::api {

class AdminRoleFilter : public drogon::HttpFilter<AdminRoleFilter> {
public:
    void doFilter(const drogon::HttpRequestPtr& req,
                  drogon::FilterCallback&& fcb,
                  drogon::FilterChainCallback&& fccb) override;
};

}
