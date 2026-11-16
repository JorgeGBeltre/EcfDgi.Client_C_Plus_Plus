#pragma once

#include <memory>
#include <optional>
#include <string>

#include "Domain/Entities/EcfClientOptions.h"
#include "Domain/Entities/ResponseModels.h"
#include "Domain/Interfaces/IEcfTransport.h"

namespace ecf::infra {

class EcfFrontendClient {
public:
    explicit EcfFrontendClient(domain::EcfClientOptions options = {},
                               std::shared_ptr<domain::IEcfTransport> transport = nullptr);

    domain::ConsultaEstadoResponse consultarEstado(
        const std::string& rncEmisor, const std::string& eNcf,
        const std::optional<std::string>& rncComprador = std::nullopt,
        const std::optional<std::string>& codigoSeguridad = std::nullopt);

    domain::RfceConsultaResponse consultarRfce(const std::string& rncEmisor,
                                               const std::string& eNcf,
                                               const std::string& codigoSeguridad);

    domain::TimbreResponse validarTimbreEcf(const domain::TimbreEcfRequest& request);
    domain::TimbreFcResponse validarTimbreFc(const domain::TimbreFcRequest& request);

private:
    std::shared_ptr<domain::IEcfTransport> transport_;
};

}
