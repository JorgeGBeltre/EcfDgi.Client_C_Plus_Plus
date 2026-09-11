#include "Api/Controllers/DocumentsController.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#include "Api/AppServices.h"
#include "Api/JsonMapping.h"
#include "Api/Security/IdempotencyHandler.h"
#include "Infrastructure/Persistence/RowMappers.h"
#include "Infrastructure/Security/EcfSecurityUtils.h"
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

std::string escapeXml(const std::string& value) {
    if (value.empty()) return value;
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::string normalizeFechaDgii(const std::string& value) {
    if (value.empty()) {
        auto now = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &tt);
#else
        localtime_r(&tt, &tm);
#endif
        char buf[32];
        std::strftime(buf, sizeof(buf), "%d-%m-%Y", &tm);
        return std::string(buf);
    }

    std::string trimmed = value;
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.front()))) trimmed.erase(trimmed.begin());
    while (!trimmed.empty() && std::isspace(static_cast<unsigned char>(trimmed.back()))) trimmed.pop_back();

    // If already dd-MM-yyyy
    static const std::regex rgx_ddmmyyyy(R"(^(\d{2})-(\d{2})-(\d{4})$)");
    std::smatch m;
    if (std::regex_match(trimmed, m, rgx_ddmmyyyy)) {
        return trimmed;
    }

    // If yyyy-MM-dd or yyyy/MM/dd
    static const std::regex rgx_iso(R"(^(\d{4})[-/](\d{2})[-/](\d{2})$)");
    if (std::regex_match(trimmed, m, rgx_iso)) {
        return m[3].str() + "-" + m[2].str() + "-" + m[1].str();
    }

    // If dd/MM/yyyy
    static const std::regex rgx_slash(R"(^(\d{2})/(\d{2})/(\d{4})$)");
    if (std::regex_match(trimmed, m, rgx_slash)) {
        return m[1].str() + "-" + m[2].str() + "-" + m[3].str();
    }

    // If yyyyMMdd
    static const std::regex rgx_compact(R"(^(\d{4})(\d{2})(\d{2})$)");
    if (std::regex_match(trimmed, m, rgx_compact)) {
        return m[3].str() + "-" + m[2].str() + "-" + m[1].str();
    }

    std::string out = trimmed;
    std::replace(out.begin(), out.end(), '/', '-');
    return out;
}

struct ProcessedLineItem {
    int lineNumber = 0;
    std::string name;
    std::string description;
    double quantity = 1.0;
    double unitPrice = 0.0;
    double discountAmount = 0.0;
    double montoItem = 0.0;
};

std::vector<ProcessedLineItem> normalizeCanonicalLines(const std::vector<app::CanonicalLineDto>& lines) {
    std::vector<ProcessedLineItem> result;

    for (const auto& line : lines) {
        double rawQty = line.quantity > 0.0 ? line.quantity : 1.0;
        double rawPrice = line.quantity > 0.0 ? line.unitPrice : line.amount;
        std::string rawName = !line.itemName.empty() ? line.itemName : "Item";

        // Negative line (discount in QuickBooks)
        if (line.amount < 0.0 || rawPrice < 0.0) {
            double absDiscount = std::abs(line.amount);
            if (!result.empty()) {
                auto& prev = result.back();
                prev.discountAmount += absDiscount;
                prev.montoItem = std::max(0.0, prev.montoItem - absDiscount);
                continue;
            }
        }

        double safeQty = rawQty > 0.0 ? rawQty : 1.0;
        double safePrice = std::max(0.0, rawPrice);
        double safeAmount = std::max(0.0, line.amount);

        std::string shortName = rawName.length() > 80 ? rawName.substr(0, 80) : rawName;
        std::string extendedDesc;
        if (rawName.length() > 80) {
            extendedDesc = rawName.length() > 1000 ? rawName.substr(0, 1000) : rawName;
        }

        ProcessedLineItem pi;
        pi.lineNumber = static_cast<int>(result.size()) + 1;
        pi.name = shortName;
        pi.description = extendedDesc;
        pi.quantity = safeQty;
        pi.unitPrice = safePrice;
        pi.discountAmount = 0.0;
        pi.montoItem = safeAmount;
        result.push_back(pi);
    }

    if (result.empty()) {
        ProcessedLineItem pi;
        pi.lineNumber = 1;
        pi.name = "Item General";
        pi.quantity = 1.0;
        pi.unitPrice = 0.0;
        pi.discountAmount = 0.0;
        pi.montoItem = 0.0;
        result.push_back(pi);
    }

    return result;
}

void appendRetencion(std::ostringstream& ss, const std::optional<app::CanonicalRetentionDto>& retention, const std::string& tipoEcf) {
    if (!retention.has_value()) return;

    ss << "      <Retencion>\n"
       << "        <IndicadorAgenteRetencionoPercepcion>" << retention->indicadorAgenteRetencionoPercepcion << "</IndicadorAgenteRetencionoPercepcion>\n";
    if (tipoEcf == "47") {
        double isr = retention->montoIsrRetenido.value_or(0.0);
        ss << std::fixed << std::setprecision(2);
        ss << "        <MontoISRRetenido>" << isr << "</MontoISRRetenido>\n";
    } else {
        ss << std::fixed << std::setprecision(2);
        ss << "        <MontoITBISRetenido>" << retention->montoItbisRetenido << "</MontoITBISRetenido>\n";
        if (retention->montoIsrRetenido.has_value()) {
            ss << "        <MontoISRRetenido>" << *retention->montoIsrRetenido << "</MontoISRRetenido>\n";
        }
    }
    ss << "      </Retencion>\n";
}

std::string buildXmlFromCanonical(const app::CanonicalDocumentDto& dto,
                                  const std::string& eNcf,
                                  const std::string& emisorRnc,
                                  const std::string& emisorRazonSocial) {
    std::ostringstream ss;
    ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
       << "<ECF>\n"
       << "  <Encabezado>\n"
       << "    <Version>1.0</Version>\n"
       << "    <IdDoc>\n";

    std::string tipoEcf = "31";
    if (!dto.tipoComprobante.empty()) {
        if (dto.tipoComprobante[0] == 'E' || dto.tipoComprobante[0] == 'e') {
            tipoEcf = dto.tipoComprobante.substr(1);
        } else {
            tipoEcf = dto.tipoComprobante;
        }
    }

    ss << "      <TipoeCF>" << tipoEcf << "</TipoeCF>\n"
       << "      <eNCF>" << eNcf << "</eNCF>\n";

    if (tipoEcf == "34") {
        ss << "      <IndicadorNotaCredito>0</IndicadorNotaCredito>\n";
    } else if (tipoEcf != "32") {
        auto now = std::chrono::system_clock::now() + std::chrono::hours(24 * 365);
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#if defined(_WIN32)
        localtime_s(&tm, &tt);
#else
        localtime_r(&tt, &tm);
#endif
        char buf[32];
        std::strftime(buf, sizeof(buf), "%d-%m-%Y", &tm);
        ss << "      <FechaVencimientoSecuencia>" << buf << "</FechaVencimientoSecuencia>\n";
    }

    if (tipoEcf != "41" && tipoEcf != "43" && tipoEcf != "47") {
        std::string tipoIngresoVal = (tipoEcf == "46") ? "02" : "01";
        ss << "      <TipoIngresos>" << tipoIngresoVal << "</TipoIngresos>\n";
    }
    ss << "      <TipoPago>1</TipoPago>\n"
       << "    </IdDoc>\n";

    ss << "    <Emisor>\n"
       << "      <RNCEmisor>" << emisorRnc << "</RNCEmisor>\n";
    std::string safeEmisorName = emisorRazonSocial.length() > 150 ? emisorRazonSocial.substr(0, 150) : emisorRazonSocial;
    ss << "      <RazonSocialEmisor>" << escapeXml(safeEmisorName) << "</RazonSocialEmisor>\n"
       << "      <DireccionEmisor>Distrito Nacional, SD</DireccionEmisor>\n";
    std::string fechaEmision = normalizeFechaDgii(dto.header.fechaEmision);
    ss << "      <FechaEmision>" << fechaEmision << "</FechaEmision>\n"
       << "    </Emisor>\n";

    if (tipoEcf != "43") {
        ss << "    <Comprador>\n";
        if (tipoEcf == "47") {
            if (!dto.header.rncComprador.empty()) {
                std::string foreignId = dto.header.rncComprador.length() > 20 ? dto.header.rncComprador.substr(0, 20) : dto.header.rncComprador;
                ss << "      <IdentificadorExtranjero>" << escapeXml(foreignId) << "</IdentificadorExtranjero>\n";
            }
        } else if (tipoEcf == "46") {
            if (!dto.header.rncComprador.empty()) {
                if (hasAlpha(dto.header.rncComprador)) {
                    std::string foreignId = dto.header.rncComprador.length() > 20 ? dto.header.rncComprador.substr(0, 20) : dto.header.rncComprador;
                    ss << "      <IdentificadorExtranjero>" << escapeXml(foreignId) << "</IdentificadorExtranjero>\n";
                } else {
                    std::string clean = cleanDigits(dto.header.rncComprador);
                    if (clean.length() == 9 || clean.length() == 11) {
                        ss << "      <RNCComprador>" << clean << "</RNCComprador>\n";
                    } else {
                        std::string foreignId = dto.header.rncComprador.length() > 20 ? dto.header.rncComprador.substr(0, 20) : dto.header.rncComprador;
                        ss << "      <IdentificadorExtranjero>" << escapeXml(foreignId) << "</IdentificadorExtranjero>\n";
                    }
                }
            }
        } else {
            if (!dto.header.rncComprador.empty()) {
                if (hasAlpha(dto.header.rncComprador) && (tipoEcf == "32" || tipoEcf == "33" || tipoEcf == "34" || tipoEcf == "44")) {
                    std::string foreignId = dto.header.rncComprador.length() > 20 ? dto.header.rncComprador.substr(0, 20) : dto.header.rncComprador;
                    ss << "      <IdentificadorExtranjero>" << escapeXml(foreignId) << "</IdentificadorExtranjero>\n";
                } else {
                    std::string clean = cleanDigits(dto.header.rncComprador);
                    if (clean.length() == 9 || clean.length() == 11 || tipoEcf == "31" || tipoEcf == "41" || tipoEcf == "45") {
                        ss << "      <RNCComprador>" << clean << "</RNCComprador>\n";
                    }
                }
            }
        }
        std::string rawComprador = dto.header.razonSocialComprador.empty()
            ? (tipoEcf == "47" ? "Beneficiario del Exterior" : (tipoEcf == "46" ? "Comprador Internacional" : "Consumidor Final"))
            : dto.header.razonSocialComprador;
        std::string safeComprador = rawComprador.length() > 150 ? rawComprador.substr(0, 150) : rawComprador;
        ss << "      <RazonSocialComprador>" << escapeXml(safeComprador) << "</RazonSocialComprador>\n"
           << "    </Comprador>\n";
    }

    ss << "    <Totales>\n";
    ss << std::fixed << std::setprecision(2);
    double total = dto.totals.montoTotal;
    double itbis = dto.totals.montoItbis;
    double gravado = dto.totals.montoGravadoTotal.value_or(dto.totals.montoSubtotal);
    double exento = dto.totals.montoExento.value_or(0.0);

    if (tipoEcf == "43" || tipoEcf == "44") {
        if (total > 0.0) ss << "      <MontoExento>" << total << "</MontoExento>\n";
        ss << "      <MontoTotal>" << total << "</MontoTotal>\n";
    } else if (tipoEcf == "47") {
        if (total > 0.0) ss << "      <MontoExento>" << total << "</MontoExento>\n";
        ss << "      <MontoTotal>" << total << "</MontoTotal>\n";
        if (dto.retention.has_value() && dto.retention->montoIsrRetenido.has_value() && *dto.retention->montoIsrRetenido > 0.0) {
            ss << "      <TotalISRRetencion>" << *dto.retention->montoIsrRetenido << "</TotalISRRetencion>\n";
        }
    } else if (tipoEcf == "46") {
        if (total > 0.0) {
            ss << "      <MontoGravadoTotal>" << total << "</MontoGravadoTotal>\n"
               << "      <MontoGravadoI3>" << total << "</MontoGravadoI3>\n"
               << "      <ITBIS3>0</ITBIS3>\n"
               << "      <TotalITBIS>0.00</TotalITBIS>\n"
               << "      <TotalITBIS3>0.00</TotalITBIS3>\n";
        }
        ss << "      <MontoTotal>" << total << "</MontoTotal>\n";
    } else {
        if (gravado > 0.0) {
            ss << "      <MontoGravadoTotal>" << gravado << "</MontoGravadoTotal>\n";
        }

        std::optional<double> slotBase[4];
        std::optional<int> slotRate[4];
        std::optional<double> slotTax[4];

        if (!dto.totals.taxBuckets.empty()) {
            for (const auto& bucket : dto.totals.taxBuckets) {
                auto it = DgiiItbisSlots.find(bucket.rate);
                if (it != DgiiItbisSlots.end()) {
                    int slot = it->second;
                    slotBase[slot] = slotBase[slot].value_or(0.0) + bucket.base;
                    slotTax[slot] = slotTax[slot].value_or(0.0) + bucket.tax;
                    slotRate[slot] = bucket.rate;
                }
            }
        } else if (gravado > 0.0) {
            slotBase[1] = gravado;
            slotTax[1] = itbis;
        }

        for (int slot = 1; slot <= 3; ++slot) {
            if (slotBase[slot].has_value()) {
                ss << "      <MontoGravadoI" << slot << ">" << *slotBase[slot] << "</MontoGravadoI" << slot << ">\n";
            }
        }

        if (exento > 0.0) {
            ss << "      <MontoExento>" << exento << "</MontoExento>\n";
        }

        for (int slot = 1; slot <= 3; ++slot) {
            if (slotRate[slot].has_value()) {
                ss << "      <ITBIS" << slot << ">" << *slotRate[slot] << "</ITBIS" << slot << ">\n";
            }
        }

        ss << "      <TotalITBIS>" << itbis << "</TotalITBIS>\n";

        for (int slot = 1; slot <= 3; ++slot) {
            if (slotTax[slot].has_value()) {
                ss << "      <TotalITBIS" << slot << ">" << *slotTax[slot] << "</TotalITBIS" << slot << ">\n";
            }
        }

        ss << "      <MontoTotal>" << total << "</MontoTotal>\n";

        if (tipoEcf == "31" || tipoEcf == "33" || tipoEcf == "34" || tipoEcf == "41") {
            if (dto.retention.has_value()) {
                if (dto.retention->montoItbisRetenido > 0.0) {
                    ss << "      <TotalITBISRetenido>" << dto.retention->montoItbisRetenido << "</TotalITBISRetenido>\n";
                }
                if (dto.retention->montoIsrRetenido.has_value() && *dto.retention->montoIsrRetenido > 0.0) {
                    ss << "      <TotalISRRetencion>" << *dto.retention->montoIsrRetenido << "</TotalISRRetencion>\n";
                }
            }
        }
    }
    ss << "    </Totales>\n"
       << "  </Encabezado>\n";

    std::optional<app::CanonicalRetentionDto> retention;
    if (tipoEcf == "31" || tipoEcf == "33" || tipoEcf == "34" || tipoEcf == "41" || tipoEcf == "47") {
        retention = dto.retention;
    }

    std::string indicadorFacturacion = "1";
    if (tipoEcf == "43" || tipoEcf == "44" || tipoEcf == "47") {
        indicadorFacturacion = "4";
    } else if (tipoEcf == "46") {
        indicadorFacturacion = "3";
    } else if (dto.totals.montoExento.value_or(0.0) > 0.0 &&
               dto.totals.montoGravadoTotal.value_or(0.0) == 0.0 &&
               dto.totals.montoItbis == 0.0) {
        indicadorFacturacion = "4";
    }

    std::string indicadorBienoServicio = "1";
    if (tipoEcf == "47") {
        indicadorBienoServicio = "2";
    } else if (tipoEcf == "41" && dto.retention.has_value() && dto.retention->montoIsrRetenido.value_or(0.0) > 0.0) {
        indicadorBienoServicio = "2";
    }

    ss << "  <DetallesItems>\n";
    auto processedItems = normalizeCanonicalLines(dto.lines);
    for (const auto& item : processedItems) {
        ss << "    <Item>\n"
           << "      <NumeroLinea>" << item.lineNumber << "</NumeroLinea>\n"
           << "      <IndicadorFacturacion>" << indicadorFacturacion << "</IndicadorFacturacion>\n";
        appendRetencion(ss, retention, tipoEcf);
        ss << "      <NombreItem>" << escapeXml(item.name) << "</NombreItem>\n"
           << "      <IndicadorBienoServicio>" << indicadorBienoServicio << "</IndicadorBienoServicio>\n";
        if (!item.description.empty()) {
            ss << "      <DescripcionItem>" << escapeXml(item.description) << "</DescripcionItem>\n";
        }
        ss << "      <CantidadItem>" << item.quantity << "</CantidadItem>\n"
           << "      <PrecioUnitarioItem>" << item.unitPrice << "</PrecioUnitarioItem>\n";
        if (tipoEcf != "43" && tipoEcf != "47" && item.discountAmount > 0.0) {
            ss << "      <DescuentoMonto>" << item.discountAmount << "</DescuentoMonto>\n";
        }
        ss << "      <MontoItem>" << item.montoItem << "</MontoItem>\n"
           << "    </Item>\n";
    }
    ss << "  </DetallesItems>\n";

    if ((tipoEcf == "33" || tipoEcf == "34" || !dto.references.correctsENcf.empty()) && !dto.references.correctsENcf.empty()) {
        std::string ncfMod = dto.references.correctsENcf;
        while (!ncfMod.empty() && std::isspace(static_cast<unsigned char>(ncfMod.front()))) ncfMod.erase(ncfMod.begin());
        while (!ncfMod.empty() && std::isspace(static_cast<unsigned char>(ncfMod.back()))) ncfMod.pop_back();

        if (ncfMod.length() < 11) {
            if ((ncfMod.rfind("1", 0) == 0 || ncfMod.rfind("2", 0) == 0 || ncfMod.rfind("4", 0) == 0) && ncfMod.length() == 9) {
                ncfMod = "B0" + ncfMod;
            } else if (ncfMod.rfind("0", 0) == 0 && ncfMod.length() == 10) {
                ncfMod = "B" + ncfMod;
            } else if (ncfMod.length() == 8 && cleanDigits(ncfMod).length() == 8) {
                ncfMod = "B02" + ncfMod;
            }
        }

        ss << "  <InformacionReferencia>\n"
           << "    <NCFModificado>" << ncfMod << "</NCFModificado>\n";
        if (dto.references.rncOtroContribuyente.has_value() && !dto.references.rncOtroContribuyente->empty()) {
            ss << "    <RNCOtroContribuyente>" << escapeXml(*dto.references.rncOtroContribuyente) << "</RNCOtroContribuyente>\n";
        }
        std::string fechaNcfModificado = normalizeFechaDgii(dto.references.fechaNcfModificado.value_or(""));
        ss << "    <FechaNCFModificado>" << fechaNcfModificado << "</FechaNCFModificado>\n";
        if (dto.references.codigoModificacion.has_value()) {
            ss << "    <CodigoModificacion>" << *dto.references.codigoModificacion << "</CodigoModificacion>\n";
        } else if (tipoEcf == "33" || tipoEcf == "34") {
            ss << "    <CodigoModificacion>1</CodigoModificacion>\n";
        }
        if ((tipoEcf == "33" || tipoEcf == "34") && dto.references.razonModificacion.has_value() && !dto.references.razonModificacion->empty()) {
            std::string safeRazon = dto.references.razonModificacion->length() > 90 ? dto.references.razonModificacion->substr(0, 90) : *dto.references.razonModificacion;
            ss << "    <RazonModificacion>" << escapeXml(safeRazon) << "</RazonModificacion>\n";
        }
        ss << "  </InformacionReferencia>\n";
    }

    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    char buf[32];
    std::strftime(buf, sizeof(buf), "%d-%m-%Y %H:%M:%S", &tm);
    ss << "  <FechaHoraFirma>" << buf << "</FechaHoraFirma>\n"
       << "</ECF>\n";

    return ss.str();
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
    doc.xmlContent = buildXmlFromCanonical(dto, doc.eNcf, emisorRnc, emisorRazonSocial);
    doc.state = "SequenceAllocated";
}

HttpResponsePtr signAndSend(domain::EcfDocument& doc, AppServices::Scope& scope, AppServices& services) {
    std::string signedXml;
    try {
        signedXml = services.signer()->signXml(doc.xmlContent, doc.rncEmisor);
        std::string secCode = EcfSecurityUtils::calcularCodigoSeguridad(signedXml);
        for (auto& c : secCode) c = static_cast<char>(std::toupper(c));

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

    if (services.signer()->usesFallbackCertificate()) {
        doc.state = "Unsigned";
        scope.docs->update(doc);
        scope.uow->saveChanges();
        Json::Value out;
        out["documentId"] = doc.id;
        out["eNcf"] = doc.eNcf;
        out["state"] = doc.state;
        out["trackId"] = doc.trackId.value_or("");
        out["securityCode"] = doc.securityCode.value_or("");
        return json(out, k202Accepted);
    }

    scope.docs->update(doc);
    scope.uow->saveChanges();

    try {
        std::string fileName = doc.rncEmisor + doc.eNcf + ".xml";
        auto response = services.ecfClient()->sendEcf(doc.signedXmlContent.value(), fileName);
        if (!response.trackId.empty()) {
            doc.trackId = response.trackId;
            doc.state = "Signed";
            doc.sentToDgiiAt = sys::utcNowIso();
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
    return json(out, k202Accepted);
}

HttpResponsePtr reconcileUncertain(domain::EcfDocument& doc,
                                   const app::CanonicalDocumentDto& dto,
                                   const std::string& editSequence,
                                   AppServices::Scope& scope,
                                   AppServices& services,
                                   const std::string& emisorRnc,
                                   const std::string& emisorRazonSocial) {
    // Check minimum age: updatedAt or createdAt
    std::string timestampStr = doc.updatedAt.value_or(doc.createdAt.value_or(""));
    if (!timestampStr.empty()) {
        // Approximate age check or pass
    }

    domain::ConsultaEstadoResponse status;
    bool querySucceeded = false;
    try {
        status = services.ecfClient()->consultarEstado(doc.rncEmisor, doc.eNcf);
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
        return json(out, k202Accepted);
    }

    // DGII confirms it never received it: treat as never transmitted
    applyCanonicalContent(doc, dto, editSequence, emisorRnc, emisorRazonSocial);
    scope.docs->update(doc);
    scope.uow->saveChanges();
    return signAndSend(doc, scope, services);
}

HttpResponsePtr handleExistingDocument(domain::EcfDocument& existingDoc,
                                       const app::CanonicalDocumentDto& dto,
                                       const std::string& editSequence,
                                       AppServices::Scope& scope,
                                       AppServices& services,
                                       const std::string& emisorRnc,
                                       const std::string& emisorRazonSocial) {
    if (NeverTransmittedStates.count(existingDoc.state)) {
        applyCanonicalContent(existingDoc, dto, editSequence, emisorRnc, emisorRazonSocial);
        scope.docs->update(existingDoc);
        scope.uow->saveChanges();
        return signAndSend(existingDoc, scope, services);
    }

    if (existingDoc.state == "Uncertain") {
        return reconcileUncertain(existingDoc, dto, editSequence, scope, services, emisorRnc, emisorRazonSocial);
    }

    if (existingDoc.editSequence.value_or("") != editSequence) {
        Json::Value conflictObj;
        conflictObj["error"] = "SourceReference.EditSequence differs from the version already processed for this TxnId. "
                              "The invoice was modified after its e-CF was issued; issue a correction document instead of resubmitting.";
        conflictObj["documentId"] = existingDoc.id;
        conflictObj["eNcf"] = existingDoc.eNcf;
        conflictObj["state"] = existingDoc.state;
        conflictObj["previousEditSequence"] = existingDoc.editSequence.value_or("");
        conflictObj["incomingEditSequence"] = editSequence;
        return json(conflictObj, k409Conflict);
    }

    Json::Value out;
    out["documentId"] = existingDoc.id;
    out["eNcf"] = existingDoc.eNcf;
    out["state"] = existingDoc.state;
    out["trackId"] = existingDoc.trackId.value_or("");
    out["securityCode"] = existingDoc.securityCode.value_or("");
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

            auto scope = services.makeScope(mapping::currentUserFrom(req));
            std::string editSequence = dto.sourceReference.editSequence;

            // Check if document for this TxnId has already been processed
            try {
                auto existing = scope.docs->getBySourceTxnId(tenantId, dto.sourceReference.txnId);
                if (existing.has_value()) {
                    auto res = handleExistingDocument(*existing, dto, editSequence, scope, services, emisorRnc, emisorRazonSocial);
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
                eNcf = services.sequenceManager()->getNextEncf(tenantId, dto.tipoComprobante);
            } catch (const std::exception& ex) {
                cb(json(err(ex.what()), k400BadRequest));
                return;
            }

            domain::EcfDocument doc;
            doc.id = sys::newUuid();
            doc.tenantId = tenantId;
            doc.sourceTxnId = dto.sourceReference.txnId;
            doc.eNcf = eNcf;
            applyCanonicalContent(doc, dto, editSequence, emisorRnc, emisorRazonSocial);

            try {
                scope.docs->add(doc);
                scope.uow->saveChanges();
            } catch (const std::exception& ex) {
                std::string what = ex.what();
                if (what.find(TenantSourceTxnUniqueConstraint) != std::string::npos || what.find("23505") != std::string::npos) {
                    // Concurrent insert race condition: winner exists
                    auto winner = scope.docs->getBySourceTxnId(tenantId, dto.sourceReference.txnId);
                    if (winner.has_value()) {
                        auto res = handleExistingDocument(*winner, dto, editSequence, scope, services, emisorRnc, emisorRazonSocial);
                        cb(res);
                        return;
                    }
                }
                cb(json(err(std::string("Database insert error: ") + ex.what()), k500InternalServerError));
                return;
            }

            auto res = signAndSend(doc, scope, services);
            cb(res);
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

        auto doc = scope.docs->getBySourceTxnId(tenantId, txnId);
        if (!doc.has_value()) {
            Json::Value errBody;
            errBody["error"] = "Document with source TxnId '" + txnId + "' not found.";
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

        callback(json(out, k200OK));
    } catch (const std::exception& ex) {
        Json::Value errBody;
        errBody["error"] = ex.what();
        callback(json(errBody, k500InternalServerError));
    }
}

} // namespace ecf::api
