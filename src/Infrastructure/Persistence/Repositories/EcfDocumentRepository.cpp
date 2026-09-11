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

std::optional<EcfDocument> EcfDocumentRepository::getBySourceTxnId(const std::string& tenantId,
                                                                  const std::string& sourceTxnId) {
    pqxx::nontransaction n(db_->connection());
    pqxx::result r = n.exec_params(
        std::string("SELECT ") + ecfDocumentColumns() +
        " FROM ecf_documents WHERE tenant_id = $1 AND source_txn_id = $2 AND is_deleted = false",
        tenantId, sourceTxnId);
    if (r.empty()) return std::nullopt;
    return mapEcfDocument(r[0]);
}

std::vector<EcfDocument> EcfDocumentRepository::getDueForStatusCheck(
    const std::string& minAgeCutoffIso, const std::string& pollDueCutoffIso, int limit) {
    pqxx::nontransaction n(db_->connection());
    pqxx::result r = n.exec_params(
        std::string("SELECT ") + ecfDocumentColumns() +
        " FROM ecf_documents WHERE (state = 'Signed' OR state = 'SentToDgii') "
        " AND sent_to_dgii_at IS NOT NULL AND sent_to_dgii_at <= $1 "
        " AND (last_status_check_at IS NULL OR last_status_check_at <= $2) "
        " AND is_deleted = false ORDER BY sent_to_dgii_at ASC LIMIT $3",
        minAgeCutoffIso, pollDueCutoffIso, limit);
    std::vector<EcfDocument> out;
    out.reserve(r.size());
    for (const auto& row : r) out.push_back(mapEcfDocument(row));
    return out;
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
            "INSERT INTO ecf_documents (id, e_ncf, rnc_emisor, rnc_comprador, tenant_id, "
            "source_txn_id, edit_sequence, document_kind, ncf, track_id, state, total_amount, itbis_amount, "
            "security_code, xml_content, signed_xml_content, dgii_response_xml, receipt_date, "
            "sent_to_dgii_at, last_status_check_at, status_check_attempts, "
            "created_at, created_by, is_deleted) "
            "VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13,$14,$15,$16,$17,$18,$19,$20,$21,$22,$23,$24)",
            e.id, e.eNcf, e.rncEmisor, e.rncComprador, e.tenantId, e.sourceTxnId,
            e.editSequence, e.documentKind, e.ncf, e.trackId, e.state, e.totalAmount, e.itbisAmount,
            e.securityCode, e.xmlContent, e.signedXmlContent, e.dgiiResponseXml, e.receiptDate,
            e.sentToDgiiAt, e.lastStatusCheckAt, e.statusCheckAttempts,
            e.createdAt, by, e.isDeleted);
    });
}

void EcfDocumentRepository::update(const EcfDocument& document) {
    EcfDocument e = document;
    const std::string at = sys::utcNowIso();
    const std::string by = db_->auditUsername();
    db_->stage([e, at, by](pqxx::work& w) {
        w.exec_params(
            "UPDATE ecf_documents SET e_ncf = $2, rnc_emisor = $3, rnc_comprador = $4, "
            "tenant_id = $5, source_txn_id = $6, edit_sequence = $7, document_kind = $8, ncf = $9, track_id = $10, "
            "state = $11, total_amount = $12, itbis_amount = $13, security_code = $14, "
            "xml_content = $15, signed_xml_content = $16, dgii_response_xml = $17, "
            "receipt_date = $18, sent_to_dgii_at = $19, last_status_check_at = $20, status_check_attempts = $21, "
            "updated_at = $22, updated_by = $23 WHERE id = $1",
            e.id, e.eNcf, e.rncEmisor, e.rncComprador, e.tenantId, e.sourceTxnId,
            e.editSequence, e.documentKind, e.ncf, e.trackId, e.state, e.totalAmount, e.itbisAmount,
            e.securityCode, e.xmlContent, e.signedXmlContent, e.dgiiResponseXml, e.receiptDate,
            e.sentToDgiiAt, e.lastStatusCheckAt, e.statusCheckAttempts,
            at, by);
    });
}

}  // namespace ecf::infra
