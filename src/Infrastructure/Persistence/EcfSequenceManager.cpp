#include "Infrastructure/Persistence/EcfSequenceManager.h"
#include <algorithm>
#include <cctype>
#include <pqxx/pqxx>
#include <stdexcept>
#include "Shared/Common/Sys.h"
#include "Infrastructure/Persistence/RowMappers.h"

namespace ecf::infra {

std::string EcfSequenceManager::getNextEncf(const std::string& tenantId, const std::string& tipoComprobante) {
    pqxx::connection conn(connectionString_);
    pqxx::work w(conn);

    // Fetch existing sequence record with FOR UPDATE lock to avoid race conditions
    pqxx::result r = w.exec_params(
        "SELECT " + std::string(ecfSequenceColumns()) + 
        " FROM ecf_sequences WHERE tenant_id = $1 AND tipo_comprobante = $2 AND is_active = true FOR UPDATE",
        tenantId, tipoComprobante
    );

    domain::EcfSequence seq;
    if (r.empty()) {
        // Fallback check between PreCertificacion and TestEcf, or Certificacion and CertEcf
        auto endsWithNoCase = [](const std::string& str, const std::string& suffix) {
            if (str.size() < suffix.size()) return false;
            return std::equal(suffix.rbegin(), suffix.rend(), str.rbegin(),
                              [](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); });
        };

        std::string altTenantId;
        const std::string sPre = ":PreCertificacion";
        const std::string sTest = ":TestEcf";
        const std::string sCert = ":Certificacion";
        const std::string sCertEcf = ":CertEcf";

        if (endsWithNoCase(tenantId, sPre)) {
            altTenantId = tenantId.substr(0, tenantId.size() - sPre.size()) + sTest;
        } else if (endsWithNoCase(tenantId, sTest)) {
            altTenantId = tenantId.substr(0, tenantId.size() - sTest.size()) + sPre;
        } else if (endsWithNoCase(tenantId, sCert)) {
            altTenantId = tenantId.substr(0, tenantId.size() - sCert.size()) + sCertEcf;
        } else if (endsWithNoCase(tenantId, sCertEcf)) {
            altTenantId = tenantId.substr(0, tenantId.size() - sCertEcf.size()) + sCert;
        }

        if (!altTenantId.empty()) {
            pqxx::result r_alt = w.exec_params(
                "SELECT " + std::string(ecfSequenceColumns()) + 
                " FROM ecf_sequences WHERE tenant_id = $1 AND tipo_comprobante = $2 AND is_active = true FOR UPDATE",
                altTenantId, tipoComprobante
            );
            if (!r_alt.empty()) {
                seq = mapEcfSequence(r_alt[0]);
                seq.tenantId = tenantId;
                w.exec_params("UPDATE ecf_sequences SET tenant_id = $1 WHERE id = $2", seq.tenantId, seq.id);
            }
        }
    }

    if (seq.id == 0 && r.empty()) {
        throw std::runtime_error("No existe un rango de secuencias eNCF activo y autorizado para el tipo '" + 
                                 tipoComprobante + "' en el ámbito '" + tenantId + "'. Debe registrar el rango otorgado por la DGII antes de emitir.");
    } else if (seq.id == 0 && !r.empty()) {
        seq = mapEcfSequence(r[0]);
    }

    if (!seq.fechaVencimiento.has_value() || seq.fechaVencimiento->empty()) {
        throw std::runtime_error("La secuencia eNCF para el tipo '" + tipoComprobante + "' no tiene fecha de vencimiento configurada.");
    }

    if (seq.fechaVencimiento.value() < sys::utcNowIso()) {
        throw std::runtime_error("El rango autorizado eNCF para el tipo '" + tipoComprobante + "' ha expirado el " + seq.fechaVencimiento.value() + ".");
    }

    if (seq.secuenciaActual < seq.rangoDesde - 1) {
        seq.secuenciaActual = seq.rangoDesde - 1;
    }

    if (seq.secuenciaActual >= seq.rangoHasta) {
        throw std::runtime_error("Rango de secuencia eNCF agotado para el tipo '" + tipoComprobante + "'.");
    }

    seq.secuenciaActual++;
    seq.updatedAt = sys::utcNowIso();

    w.exec_params(
        "UPDATE ecf_sequences SET secuencia_actual = $1, updated_at = $2 WHERE id = $3",
        seq.secuenciaActual, seq.updatedAt, seq.id
    );

    w.commit();

    return seq.getCurrentEncfFormatted();
}

void EcfSequenceManager::releaseUnusedEncf(const std::string& tenantId, const std::string& tipoComprobante, const std::string& encf) {
    if (encf.size() <= 3) return;
    try {
        std::string numPart = encf.substr(3);
        long long seqNum = std::stoll(numPart);

        pqxx::connection conn(connectionString_);
        pqxx::work w(conn);

        std::string altTenantId;
        auto pos = tenantId.find(':');
        if (pos != std::string::npos) {
            altTenantId = tenantId.substr(0, pos);
        }

        if (!altTenantId.empty()) {
            w.exec_params(
                "UPDATE ecf_sequences SET secuencia_actual = secuencia_actual - 1, updated_at = NOW() "
                "WHERE (tenant_id = $1 OR tenant_id = $2) AND tipo_comprobante = $3 AND secuencia_actual = $4",
                tenantId, altTenantId, tipoComprobante, seqNum
            );
        } else {
            w.exec_params(
                "UPDATE ecf_sequences SET secuencia_actual = secuencia_actual - 1, updated_at = NOW() "
                "WHERE tenant_id = $1 AND tipo_comprobante = $2 AND secuencia_actual = $3",
                tenantId, tipoComprobante, seqNum
            );
        }
        w.commit();
    } catch (...) {
        // Best-effort release to avoid leaving unused sequence gaps
    }
}

} // namespace ecf::infra
