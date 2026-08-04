#pragma once

#include <string>
#include <optional>
#include <sstream>
#include <iomanip>

namespace ecf::domain {

struct EcfSequence {
    int id = 0;
    std::string tenantId = "default-tenant";
    std::string tipoComprobante;
    std::string prefix;
    long long rangoDesde = 1;
    long long rangoHasta = 9999999999;
    long long secuenciaActual = 0;
    std::optional<std::string> fechaVencimiento; // ISO-8601 UTC
    bool isActive = true;
    std::string updatedAt; // ISO-8601 UTC

    std::string getNextEncfFormatted() const {
        std::ostringstream ss;
        ss << prefix << std::setw(10) << std::setfill('0') << (secuenciaActual + 1);
        return ss.str();
    }
};

} // namespace ecf::domain
