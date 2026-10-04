#include "Application/Services/EcfValidator.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <initializer_list>
#include <cstdio>
#include <ctime>

namespace ecf::app {

using namespace ecf::domain;

namespace {

bool isLeapYear(int y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

int daysInMonth(int m, int y) {
    switch (m) {
        case 1: case 3: case 5: case 7: case 8: case 10: case 12: return 31;
        case 4: case 6: case 9: case 11: return 30;
        case 2: return isLeapYear(y) ? 29 : 28;
        default: return 0;
    }
}

bool tryParseDate(const std::string& s, const char* fmt) {
    (void)fmt;
    if (s.size() != 10) return false;
    if (s[2] != '-' || s[5] != '-') return false;
    for (size_t i = 0; i < 10; ++i) {
        if (i == 2 || i == 5) continue;
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
    }
    int d = std::stoi(s.substr(0, 2));
    int m = std::stoi(s.substr(3, 2));
    int y = std::stoi(s.substr(6, 4));
    if (y < 1900 || y > 2100) return false;
    if (m < 1 || m > 12) return false;
    if (d < 1 || d > daysInMonth(m, y)) return false;
    return true;
}

std::string money2(double v) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.2f", v);
    return std::string(buf);
}

}  // namespace

bool EcfValidator::isValidRnc(const std::string& rnc) {
    if (rnc.empty()) return false;
    if (rnc.size() != 9 && rnc.size() != 11) return false;
    return std::all_of(rnc.begin(), rnc.end(),
                       [](unsigned char c) { return std::isdigit(c) != 0; });
}

void EcfValidator::validateTotalesConsistency(const RfceTotales& t,
                                              std::vector<std::string>& errors) {
    // BUG-111: Fundamental consistency: MontoTotal must equal MontoGravadoTotal + TotalITBIS + MontoExento (+ MontoImpuestoAdicional)
    if (t.montoGravadoTotal.has_value() || t.totalITBIS.has_value() || t.montoExento.has_value()) {
        double expectedTotal = t.montoGravadoTotal.value_or(0.0) +
                               t.totalITBIS.value_or(0.0) +
                               t.montoExento.value_or(0.0) +
                               t.montoImpuestoAdicional.value_or(0.0);
        if (std::fabs(t.montoTotal - expectedTotal) > 0.01) {
            errors.push_back("MontoTotal no coincide con la suma de MontoGravadoTotal + TotalITBIS + MontoExento.");
        }
    }

    // The per-rate breakdown fields are optional. Only cross-check the aggregate
    // against the breakdown when at least one breakdown component is provided;
    // a document that carries only the aggregate total is valid on its own.
    if (t.montoGravadoTotal.has_value() &&
        (t.montoGravadoI1.has_value() || t.montoGravadoI2.has_value() ||
         t.montoGravadoI3.has_value())) {
        double expected = t.montoGravadoI1.value_or(0) + t.montoGravadoI2.value_or(0) +
                          t.montoGravadoI3.value_or(0);
        if (std::fabs(*t.montoGravadoTotal - expected) > 0.01)
            errors.push_back(
                "MontoGravadoTotal no coincide con la suma de MontoGravadoI1+I2+I3.");
    }

    if (t.totalITBIS.has_value() &&
        (t.totalITBIS1.has_value() || t.totalITBIS2.has_value() ||
         t.totalITBIS3.has_value())) {
        double expected = t.totalITBIS1.value_or(0) + t.totalITBIS2.value_or(0) +
                          t.totalITBIS3.value_or(0);
        if (std::fabs(*t.totalITBIS - expected) > 0.01)
            errors.push_back("TotalITBIS no coincide con la suma de TotalITBIS1+2+3.");
    }

    if (t.montoPeriodo.has_value() && t.montoNoFacturable.has_value()) {
        double expected = t.montoTotal + *t.montoNoFacturable;
        if (std::fabs(*t.montoPeriodo - expected) > 0.01)
            errors.push_back("MontoPeriodo debe ser igual a MontoTotal + MontoNoFacturable.");
    }
}

ValidationResult EcfValidator::validateRfce(const Rfce& rfce) {
    std::vector<std::string> errors;
    const auto& e = rfce.encabezado;

    if (!isValidRnc(e.emisor.rncEmisor))
        errors.push_back("RNCEmisor inválido: debe tener 9 u 11 dígitos numéricos.");

    if (e.idDoc.eNcf.size() != 13)
        errors.push_back("eNCF inválido: debe tener exactamente 13 caracteres.");

    if (e.idDoc.tipoeCF != "32")
        errors.push_back("TipoeCF debe ser 32 para RFCE.");

    if (!tryParseDate(e.emisor.fechaEmision, "dd-MM-yyyy"))
        errors.push_back("FechaEmision inválida: formato requerido dd-MM-AAAA.");

    if (e.totales.montoTotal < 0)
        errors.push_back("MontoTotal no puede ser negativo.");

    if (e.totales.montoTotal >= kRfceThreshold)
        errors.push_back("MontoTotal >= " + money2(kRfceThreshold) +
                         ": usar recepcion (e-CF completo), no recepcionfc.");

    if (e.comprador.has_value()) {
        const auto& c = *e.comprador;
        if (c.rncComprador && !c.rncComprador->empty() &&
            c.identificadorExtranjero && !c.identificadorExtranjero->empty())
            errors.push_back(
                "RNCComprador e IdentificadorExtranjero son mutuamente excluyentes.");
    }

    validateTotalesConsistency(e.totales, errors);

    if (e.idDoc.tablaFormasPago.size() > 7)
        errors.push_back("TablaFormasPago no puede tener más de 7 items.");

    if (e.totales.impuestosAdicionales.size() > 20)
        errors.push_back("ImpuestosAdicionales no puede tener más de 20 items.");

    return ValidationResult(std::move(errors));
}

}  // namespace ecf::app
