#pragma once

#include <drogon/HttpFilter.h>

namespace ecf::api {

class JwtAuthFilter : public drogon::HttpFilter<JwtAuthFilter> {
public:
    void doFilter(const drogon::HttpRequestPtr& req,
                  drogon::FilterCallback&& fcb,
                  drogon::FilterChainCallback&& fccb) override;
};

}
