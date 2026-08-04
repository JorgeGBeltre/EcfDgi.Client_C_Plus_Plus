#pragma once

#include <drogon/HttpFilter.h>

namespace ecf::api {

class UserOrWorkerFilter : public drogon::HttpFilter<UserOrWorkerFilter> {
public:
    void doFilter(const drogon::HttpRequestPtr& req,
                  drogon::FilterCallback&& fcb,
                  drogon::FilterChainCallback&& fccb) override;
};

} // namespace ecf::api
