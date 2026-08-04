#include "Api/Controllers/DocumentsController.h"
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include "Api/AppServices.h"
#include "Api/JsonMapping.h"
#include "Api/Security/IdempotencyHandler.h"
#include "Shared/Common/Sys.h"
#include "Infrastructure/Persistence/RowMappers.h"

using namespace drogon;
using namespace ecf::infra;

namespace ecf::api {

namespace {
HttpResponsePtr json(const Json::Value& body, HttpStatusCode code) {
    auto resp = HttpResponse::newHttpJsonResponse(body);
    resp->setStatusCode(code);
    return resp;
}
Json::Value err(const std::string& message) {
    Json::Value v;
    v["error"] = message;
    return v;
}

std::string buildXmlFromCanonical(const app::CanonicalDocumentDto& dto, const std::string& eNcf) {
    std::ostringstream ss;
    ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
       << "<ECF xmlns=\"http://dgii.gov.do/ecf/messages/v1\">\n"
       << "  <Encabezado>\n"
       << "    <IdDoc><eNCF>" << eNcf << "</eNCF><TipoIngreso>01</TipoIngreso></IdDoc>\n"
       << "    <Emisor><RNCEmisor>" << dto.header.rncEmisor << "</RNCEmisor><RazonSocial>" << dto.header.razonSocialEmisor << "</RazonSocial></Emisor>\n"
       << "    <Comprador><RNCComprador>" << dto.header.rncComprador << "</RNCComprador><RazonSocial>" << dto.header.razonSocialComprador << "</RazonSocial></Comprador>\n";
    ss << std::fixed << std::setprecision(2);
    ss << "    <Totales><MontoTotal>" << dto.totals.montoTotal << "</MontoTotal><MontoItbis>" << dto.totals.montoItbis << "</MontoItbis></Totales>\n"
       << "  </Encabezado>\n"
       << "</ECF>\n";
    return ss.str();
}
} // namespace

void DocumentsController::submit(const HttpRequestPtr& req,
                                 std::function<void(const HttpResponsePtr&)>&& callback) {
    std::string tenantId = "default-tenant";
    if (req->attributes()->find("tenantId")) {
        tenantId = req->attributes()->get<std::string>("tenantId");
    }
    std::string workerKeyId = "default-worker";
    if (req->attributes()->find("workerKeyId")) {
        workerKeyId = req->attributes()->get<std::string>("workerKeyId");
    }

    auto& services = AppServices::instance();
    IdempotencyHandler::handle(
        services.idempotencyStore(),
        req,
        tenantId,
        workerKeyId,
        std::move(callback),
        [req, tenantId, &services](std::function<void(const HttpResponsePtr&)>&& cb) {
            auto body = req->getJsonObject();
            if (!body) {
                cb(json(err("Invalid JSON body."), k400BadRequest));
                return;
            }

            app::CanonicalDocumentDto dto;
            try {
                dto = mapping::canonicalDocumentFromJson(*body);
            } catch (const std::exception& ex) {
                cb(json(err(std::string("JSON parsing error: ") + ex.what()), k400BadRequest));
                return;
            }

            if (dto.sourceReference.txnId.empty()) {
                cb(json(err("SourceReference.TxnId is required."), k400BadRequest));
                return;
            }

            auto scope = services.makeScope(mapping::currentUserFrom(req));

            // Check if document for this TxnId has already been processed
            try {
                pqxx::nontransaction nt(scope.db->connection());
                pqxx::result res = nt.exec_params(
                    "SELECT " + std::string(ecfDocumentColumns()) + " FROM ecf_documents WHERE tenant_id = $1 AND source_txn_id = $2 AND is_deleted = false",
                    tenantId, dto.sourceReference.txnId
                );

                if (!res.empty()) {
                    domain::EcfDocument existingDoc = mapEcfDocument(res[0]);
                    Json::Value out;
                    out["documentId"] = existingDoc.id;
                    out["eNcf"] = existingDoc.eNcf;
                    out["state"] = existingDoc.state;
                    out["trackId"] = existingDoc.trackId.has_value() ? *existingDoc.trackId : "";
                    out["securityCode"] = existingDoc.securityCode.has_value() ? *existingDoc.securityCode : "";
                    cb(json(out, k202Accepted));
                    return;
                }
            } catch (const std::exception& ex) {
                cb(json(err(std::string("Database query error: ") + ex.what()), k500InternalServerError));
                return;
            }

            // 1. Allocate eNCF Sequence
            std::string eNcf;
            try {
                eNcf = services.sequenceManager()->getNextEncf(tenantId, dto.tipoComprobante);
            } catch (const std::exception& ex) {
                cb(json(err(ex.what()), k400BadRequest));
                return;
            }

            // 2. Build Fiscal XML Content
            std::string rawXml = buildXmlFromCanonical(dto, eNcf);

            // 3. Compute Security Code (6-char Base64 hash prefix)
            std::ostringstream ssSec;
            ssSec << std::fixed << std::setprecision(2) << dto.totals.montoTotal;
            std::string inputSec = eNcf + ":" + ssSec.str() + ":" + dto.header.rncEmisor;

            std::vector<unsigned char> sha256Bytes(32);
            unsigned int len = 0;
            EVP_MD_CTX* ctx = EVP_MD_CTX_new();
            EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr);
            EVP_DigestUpdate(ctx, inputSec.c_str(), inputSec.size());
            EVP_DigestFinal_ex(ctx, sha256Bytes.data(), &len);
            EVP_MD_CTX_free(ctx);

            BIO* b64 = BIO_new(BIO_f_base64());
            BIO* mem = BIO_new(BIO_s_mem());
            BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
            BIO_push(b64, mem);
            BIO_write(b64, sha256Bytes.data(), len);
            (void)BIO_flush(b64);
            char* b64Data = nullptr;
            long b64Len = BIO_get_mem_data(mem, &b64Data);
            std::string b64Str(b64Data, b64Len);
            BIO_free_all(b64);

            std::string secCode = b64Str.substr(0, 6);
            std::transform(secCode.begin(), secCode.end(), secCode.begin(), ::toupper);

            domain::EcfDocument doc;
            doc.id = sys::newUuid();
            doc.tenantId = tenantId;
            doc.sourceTxnId = dto.sourceReference.txnId;
            doc.documentKind = dto.documentKind;
            if (dto.ncf) doc.ncf = *dto.ncf;
            doc.eNcf = eNcf;
            doc.rncEmisor = dto.header.rncEmisor;
            doc.rncComprador = dto.header.rncComprador;
            doc.totalAmount = dto.totals.montoTotal;
            doc.itbisAmount = dto.totals.montoItbis;
            doc.securityCode = secCode;
            doc.xmlContent = rawXml;
            doc.state = "SequenceAllocated";

            try {
                scope.docs->add(doc);
                scope.uow->saveChanges();

                // 4. Mark Signed & Submit
                doc.signedXmlContent = rawXml;
                doc.state = "Signed";
                scope.docs->update(doc);
                scope.uow->saveChanges();

                std::string fileName = doc.rncEmisor + doc.eNcf + ".xml";
                auto response = services.ecfClient()->sendEcf(doc.signedXmlContent.value(), fileName);
                if (!response.trackId.empty()) {
                    doc.trackId = response.trackId;
                    doc.state = "SentToDgii";
                } else {
                    doc.state = "RejectedByDgii";
                }
            } catch (...) {
                doc.state = "Uncertain";
            }

            try {
                scope.docs->update(doc);
                scope.uow->saveChanges();
            } catch (...) {}

            Json::Value out;
            out["documentId"] = doc.id;
            out["eNcf"] = doc.eNcf;
            out["state"] = doc.state;
            out["trackId"] = doc.trackId.has_value() ? *doc.trackId : "";
            out["securityCode"] = doc.securityCode.has_value() ? *doc.securityCode : "";
            cb(json(out, k202Accepted));
        }
    );
}

void DocumentsController::getBySourceTxnId(const HttpRequestPtr& req,
                                          std::function<void(const HttpResponsePtr&)>&& callback,
                                          std::string txnId) {
    std::string tenantId = "default-tenant";
    if (req->attributes()->find("tenantId")) {
        tenantId = req->attributes()->get<std::string>("tenantId");
    }

    try {
        auto& services = AppServices::instance();
        auto scope = services.makeScope(mapping::currentUserFrom(req));

        pqxx::nontransaction nt(scope.db->connection());
        pqxx::result res = nt.exec_params(
            "SELECT " + std::string(ecfDocumentColumns()) + " FROM ecf_documents WHERE tenant_id = $1 AND source_txn_id = $2 AND is_deleted = false",
            tenantId, txnId
        );

        if (res.empty()) {
            Json::Value errBody;
            errBody["error"] = "Document with source TxnId '" + txnId + "' not found.";
            callback(json(errBody, k404NotFound));
            return;
        }

        domain::EcfDocument doc = mapEcfDocument(res[0]);

        Json::Value out;
        out["documentId"] = doc.id;
        if (doc.ncf) out["ncf"] = *doc.ncf;
        out["eNcf"] = doc.eNcf;
        out["state"] = doc.state;
        out["trackId"] = doc.trackId.has_value() ? *doc.trackId : "";
        out["securityCode"] = doc.securityCode.has_value() ? *doc.securityCode : "";
        out["receiptDate"] = doc.receiptDate.has_value() ? *doc.receiptDate : "";

        callback(json(out, k200OK));
    } catch (const std::exception& ex) {
        Json::Value errBody;
        errBody["error"] = ex.what();
        callback(json(errBody, k500InternalServerError));
    }
}

} // namespace ecf::api
