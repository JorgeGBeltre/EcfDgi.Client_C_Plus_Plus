#include "Api/Controllers/DocumentsController.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include <drogon/utils/Utilities.h>

#include "Api/AppServices.h"
#include "Api/JsonMapping.h"
#include "Api/Security/IdempotencyHandler.h"
#include "Application/Ecf/CanonicalXmlBuilder.h"
#include "Infrastructure/Dgii/EcfEnvironmentConfig.h"
#include "Infrastructure/EcfClient.h"
#include "Infrastructure/Persistence/RowMappers.h"
#include "Infrastructure/Security/EcfSecurityUtils.h"
#include "Infrastructure/Security/EcfXmlSigner.h"
#include "Infrastructure/Serialization/EcfXsdFileNameResolver.h"
#include "Shared/Common/Sys.h"

using namespace drogon;
using namespace ecf::infra;

namespace ecf::api {

namespace {

const std::unordered_set<std::string> NeverTransmittedStates = {
    "SequenceAllocated",
    "SigningFailed",
    "SchemaInvalid",
    "Unsigned"
};

const std::unordered_map<int, int> DgiiItbisSlots = {
    {18, 1},
    {16, 2},
    {0, 3}
};

const std::string TenantSourceTxnUniqueConstraint = "uq_ecf_documents_tenant_source_txn";
const auto MinimumUncertainAgeBeforeReconciliation = std::chrono::minutes(2);

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

std::string cleanDigits(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (std::isdigit(static_cast<unsigned char>(c))) out.push_back(c);
    }
    return out;
}

bool hasAlpha(const std::string& s) {
    for (char c : s) {
        if (std::isalpha(static_cast<unsigned char>(c))) return true;
    }
    return false;
}

void applyCanonicalContent(domain::EcfDocument& doc,
                           const app::CanonicalDocumentDto& dto,
                           const std::string& editSequence,
                           const std::string& emisorRnc,
                           const std::string& emisorRazonSocial) {
    doc.editSequence = editSequence;
    doc.documentKind = dto.documentKind.empty() ? "Invoice" : dto.documentKind;
    if (dto.ncf.has_value()) doc.ncf = *dto.ncf;
    doc.rncEmisor = emisorRnc;
    doc.rncComprador = dto.header.rncComprador;
    doc.totalAmount = dto.totals.montoTotal;
    doc.itbisAmount = dto.totals.montoItbis;
    doc.xmlContent = app::buildXmlFromCanonical(dto, doc.eNcf, emisorRnc, emisorRazonSocial);
    doc.state = "SequenceAllocated";
}

domain::AmbienteEnum resolveAmbienteEnum(const std::string& rawEnv, domain::AmbienteEnum defaultAmbiente) {
    if (rawEnv.empty()) return defaultAmbiente;
    std::string lower = rawEnv;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    if (lower == "test" || lower == "testecf" || lower.find("precert") != std::string::npos)
        return domain::AmbienteEnum::PreCertificacion;
    if (lower == "cert" || lower == "certecf" || lower.find("certific") != std::string::npos || lower.find("homolog") != std::string::npos)
        return domain::AmbienteEnum::Certificacion;
    if (lower == "prod" || lower == "ecf" || lower.find("producc") != std::string::npos)
        return domain::AmbienteEnum::Produccion;
    if (lower == "1" || lower == "precertificacion") return domain::AmbienteEnum::PreCertificacion;
    if (lower == "2" || lower == "produccion") return domain::AmbienteEnum::Produccion;
    if (lower == "3" || lower == "certificacion") return domain::AmbienteEnum::Certificacion;
    return defaultAmbiente;
}

std::string ambienteToString(domain::AmbienteEnum amb) {
    switch (amb) {
        case domain::AmbienteEnum::PreCertificacion: return "PreCertificacion";
        case domain::AmbienteEnum::Produccion: return "Produccion";
        case domain::AmbienteEnum::Certificacion: return "Certificacion";
        default: return "Certificacion";
    }
}

std::shared_ptr<domain::IEcfXmlSigner> resolveSigner(
    const std::string& tenantId,
    const std::string& rncEmisor,
    const std::optional<app::CanonicalCertificateDto>& certDto,
    bool isDefaultFallback,
    AppServices& services) {
    if (isDefaultFallback) {
        return services.signer();
    }

    if (certDto.has_value() && certDto->certificateBase64.has_value() && !certDto->certificateBase64->empty()) {
        try {
            auto rawBytes = drogon::utils::base64Decode(*certDto->certificateBase64);
            std::vector<unsigned char> bytes(rawBytes.begin(), rawBytes.end());
            std::string pwd = certDto->password.value_or("");
            return std::make_shared<infra::EcfXmlSigner>(bytes, pwd);
        } catch (...) {
            // fallback
        }
    }

    if (certDto.has_value() && certDto->certificatePath.has_value() && !certDto->certificatePath->empty()) {
        try {
            std::string pwd = certDto->password.value_or("");
            return std::make_shared<infra::EcfXmlSigner>(*certDto->certificatePath, pwd);
        } catch (...) {
            // fallback
        }
    }

    // Cargar certificado dinámico real desde PostgreSQL (Tenants)
    if (services.tenantSignerResolver() && !rncEmisor.empty()) {
        try {
            auto tenantSigner = services.tenantSignerResolver()->resolveSigner(rncEmisor);
            if (tenantSigner && tenantSigner != services.signer()) {
                return tenantSigner;
            }
        } catch (...) {
            // fallback
        }
    }

    // Default certificate directory search
    std::string defaultCertDir = "/app/certificates";
    std::string tenantCertFile = defaultCertDir + "/" + tenantId + ".pfx";
    std::ifstream tf(tenantCertFile);
    if (tf.good()) {
        std::string pwd = (certDto.has_value() && certDto->password.has_value()) ? *certDto->password : "EcfTestPassword123!";
        try {
            return std::make_shared<infra::EcfXmlSigner>(tenantCertFile, pwd);
        } catch (...) {}
    }

    std::string rncCertFile = defaultCertDir + "/" + rncEmisor + ".pfx";
    std::ifstream rf(rncCertFile);
    if (rf.good()) {
        std::string pwd = (certDto.has_value() && certDto->password.has_value()) ? *certDto->password : "EcfTestPassword123!";
        try {
            return std::make_shared<infra::EcfXmlSigner>(rncCertFile, pwd);
        } catch (...) {}
    }

    return services.signer();
}

std::shared_ptr<domain::IEcfClient> resolveEcfClient(
    const std::string& rncEmisor,
    std::shared_ptr<domain::IEcfXmlSigner> signer,
    domain::AmbienteEnum ambiente,
    bool isDefaultFallback,
    AppServices& services) {
    if (isDefaultFallback || (signer == services.signer() &&
        rncEmisor == services.emisorOptions().rnc &&
        ambiente == services.ecfClientOptions().toAmbiente(services.ecfClientOptions().environment))) {
        return services.ecfClient();
    }

    domain::EcfClientOptions tenantOptions;
    tenantOptions.rncEmisor = rncEmisor;
    switch (ambiente) {
        case domain::AmbienteEnum::PreCertificacion: tenantOptions.environment = domain::EcfEnvironment::Test; break;
        case domain::AmbienteEnum::Certificacion: tenantOptions.environment = domain::EcfEnvironment::Cert; break;
        case domain::AmbienteEnum::Produccion: tenantOptions.environment = domain::EcfEnvironment::Prod; break;
    }
    tenantOptions.mode = domain::IntegrationMode::DgiiDirect;
    tenantOptions.validateSchemasLocal = services.ecfClientOptions().validateSchemasLocal;
    tenantOptions.xsdDirectoryPath = services.ecfClientOptions().xsdDirectoryPath;

    return std::make_shared<infra::EcfClient>(tenantOptions, nullptr, services.cacheService(), services.schemaValidator(), signer);
}

HttpResponsePtr signAndSend(domain::EcfDocument& doc, AppServices::Scope& scope, AppServices& services,
                            std::shared_ptr<domain::IEcfXmlSigner> effectiveSigner = nullptr,
                            std::shared_ptr<domain::IEcfClient> effectiveClient = nullptr) {
    auto signer = effectiveSigner ? effectiveSigner : services.signer();
    auto client = effectiveClient ? effectiveClient : services.ecfClient();

    std::string signedXml;
    try {
        signedXml = signer->signXml(doc.xmlContent, doc.rncEmisor);
        std::string secCode = EcfSecurityUtils::calcularCodigoSeguridad(signedXml);

        doc.signedXmlContent = signedXml;
        doc.securityCode = secCode;
        doc.state = "AwaitingTransmission";
    } catch (const std::exception& ex) {
        doc.state = "SigningFailed";
        doc.signedXmlContent = doc.xmlContent;
        scope.docs->update(doc);
        scope.uow->saveChanges();
        return json(err(std::string("Error al firmar digitalmente el XML de e-CF: ") + ex.what()), k400BadRequest);
    }

    const auto& ecfOpts = services.ecfClientOptions();
    if (ecfOpts.validateSchemasLocal && ecfOpts.xsdDirectoryPath.has_value() && !ecfOpts.xsdDirectoryPath->empty()) {
        std::string xsdFileName = EcfXsdFileNameResolver::resolve(signedXml);
        if (!xsdFileName.empty()) {
            std::string xsdPath = *ecfOpts.xsdDirectoryPath + "/" + xsdFileName;
            auto xsdResult = services.schemaValidator()->validate(signedXml, xsdPath);
            if (!xsdResult.isValid) {
                doc.state = "SchemaInvalid";
                scope.docs->update(doc);
                scope.uow->saveChanges();
                Json::Value details(Json::arrayValue);
                for (const auto& error : xsdResult.errors) details.append(error);
                Json::Value responseObj;
                responseObj["error"] = "El XML firmado no es válido contra el esquema DGII (validación local, antes de enviar a DGII).";
                responseObj["details"] = details;
                return json(responseObj, k400BadRequest);
            }
        }
    }

    if (signer->usesFallbackCertificate()) {
        doc.state = "Unsigned";
        scope.docs->update(doc);
        scope.uow->saveChanges();
        Json::Value out;
        out["documentId"] = doc.id;
        out["eNcf"] = doc.eNcf;
        out["state"] = doc.state;
        out["trackId"] = doc.trackId.value_or("");
        out["securityCode"] = doc.securityCode.value_or("");
        out["signedXml"] = doc.signedXmlContent.value_or("");
        out["dgiiResponse"] = doc.dgiiResponseXml.value_or("");
        return json(out, k202Accepted);
    }

    scope.docs->update(doc);
    scope.uow->saveChanges();

    try {
        std::string fileName = doc.rncEmisor + doc.eNcf + ".xml";
        auto response = client->sendEcf(doc.signedXmlContent.value(), fileName);
        if (!response.trackId.empty()) {
            doc.trackId = response.trackId;
            doc.state = "Signed";
            doc.sentToDgiiAt = sys::utcNowIso();

            // Immediate status check with DGII in case it was processed synchronously (DGII takes ~1.5s)
            try {
                std::this_thread::sleep_for(std::chrono::milliseconds(1500));
                auto resultado = client->consultarResultado(*doc.trackId);
                if (!resultado.estado.empty()) {
                    std::string estado = resultado.estado;
                    while (!estado.empty() && std::isspace(static_cast<unsigned char>(estado.front()))) estado.erase(estado.begin());
                    while (!estado.empty() && std::isspace(static_cast<unsigned char>(estado.back()))) estado.pop_back();

                    std::string lowerEstado = estado;
                    std::transform(lowerEstado.begin(), lowerEstado.end(), lowerEstado.begin(), ::tolower);

                    if (lowerEstado == "aceptado" || lowerEstado == "aceptado condicional") {
                        doc.state = "AcceptedByDgii";
                        doc.dgiiResponseXml = "Aceptado por DGII: " + estado;
                    } else if (lowerEstado == "rechazado") {
                        doc.state = "RejectedByDgii";
                        std::string errors = "Rechazado por DGII";
                        if (!resultado.mensajes.empty()) {
                            errors = "";
                            for (size_t i = 0; i < resultado.mensajes.size(); ++i) {
                                if (i > 0) errors += "; ";
                                errors += "[" + resultado.mensajes[i].codigo + "] " + resultado.mensajes[i].valor;
                            }
                        }
                        doc.dgiiResponseXml = errors;
                    }
                }
            } catch (...) {
                // Immediate check didn't complete; will be polled by background reconciler
            }
        } else {
            doc.state = "RejectedByDgii";
        }
    } catch (...) {
        doc.state = "Uncertain";
    }

    scope.docs->update(doc);
    scope.uow->saveChanges();

    Json::Value out;
    out["documentId"] = doc.id;
    out["eNcf"] = doc.eNcf;
    out["state"] = doc.state;
    out["trackId"] = doc.trackId.value_or("");
    out["securityCode"] = doc.securityCode.value_or("");
    out["signedXml"] = doc.signedXmlContent.value_or("");
    out["dgiiResponse"] = doc.dgiiResponseXml.value_or("");
    return json(out, k202Accepted);
}

HttpResponsePtr reconcileUncertain(domain::EcfDocument& doc,
                                   const app::CanonicalDocumentDto& dto,
                                   const std::string& editSequence,
                                   AppServices::Scope& scope,
                                   AppServices& services,
                                   const std::string& emisorRnc,
                                   const std::string& emisorRazonSocial,
                                   std::shared_ptr<domain::IEcfXmlSigner> effectiveSigner = nullptr,
                                   std::shared_ptr<domain::IEcfClient> effectiveClient = nullptr) {
    auto client = effectiveClient ? effectiveClient : services.ecfClient();

    // Check minimum age: updatedAt or createdAt
    std::string timestampStr = doc.updatedAt.value_or(doc.createdAt);
    if (!timestampStr.empty()) {
        auto docTime = sys::parseIsoUtc(timestampStr);
        auto now = std::chrono::system_clock::now();
        if (now - docTime < MinimumUncertainAgeBeforeReconciliation) {
            Json::Value out;
            out["documentId"] = doc.id;
            out["eNcf"] = doc.eNcf;
            out["state"] = doc.state;
            out["trackId"] = doc.trackId.value_or("");
            out["securityCode"] = doc.securityCode.value_or("");
            out["signedXml"] = doc.signedXmlContent.value_or("");
            out["dgiiResponse"] = doc.dgiiResponseXml.value_or("");
            return json(out, k202Accepted);
        }
    }

    domain::ConsultaEstadoResponse status;
    bool querySucceeded = false;
    try {
        status = client->consultarEstado(doc.rncEmisor, doc.eNcf);
        querySucceeded = true;
    } catch (...) {
        querySucceeded = false;
    }

    if (!querySucceeded) {
        Json::Value out;
        out["documentId"] = doc.id;
        out["eNcf"] = doc.eNcf;
        out["state"] = doc.state;
        out["trackId"] = doc.trackId.value_or("");
        out["securityCode"] = doc.securityCode.value_or("");
        out["signedXml"] = doc.signedXmlContent.value_or("");
        out["dgiiResponse"] = doc.dgiiResponseXml.value_or("");
        return json(out, k202Accepted);
    }

    std::string estadoTrim = status.estado;
    while (!estadoTrim.empty() && std::isspace(static_cast<unsigned char>(estadoTrim.front()))) estadoTrim.erase(estadoTrim.begin());
    while (!estadoTrim.empty() && std::isspace(static_cast<unsigned char>(estadoTrim.back()))) estadoTrim.pop_back();

    bool isNotFound = (estadoTrim == "No encontrado" || estadoTrim == "no encontrado");
    if (!isNotFound && !estadoTrim.empty()) {
        doc.state = "Signed";
        scope.docs->update(doc);
        scope.uow->saveChanges();
        Json::Value out;
        out["documentId"] = doc.id;
        out["eNcf"] = doc.eNcf;
        out["state"] = doc.state;
        out["trackId"] = doc.trackId.value_or("");
        out["securityCode"] = doc.securityCode.value_or("");
        out["signedXml"] = doc.signedXmlContent.value_or("");
        out["dgiiResponse"] = doc.dgiiResponseXml.value_or("");
        return json(out, k202Accepted);
    }

    // DGII confirms it never received it: treat as never transmitted
    applyCanonicalContent(doc, dto, editSequence, emisorRnc, emisorRazonSocial);
    scope.docs->update(doc);
    scope.uow->saveChanges();
    return signAndSend(doc, scope, services, effectiveSigner, effectiveClient);
}

HttpResponsePtr handleExistingDocument(domain::EcfDocument& existingDoc,
                                       const app::CanonicalDocumentDto& dto,
                                       const std::string& editSequence,
                                       AppServices::Scope& scope,
                                       AppServices& services,
                                       const std::string& emisorRnc,
                                       const std::string& emisorRazonSocial,
                                       std::shared_ptr<domain::IEcfXmlSigner> effectiveSigner = nullptr,
                                       std::shared_ptr<domain::IEcfClient> effectiveClient = nullptr,
                                       const std::string& tenantId = "default-tenant",
                                       domain::AmbienteEnum ambiente = domain::AmbienteEnum::Certificacion,
                                       bool isDefaultFallback = true) {
    if (NeverTransmittedStates.count(existingDoc.state)) {
        applyCanonicalContent(existingDoc, dto, editSequence, emisorRnc, emisorRazonSocial);
        scope.docs->update(existingDoc);
        scope.uow->saveChanges();
        return signAndSend(existingDoc, scope, services, effectiveSigner, effectiveClient);
    }

    if (existingDoc.state == "Uncertain") {
        return reconcileUncertain(existingDoc, dto, editSequence, scope, services, emisorRnc, emisorRazonSocial, effectiveSigner, effectiveClient);
    }

    if (existingDoc.state == "RejectedByDgii") {
        // Re-issuance of rejected e-CF: allocate fresh eNCF and re-transmit
        std::string sequenceScope = isDefaultFallback ? "default-tenant" : (tenantId + ":" + ambienteToString(ambiente));
        std::string newEncf = services.sequenceManager()->getNextEncf(sequenceScope, dto.tipoComprobante.empty() ? "E31" : dto.tipoComprobante);
        existingDoc.eNcf = newEncf;
        existingDoc.trackId = std::nullopt;
        existingDoc.securityCode = std::nullopt;
        existingDoc.dgiiResponseXml = std::nullopt;
        existingDoc.sentToDgiiAt = std::nullopt;
        existingDoc.state = "AwaitingTransmission";

        applyCanonicalContent(existingDoc, dto, editSequence, emisorRnc, emisorRazonSocial);
        scope.docs->update(existingDoc);
        scope.uow->saveChanges();
        return signAndSend(existingDoc, scope, services, effectiveSigner, effectiveClient);
    }

    if (existingDoc.editSequence != editSequence) {
        Json::Value conflictObj;
        conflictObj["error"] = "SourceReference.EditSequence differs from the version already processed for this TxnId. "
                              "The invoice was modified after its e-CF was issued; issue a correction document instead of resubmitting.";
        conflictObj["documentId"] = existingDoc.id;
        conflictObj["eNcf"] = existingDoc.eNcf;
        conflictObj["state"] = existingDoc.state;
        conflictObj["previousEditSequence"] = existingDoc.editSequence;
        conflictObj["incomingEditSequence"] = editSequence;
        return json(conflictObj, k409Conflict);
    }

    Json::Value out;
    out["documentId"] = existingDoc.id;
    out["eNcf"] = existingDoc.eNcf;
    out["state"] = existingDoc.state;
    out["trackId"] = existingDoc.trackId.value_or("");
    out["securityCode"] = existingDoc.securityCode.value_or("");
    out["signedXml"] = existingDoc.signedXmlContent.value_or("");
    out["dgiiResponse"] = existingDoc.dgiiResponseXml.value_or("");
    return json(out, k202Accepted);
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

            std::string tipoComprobante = dto.tipoComprobante;
            std::transform(tipoComprobante.begin(), tipoComprobante.end(), tipoComprobante.begin(), ::toupper);

            // Pre-allocation type-specific validations
            if (tipoComprobante == "E34" || tipoComprobante == "E33") {
                if (dto.references.correctsENcf.empty()) {
                    cb(json(err("References.CorrectsENcf (NCFModificado) is required for TipoComprobante " + dto.tipoComprobante + "."), k400BadRequest));
                    return;
                }
                if (!dto.references.codigoModificacion.has_value()) {
                    cb(json(err("References.CodigoModificacion is required for TipoComprobante " + dto.tipoComprobante + "."), k400BadRequest));
                    return;
                }
            } else if (tipoComprobante == "E31" || tipoComprobante == "E45") {
                if (dto.header.rncComprador.empty()) {
                    cb(json(err("Header.RncComprador es obligatorio para TipoComprobante " + dto.tipoComprobante + ". El cliente no tiene RNC/cédula registrado — corrígelo en el ERP, o emítelo como Factura de Consumo (E32), que no lo exige."), k400BadRequest));
                    return;
                }
                std::string clean = cleanDigits(dto.header.rncComprador);
                if (clean.length() != 9 && clean.length() != 11) {
                    cb(json(err("Header.RncComprador '" + dto.header.rncComprador + "' es inválido para " + dto.tipoComprobante + ". Debe tener exactamente 9 dígitos (RNC) u 11 dígitos (Cédula)."), k400BadRequest));
                    return;
                }
            } else if (tipoComprobante == "E32") {
                if (dto.totals.montoTotal >= 250000.0) {
                    if (dto.header.rncComprador.empty()) {
                        cb(json(err("Header.RncComprador (o pasaporte/identificación extranjera) es obligatorio para Factura de Consumo (E32) con MontoTotal ≥ RD$250,000.00 (DGII Regla de Validación tipo 32)."), k400BadRequest));
                        return;
                    }
                    std::string clean = cleanDigits(dto.header.rncComprador);
                    if (!hasAlpha(dto.header.rncComprador) && clean.length() != 9 && clean.length() != 11) {
                        cb(json(err("Header.RncComprador '" + dto.header.rncComprador + "' es inválido para E32 ≥ RD$250,000.00. Debe tener 9 dígitos (RNC), 11 dígitos (Cédula), o ser Identificador Extranjero."), k400BadRequest));
                        return;
                    }
                }
            } else if (tipoComprobante == "E41") {
                if (dto.header.rncComprador.empty()) {
                    cb(json(err("Header.RncComprador is required for TipoComprobante E41 (the informal vendor's RNC/Cédula)."), k400BadRequest));
                    return;
                }
                std::string clean = cleanDigits(dto.header.rncComprador);
                if (clean.length() != 9 && clean.length() != 11) {
                    cb(json(err("Header.RncComprador '" + dto.header.rncComprador + "' es inválido para E41. Debe tener 9 dígitos (RNC) u 11 dígitos (Cédula)."), k400BadRequest));
                    return;
                }
                if (!dto.retention.has_value()) {
                    cb(json(err("Retention is required for TipoComprobante E41."), k400BadRequest));
                    return;
                }
            } else if (tipoComprobante == "E47") {
                if (!dto.retention.has_value()) {
                    cb(json(err("Retention is required for TipoComprobante E47."), k400BadRequest));
                    return;
                }
            }

            // Validate ITBIS tax buckets
            for (const auto& bucket : dto.totals.taxBuckets) {
                if (DgiiItbisSlots.find(bucket.rate) == DgiiItbisSlots.end()) {
                    cb(json(err("Tasa(s) de ITBIS sin bucket DGII: " + std::to_string(bucket.rate) + ". Solo se admiten 18, 16, 0 (I1/I2/I3)."), k400BadRequest));
                    return;
                }
            }

            std::string emisorRnc = services.emisorOptions().rnc;
            if (emisorRnc.empty()) emisorRnc = services.ecfClientOptions().rncEmisor.value_or("101672919");
            std::string emisorRazonSocial = services.emisorOptions().razonSocial;
            if (emisorRazonSocial.empty()) emisorRazonSocial = "WILLY CHIC DOMINICANA SRL";

            std::string effectiveTenantId = tenantId;
            if (effectiveTenantId == "default-tenant") {
                std::string hTenant = req->getHeader("X-Tenant-Id");
                if (!hTenant.empty()) {
                    effectiveTenantId = hTenant;
                } else if (dto.tenantId.has_value() && !dto.tenantId->empty()) {
                    effectiveTenantId = *dto.tenantId;
                }
            }

            std::string rawEnv = req->getHeader("X-Environment");
            if (rawEnv.empty() && dto.environment.has_value()) {
                rawEnv = *dto.environment;
            }
            auto defaultAmbiente = services.ecfClientOptions().toAmbiente(services.ecfClientOptions().environment);
            auto ambiente = resolveAmbienteEnum(rawEnv, defaultAmbiente);

            bool isDefaultFallback = (effectiveTenantId == "default-tenant" && rawEnv.empty() &&
                                      req->getHeader("X-Tenant-Id").empty() &&
                                      (!dto.tenantId.has_value() || dto.tenantId->empty()));
            std::string sequenceScope = isDefaultFallback ? "default-tenant" : (effectiveTenantId + ":" + ambienteToString(ambiente));

            std::string effectiveRnc = (!isDefaultFallback && !dto.header.rncEmisor.empty())
                ? dto.header.rncEmisor
                : emisorRnc;

            std::string effectiveRazonSocial = (!isDefaultFallback && !dto.header.razonSocialEmisor.empty())
                ? dto.header.razonSocialEmisor
                : emisorRazonSocial;

            auto effectiveSigner = resolveSigner(effectiveTenantId, effectiveRnc, dto.certificate, isDefaultFallback, services);
            auto effectiveClient = resolveEcfClient(effectiveRnc, effectiveSigner, ambiente, isDefaultFallback, services);

            auto scope = services.makeScope(mapping::currentUserFrom(req));
            std::string editSequence = dto.sourceReference.editSequence;
            std::string ambStr = ambienteToString(ambiente);

            // Check if document for this TxnId has already been processed in this ambiente
            try {
                auto existing = scope.docs->getBySourceTxnId(effectiveTenantId, dto.sourceReference.txnId, ambStr);
                if (existing.has_value()) {
                    auto res = handleExistingDocument(*existing, dto, editSequence, scope, services, effectiveRnc, effectiveRazonSocial, effectiveSigner, effectiveClient, effectiveTenantId, ambiente, isDefaultFallback);
                    cb(res);
                    return;
                }
            } catch (const std::exception& ex) {
                cb(json(err(std::string("Database query error: ") + ex.what()), k500InternalServerError));
                return;
            }

            // Allocate eNCF Sequence
            std::string eNcf;
            try {
                eNcf = services.sequenceManager()->getNextEncf(sequenceScope, dto.tipoComprobante);
            } catch (const std::exception& ex) {
                cb(json(err(ex.what()), k400BadRequest));
                return;
            }

            domain::EcfDocument doc;
            doc.id = sys::newUuid();
            doc.tenantId = effectiveTenantId;
            doc.sourceTxnId = dto.sourceReference.txnId;
            doc.eNcf = eNcf;
            doc.ambiente = ambStr;
            applyCanonicalContent(doc, dto, editSequence, effectiveRnc, effectiveRazonSocial);

            try {
                scope.docs->add(doc);
                scope.uow->saveChanges();
            } catch (const std::exception& ex) {
                std::string what = ex.what();
                if (what.find(TenantSourceTxnUniqueConstraint) != std::string::npos || what.find("23505") != std::string::npos) {
                    // Concurrent insert race condition: winner exists
                    auto winner = scope.docs->getBySourceTxnId(effectiveTenantId, dto.sourceReference.txnId, ambStr);
                    if (winner.has_value()) {
                        auto res = handleExistingDocument(*winner, dto, editSequence, scope, services, effectiveRnc, effectiveRazonSocial, effectiveSigner, effectiveClient, effectiveTenantId, ambiente, isDefaultFallback);
                        cb(res);
                        return;
                    }
                }
                cb(json(err(std::string("Database insert error: ") + ex.what()), k500InternalServerError));
                return;
            }

            auto res = signAndSend(doc, scope, services, effectiveSigner, effectiveClient);
            cb(res);
        }
    );
}

void DocumentsController::getBySourceTxnId(const HttpRequestPtr& req,
                                          std::function<void(const HttpResponsePtr&)>&& callback,
                                          std::string txnId) {
    std::string tenantId = "default-tenant";
    if (req->attributes()->find("tenantId")) {
        std::string t = req->attributes()->get<std::string>("tenantId");
        if (!t.empty() && t != "default-tenant") tenantId = t;
    }
    if (tenantId == "default-tenant") {
        std::string hTenant = req->getHeader("X-Tenant-Id");
        if (!hTenant.empty()) tenantId = hTenant;
    }

    try {
        auto& services = AppServices::instance();
        auto scope = services.makeScope(mapping::currentUserFrom(req));

        auto doc = scope.docs->getBySourceTxnId(tenantId, txnId);
        if (!doc.has_value()) {
            Json::Value errBody;
            errBody["error"] = "Document with source TxnId '" + txnId + "' not found.";
            callback(json(errBody, k404NotFound));
            return;
        }

        // Si el comprobante sigue en Signed con TrackId, verificar dinámicamente con DGII
        if (doc->state == "Signed" && doc->trackId.has_value() && !doc->trackId->empty()) {
            try {
                auto effectiveSigner = resolveSigner(doc->tenantId, doc->rncEmisor, std::nullopt, false, services);
                auto defaultAmbiente = services.ecfClientOptions().toAmbiente(services.ecfClientOptions().environment);
                auto amb = resolveAmbienteEnum(doc->ambiente.value_or(""), defaultAmbiente);
                auto client = resolveEcfClient(doc->rncEmisor, effectiveSigner, amb, false, services);
                auto resultado = client->consultarResultado(*doc->trackId);
                if (!resultado.estado.empty()) {
                    std::string estado = resultado.estado;
                    while (!estado.empty() && std::isspace(static_cast<unsigned char>(estado.front()))) estado.erase(estado.begin());
                    while (!estado.empty() && std::isspace(static_cast<unsigned char>(estado.back()))) estado.pop_back();

                    std::string lowerEstado = estado;
                    std::transform(lowerEstado.begin(), lowerEstado.end(), lowerEstado.begin(), ::tolower);

                    if (lowerEstado == "aceptado" || lowerEstado == "aceptado condicional") {
                        doc->state = "AcceptedByDgii";
                        doc->dgiiResponseXml = "Aceptado por DGII: " + estado;
                        scope.docs->update(*doc);
                        scope.uow->saveChanges();
                    } else if (lowerEstado == "rechazado") {
                        doc->state = "RejectedByDgii";
                        std::string errors = "Rechazado por DGII";
                        if (!resultado.mensajes.empty()) {
                            errors = "";
                            for (size_t i = 0; i < resultado.mensajes.size(); ++i) {
                                if (i > 0) errors += "; ";
                                errors += "[" + resultado.mensajes[i].codigo + "] " + resultado.mensajes[i].valor;
                            }
                        }
                        doc->dgiiResponseXml = errors;
                        scope.docs->update(*doc);
                        scope.uow->saveChanges();
                    }
                }
            } catch (const std::exception& ex) {
                // Log and continue, return current DB state
            }
        }

        Json::Value out;
        out["documentId"] = doc->id;
        if (doc->ncf.has_value()) out["ncf"] = *doc->ncf;
        out["eNcf"] = doc->eNcf;
        out["state"] = doc->state;
        out["trackId"] = doc->trackId.value_or("");
        out["securityCode"] = doc->securityCode.value_or("");
        out["receiptDate"] = doc->receiptDate.value_or("");
        out["signedXml"] = doc->signedXmlContent.value_or("");
        out["dgiiResponse"] = doc->dgiiResponseXml.value_or("");

        callback(json(out, k200OK));
    } catch (const std::exception& ex) {
        Json::Value errBody;
        errBody["error"] = ex.what();
        callback(json(errBody, k500InternalServerError));
    }
}

void DocumentsController::getXmlBySourceTxnId(const HttpRequestPtr& req,
                                              std::function<void(const HttpResponsePtr&)>&& callback,
                                              std::string txnId) {
    std::string tenantId = "default-tenant";
    if (req->attributes()->find("tenantId")) {
        std::string t = req->attributes()->get<std::string>("tenantId");
        if (!t.empty() && t != "default-tenant") tenantId = t;
    }
    if (tenantId == "default-tenant") {
        std::string hTenant = req->getHeader("X-Tenant-Id");
        if (!hTenant.empty()) tenantId = hTenant;
    }

    try {
        auto& services = AppServices::instance();
        auto scope = services.makeScope(mapping::currentUserFrom(req));

        auto doc = scope.docs->getBySourceTxnId(tenantId, txnId);
        if (!doc.has_value() || !doc->signedXmlContent.has_value() || doc->signedXmlContent->empty()) {
            Json::Value errBody;
            errBody["error"] = "Document with source TxnId '" + txnId + "' not found or has no XML.";
            callback(json(errBody, k404NotFound));
            return;
        }

        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k200OK);
        resp->setContentTypeCode(CT_APPLICATION_XML);
        std::string fileName = doc->rncEmisor + "-" + doc->eNcf + ".xml";
        resp->addHeader("Content-Disposition", "attachment; filename=\"" + fileName + "\"");
        resp->setBody(*doc->signedXmlContent);
        callback(resp);
    } catch (const std::exception& ex) {
        Json::Value errBody;
        errBody["error"] = ex.what();
        callback(json(errBody, k500InternalServerError));
    }
}

void DocumentsController::getXmlById(const HttpRequestPtr& req,
                                    std::function<void(const HttpResponsePtr&)>&& callback,
                                    std::string id) {
    std::string tenantId = "default-tenant";
    if (req->attributes()->find("tenantId")) {
        std::string t = req->attributes()->get<std::string>("tenantId");
        if (!t.empty() && t != "default-tenant") tenantId = t;
    }
    if (tenantId == "default-tenant") {
        std::string hTenant = req->getHeader("X-Tenant-Id");
        if (!hTenant.empty()) tenantId = hTenant;
    }

    try {
        auto& services = AppServices::instance();
        auto scope = services.makeScope(mapping::currentUserFrom(req));

        auto doc = scope.docs->getById(id);
        if (!doc.has_value() || (tenantId != "default-tenant" && doc->tenantId != tenantId) || !doc->signedXmlContent.has_value() || doc->signedXmlContent->empty()) {
            Json::Value errBody;
            errBody["error"] = "Document '" + id + "' not found or has no XML.";
            callback(json(errBody, k404NotFound));
            return;
        }

        auto resp = HttpResponse::newHttpResponse();
        resp->setStatusCode(k200OK);
        resp->setContentTypeCode(CT_APPLICATION_XML);
        std::string fileName = doc->rncEmisor + "-" + doc->eNcf + ".xml";
        resp->addHeader("Content-Disposition", "attachment; filename=\"" + fileName + "\"");
        resp->setBody(*doc->signedXmlContent);
        callback(resp);
    } catch (const std::exception& ex) {
        Json::Value errBody;
        errBody["error"] = ex.what();
        callback(json(errBody, k500InternalServerError));
    }
}

void DocumentsController::getById(const HttpRequestPtr& req,
                                  std::function<void(const HttpResponsePtr&)>&& callback,
                                  std::string id) {
    std::string tenantId = "default-tenant";
    if (req->attributes()->find("tenantId")) {
        std::string t = req->attributes()->get<std::string>("tenantId");
        if (!t.empty() && t != "default-tenant") tenantId = t;
    }
    if (tenantId == "default-tenant") {
        std::string hTenant = req->getHeader("X-Tenant-Id");
        if (!hTenant.empty()) tenantId = hTenant;
    }

    try {
        auto& services = AppServices::instance();
        auto scope = services.makeScope(mapping::currentUserFrom(req));

        auto doc = scope.docs->getById(id);
        if (!doc.has_value() || (tenantId != "default-tenant" && doc->tenantId != tenantId)) {
            Json::Value errBody;
            errBody["error"] = "Document '" + id + "' not found.";
            callback(json(errBody, k404NotFound));
            return;
        }

        Json::Value out;
        out["documentId"] = doc->id;
        if (doc->ncf.has_value()) out["ncf"] = *doc->ncf;
        out["eNcf"] = doc->eNcf;
        out["state"] = doc->state;
        out["trackId"] = doc->trackId.value_or("");
        out["securityCode"] = doc->securityCode.value_or("");
        out["receiptDate"] = doc->receiptDate.value_or("");
        out["signedXml"] = doc->signedXmlContent.value_or("");
        out["dgiiResponse"] = doc->dgiiResponseXml.value_or("");

        callback(json(out, k200OK));
    } catch (const std::exception& ex) {
        Json::Value errBody;
        errBody["error"] = ex.what();
        callback(json(errBody, k500InternalServerError));
    }
}

} // namespace ecf::api
