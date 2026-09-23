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

std::vector<ProcessedLineItem> normalizeCanonicalLines(const std::vector<CanonicalLineDto>& lines, double defaultTotal) {
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

        // Regla DGII: Líneas con valor 0 o precio 0 no se agregan al XML
        if (line.amount == 0.0 || rawPrice == 0.0) {
            continue;
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
        pi.unitPrice = defaultTotal;
        pi.discountAmount = 0.0;
        pi.montoItem = defaultTotal;
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
        ss << "      <IndicadorNotaCredito>0</IndicadorNotaCredito>\n";
    } else if (tipoEcf != "32") {
        std::string fechaVenc;
        if (dto.fechaVencimientoSecuencia.has_value() && !dto.fechaVencimientoSecuencia->empty()) {
            fechaVenc = normalizeFechaDgii(*dto.fechaVencimientoSecuencia);
        } else if (dto.header.fechaVencimientoSecuencia.has_value() && !dto.header.fechaVencimientoSecuencia->empty()) {
            fechaVenc = normalizeFechaDgii(*dto.header.fechaVencimientoSecuencia);
        } else {
            auto now = std::chrono::system_clock::now();
            std::time_t tt = std::chrono::system_clock::to_time_t(now);
            std::tm tm{};
#if defined(_WIN32)
            localtime_s(&tm, &tt);
#else
            localtime_r(&tt, &tm);
#endif
            int currentYear = tm.tm_year + 1900;
            int year = std::max(2027, currentYear + 1);
            fechaVenc = "31-12-" + std::to_string(year);
        }
        ss << "      <FechaVencimientoSecuencia>" << fechaVenc << "</FechaVencimientoSecuencia>\n";
    }

    if (tipoEcf == "31" || tipoEcf == "32" || tipoEcf == "33" || tipoEcf == "34" || tipoEcf == "41" || tipoEcf == "45") {
        ss << "      <IndicadorMontoGravado>0</IndicadorMontoGravado>\n";
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

    std::optional<CanonicalRetentionDto> retention;
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
    auto processedItems = normalizeCanonicalLines(dto.lines, std::max(0.0, dto.totals.montoTotal));
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
            ss << "      <DescuentoMonto>" << item.discountAmount << "</DescuentoMonto>\n"
               << "      <TablaSubDescuento>\n"
               << "        <SubDescuento>\n"
               << "          <TipoSubDescuento>$</TipoSubDescuento>\n"
               << "          <MontoSubDescuento>" << item.discountAmount << "</MontoSubDescuento>\n"
               << "        </SubDescuento>\n"
               << "      </TablaSubDescuento>\n";
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

std::string buildRfceXml(const domain::EcfDocument& doc,
                         const CanonicalDocumentDto* dto,
                         const std::string& emisorRazonSocial) {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
       << "<RFCE>\n"
       << "  <Encabezado>\n"
       << "    <Version>1.0</Version>\n"
       << "    <IdDoc>\n"
       << "      <TipoeCF>32</TipoeCF>\n"
       << "      <eNCF>" << doc.eNcf << "</eNCF>\n"
       << "      <TipoIngresos>01</TipoIngresos>\n"
       << "      <TipoPago>1</TipoPago>\n"
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

    if (doc.itbisAmount > 0.0) {
        double montoGravado = std::max(0.0, doc.totalAmount - doc.itbisAmount);
        ss << "      <MontoGravadoTotal>" << montoGravado << "</MontoGravadoTotal>\n"
           << "      <MontoGravadoI1>" << montoGravado << "</MontoGravadoI1>\n"
           << "      <TotalITBIS>" << doc.itbisAmount << "</TotalITBIS>\n"
           << "      <TotalITBIS1>" << doc.itbisAmount << "</TotalITBIS1>\n";
    } else {
        ss << "      <MontoExento>" << doc.totalAmount << "</MontoExento>\n";
    }
    ss << "      <MontoTotal>" << doc.totalAmount << "</MontoTotal>\n"
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
