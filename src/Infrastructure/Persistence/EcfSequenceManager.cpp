#include "Infrastructure/Persistence/EcfSequenceManager.h"
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
        // Provision a new sequence
        std::string prefix = tipoComprobante;
        if (!prefix.empty() && prefix[0] != 'E' && prefix[0] != 'e') {
            prefix = "E" + prefix;
        }
        seq.tenantId = tenantId;
        seq.tipoComprobante = tipoComprobante;
        seq.prefix = prefix;
        seq.rangoDesde = 1;
        seq.rangoHasta = 9999999999LL;
        seq.secuenciaActual = 0;
        seq.isActive = true;
        seq.updatedAt = sys::utcNowIso();

        w.exec_params(
            "INSERT INTO ecf_sequences (tenant_id, tipo_comprobante, prefix, rango_desde, rango_hasta, "
            "secuencia_actual, fecha_vencimiento, is_active, updated_at) "
            "VALUES ($1, $2, $3, $4, $5, $6, NULL, $7, $8)",
            seq.tenantId, seq.tipoComprobante, seq.prefix, seq.rangoDesde, seq.rangoHasta,
            seq.secuenciaActual, seq.isActive, seq.updatedAt
        );

        // Fetch back to get the database-assigned SERIAL id
        pqxx::result r_new = w.exec_params(
            "SELECT " + std::string(ecfSequenceColumns()) + 
            " FROM ecf_sequences WHERE tenant_id = $1 AND tipo_comprobante = $2 AND is_active = true",
            tenantId, tipoComprobante
        );
        if (r_new.empty()) {
            throw std::runtime_error("Fallo al crear la secuencia eNCF para: " + tipoComprobante);
        }
        seq = mapEcfSequence(r_new[0]);
    } else {
        seq = mapEcfSequence(r[0]);
    }

    if (seq.fechaVencimiento && !seq.fechaVencimiento->empty()) {
        // Check date expiration
        if (seq.fechaVencimiento.value() < sys::utcNowIso()) {
            throw std::runtime_error("El rango autorizado eNCF para el tipo '" + tipoComprobante + "' ha expirado.");
        }
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

    return seq.getNextEncfFormatted();
}

} // namespace ecf::infra
