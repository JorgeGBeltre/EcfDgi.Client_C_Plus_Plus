#pragma once
// Talks to the live DGII REST endpoints (bearer token from EcfTokenManager).

#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <cpr/cpr.h>

#include "Domain/Exceptions/EcfException.h"
#include "Domain/Interfaces/IEcfTransport.h"
#include "Infrastructure/Dgii/EcfEnvironmentConfig.h"
#include "Infrastructure/Dgii/EcfTokenManager.h"
#include "Infrastructure/Serialization/EcfXmlSerializer.h"

namespace ecf::infra {

// PERF-001: Explicit timeouts for DGII communications (prevent unconfigured blocking I/O)
constexpr std::chrono::milliseconds kDefaultConnectTimeout{5000};
constexpr std::chrono::milliseconds kDefaultReadTimeout{15000};
constexpr std::chrono::milliseconds kReceptionTimeout{30000};

class DgiiDirectTransport : public domain::IEcfTransport {
public:
    DgiiDirectTransport(std::shared_ptr<EcfTokenManager> tokenManager,
                        EcfEnvironmentConfig config);

    domain::EcfRecepcionResponse sendEcf(const std::string& xmlContent,
                                         const std::string& fileName) override;
    domain::RfceRecepcionResponse sendRfce(const std::string& xmlContent,
                                           const std::string& fileName) override;
    domain::ConsultaResultadoResponse consultarResultado(const std::string& trackId) override;
    domain::ConsultaEstadoResponse consultarEstado(
        const domain::ConsultaEstadoRequest& req) override;
    std::vector<domain::TrackIdDetalle> consultarTrackIds(const std::string& rncEmisor,
                                                          const std::string& eNcf) override;
    domain::RfceConsultaResponse consultarRfce(const std::string& rncEmisor,
                                               const std::string& eNcf,
                                               const std::string& codigoSeguridad) override;
    domain::AprobacionComercialResponse sendAprobacionComercial(
        const std::string& xmlContent, const std::string& fileName) override;
    domain::AnulacionResponse anularRangos(const std::string& xmlContent) override;
    std::vector<domain::DirectorioContribuyente> consultarDirectorio() override;
    domain::DirectorioContribuyente consultarDirectorioPorRnc(const std::string& rnc) override;
    domain::TimbreResponse consultarTimbre(const domain::TimbreEcfRequest& req) override;
    domain::TimbreFcResponse consultarTimbreFc(const domain::TimbreFcRequest& req) override;
    std::vector<domain::EstatusServicio> consultarEstatusServicios() override;
    std::vector<domain::VentanaMantenimiento> consultarVentanasMantenimiento() override;
    std::string verificarEstadoAmbiente(domain::AmbienteEnum ambiente) override;

    // BUG-014: Validates that response did not suffer transport/network failure or 5xx server crash
    static void validateDgiiResponse(const cpr::Response& resp) {
        if (resp.error.code != cpr::ErrorCode::OK) {
            throw domain::EcfException("Fallo de red/transporte con DGII: " + resp.error.message +
                                       " (codigo " + std::to_string(static_cast<int>(resp.error.code)) + ")");
        }
        if (resp.status_code == 0) {
            throw domain::EcfException("No se pudo conectar con DGII (HTTP 0)");
        }
        if (resp.status_code >= 500) {
            throw domain::EcfException("Fallo en servidor DGII (HTTP " + std::to_string(resp.status_code) + "): " + resp.text);
        }
        if (resp.status_code == 401 || resp.status_code == 429) {
            throw domain::EcfException("Petición rechazada por DGII (HTTP " + std::to_string(resp.status_code) + "): " + resp.text);
        }
    }

private:
    // Fetches the bearer token; throws if no token manager was supplied
    // (e.g. EcfFrontendClient built without a signing certificate).
    std::string getAuthToken();

    template <typename RequestFn>
    cpr::Response sendWithReactiveAuth(RequestFn&& fn) {
        auto token = getAuthToken();
        auto resp = fn(token);
        if (resp.status_code == 401 && tokenManager_) {
            tokenManager_->invalidate();
            auto freshToken = tokenManager_->getToken();
            resp = fn(freshToken);
        }
        validateDgiiResponse(resp);
        return resp;
    }

    std::shared_ptr<EcfTokenManager> tokenManager_;
    EcfEnvironmentConfig config_;
    EcfXmlSerializer xmlSerializer_;
};

}  // namespace ecf::infra
