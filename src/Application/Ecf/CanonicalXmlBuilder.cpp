#include "CanonicalXmlBuilder.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <regex>
#include <sstream>
#include <unordered_map>

namespace ecf::app {

namespace {

const std::unordered_map<int, int> DgiiItbisSlots = {
    {18, 1},
    {16, 2},
    {0, 3}
};

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

std::tm getDominicanTm(std::time_t tt) {
    // Dominican Republic is UTC-4 year-round (no DST)
    std::time_t dom_tt = tt - (4 * 3600);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &dom_tt);
#else
    gmtime_r(&dom_tt, &tm);
#endif
    return tm;
}

std::string formatMoney2(double val) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(2) << val;
    return os.str();
}

std::string formatPrice4(double val) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(4) << val;
    std::string s = os.str();
    size_t dot = s.find('.');
    if (dot != std::string::npos) {
        while (s.size() > dot + 3 && s.back() == '0') {
            s.pop_back();
        }
    }
    return s;
}

std::string formatQty2(double val) {
    std::ostringstream os;
    os << std::fixed << std::setprecision(2) << val;
    std::string s = os.str();
    while (s.find('.') != std::string::npos && (s.back() == '0' || s.back() == '.')) {
        if (s.back() == '.') {
            s.pop_back();
            break;
        }
        s.pop_back();
    }
    return s;
}

} // namespace

std::string escapeXml(const std::string& value) {
    if (value.empty()) return value;
    std::string out;
    out.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(value[i]);
        if (c == '&') {
            out += "&amp;";
        } else if (c == '<') {
            out += "&lt;";
        } else if (c == '>') {
            out += "&gt;";
        } else if (c == '"') {
            out += "&quot;";
        } else if (c == '\'') {
            out += "&apos;";
        } else if (c == 0xC2 && i + 1 < value.size() && static_cast<unsigned char>(value[i+1]) == 0xA9) {
            out += "&#169;";
            ++i;
        } else if (c == 0xC2 && i + 1 < value.size() && static_cast<unsigned char>(value[i+1]) == 0xAE) {
            out += "&#174;";
            ++i;
        } else if (c == 0xE2 && i + 2 < value.size() && static_cast<unsigned char>(value[i+1]) == 0x82 && static_cast<unsigned char>(value[i+2]) == 0xAC) {
            out += "&#8364;";
            i += 2;
        } else {
            out += static_cast<char>(c);
        }
    }
    return out;
}

std::string normalizeFechaDgii(const std::string& value) {
    if (value.empty()) {
        auto now = std::chrono::system_clock::now();
        std::time_t tt = std::chrono::system_clock::to_time_t(now);
        std::tm tm = getDominicanTm(tt);
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

int parseDateToDays(const std::string& raw) {
    if (raw.empty()) return 0;
    std::string norm = normalizeFechaDgii(raw);
    static const std::regex rgx(R"(^(\d{2})-(\d{2})-(\d{4})$)");
    std::smatch m;
    if (std::regex_match(norm, m, rgx)) {
        int d = std::stoi(m[1].str());
        int mon = std::stoi(m[2].str());
        int y = std::stoi(m[3].str());
        std::tm tm = {};
        tm.tm_mday = d;
        tm.tm_mon = mon - 1;
        tm.tm_year = y - 1900;
        std::time_t t = std::mktime(&tm);
        if (t != static_cast<std::time_t>(-1)) {
            return static_cast<int>(t / 86400);
        }
    }
    return 0;
}

std::vector<ProcessedLineItem> normalizeCanonicalLines(const std::vector<CanonicalLineDto>& lines, double defaultTotal) {
    std::vector<ProcessedLineItem> result;

    for (const auto& line : lines) {
        double rawQty = line.quantity > 0.0 ? line.quantity : 1.0;
        double rawPrice = line.unitPrice;
        double rawAmount = line.amount;
        std::string rawName = !line.itemName.empty() ? line.itemName : "Item";

        // Negative line (discount in QuickBooks)
        if (rawAmount < 0.0 || rawPrice < 0.0) {
            double absDiscount = std::abs(rawAmount != 0.0 ? rawAmount : rawPrice * rawQty);
            if (!result.empty()) {
                auto& prev = result.back();
                prev.discountAmount += absDiscount;
                prev.montoItem = std::max(0.0, prev.montoItem - absDiscount);
                continue;
            }
        }

        // Derive price or amount if omitted
        if (rawAmount > 0.0 && rawPrice <= 0.0) {
            rawPrice = rawAmount / rawQty;
        } else if (rawAmount <= 0.0 && rawPrice > 0.0) {
            rawAmount = rawPrice * rawQty;
        }

        // Regla DGII: Líneas con valor 0 y precio 0 no se agregan al XML
        if (rawAmount <= 0.0 && rawPrice <= 0.0) {
            continue;
        }

        double safeQty = rawQty > 0.0 ? std::round(rawQty * 100.0) / 100.0 : 1.0;
        double safePrice = std::max(0.0, std::round(rawPrice * 10000.0) / 10000.0);
        double safeDiscount = line.discountAmount.has_value() ? std::max(0.0, std::round(*line.discountAmount * 100.0) / 100.0) : 0.0;
        double safeAmount = std::max(0.0, std::round(rawAmount * 100.0) / 100.0);

        double calculatedAmount = std::max(0.0, std::round(((safePrice * safeQty) - safeDiscount) * 100.0) / 100.0);

        if (safeAmount == 0.0 && safePrice > 0.0) {
            safeAmount = calculatedAmount;
        } else if (std::abs(safeAmount - calculatedAmount) <= 0.05) {
            // Reconciliar discrepancia de redondeo para garantizar la regla estricta DGII:
            // CantidadItem * PrecioUnitarioItem - DescuentoMonto = MontoItem
            safeAmount = calculatedAmount;
        }

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
        pi.discountAmount = safeDiscount;
        pi.montoItem = safeAmount;

        // Line-level IndicadorFacturacion (BUG-049)
        if (line.indicadorFacturacion.has_value() && *line.indicadorFacturacion >= 1 && *line.indicadorFacturacion <= 4) {
            pi.indicadorFacturacion = *line.indicadorFacturacion;
        } else if (line.taxRate.has_value()) {
            if (*line.taxRate == 18) pi.indicadorFacturacion = 1;
            else if (*line.taxRate == 16) pi.indicadorFacturacion = 2;
            else if (*line.taxRate == 0) {
                if (dto.totals.montoExento.value_or(0.0) > 0.0 && dto.totals.montoGravadoTotal.value_or(0.0) == 0.0) {
                    pi.indicadorFacturacion = 4;
                } else {
                    pi.indicadorFacturacion = 3;
                }
            }
            else pi.indicadorFacturacion = 1;
        } else if (line.taxAmount.has_value()) {
            if (*line.taxAmount > 0.0) {
                pi.indicadorFacturacion = 1;
            } else {
                if (dto.totals.montoExento.value_or(0.0) > 0.0) {
                    pi.indicadorFacturacion = 4;
                } else {
                    pi.indicadorFacturacion = 3;
                }
            }
        } else if (dto.totals.montoExento.value_or(0.0) > 0.0 && dto.totals.montoGravadoTotal.value_or(0.0) == 0.0) {
            pi.indicadorFacturacion = 4;
        } else {
            pi.indicadorFacturacion = 1;
        }

        pi.montoItbisRetenido = line.montoItbisRetenido;
        pi.montoIsrRetenido = line.montoIsrRetenido;

        result.push_back(pi);
    }

    if (result.empty()) {
        ProcessedLineItem pi;
        pi.lineNumber = 1;
        pi.name = "Item General";
        pi.quantity = 1.0;
        pi.unitPrice = defaultTotal;
        pi.discountAmount = 0.0;
        pi.montoItem = defaultTotal;
        pi.indicadorFacturacion = 1;
        result.push_back(pi);
    }

    return result;
}

void appendRetencion(std::ostringstream& ss, const std::optional<CanonicalRetentionDto>& retention, const std::string& tipoEcf) {
    if (!retention.has_value()) return;

    ss << "      <Retencion>\n"
       << "        <IndicadorAgenteRetencionoPercepcion>" << retention->indicadorAgenteRetencionoPercepcion << "</IndicadorAgenteRetencionoPercepcion>\n";
    if (tipoEcf == "47") {
        double isr = retention->montoIsrRetenido.value_or(0.0);
        ss << "        <MontoISRRetenido>" << formatMoney2(isr) << "</MontoISRRetenido>\n";
    } else {
        ss << "        <MontoITBISRetenido>" << formatMoney2(retention->montoItbisRetenido) << "</MontoITBISRetenido>\n";
        if (retention->montoIsrRetenido.has_value()) {
            ss << "        <MontoISRRetenido>" << formatMoney2(*retention->montoIsrRetenido) << "</MontoISRRetenido>\n";
        }
    }
    ss << "      </Retencion>\n";
}

void appendLineRetencion(std::ostringstream& ss,
                         const ProcessedLineItem& item,
                         int indicadorAgente,
                         const std::string& tipoEcf) {
    double itbis = item.montoItbisRetenido.value_or(0.0);
    double isr = item.montoIsrRetenido.value_or(0.0);

    if (tipoEcf == "47") {
        ss << "      <Retencion>\n"
           << "        <IndicadorAgenteRetencionoPercepcion>" << indicadorAgente << "</IndicadorAgenteRetencionoPercepcion>\n"
           << "        <MontoISRRetenido>" << formatMoney2(isr) << "</MontoISRRetenido>\n"
           << "      </Retencion>\n";
    } else if (tipoEcf == "41") {
        ss << "      <Retencion>\n"
           << "        <IndicadorAgenteRetencionoPercepcion>" << indicadorAgente << "</IndicadorAgenteRetencionoPercepcion>\n"
           << "        <MontoITBISRetenido>" << formatMoney2(itbis) << "</MontoITBISRetenido>\n";
        if (isr > 0.0) {
            ss << "        <MontoISRRetenido>" << formatMoney2(isr) << "</MontoISRRetenido>\n";
        }
        ss << "      </Retencion>\n";
    } else if (tipoEcf == "31" || tipoEcf == "33" || tipoEcf == "34") {
        if (itbis > 0.0 || isr > 0.0) {
            ss << "      <Retencion>\n"
               << "        <IndicadorAgenteRetencionoPercepcion>" << indicadorAgente << "</IndicadorAgenteRetencionoPercepcion>\n";
            if (itbis > 0.0) {
                ss << "        <MontoITBISRetenido>" << formatMoney2(itbis) << "</MontoITBISRetenido>\n";
            }
            if (isr > 0.0) {
                ss << "        <MontoISRRetenido>" << formatMoney2(isr) << "</MontoISRRetenido>\n";
            }
            ss << "      </Retencion>\n";
        }
    }
}

std::string buildXmlFromCanonical(const CanonicalDocumentDto& dto,
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
        int inc = 0;
        if (dto.references.indicadorNotaCredito.has_value()) {
            inc = *dto.references.indicadorNotaCredito;
        } else if (dto.references.fechaNcfModificado.has_value() && !dto.references.fechaNcfModificado->empty()) {
            int issueDays = parseDateToDays(dto.header.fechaEmision);
            int modDays = parseDateToDays(*dto.references.fechaNcfModificado);
            if (issueDays > 0 && modDays > 0 && (issueDays - modDays) > 30) {
                inc = 1;
            }
        }
        ss << "      <IndicadorNotaCredito>" << inc << "</IndicadorNotaCredito>\n";
    } else if (tipoEcf != "32") {
        std::string fechaVenc;
        if (dto.fechaVencimientoSecuencia.has_value() && !dto.fechaVencimientoSecuencia->empty()) {
            fechaVenc = normalizeFechaDgii(*dto.fechaVencimientoSecuencia);
        } else if (dto.header.fechaVencimientoSecuencia.has_value() && !dto.header.fechaVencimientoSecuencia->empty()) {
            fechaVenc = normalizeFechaDgii(*dto.header.fechaVencimientoSecuencia);
        } else {
            // Si el ambiente es pruebas (PreCertificación/Certificación), la vigencia en DGII es fija a 31-12-2028.
            // En producción, es dinámicamente según normativa DGII (31 de diciembre del año posterior).
            bool isTestOrCert = false;
            if (dto.environment.has_value()) {
                std::string env = *dto.environment;
                std::transform(env.begin(), env.end(), env.begin(), ::tolower);
                if (env == "precertificacion" || env == "certificacion" || env == "test" || env == "cert") {
                    isTestOrCert = true;
                }
            }
            int year = 0;
            if (isTestOrCert) {
                year = 2028;
            } else {
                auto now = std::chrono::system_clock::now();
                std::time_t tt = std::chrono::system_clock::to_time_t(now);
                std::tm tm = getDominicanTm(tt);
                int currentYear = tm.tm_year + 1900;
                year = std::max(2027, currentYear + 1);
            }
            fechaVenc = "31-12-" + std::to_string(year);
        }
        ss << "      <FechaVencimientoSecuencia>" << fechaVenc << "</FechaVencimientoSecuencia>\n";
    }

    if (tipoEcf == "31" || tipoEcf == "32" || tipoEcf == "33" || tipoEcf == "34" || tipoEcf == "41" || tipoEcf == "45") {
        ss << "      <IndicadorMontoGravado>0</IndicadorMontoGravado>\n";
    }

    if (tipoEcf != "41" && tipoEcf != "43" && tipoEcf != "47") {
        std::string tipoIngresoVal;
        if (dto.header.tipoIngresos.has_value() && !dto.header.tipoIngresos->empty()) {
            tipoIngresoVal = *dto.header.tipoIngresos;
            if (tipoIngresoVal.length() == 1) tipoIngresoVal = "0" + tipoIngresoVal;
        } else {
            tipoIngresoVal = (tipoEcf == "46") ? "02" : "01";
        }
        ss << "      <TipoIngresos>" << tipoIngresoVal << "</TipoIngresos>\n";
    }

    int tipoPagoVal = 1;
    if (dto.header.tipoPago.has_value() && *dto.header.tipoPago >= 1 && *dto.header.tipoPago <= 3) {
        tipoPagoVal = *dto.header.tipoPago;
    }
    ss << "      <TipoPago>" << tipoPagoVal << "</TipoPago>\n";
    if (tipoPagoVal == 2 && dto.header.fechaLimitePago.has_value() && !dto.header.fechaLimitePago->empty()) {
        ss << "      <FechaLimitePago>" << normalizeFechaDgii(*dto.header.fechaLimitePago) << "</FechaLimitePago>\n";
    }
    ss << "    </IdDoc>\n";

    ss << "    <Emisor>\n"
       << "      <RNCEmisor>" << emisorRnc << "</RNCEmisor>\n";
    std::string safeEmisorName = emisorRazonSocial.length() > 150 ? emisorRazonSocial.substr(0, 150) : emisorRazonSocial;
    ss << "      <RazonSocialEmisor>" << escapeXml(safeEmisorName) << "</RazonSocialEmisor>\n";

    std::string direccionEmisor = "Distrito Nacional, SD";
    if (dto.header.direccionEmisor.has_value() && !dto.header.direccionEmisor->empty()) {
        direccionEmisor = *dto.header.direccionEmisor;
    }
    std::string safeDireccion = direccionEmisor.length() > 100 ? direccionEmisor.substr(0, 100) : direccionEmisor;
    ss << "      <DireccionEmisor>" << escapeXml(safeDireccion) << "</DireccionEmisor>\n";

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
            } else {
                // Fallback obligatorio DGII para E46 cuando el cliente extranjero no posee RNC Dominicano
                ss << "      <IdentificadorExtranjero>EXTRANJERO</IdentificadorExtranjero>\n";
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
        ss << "      <RazonSocialComprador>" << escapeXml(safeComprador) << "</RazonSocialComprador>\n";
        if (tipoEcf != "47" && dto.header.correoComprador.has_value() && !dto.header.correoComprador->empty()) {
            std::string rawEmail = *dto.header.correoComprador;
            size_t delimPos = rawEmail.find_first_of(";,");
            std::string primaryEmail = (delimPos != std::string::npos) ? rawEmail.substr(0, delimPos) : rawEmail;
            while (!primaryEmail.empty() && std::isspace(static_cast<unsigned char>(primaryEmail.front()))) primaryEmail.erase(primaryEmail.begin());
            while (!primaryEmail.empty() && std::isspace(static_cast<unsigned char>(primaryEmail.back()))) primaryEmail.pop_back();
            if (primaryEmail.length() > 80) primaryEmail = primaryEmail.substr(0, 80);
            static const std::regex emailRegex(R"(^\w+([-+.]\w+)*@\w+([-.]\w+)*\.\w+([-.]\w+)*$)");
            if (std::regex_match(primaryEmail, emailRegex)) {
                ss << "      <CorreoComprador>" << escapeXml(primaryEmail) << "</CorreoComprador>\n";
            }
        }
        ss << "    </Comprador>\n";
    }

    ss << "    <Totales>\n";
    double total = dto.totals.montoTotal;
    double itbis = dto.totals.montoItbis;
    double gravado = dto.totals.montoGravadoTotal.value_or(dto.totals.montoSubtotal);
    double exento = dto.totals.montoExento.value_or(0.0);

    if (tipoEcf == "43" || tipoEcf == "44") {
        if (total > 0.0) ss << "      <MontoExento>" << formatMoney2(total) << "</MontoExento>\n";
        ss << "      <MontoTotal>" << formatMoney2(total) << "</MontoTotal>\n";
    } else if (tipoEcf == "47") {
        if (total > 0.0) ss << "      <MontoExento>" << formatMoney2(total) << "</MontoExento>\n";
        ss << "      <MontoTotal>" << formatMoney2(total) << "</MontoTotal>\n";
        if (dto.retention.has_value() && dto.retention->montoIsrRetenido.has_value() && *dto.retention->montoIsrRetenido > 0.0) {
            ss << "      <TotalISRRetencion>" << formatMoney2(*dto.retention->montoIsrRetenido) << "</TotalISRRetencion>\n";
        }
    } else if (tipoEcf == "46") {
        if (total > 0.0) {
            ss << "      <MontoGravadoTotal>" << formatMoney2(total) << "</MontoGravadoTotal>\n"
               << "      <MontoGravadoI3>" << formatMoney2(total) << "</MontoGravadoI3>\n"
               << "      <ITBIS3>0</ITBIS3>\n"
               << "      <TotalITBIS>0.00</TotalITBIS>\n"
               << "      <TotalITBIS3>0.00</TotalITBIS3>\n";
        }
        ss << "      <MontoTotal>" << formatMoney2(total) << "</MontoTotal>\n";
    } else {
        if (gravado > 0.0) {
            ss << "      <MontoGravadoTotal>" << formatMoney2(gravado) << "</MontoGravadoTotal>\n";
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
            slotRate[1] = 18;
        }

        for (int slot = 1; slot <= 3; ++slot) {
            if (slotBase[slot].has_value()) {
                ss << "      <MontoGravadoI" << slot << ">" << formatMoney2(*slotBase[slot]) << "</MontoGravadoI" << slot << ">\n";
            }
        }

        if (exento > 0.0) {
            ss << "      <MontoExento>" << formatMoney2(exento) << "</MontoExento>\n";
        }

        for (int slot = 1; slot <= 3; ++slot) {
            if (slotRate[slot].has_value()) {
                ss << "      <ITBIS" << slot << ">" << *slotRate[slot] << "</ITBIS" << slot << ">\n";
            }
        }

        ss << "      <TotalITBIS>" << formatMoney2(itbis) << "</TotalITBIS>\n";

        for (int slot = 1; slot <= 3; ++slot) {
            if (slotTax[slot].has_value()) {
                ss << "      <TotalITBIS" << slot << ">" << formatMoney2(*slotTax[slot]) << "</TotalITBIS" << slot << ">\n";
            }
        }

        ss << "      <MontoTotal>" << formatMoney2(total) << "</MontoTotal>\n";

        if (tipoEcf == "31" || tipoEcf == "33" || tipoEcf == "34" || tipoEcf == "41") {
            if (dto.retention.has_value()) {
                if (dto.retention->montoItbisRetenido > 0.0) {
                    ss << "      <TotalITBISRetenido>" << formatMoney2(dto.retention->montoItbisRetenido) << "</TotalITBISRetenido>\n";
                }
                if (dto.retention->montoIsrRetenido.has_value() && *dto.retention->montoIsrRetenido > 0.0) {
                    ss << "      <TotalISRRetencion>" << formatMoney2(*dto.retention->montoIsrRetenido) << "</TotalISRRetencion>\n";
                }
            }
        }
    }
    ss << "    </Totales>\n"
       << "  </Encabezado>\n";

    std::string indicadorBienoServicio = "1";
    if (tipoEcf == "47") {
        indicadorBienoServicio = "2";
    } else if (tipoEcf == "41" && dto.retention.has_value() && dto.retention->montoIsrRetenido.value_or(0.0) > 0.0) {
        indicadorBienoServicio = "2";
    }

    auto processedItems = normalizeCanonicalLines(dto.lines, std::max(0.0, dto.totals.montoTotal));

    // Prorate document retentions across items if items lack individual retentions
    if (dto.retention.has_value()) {
        bool hasLineRetentions = false;
        for (const auto& pi : processedItems) {
            if ((pi.montoItbisRetenido.has_value() && *pi.montoItbisRetenido > 0.0) ||
                (pi.montoIsrRetenido.has_value() && *pi.montoIsrRetenido > 0.0)) {
                hasLineRetentions = true;
                break;
            }
        }

        if (!hasLineRetentions && !processedItems.empty()) {
            double docItbis = dto.retention->montoItbisRetenido;
            double docIsr = dto.retention->montoIsrRetenido.value_or(0.0);

            double sumAmounts = 0.0;
            for (const auto& pi : processedItems) {
                sumAmounts += pi.montoItem;
            }

            double assignedItbis = 0.0;
            double assignedIsr = 0.0;

            for (size_t i = 0; i < processedItems.size(); ++i) {
                if (i == processedItems.size() - 1) {
                    processedItems[i].montoItbisRetenido = std::max(0.0, std::round((docItbis - assignedItbis) * 100.0) / 100.0);
                    processedItems[i].montoIsrRetenido = std::max(0.0, std::round((docIsr - assignedIsr) * 100.0) / 100.0);
                } else if (sumAmounts > 0.0) {
                    double share = processedItems[i].montoItem / sumAmounts;
                    double itbisShare = std::round(docItbis * share * 100.0) / 100.0;
                    double isrShare = std::round(docIsr * share * 100.0) / 100.0;
                    processedItems[i].montoItbisRetenido = itbisShare;
                    processedItems[i].montoIsrRetenido = isrShare;
                    assignedItbis += itbisShare;
                    assignedIsr += isrShare;
                } else {
                    processedItems[i].montoItbisRetenido = 0.0;
                    processedItems[i].montoIsrRetenido = 0.0;
                }
            }
        }
    }

    ss << "  <DetallesItems>\n";
    for (const auto& item : processedItems) {
        int itemIndicador = item.indicadorFacturacion;
        if (tipoEcf == "43" || tipoEcf == "44" || tipoEcf == "47") {
            itemIndicador = 4;
        } else if (tipoEcf == "46") {
            itemIndicador = 3;
        } else if (dto.totals.montoExento.value_or(0.0) > 0.0 &&
                   dto.totals.montoGravadoTotal.value_or(0.0) == 0.0 &&
                   dto.totals.montoItbis == 0.0) {
            itemIndicador = 4;
        }

        ss << "    <Item>\n"
           << "      <NumeroLinea>" << item.lineNumber << "</NumeroLinea>\n"
           << "      <IndicadorFacturacion>" << itemIndicador << "</IndicadorFacturacion>\n";

        if (dto.retention.has_value()) {
            appendLineRetencion(ss, item, dto.retention->indicadorAgenteRetencionoPercepcion, tipoEcf);
        }

        ss << "      <NombreItem>" << escapeXml(item.name) << "</NombreItem>\n"
           << "      <IndicadorBienoServicio>" << indicadorBienoServicio << "</IndicadorBienoServicio>\n";
        if (!item.description.empty()) {
            ss << "      <DescripcionItem>" << escapeXml(item.description) << "</DescripcionItem>\n";
        }
        ss << "      <CantidadItem>" << formatQty2(item.quantity) << "</CantidadItem>\n"
           << "      <PrecioUnitarioItem>" << formatPrice4(item.unitPrice) << "</PrecioUnitarioItem>\n";
        if (tipoEcf != "43" && tipoEcf != "47" && item.discountAmount > 0.0) {
            ss << "      <DescuentoMonto>" << formatMoney2(item.discountAmount) << "</DescuentoMonto>\n"
               << "      <TablaSubDescuento>\n"
               << "        <SubDescuento>\n"
               << "          <TipoSubDescuento>$</TipoSubDescuento>\n"
               << "          <MontoSubDescuento>" << formatMoney2(item.discountAmount) << "</MontoSubDescuento>\n"
               << "        </SubDescuento>\n"
               << "      </TablaSubDescuento>\n";
        }
        ss << "      <MontoItem>" << formatMoney2(item.montoItem) << "</MontoItem>\n"
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
    std::tm tm = getDominicanTm(tt);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%d-%m-%Y %H:%M:%S", &tm);
    ss << "  <FechaHoraFirma>" << buf << "</FechaHoraFirma>\n"
       << "</ECF>\n";

    return ss.str();
}

std::string buildRfceXml(const domain::EcfDocument& doc,
                         const CanonicalDocumentDto* dto,
                         const std::string& emisorRazonSocial) {
    std::ostringstream ss;
    ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
       << "<RFCE>\n"
       << "  <Encabezado>\n"
       << "    <Version>1.0</Version>\n"
       << "    <IdDoc>\n"
       << "      <TipoeCF>32</TipoeCF>\n"
       << "      <eNCF>" << doc.eNcf << "</eNCF>\n";
    std::string rfceTipoIngreso = "01";
    if (dto && dto->header.tipoIngresos.has_value() && !dto->header.tipoIngresos->empty()) {
        rfceTipoIngreso = *dto->header.tipoIngresos;
        if (rfceTipoIngreso.length() == 1) rfceTipoIngreso = "0" + rfceTipoIngreso;
    }
    int rfceTipoPago = 1;
    if (dto && dto->header.tipoPago.has_value() && *dto->header.tipoPago >= 1 && *dto->header.tipoPago <= 3) {
        rfceTipoPago = *dto->header.tipoPago;
    }
    ss << "      <TipoIngresos>" << rfceTipoIngreso << "</TipoIngresos>\n"
       << "      <TipoPago>" << rfceTipoPago << "</TipoPago>\n"
       << "    </IdDoc>\n"
       << "    <Emisor>\n"
       << "      <RNCEmisor>" << doc.rncEmisor << "</RNCEmisor>\n";

    std::string safeEmisorName = emisorRazonSocial.length() > 150 ? emisorRazonSocial.substr(0, 150) : emisorRazonSocial;
    ss << "      <RazonSocialEmisor>" << escapeXml(safeEmisorName) << "</RazonSocialEmisor>\n";

    std::string rawFechaEmision = (dto && !dto->header.fechaEmision.empty()) ? dto->header.fechaEmision : "";
    std::string fechaEmision = normalizeFechaDgii(rawFechaEmision);
    ss << "      <FechaEmision>" << fechaEmision << "</FechaEmision>\n"
       << "    </Emisor>\n"
       << "    <Comprador>\n";

    if (doc.rncComprador.has_value() && !doc.rncComprador->empty()) {
        std::string cleanRnc = cleanDigits(*doc.rncComprador);
        if (cleanRnc.length() == 9 || cleanRnc.length() == 11) {
            ss << "      <RNCComprador>" << cleanRnc << "</RNCComprador>\n";
        }
    }

    std::string compradorName = (dto && !dto->header.razonSocialComprador.empty())
        ? dto->header.razonSocialComprador
        : "CONSUMIDOR FINAL";
    std::string safeCompradorName = compradorName.length() > 150 ? compradorName.substr(0, 150) : compradorName;
    ss << "      <RazonSocialComprador>" << escapeXml(safeCompradorName) << "</RazonSocialComprador>\n"
       << "    </Comprador>\n"
       << "    <Totales>\n";

    if (dto && (!dto->totals.taxBuckets.empty() || dto->totals.montoGravadoTotal.has_value() || dto->totals.montoExento.has_value())) {
        std::optional<double> slotBase[4];
        std::optional<double> slotTax[4];
        double gravadoTotal = dto->totals.montoGravadoTotal.value_or(0.0);
        double exentoTotal = dto->totals.montoExento.value_or(0.0);
        double itbisTotal = dto->totals.montoItbis;

        if (!dto->totals.taxBuckets.empty()) {
            for (const auto& bucket : dto->totals.taxBuckets) {
                auto it = DgiiItbisSlots.find(bucket.rate);
                if (it != DgiiItbisSlots.end()) {
                    int slot = it->second;
                    slotBase[slot] = slotBase[slot].value_or(0.0) + bucket.base;
                    slotTax[slot] = slotTax[slot].value_or(0.0) + bucket.tax;
                }
            }
            if (gravadoTotal == 0.0) {
                gravadoTotal = slotBase[1].value_or(0.0) + slotBase[2].value_or(0.0) + slotBase[3].value_or(0.0);
            }
        } else if (doc.itbisAmount > 0.0) {
            double base = std::max(0.0, doc.totalAmount - doc.itbisAmount);
            slotBase[1] = base;
            slotTax[1] = doc.itbisAmount;
            gravadoTotal = base;
        } else {
            exentoTotal = doc.totalAmount;
        }

        if (gravadoTotal > 0.0) {
            ss << "      <MontoGravadoTotal>" << formatMoney2(gravadoTotal) << "</MontoGravadoTotal>\n";
        }
        for (int slot = 1; slot <= 3; ++slot) {
            if (slotBase[slot].has_value() && *slotBase[slot] > 0.0) {
                ss << "      <MontoGravadoI" << slot << ">" << formatMoney2(*slotBase[slot]) << "</MontoGravadoI" << slot << ">\n";
            }
        }
        if (exentoTotal > 0.0) {
            ss << "      <MontoExento>" << formatMoney2(exentoTotal) << "</MontoExento>\n";
        }
        if (itbisTotal > 0.0 || (slotTax[1].value_or(0.0) + slotTax[2].value_or(0.0) + slotTax[3].value_or(0.0)) > 0.0) {
            double totalItbisVal = itbisTotal > 0.0 ? itbisTotal : (slotTax[1].value_or(0.0) + slotTax[2].value_or(0.0) + slotTax[3].value_or(0.0));
            ss << "      <TotalITBIS>" << formatMoney2(totalItbisVal) << "</TotalITBIS>\n";
            for (int slot = 1; slot <= 3; ++slot) {
                if (slotTax[slot].has_value() && *slotTax[slot] > 0.0) {
                    ss << "      <TotalITBIS" << slot << ">" << formatMoney2(*slotTax[slot]) << "</TotalITBIS" << slot << ">\n";
                }
            }
        }
    } else if (doc.itbisAmount > 0.0) {
        double montoGravado = std::max(0.0, doc.totalAmount - doc.itbisAmount);
        ss << "      <MontoGravadoTotal>" << formatMoney2(montoGravado) << "</MontoGravadoTotal>\n"
           << "      <MontoGravadoI1>" << formatMoney2(montoGravado) << "</MontoGravadoI1>\n"
           << "      <TotalITBIS>" << formatMoney2(doc.itbisAmount) << "</TotalITBIS>\n"
           << "      <TotalITBIS1>" << formatMoney2(doc.itbisAmount) << "</TotalITBIS1>\n";
    } else {
        ss << "      <MontoExento>" << formatMoney2(doc.totalAmount) << "</MontoExento>\n";
    }
    ss << "      <MontoTotal>" << formatMoney2(doc.totalAmount) << "</MontoTotal>\n"
       << "    </Totales>\n"
       << "    <CodigoSeguridadeCF>" << doc.securityCode.value_or("") << "</CodigoSeguridadeCF>\n"
       << "  </Encabezado>\n"
       << "</RFCE>";

    std::string result = ss.str();
    while (!result.empty() && std::isspace(static_cast<unsigned char>(result.back()))) {
        result.pop_back();
    }
    return result;
}

} // namespace ecf::app
