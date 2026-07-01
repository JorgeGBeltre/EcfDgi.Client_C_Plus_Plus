#pragma once
// Conversions between domain models/DTOs and jsoncpp (Drogon's JSON type),
// plus a helper to build a CurrentUserService from a request's JWT claims.

#include <memory>

#include <drogon/HttpRequest.h>
#include <json/json.h>

#include "Application/Auth/AuthDtos.h"
#include "Application/Customers/CustomerDtos.h"
#include "Domain/Entities/ResponseModels.h"
#include "Domain/Entities/Rfce.h"
#include "Domain/Interfaces/ISecurity.h"

namespace ecf::api::mapping {

Json::Value toJson(const domain::EcfRecepcionResponse& r);
Json::Value toJson(const domain::RfceRecepcionResponse& r);
Json::Value toJson(const domain::ConsultaEstadoResponse& r);
Json::Value toJson(const app::AuthResponseDto& d);
Json::Value toJson(const app::CustomerDto& d);

domain::Rfce rfceFromJson(const Json::Value& j);

std::shared_ptr<domain::ICurrentUserService> currentUserFrom(
    const drogon::HttpRequestPtr& req);

}  // namespace ecf::api::mapping
