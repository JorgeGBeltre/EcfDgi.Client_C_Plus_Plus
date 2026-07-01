#include "Infrastructure/Persistence/Repositories/EcfDocumentRepository.h"

#include "Infrastructure/Persistence/RowMappers.h"
#include "Shared/Common/Sys.h"

namespace ecf::infra {

using domain::EcfDocument;

namespace {
std::optional<EcfDocument> queryOne(DbContext& db, const std::string& where,
                                    const std::string& arg) {
    pqxx::nontransaction n(db.connection());
    pqxx::result r = n.exec_params(
        std::string("SELECT ") + ecfDocumentColumns() + " FROM ecf_documents WHERE " +
            where + " AND is_deleted = false",
        arg);
    if (r.empty()) return std::nullopt;
    return mapEcfDocument(r[0]);
}
}  // namespace

std::optional<EcfDocument> EcfDocumentRepository::getById(const std::string& id) {
    return queryOne(*db_, "id = $1", id);
}

std::optional<EcfDocument> EcfDocumentRepository::getByENcf(const std::string& eNcf) {
    return queryOne(*db_, "e_ncf = $1", eNcf);
}

std::optional<EcfDocument> EcfDocumentRepository::getByTrackId(const std::string& trackId) {
    return queryOne(*db_, "track_id = $1", trackId);
}

std::vector<EcfDocument> EcfDocumentRepository::getAll() {
    pqxx::nontransaction n(db_->connection());
    pqxx::result r = n.exec(
        std::string("SELECT ") + ecfDocumentColumns() +
        " FROM ecf_documents WHERE is_deleted = false ORDER BY created_at");
    std::vector<EcfDocument> out;
    out.reserve(r.size());
    for (const auto& row : r) out.push_back(mapEcfDocument(row));
    return out;
}

void EcfDocumentRepository::add(const EcfDocument& document) {
    EcfDocument e = document;
    if (e.id.empty()) e.id = sys::newUuid();
    e.createdAt = sys::utcNowIso();
    const std::string by = db_->auditUsername();
    e.isDeleted = false;
    db_->stage([e, by](pqxx::work& w) {
        w.exec_params(
            "INSERT INTO ecf_documents (id, e_ncf, rnc_emisor, rnc_comprador, track_id, "
            "state, total_amount, itbis_amount, security_code, xml_content, receipt_date, "
            "created_at, created_by, is_deleted) "
            "VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13,$14)",
            e.id, e.eNcf, e.rncEmisor, e.rncComprador, e.trackId, e.state, e.totalAmount,
            e.itbisAmount, e.securityCode, e.xmlContent, e.receiptDate, e.createdAt, by,
            e.isDeleted);
    });
}

void EcfDocumentRepository::update(const EcfDocument& document) {
    EcfDocument e = document;
    const std::string at = sys::utcNowIso();
    const std::string by = db_->auditUsername();
    db_->stage([e, at, by](pqxx::work& w) {
        w.exec_params(
            "UPDATE ecf_documents SET e_ncf = $2, rnc_emisor = $3, rnc_comprador = $4, "
            "track_id = $5, state = $6, total_amount = $7, itbis_amount = $8, "
            "security_code = $9, xml_content = $10, receipt_date = $11, "
            "updated_at = $12, updated_by = $13 WHERE id = $1",
            e.id, e.eNcf, e.rncEmisor, e.rncComprador, e.trackId, e.state, e.totalAmount,
            e.itbisAmount, e.securityCode, e.xmlContent, e.receiptDate, at, by);
    });
}

}  // namespace ecf::infra
