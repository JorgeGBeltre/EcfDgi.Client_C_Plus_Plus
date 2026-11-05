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
#include <unordered_set>

namespace ecf::app {

namespace {

const std::unordered_map<int, int> DgiiItbisSlots = {
    {18, 1},
    {16, 2},
    {0, 3}
};

const std::unordered_set<std::string> ValidDgiiForeignCurrencyCodes = {
    "BRL", "CAD", "CHF", "CHY", "XDR", "DKK", "EUR", "GBP",
    "JPY", "NOK", "SCP", "SEK", "USD", "VEF", "HTG", "MXN", "COP"
};

double round2(double val) {
    return std::round(val * 100.0) / 100.0;
}

double round4(double val) {
    return std::round(val * 10000.0) / 10000.0;
}

std::string trimCopy(const std::string& s) {
    size_t start = 0;
    while (start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))) ++start;
    size_t end = s.size();
    while (end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))) --end;
    return s.substr(start, end - start);
}

std::string toUpperCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

bool startsWithIgnoreCase(const std::string& s, const std::string& prefix) {
    if (s.size() < prefix.size()) return false;
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (std::toupper(static_cast<unsigned char>(s[i])) !=
            std::toupper(static_cast<unsigned char>(prefix[i]))) {
            return false;
        }
    }
    return true;
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

CanonicalCurrencyResolution resolveCanonicalCurrency(const std::optional<CanonicalCurrencyDto>& currency) {
    CanonicalCurrencyResolution res;
    if (!currency.has_value()) {
        return res;
    }
    std::string rawCode = trimCopy(currency->code);
    if (rawCode.empty()) {
        return res;
    }
    std::string code = toUpperCopy(rawCode);
    if (code == "DOP" || code == "RD$" || code == "RDS") {
        return res;
    }
    if (code == "CNY") code = "CHY";
    else if (code == "US$") code = "USD";

    res.isForeign = true;
    res.code = code;
    res.exchangeRate = currency->exchangeRate;

    if (ValidDgiiForeignCurrencyCodes.find(code) == ValidDgiiForeignCurrencyCodes.end()) {
        res.validationError = "Currency.Code '" + currency->code + "' no es una moneda extranjera válida según el esquema TipoMonedaType de la DGII.";
        return res;
    }
    if (currency->exchangeRate <= 0.0 || currency->exchangeRate >= 1000.0) {
        res.validationError = "Currency.ExchangeRate debe ser mayor a 0 y menor a 1000 para comprobantes en Otra Moneda.";
        return res;
    }
    res.exchangeRate = round4(currency->exchangeRate);
    return res;
}

std::string normalizeNcfReference(const std::string& raw) {
    std::string s = trimCopy(raw);
    if (s.empty()) return s;
    std::string upper = toUpperCopy(s);

    bool allDigits = !upper.empty() && std::all_of(upper.begin(), upper.end(),
        [](unsigned char c) { return std::isdigit(c) != 0; });

    if (allDigits) {
        if (upper.length() == 9) {
            char first = upper[0];
            if (first == '1' || first == '2' || first == '3' || first == '4') {
                return "B0" + upper;
            }
            if (first == '5' || first == '6' || first == '7') {
                return "B1" + upper;
            }
        } else if (upper.length() == 10) {
            std::string p2 = upper.substr(0, 2);
            if (p2 == "01" || p2 == "02" || p2 == "03" || p2 == "04" ||
                p2 == "14" || p2 == "15" || p2 == "16" || p2 == "17") {
                return "B" + upper;
            }
        } else if (upper.length() == 8) {
            return "B02" + upper;
        }
    }
    return upper;
}

std::string escapeXml(const std::string& value) {
    if (value.empty()) return value;
    std::string out;
    out.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(value[i]);
        
        // Strip illegal XML 1.0 control characters (BUG-092)
        if (c < 0x20 && c != 0x09 && c != 0x0A && c != 0x0D) {
            continue;
        }

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

std::string truncateUtf8(const std::string& str, size_t maxChars) {
    if (str.empty() || maxChars == 0) return "";
    
    size_t charCount = 0;
    size_t byteIndex = 0;
    
    while (byteIndex < str.size() && charCount < maxChars) {
        unsigned char c = static_cast<unsigned char>(str[byteIndex]);
        size_t charLen = 1;
        if ((c & 0x80) == 0) {
            charLen = 1;
        } else if ((c & 0xE0) == 0xC0) {
            charLen = 2;
        } else if ((c & 0xF0) == 0xE0) {
            charLen = 3;
        } else if ((c & 0xF8) == 0xF0) {
            charLen = 4;
        } else {
            // Continuation byte or invalid leading byte without header; step 1 byte
            byteIndex += 1;
            charCount++;
            continue;
        }
        
        if (byteIndex + charLen > str.size()) {
            // Incomplete multi-byte sequence at string boundary; stop before splitting (BUG-092)
            break;
        }
        
        bool validContinuation = true;
        for (size_t k = 1; k < charLen; ++k) {
            unsigned char cont = static_cast<unsigned char>(str[byteIndex + k]);
            if ((cont & 0xC0) != 0x80) {
                validContinuation = false;
                break;
            }
        }
        if (!validContinuation) {
            byteIndex += 1;
            charCount++;
            continue;
        }
        
        byteIndex += charLen;
        charCount++;
    }
    
    return str.substr(0, byteIndex);
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
        double rawPrice = line.quantity > 0.0 ? line.unitPrice : (line.unitPrice != 0.0 ? line.unitPrice : line.amount);
        double rawAmount = line.amount;
        std::string rawName = !line.itemName.empty() ? line.itemName : "Item";

        // Negative line (discount in QuickBooks)
        if (rawAmount < 0.0 || rawPrice < 0.0) {
            double absDiscount = std::abs(rawAmount != 0.0 ? rawAmount : rawPrice * rawQty);
            if (!result.empty()) {
                auto& prev = result.back();
                prev.discountAmount = round2(prev.discountAmount + absDiscount);
                prev.montoItem = std::max(0.0, round2(prev.montoItem - absDiscount));
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

        double safeQty = rawQty > 0.0 ? round2(rawQty) : 1.0;
        double safePrice = std::max(0.0, round4(rawPrice));
        double safeAmount = std::max(0.0, round2(rawAmount));
        double incomingDiscount = line.discountAmount > 0.0 ? std::max(0.0, round2(line.discountAmount)) : 0.0;
        double effectiveDiscount = 0.0;

        if (incomingDiscount > 0.0) {
            double calcWithDiscount = round2((safePrice * safeQty) - incomingDiscount);
            if (safeAmount == 0.0 && safePrice > 0.0) {
                safeAmount = std::max(0.0, calcWithDiscount);
                effectiveDiscount = incomingDiscount;
            } else if (std::abs(safeAmount - calcWithDiscount) <= 0.05) {
                safeAmount = std::max(0.0, calcWithDiscount);
                effectiveDiscount = incomingDiscount;
            } else if (std::abs(round2(safeQty * safePrice) - safeAmount) <= 0.05 && safeQty > 0.0) {
                // Pre-folded net line (ERPConnector / SaaS-Ecf-Back folded discount into Amount & UnitPrice):
                // Reconstruct gross UnitPrice so CantidadItem * PrecioUnitarioItem - DescuentoMonto == MontoItem holds.
                double grossAmount = safeAmount + incomingDiscount;
                double grossPrice2 = round2(grossAmount / safeQty);
                if (std::abs(round2(safeQty * grossPrice2) - incomingDiscount - safeAmount) <= 0.01) {
                    safePrice = grossPrice2;
                    effectiveDiscount = incomingDiscount;
                } else {
                    double grossPrice4 = round4(grossAmount / safeQty);
                    if (std::abs(round2(safeQty * grossPrice4) - incomingDiscount - safeAmount) <= 0.01) {
                        safePrice = grossPrice4;
                        effectiveDiscount = incomingDiscount;
                    }
                }
            }
        } else {
            double calculatedAmount = std::max(0.0, round2(safePrice * safeQty));
            if (safeAmount == 0.0 && safePrice > 0.0) {
                safeAmount = calculatedAmount;
            } else if (std::abs(safeAmount - calculatedAmount) <= 0.05) {
                safeAmount = calculatedAmount;
            }
        }

        std::string shortName = truncateUtf8(rawName, 80);
        std::string extendedDesc;
        if (rawName.length() > 80) {
            extendedDesc = truncateUtf8(rawName, 1000);
        }

        ProcessedLineItem pi;
        pi.lineNumber = static_cast<int>(result.size()) + 1;
        pi.name = shortName;
        pi.description = extendedDesc;
        pi.quantity = safeQty;
        pi.unitPrice = safePrice;
        pi.discountAmount = effectiveDiscount;
        pi.montoItem = safeAmount;

        // Line-level IndicadorFacturacion (BUG-049)
        if (line.indicadorFacturacion.has_value() && *line.indicadorFacturacion >= 0 && *line.indicadorFacturacion <= 4) {
            pi.indicadorFacturacion = *line.indicadorFacturacion;
            pi.hasExplicitIndicador = true;
        } else if (line.taxRate.has_value()) {
            if (*line.taxRate == 18) pi.indicadorFacturacion = 1;
            else if (*line.taxRate == 16) pi.indicadorFacturacion = 2;
            else if (*line.taxRate == 0) pi.indicadorFacturacion = 4;
            else pi.indicadorFacturacion = 1;
            pi.hasExplicitIndicador = true;
        } else if (line.taxAmount.has_value()) {
            pi.indicadorFacturacion = (*line.taxAmount > 0.0) ? 1 : 4;
            pi.hasExplicitIndicador = true;
        } else {
            pi.indicadorFacturacion = 1;
            pi.hasExplicitIndicador = false;
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
        pi.hasExplicitIndicador = false;
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
    double unusedTotalDop = 0.0;
    double unusedItbisDop = 0.0;
    return buildXmlFromCanonical(dto, eNcf, emisorRnc, emisorRazonSocial, unusedTotalDop, unusedItbisDop);
}

std::string buildXmlFromCanonical(const CanonicalDocumentDto& dto,
                                  const std::string& eNcf,
                                  const std::string& emisorRnc,
                                  const std::string& emisorRazonSocial,
                                  double& computedTotalDop,
                                  double& computedItbisDop) {
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

    std::string rawRefEncf = trimCopy(dto.references.correctsENcf);
    std::string normalizedRefEncf = rawRefEncf.empty() ? "" : normalizeNcfReference(rawRefEncf);

    bool modifiesGastosMenores = (tipoEcf == "33" || tipoEcf == "34") &&
        (startsWithIgnoreCase(rawRefEncf, "E43") ||
         startsWithIgnoreCase(normalizedRefEncf, "E43") ||
         startsWithIgnoreCase(normalizedRefEncf, "B13"));

    bool modifiesExport = (tipoEcf == "33" || tipoEcf == "34") &&
        (startsWithIgnoreCase(rawRefEncf, "E46") ||
         startsWithIgnoreCase(normalizedRefEncf, "E46") ||
         startsWithIgnoreCase(normalizedRefEncf, "B16"));

    bool modifiesExpenseEcf = modifiesGastosMenores || ((tipoEcf == "33" || tipoEcf == "34") &&
        (startsWithIgnoreCase(rawRefEncf, "E41") ||
         startsWithIgnoreCase(normalizedRefEncf, "E41") ||
         startsWithIgnoreCase(normalizedRefEncf, "B11") ||
         startsWithIgnoreCase(rawRefEncf, "E47") ||
         startsWithIgnoreCase(normalizedRefEncf, "E47") ||
         startsWithIgnoreCase(normalizedRefEncf, "B17")));

    bool hasExplicitTaxedLine = false;
    for (const auto& l : dto.lines) {
        if ((l.indicadorFacturacion.has_value() && (*l.indicadorFacturacion >= 1 && *l.indicadorFacturacion <= 3)) ||
            (l.taxRate.has_value() && (*l.taxRate == 18.0 || *l.taxRate == 16.0)) ||
            (l.taxAmount.has_value() && *l.taxAmount > 0.0)) {
            hasExplicitTaxedLine = true;
            break;
        }
    }
    bool hasTaxedBucket = false;
    for (const auto& b : dto.totals.taxBuckets) {
        if (b.base > 0.0) {
            hasTaxedBucket = true;
            break;
        }
    }
    bool isPurelyExemptOrNonTaxable = modifiesGastosMenores ||
        (!hasExplicitTaxedLine && !hasTaxedBucket &&
         dto.totals.montoExento.value_or(0.0) > 0.0 &&
         dto.totals.montoGravadoTotal.value_or(0.0) == 0.0 &&
         dto.totals.montoItbis == 0.0);

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

    if ((tipoEcf == "31" || tipoEcf == "32" || tipoEcf == "33" || tipoEcf == "34" || tipoEcf == "41" || tipoEcf == "45") &&
        !isPurelyExemptOrNonTaxable) {
        ss << "      <IndicadorMontoGravado>0</IndicadorMontoGravado>\n";
    }

    if (tipoEcf != "41" && tipoEcf != "43" && tipoEcf != "47" && !modifiesExpenseEcf) {
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
    std::string safeEmisorName = truncateUtf8(emisorRazonSocial, 150);
    ss << "      <RazonSocialEmisor>" << escapeXml(safeEmisorName) << "</RazonSocialEmisor>\n";

    std::string direccionEmisor = "Distrito Nacional, SD";
    if (dto.header.direccionEmisor.has_value() && !dto.header.direccionEmisor->empty()) {
        direccionEmisor = *dto.header.direccionEmisor;
    }
    std::string safeDireccion = truncateUtf8(direccionEmisor, 100);
    ss << "      <DireccionEmisor>" << escapeXml(safeDireccion) << "</DireccionEmisor>\n";

    std::string fechaEmision = normalizeFechaDgii(dto.header.fechaEmision);
    ss << "      <FechaEmision>" << fechaEmision << "</FechaEmision>\n"
       << "    </Emisor>\n";

    if (tipoEcf != "43" && !modifiesGastosMenores) {
        ss << "    <Comprador>\n";
        std::string explicitForeignId = dto.header.identificadorExtranjero.has_value()
            ? trimCopy(*dto.header.identificadorExtranjero)
            : "";

        if (tipoEcf == "47") {
            std::string rawForeign = !explicitForeignId.empty() ? explicitForeignId : dto.header.rncComprador;
            if (!rawForeign.empty()) {
                std::string foreignId = truncateUtf8(rawForeign, 20);
                ss << "      <IdentificadorExtranjero>" << escapeXml(foreignId) << "</IdentificadorExtranjero>\n";
            }
        } else if (tipoEcf == "46") {
            if (!explicitForeignId.empty()) {
                std::string foreignId = truncateUtf8(explicitForeignId, 20);
                ss << "      <IdentificadorExtranjero>" << escapeXml(foreignId) << "</IdentificadorExtranjero>\n";
            } else if (!dto.header.rncComprador.empty()) {
                if (hasAlpha(dto.header.rncComprador)) {
                    std::string foreignId = truncateUtf8(dto.header.rncComprador, 20);
                    ss << "      <IdentificadorExtranjero>" << escapeXml(foreignId) << "</IdentificadorExtranjero>\n";
                } else {
                    std::string clean = cleanDigits(dto.header.rncComprador);
                    if (clean.length() == 9 || clean.length() == 11) {
                        ss << "      <RNCComprador>" << clean << "</RNCComprador>\n";
                    } else {
                        std::string foreignId = truncateUtf8(dto.header.rncComprador, 20);
                        ss << "      <IdentificadorExtranjero>" << escapeXml(foreignId) << "</IdentificadorExtranjero>\n";
                    }
                }
            } else {
                // Fallback obligatorio DGII para E46 cuando el cliente extranjero no posee RNC Dominicano
                ss << "      <IdentificadorExtranjero>EXTRANJERO</IdentificadorExtranjero>\n";
            }
        } else {
            if (!explicitForeignId.empty() && (tipoEcf == "32" || tipoEcf == "33" || tipoEcf == "34" || tipoEcf == "44")) {
                std::string foreignId = truncateUtf8(explicitForeignId, 20);
                ss << "      <IdentificadorExtranjero>" << escapeXml(foreignId) << "</IdentificadorExtranjero>\n";
            } else if (!dto.header.rncComprador.empty()) {
                if (hasAlpha(dto.header.rncComprador) && (tipoEcf == "32" || tipoEcf == "33" || tipoEcf == "34" || tipoEcf == "44")) {
                    std::string foreignId = truncateUtf8(dto.header.rncComprador, 20);
                    ss << "      <IdentificadorExtranjero>" << escapeXml(foreignId) << "</IdentificadorExtranjero>\n";
                } else {
                    std::string clean = cleanDigits(dto.header.rncComprador);
                    if (clean.length() == 9 || clean.length() == 11 || tipoEcf == "31" || tipoEcf == "41" || tipoEcf == "45") {
                        ss << "      <RNCComprador>" << clean << "</RNCComprador>\n";
                    }
                }
            } else if ((tipoEcf == "33" || tipoEcf == "34") &&
                       (startsWithIgnoreCase(rawRefEncf, "E46") ||
                        startsWithIgnoreCase(normalizedRefEncf, "E46") ||
                        startsWithIgnoreCase(normalizedRefEncf, "B16"))) {
                ss << "      <IdentificadorExtranjero>EXTRANJERO</IdentificadorExtranjero>\n";
            }
        }
        std::string rawComprador = dto.header.razonSocialComprador.empty()
            ? (tipoEcf == "47" ? "Beneficiario del Exterior" : (tipoEcf == "46" ? "Comprador Internacional" : "Consumidor Final"))
            : dto.header.razonSocialComprador;
        std::string safeComprador = truncateUtf8(rawComprador, 150);
        ss << "      <RazonSocialComprador>" << escapeXml(safeComprador) << "</RazonSocialComprador>\n";
        if (tipoEcf != "47" && dto.header.correoComprador.has_value() && !dto.header.correoComprador->empty()) {
            std::string rawEmail = *dto.header.correoComprador;
            size_t delimPos = rawEmail.find_first_of(";,");
            std::string primaryEmail = (delimPos != std::string::npos) ? rawEmail.substr(0, delimPos) : rawEmail;
            primaryEmail = trimCopy(primaryEmail);
            if (primaryEmail.length() > 80) primaryEmail = truncateUtf8(primaryEmail, 80);
            static const std::regex emailRegex(R"(^\w+([-+.]\w+)*@\w+([-.]\w+)*\.\w+([-.]\w+)*$)");
            if (std::regex_match(primaryEmail, emailRegex)) {
                ss << "      <CorreoComprador>" << escapeXml(primaryEmail) << "</CorreoComprador>\n";
            }
        }
        ss << "    </Comprador>\n";
    }

    auto currencyRes = resolveCanonicalCurrency(dto.currency);
    bool isForeignCurrency = currencyRes.isForeign && currencyRes.validationError.empty();
    double exchangeRate = isForeignCurrency ? currencyRes.exchangeRate : 1.0;

    bool hasOnly16Bucket = !dto.totals.taxBuckets.empty() &&
        std::all_of(dto.totals.taxBuckets.begin(), dto.totals.taxBuckets.end(),
                    [](const CanonicalTaxBucketDto& b) { return b.rate == 16; }) &&
        std::any_of(dto.totals.taxBuckets.begin(), dto.totals.taxBuckets.end(),
                    [](const CanonicalTaxBucketDto& b) { return b.base > 0.0; });

    int defaultIndicador = 1;
    if (tipoEcf == "43" || tipoEcf == "44" || tipoEcf == "47") {
        defaultIndicador = 4;
    } else if (tipoEcf == "46" || modifiesExport) {
        defaultIndicador = 3;
    } else if (isPurelyExemptOrNonTaxable) {
        defaultIndicador = 4;
    } else if (hasOnly16Bucket) {
        defaultIndicador = 2;
    }

    std::string indicadorBienoServicio = "1";
    if (tipoEcf == "47") {
        indicadorBienoServicio = "2";
    } else if (tipoEcf == "41" && dto.retention.has_value() && dto.retention->montoIsrRetenido.value_or(0.0) > 0.0) {
        indicadorBienoServicio = "2";
    }

    double fallbackLineDefault = std::max(0.0,
        (tipoEcf == "43" || tipoEcf == "44" || tipoEcf == "46" || tipoEcf == "47" || modifiesExport)
            ? dto.totals.montoTotal
            : (dto.totals.montoSubtotal > 0.0
                ? dto.totals.montoSubtotal
                : std::max(0.0, dto.totals.montoTotal - dto.totals.montoItbis)));

    auto processedItems = normalizeCanonicalLines(dto.lines, fallbackLineDefault);

    for (auto& item : processedItems) {
        int effInd = item.hasExplicitIndicador ? item.indicadorFacturacion : defaultIndicador;
        if (tipoEcf == "43" || tipoEcf == "44" || tipoEcf == "47" || modifiesGastosMenores) {
            effInd = 4;
        } else if (tipoEcf == "46" || modifiesExport) {
            effInd = (item.hasExplicitIndicador && item.indicadorFacturacion == 0) ? 0 : 3;
        } else if (isPurelyExemptOrNonTaxable) {
            effInd = 4;
        }
        item.indicadorFacturacion = effInd;

        if (isForeignCurrency) {
            item.precioOtraMoneda = round4(item.unitPrice);
            item.discountOtraMoneda = (tipoEcf != "43" && tipoEcf != "47" && item.discountAmount > 0.0)
                ? round2(item.discountAmount)
                : 0.0;
            item.montoItemOtraMoneda = round2(item.montoItem);
            item.discountDop = item.discountOtraMoneda > 0.0
                ? round2(item.discountOtraMoneda * exchangeRate)
                : 0.0;

            if (std::abs(item.quantity - 1.0) < 1e-9 && item.discountDop == 0.0) {
                item.montoItemDop = round2(item.montoItemOtraMoneda * exchangeRate);
                item.unitPriceDop = item.montoItemDop;
            } else {
                item.unitPriceDop = round4(item.precioOtraMoneda * exchangeRate);
                item.montoItemDop = std::max(0.0, round2(round2(item.quantity * item.unitPriceDop) - item.discountDop));
            }
        } else {
            item.unitPriceDop = item.unitPrice;
            item.discountDop = (tipoEcf != "43" && tipoEcf != "47") ? item.discountAmount : 0.0;
            item.montoItemDop = item.montoItem;
        }

        if (isForeignCurrency) {
            if (item.montoItbisRetenido.has_value()) {
                item.montoItbisRetenido = round2(*item.montoItbisRetenido * exchangeRate);
            }
            if (item.montoIsrRetenido.has_value()) {
                item.montoIsrRetenido = round2(*item.montoIsrRetenido * exchangeRate);
            }
        }
    }

    std::optional<CanonicalRetentionDto> effectiveRetention;
    if (tipoEcf == "31" || tipoEcf == "33" || tipoEcf == "34" || tipoEcf == "41" || tipoEcf == "47") {
        if (dto.retention.has_value()) {
            CanonicalRetentionDto r = *dto.retention;
            if (isForeignCurrency) {
                r.montoItbisRetenido = round2(r.montoItbisRetenido * exchangeRate);
                if (r.montoIsrRetenido.has_value()) {
                    r.montoIsrRetenido = round2(*r.montoIsrRetenido * exchangeRate);
                }
            }
            effectiveRetention = r;
        } else if (tipoEcf == "41" || tipoEcf == "47") {
            // DGII Rule 11160: <Retencion> is mandatory per line for E41 and E47
            CanonicalRetentionDto r;
            r.indicadorAgenteRetencionoPercepcion = 1;
            r.montoItbisRetenido = 0.0;
            if (tipoEcf == "47") r.montoIsrRetenido = 0.0;
            effectiveRetention = r;
        }
    }

    computedTotalDop = 0.0;
    computedItbisDop = 0.0;
    std::ostringstream otraMonedaSs;
    bool emitOtraMoneda = false;

    ss << "    <Totales>\n";
    if (!isForeignCurrency) {
        double total = dto.totals.montoTotal;
        double itbis = dto.totals.montoItbis;
        double exento = modifiesGastosMenores
            ? (total > 0.0 ? total : dto.totals.montoExento.value_or(dto.totals.montoSubtotal))
            : dto.totals.montoExento.value_or(0.0);
        double gravado = (modifiesGastosMenores || isPurelyExemptOrNonTaxable)
            ? 0.0
            : dto.totals.montoGravadoTotal.value_or(std::max(0.0, dto.totals.montoSubtotal - exento));

        if (tipoEcf == "43" || tipoEcf == "44") {
            if (total > 0.0) ss << "      <MontoExento>" << formatMoney2(total) << "</MontoExento>\n";
            ss << "      <MontoTotal>" << formatMoney2(total) << "</MontoTotal>\n";
            computedTotalDop = total;
        } else if (tipoEcf == "47") {
            if (total > 0.0) ss << "      <MontoExento>" << formatMoney2(total) << "</MontoExento>\n";
            ss << "      <MontoTotal>" << formatMoney2(total) << "</MontoTotal>\n";
            if (effectiveRetention.has_value() && effectiveRetention->montoIsrRetenido.has_value() && *effectiveRetention->montoIsrRetenido > 0.0) {
                ss << "      <TotalISRRetencion>" << formatMoney2(*effectiveRetention->montoIsrRetenido) << "</TotalISRRetencion>\n";
            }
            computedTotalDop = total;
        } else if (tipoEcf == "46") {
            if (total > 0.0) {
                ss << "      <MontoGravadoTotal>" << formatMoney2(total) << "</MontoGravadoTotal>\n"
                   << "      <MontoGravadoI3>" << formatMoney2(total) << "</MontoGravadoI3>\n"
                   << "      <ITBIS3>0</ITBIS3>\n"
                   << "      <TotalITBIS>0.00</TotalITBIS>\n"
                   << "      <TotalITBIS3>0.00</TotalITBIS3>\n";
            }
            ss << "      <MontoTotal>" << formatMoney2(total) << "</MontoTotal>\n";
            computedTotalDop = total;
        } else {
            if (gravado > 0.0) {
                ss << "      <MontoGravadoTotal>" << formatMoney2(gravado) << "</MontoGravadoTotal>\n";
            }

            std::optional<double> slotBase[4];
            std::optional<int> slotRate[4];
            std::optional<double> slotTax[4];

            if (!dto.totals.taxBuckets.empty() && !isPurelyExemptOrNonTaxable) {
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
                if (modifiesExport) {
                    slotBase[3] = gravado;
                    slotTax[3] = 0.0;
                    slotRate[3] = 0;
                } else {
                    slotBase[1] = gravado;
                    slotTax[1] = itbis;
                    slotRate[1] = 18;
                }
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

            if ((slotTax[1].has_value() || slotTax[2].has_value() || slotTax[3].has_value() || !isPurelyExemptOrNonTaxable) &&
                !isPurelyExemptOrNonTaxable) {
                ss << "      <TotalITBIS>" << formatMoney2(itbis) << "</TotalITBIS>\n";
            }

            for (int slot = 1; slot <= 3; ++slot) {
                if (slotTax[slot].has_value()) {
                    ss << "      <TotalITBIS" << slot << ">" << formatMoney2(*slotTax[slot]) << "</TotalITBIS" << slot << ">\n";
                }
            }

            ss << "      <MontoTotal>" << formatMoney2(total) << "</MontoTotal>\n";

            if (tipoEcf == "31" || tipoEcf == "33" || tipoEcf == "34" || tipoEcf == "41") {
                if (effectiveRetention.has_value()) {
                    if (tipoEcf == "41" || effectiveRetention->montoItbisRetenido > 0.0) {
                        ss << "      <TotalITBISRetenido>" << formatMoney2(effectiveRetention->montoItbisRetenido) << "</TotalITBISRetenido>\n";
                    }
                    if (effectiveRetention->montoIsrRetenido.has_value() && *effectiveRetention->montoIsrRetenido > 0.0) {
                        ss << "      <TotalISRRetencion>" << formatMoney2(*effectiveRetention->montoIsrRetenido) << "</TotalISRRetencion>\n";
                    }
                }
            }
            computedTotalDop = total;
            computedItbisDop = isPurelyExemptOrNonTaxable ? 0.0 : itbis;
        }
    } else {
        // Foreign currency (<OtraMoneda>): <Totales> in DOP (Moneda Nacional) and <OtraMoneda> in foreign currency
        emitOtraMoneda = true;
        otraMonedaSs << "    <OtraMoneda>\n"
                     << "      <TipoMoneda>" << escapeXml(currencyRes.code) << "</TipoMoneda>\n"
                     << "      <TipoCambio>" << formatPrice4(exchangeRate) << "</TipoCambio>\n";

        if (tipoEcf == "43" || tipoEcf == "44" || tipoEcf == "47") {
            double exentoOtra = 0.0;
            double exentoDop = 0.0;
            for (const auto& item : processedItems) {
                exentoOtra = round2(exentoOtra + item.montoItemOtraMoneda);
                exentoDop = round2(exentoDop + item.montoItemDop);
            }
            if (exentoDop > 0.0) {
                ss << "      <MontoExento>" << formatMoney2(exentoDop) << "</MontoExento>\n";
            }
            ss << "      <MontoTotal>" << formatMoney2(exentoDop) << "</MontoTotal>\n";
            if (tipoEcf == "47" && effectiveRetention.has_value() && effectiveRetention->montoIsrRetenido.has_value() && *effectiveRetention->montoIsrRetenido > 0.0) {
                ss << "      <TotalISRRetencion>" << formatMoney2(*effectiveRetention->montoIsrRetenido) << "</TotalISRRetencion>\n";
            }
            if (exentoOtra > 0.0) {
                otraMonedaSs << "      <MontoExentoOtraMoneda>" << formatMoney2(exentoOtra) << "</MontoExentoOtraMoneda>\n";
            }
            otraMonedaSs << "      <MontoTotalOtraMoneda>" << formatMoney2(exentoOtra) << "</MontoTotalOtraMoneda>\n";
            computedTotalDop = exentoDop;
            computedItbisDop = 0.0;
        } else if (tipoEcf == "46") {
            double gravado3Otra = 0.0;
            double gravado3Dop = 0.0;
            for (const auto& item : processedItems) {
                if (item.indicadorFacturacion != 0) {
                    gravado3Otra = round2(gravado3Otra + item.montoItemOtraMoneda);
                    gravado3Dop = round2(gravado3Dop + item.montoItemDop);
                }
            }
            if (gravado3Dop > 0.0) {
                ss << "      <MontoGravadoTotal>" << formatMoney2(gravado3Dop) << "</MontoGravadoTotal>\n"
                   << "      <MontoGravadoI3>" << formatMoney2(gravado3Dop) << "</MontoGravadoI3>\n"
                   << "      <ITBIS3>0</ITBIS3>\n"
                   << "      <TotalITBIS>0.00</TotalITBIS>\n"
                   << "      <TotalITBIS3>0.00</TotalITBIS3>\n";
            }
            ss << "      <MontoTotal>" << formatMoney2(gravado3Dop) << "</MontoTotal>\n";
            if (gravado3Otra > 0.0) {
                otraMonedaSs << "      <MontoGravadoTotalOtraMoneda>" << formatMoney2(gravado3Otra) << "</MontoGravadoTotalOtraMoneda>\n"
                             << "      <MontoGravado3OtraMoneda>" << formatMoney2(gravado3Otra) << "</MontoGravado3OtraMoneda>\n"
                             << "      <TotalITBISOtraMoneda>0.00</TotalITBISOtraMoneda>\n"
                             << "      <TotalITBIS3OtraMoneda>0.00</TotalITBIS3OtraMoneda>\n";
            }
            otraMonedaSs << "      <MontoTotalOtraMoneda>" << formatMoney2(gravado3Otra) << "</MontoTotalOtraMoneda>\n";
            computedTotalDop = gravado3Dop;
            computedItbisDop = 0.0;
        } else {
            std::optional<double> slotBaseOtra[4];
            std::optional<double> slotTaxOtra[4];
            std::optional<double> slotBaseDop[4];
            std::optional<double> slotTaxDop[4];
            std::optional<int> slotRate[4];
            double exentoOtra = 0.0;
            double exentoDop = 0.0;

            bool linesHaveExplicitIndicators = std::any_of(processedItems.begin(), processedItems.end(),
                [](const ProcessedLineItem& p) { return p.hasExplicitIndicador; });

            if (!linesHaveExplicitIndicators && !dto.totals.taxBuckets.empty() && !isPurelyExemptOrNonTaxable) {
                for (const auto& bucket : dto.totals.taxBuckets) {
                    auto it = DgiiItbisSlots.find(bucket.rate);
                    if (it != DgiiItbisSlots.end()) {
                        int slot = it->second;
                        slotBaseOtra[slot] = round2(slotBaseOtra[slot].value_or(0.0) + round2(bucket.base));
                        slotTaxOtra[slot] = round2(slotTaxOtra[slot].value_or(0.0) + round2(bucket.tax));
                        slotRate[slot] = bucket.rate;
                    }
                }
                exentoOtra = round2(dto.totals.montoExento.value_or(0.0));
                for (int slot = 1; slot <= 3; ++slot) {
                    if (slotBaseOtra[slot].has_value()) {
                        slotBaseDop[slot] = round2(*slotBaseOtra[slot] * exchangeRate);
                    }
                }
                exentoDop = exentoOtra > 0.0 ? round2(exentoOtra * exchangeRate) : 0.0;

                double sumLinesDop = 0.0;
                for (const auto& item : processedItems) {
                    if (item.indicadorFacturacion != 0) sumLinesDop = round2(sumLinesDop + item.montoItemDop);
                }
                double currentBaseDop = slotBaseDop[1].value_or(0.0) + slotBaseDop[2].value_or(0.0) + slotBaseDop[3].value_or(0.0) + exentoDop;
                double diffDop = round2(sumLinesDop - currentBaseDop);
                if (diffDop != 0.0 && std::abs(diffDop) <= 0.10) {
                    if (slotBaseDop[1].value_or(0.0) > 0.0) slotBaseDop[1] = round2(*slotBaseDop[1] + diffDop);
                    else if (slotBaseDop[2].value_or(0.0) > 0.0) slotBaseDop[2] = round2(*slotBaseDop[2] + diffDop);
                    else if (slotBaseDop[3].value_or(0.0) > 0.0) slotBaseDop[3] = round2(*slotBaseDop[3] + diffDop);
                    else if (exentoDop > 0.0) exentoDop = round2(exentoDop + diffDop);
                }
            } else if (!linesHaveExplicitIndicators && !isPurelyExemptOrNonTaxable &&
                       dto.totals.montoExento.value_or(0.0) > 0.0 &&
                       dto.totals.montoGravadoTotal.value_or(0.0) > 0.0) {
                double gOtra = round2(dto.totals.montoGravadoTotal.value_or(dto.totals.montoSubtotal));
                exentoOtra = round2(dto.totals.montoExento.value_or(0.0));
                slotBaseOtra[1] = gOtra;
                slotTaxOtra[1] = round2(dto.totals.montoItbis);
                slotRate[1] = 18;
                slotBaseDop[1] = round2(gOtra * exchangeRate);
                exentoDop = round2(exentoOtra * exchangeRate);

                double sumLinesDop = 0.0;
                for (const auto& item : processedItems) {
                    if (item.indicadorFacturacion != 0) sumLinesDop = round2(sumLinesDop + item.montoItemDop);
                }
                double diffDop = round2(sumLinesDop - (slotBaseDop[1].value_or(0.0) + exentoDop));
                if (diffDop != 0.0 && std::abs(diffDop) <= 0.10) {
                    slotBaseDop[1] = round2(*slotBaseDop[1] + diffDop);
                }
            } else {
                for (const auto& item : processedItems) {
                    if (item.indicadorFacturacion == 1) {
                        slotBaseOtra[1] = round2(slotBaseOtra[1].value_or(0.0) + item.montoItemOtraMoneda);
                        slotBaseDop[1] = round2(slotBaseDop[1].value_or(0.0) + item.montoItemDop);
                        slotRate[1] = 18;
                    } else if (item.indicadorFacturacion == 2) {
                        slotBaseOtra[2] = round2(slotBaseOtra[2].value_or(0.0) + item.montoItemOtraMoneda);
                        slotBaseDop[2] = round2(slotBaseDop[2].value_or(0.0) + item.montoItemDop);
                        slotRate[2] = 16;
                    } else if (item.indicadorFacturacion == 3) {
                        slotBaseOtra[3] = round2(slotBaseOtra[3].value_or(0.0) + item.montoItemOtraMoneda);
                        slotBaseDop[3] = round2(slotBaseDop[3].value_or(0.0) + item.montoItemDop);
                        slotRate[3] = 0;
                    } else if (item.indicadorFacturacion == 4) {
                        exentoOtra = round2(exentoOtra + item.montoItemOtraMoneda);
                        exentoDop = round2(exentoDop + item.montoItemDop);
                    }
                }
                if (slotBaseOtra[1].has_value()) slotTaxOtra[1] = round2(*slotBaseOtra[1] * 0.18);
                if (slotBaseOtra[2].has_value()) slotTaxOtra[2] = round2(*slotBaseOtra[2] * 0.16);
                if (slotBaseOtra[3].has_value()) slotTaxOtra[3] = 0.0;
            }

            if (slotBaseDop[1].has_value()) slotTaxDop[1] = round2(*slotBaseDop[1] * 0.18);
            if (slotBaseDop[2].has_value()) slotTaxDop[2] = round2(*slotBaseDop[2] * 0.16);
            if (slotBaseDop[3].has_value()) slotTaxDop[3] = 0.0;

            double gravadoOtra = round2(slotBaseOtra[1].value_or(0.0) + slotBaseOtra[2].value_or(0.0) + slotBaseOtra[3].value_or(0.0));
            double itbisOtra = round2(slotTaxOtra[1].value_or(0.0) + slotTaxOtra[2].value_or(0.0) + slotTaxOtra[3].value_or(0.0));
            double totalOtra = round2(gravadoOtra + exentoOtra + itbisOtra);

            double gravadoDop = round2(slotBaseDop[1].value_or(0.0) + slotBaseDop[2].value_or(0.0) + slotBaseDop[3].value_or(0.0));
            double itbisDop = round2(slotTaxDop[1].value_or(0.0) + slotTaxDop[2].value_or(0.0) + slotTaxDop[3].value_or(0.0));
            double totalDop = round2(gravadoDop + exentoDop + itbisDop);

            if (tipoEcf == "41" && effectiveRetention.has_value() && dto.retention.has_value() &&
                std::abs(dto.retention->montoItbisRetenido - dto.totals.montoItbis) <= 0.02) {
                effectiveRetention->montoItbisRetenido = itbisDop;
            }

            if (gravadoDop > 0.0) {
                ss << "      <MontoGravadoTotal>" << formatMoney2(gravadoDop) << "</MontoGravadoTotal>\n";
            }
            for (int slot = 1; slot <= 3; ++slot) {
                if (slotBaseDop[slot].has_value()) {
                    ss << "      <MontoGravadoI" << slot << ">" << formatMoney2(*slotBaseDop[slot]) << "</MontoGravadoI" << slot << ">\n";
                }
            }
            if (exentoDop > 0.0) {
                ss << "      <MontoExento>" << formatMoney2(exentoDop) << "</MontoExento>\n";
            }
            for (int slot = 1; slot <= 3; ++slot) {
                if (slotRate[slot].has_value()) {
                    ss << "      <ITBIS" << slot << ">" << *slotRate[slot] << "</ITBIS" << slot << ">\n";
                }
            }
            if (slotTaxDop[1].has_value() || slotTaxDop[2].has_value() || slotTaxDop[3].has_value()) {
                ss << "      <TotalITBIS>" << formatMoney2(itbisDop) << "</TotalITBIS>\n";
            }
            for (int slot = 1; slot <= 3; ++slot) {
                if (slotTaxDop[slot].has_value()) {
                    ss << "      <TotalITBIS" << slot << ">" << formatMoney2(*slotTaxDop[slot]) << "</TotalITBIS" << slot << ">\n";
                }
            }
            ss << "      <MontoTotal>" << formatMoney2(totalDop) << "</MontoTotal>\n";
            if (tipoEcf == "31" || tipoEcf == "33" || tipoEcf == "34" || tipoEcf == "41") {
                if (effectiveRetention.has_value()) {
                    if (tipoEcf == "41" || effectiveRetention->montoItbisRetenido > 0.0) {
                        ss << "      <TotalITBISRetenido>" << formatMoney2(effectiveRetention->montoItbisRetenido) << "</TotalITBISRetenido>\n";
                    }
                    if (effectiveRetention->montoIsrRetenido.has_value() && *effectiveRetention->montoIsrRetenido > 0.0) {
                        ss << "      <TotalISRRetencion>" << formatMoney2(*effectiveRetention->montoIsrRetenido) << "</TotalISRRetencion>\n";
                    }
                }
            }

            if (gravadoOtra > 0.0) {
                otraMonedaSs << "      <MontoGravadoTotalOtraMoneda>" << formatMoney2(gravadoOtra) << "</MontoGravadoTotalOtraMoneda>\n";
            }
            for (int slot = 1; slot <= 3; ++slot) {
                if (slotBaseOtra[slot].has_value()) {
                    otraMonedaSs << "      <MontoGravado" << slot << "OtraMoneda>" << formatMoney2(*slotBaseOtra[slot]) << "</MontoGravado" << slot << "OtraMoneda>\n";
                }
            }
            if (exentoOtra > 0.0) {
                otraMonedaSs << "      <MontoExentoOtraMoneda>" << formatMoney2(exentoOtra) << "</MontoExentoOtraMoneda>\n";
            }
            if (gravadoOtra > 0.0 || itbisOtra > 0.0) {
                otraMonedaSs << "      <TotalITBISOtraMoneda>" << formatMoney2(itbisOtra) << "</TotalITBISOtraMoneda>\n";
            }
            for (int slot = 1; slot <= 3; ++slot) {
                if (slotTaxOtra[slot].has_value()) {
                    otraMonedaSs << "      <TotalITBIS" << slot << "OtraMoneda>" << formatMoney2(*slotTaxOtra[slot]) << "</TotalITBIS" << slot << "OtraMoneda>\n";
                }
            }
            otraMonedaSs << "      <MontoTotalOtraMoneda>" << formatMoney2(totalOtra) << "</MontoTotalOtraMoneda>\n";

            computedTotalDop = totalDop;
            computedItbisDop = itbisDop;
        }
        otraMonedaSs << "    </OtraMoneda>\n";
    }
    ss << "    </Totales>\n";
    if (emitOtraMoneda) {
        ss << otraMonedaSs.str();
    }
    ss << "  </Encabezado>\n";

    // Prorate document retentions across items if items lack individual retentions
    if (effectiveRetention.has_value()) {
        bool hasLineRetentions = false;
        for (const auto& pi : processedItems) {
            if ((pi.montoItbisRetenido.has_value() && *pi.montoItbisRetenido > 0.0) ||
                (pi.montoIsrRetenido.has_value() && *pi.montoIsrRetenido > 0.0)) {
                hasLineRetentions = true;
                break;
            }
        }

        if (!hasLineRetentions && !processedItems.empty()) {
            double docItbis = effectiveRetention->montoItbisRetenido;
            double docIsr = effectiveRetention->montoIsrRetenido.value_or(0.0);

            double sumAmounts = 0.0;
            for (const auto& pi : processedItems) {
                sumAmounts += pi.montoItemDop;
            }

            double assignedItbis = 0.0;
            double assignedIsr = 0.0;

            for (size_t i = 0; i < processedItems.size(); ++i) {
                if (i == processedItems.size() - 1) {
                    processedItems[i].montoItbisRetenido = std::max(0.0, round2(docItbis - assignedItbis));
                    processedItems[i].montoIsrRetenido = std::max(0.0, round2(docIsr - assignedIsr));
                } else if (sumAmounts > 0.0) {
                    double share = processedItems[i].montoItemDop / sumAmounts;
                    double itbisShare = round2(docItbis * share);
                    double isrShare = round2(docIsr * share);
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
        ss << "    <Item>\n"
           << "      <NumeroLinea>" << item.lineNumber << "</NumeroLinea>\n"
           << "      <IndicadorFacturacion>" << item.indicadorFacturacion << "</IndicadorFacturacion>\n";

        if (effectiveRetention.has_value()) {
            appendLineRetencion(ss, item, effectiveRetention->indicadorAgenteRetencionoPercepcion, tipoEcf);
        }

        ss << "      <NombreItem>" << escapeXml(item.name) << "</NombreItem>\n"
           << "      <IndicadorBienoServicio>" << indicadorBienoServicio << "</IndicadorBienoServicio>\n";
        if (!item.description.empty()) {
            ss << "      <DescripcionItem>" << escapeXml(item.description) << "</DescripcionItem>\n";
        }
        ss << "      <CantidadItem>" << formatQty2(item.quantity) << "</CantidadItem>\n"
           << "      <PrecioUnitarioItem>" << formatPrice4(item.unitPriceDop) << "</PrecioUnitarioItem>\n";
        if (tipoEcf != "43" && tipoEcf != "47" && item.discountDop > 0.0) {
            ss << "      <DescuentoMonto>" << formatMoney2(item.discountDop) << "</DescuentoMonto>\n"
               << "      <TablaSubDescuento>\n"
               << "        <SubDescuento>\n"
               << "          <TipoSubDescuento>$</TipoSubDescuento>\n"
               << "          <MontoSubDescuento>" << formatMoney2(item.discountDop) << "</MontoSubDescuento>\n"
               << "        </SubDescuento>\n"
               << "      </TablaSubDescuento>\n";
        }
        if (isForeignCurrency) {
            ss << "      <OtraMonedaDetalle>\n"
               << "        <PrecioOtraMoneda>" << formatPrice4(item.precioOtraMoneda) << "</PrecioOtraMoneda>\n";
            if (tipoEcf != "43" && tipoEcf != "47" && item.discountOtraMoneda > 0.0) {
                ss << "        <DescuentoOtraMoneda>" << formatMoney2(item.discountOtraMoneda) << "</DescuentoOtraMoneda>\n";
            }
            ss << "        <MontoItemOtraMoneda>" << formatMoney2(item.montoItemOtraMoneda) << "</MontoItemOtraMoneda>\n"
               << "      </OtraMonedaDetalle>\n";
        }
        ss << "      <MontoItem>" << formatMoney2(item.montoItemDop) << "</MontoItem>\n"
           << "    </Item>\n";
    }
    ss << "  </DetallesItems>\n";

    // DGII Rule [613]: <InformacionReferencia> is mandatory for 33/34; for other types, ONLY allowed when CodigoModificacion == 4
    bool allowReference = (tipoEcf == "33" || tipoEcf == "34" ||
                           (dto.references.codigoModificacion.has_value() && *dto.references.codigoModificacion == 4));
    if (allowReference && !normalizedRefEncf.empty()) {
        ss << "  <InformacionReferencia>\n"
           << "    <NCFModificado>" << escapeXml(normalizedRefEncf) << "</NCFModificado>\n";
        if (dto.references.rncOtroContribuyente.has_value() && !dto.references.rncOtroContribuyente->empty()) {
            ss << "    <RNCOtroContribuyente>" << escapeXml(trimCopy(*dto.references.rncOtroContribuyente)) << "</RNCOtroContribuyente>\n";
        }
        std::string fechaNcfModificado = normalizeFechaDgii(dto.references.fechaNcfModificado.value_or(""));
        ss << "    <FechaNCFModificado>" << fechaNcfModificado << "</FechaNCFModificado>\n";
        if (dto.references.codigoModificacion.has_value()) {
            ss << "    <CodigoModificacion>" << *dto.references.codigoModificacion << "</CodigoModificacion>\n";
        } else if (tipoEcf == "33" || tipoEcf == "34") {
            ss << "    <CodigoModificacion>1</CodigoModificacion>\n";
        }
        if ((tipoEcf == "33" || tipoEcf == "34") && dto.references.razonModificacion.has_value() && !dto.references.razonModificacion->empty()) {
            std::string safeRazon = truncateUtf8(*dto.references.razonModificacion, 90);
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

    std::string safeEmisorName = truncateUtf8(emisorRazonSocial, 150);
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
    std::string safeCompradorName = truncateUtf8(compradorName, 150);
    ss << "      <RazonSocialComprador>" << escapeXml(safeCompradorName) << "</RazonSocialComprador>\n"
       << "    </Comprador>\n"
       << "    <Totales>\n";

    bool dtoIsForeign = dto && resolveCanonicalCurrency(dto->currency).isForeign;
    if (!dtoIsForeign && dto && (!dto->totals.taxBuckets.empty() || dto->totals.montoGravadoTotal.has_value() || dto->totals.montoExento.has_value())) {
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
