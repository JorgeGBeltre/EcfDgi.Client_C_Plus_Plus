#include "Application/Services/EcfValidator.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <ctime>

namespace ecf::app {

using namespace ecf::domain;

namespace {

bool tryParseDate(const std::string& s, const char* fmt) {
    std::tm tm{};
    // Portable check for the "dd-MM-yyyy" format via sscanf.
    (void)fmt;
    int d, m, y;
    if (std::sscanf(s.c_str(), "%2d-%2d-%4d", &d, &m, &y) != 3) return false;
    if (m < 1 || m > 12 || d < 1 || d > 31) return false;
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
