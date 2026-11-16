#pragma once

#include <memory>

#include <drogon/HttpRequest.h>
#include <json/json.h>

#include "Application/Auth/AuthDtos.h"
#include "Application/Customers/CustomerDtos.h"
#include "Domain/Entities/ResponseModels.h"
#include "Domain/Entities/Rfce.h"
#include "Domain/Interfaces/ISecurity.h"
#include "Application/Ecf/CanonicalDocumentDto.h"

namespace ecf::api::mapping {

Json::Value toJson(const domain::EcfRecepcionResponse& r);
Json::Value toJson(const domain::RfceRecepcionResponse& r);
Json::Value toJson(const domain::ConsultaEstadoResponse& r);
Json::Value toJson(const domain::AprobacionComercialResponse& r);
Json::Value toJson(const domain::DirectorioContribuyente& d);
Json::Value toJson(const app::AuthResponseDto& d);
Json::Value toJson(const app::CustomerDto& d);
Json::Value toJson(const app::CanonicalDocumentDto& d);

domain::Rfce rfceFromJson(const Json::Value& j);
app::CanonicalDocumentDto canonicalDocumentFromJson(const Json::Value& j);

std::shared_ptr<domain::ICurrentUserService> currentUserFrom(
    const drogon::HttpRequestPtr& req);

}
