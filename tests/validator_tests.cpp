// Minimal tests for EcfValidator.
// No external test framework — returns non-zero on the first failure.

#include <cstdio>
#include <string>

#include "Application/Services/EcfValidator.h"
#include "Domain/Entities/Rfce.h"

using namespace ecf;

static int failures = 0;
#define CHECK(cond, msg)                                        \
    do {                                                        \
        if (!(cond)) {                                          \
            std::printf("FAIL: %s\n", msg);                     \
            ++failures;                                         \
        } else {                                                \
            std::printf("ok:   %s\n", msg);                     \
        }                                                       \
    } while (0)

static domain::Rfce makeValidRfce() {
    domain::Rfce r;
    r.encabezado.idDoc.tipoeCF = "32";
    r.encabezado.idDoc.eNcf = "E320000000001";  // 13 chars
    r.encabezado.emisor.rncEmisor = "101672919";  // 9 digits
    r.encabezado.emisor.razonSocialEmisor = "ACME SRL";
    r.encabezado.emisor.fechaEmision = "30-06-2026";  // dd-MM-yyyy
    r.encabezado.totales.montoTotal = 1180.00;
    r.encabezado.totales.totalITBIS = 180.00;
    return r;
}

int main() {
    app::EcfValidator validator;

    {
        auto rfce = makeValidRfce();
        auto result = validator.validateRfce(rfce);
        CHECK(result.isValid(), "valid RFCE passes validation");
    }

    {
        auto rfce = makeValidRfce();
        rfce.encabezado.emisor.rncEmisor = "12A45";  // invalid
        auto result = validator.validateRfce(rfce);
        CHECK(!result.isValid(), "invalid RNC is rejected");
    }

    {
        auto rfce = makeValidRfce();
        rfce.encabezado.idDoc.eNcf = "SHORT";  // not 13 chars
        auto result = validator.validateRfce(rfce);
        CHECK(!result.isValid(), "wrong-length eNCF is rejected");
    }

    {
        auto rfce = makeValidRfce();
        rfce.encabezado.totales.montoTotal = 300000.0;  // >= 250k threshold
        auto result = validator.validateRfce(rfce);
        CHECK(!result.isValid(), "amount over RFCE threshold is rejected");
    }

    std::printf("\n%s (%d failure(s))\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED",
                failures);
    return failures == 0 ? 0 : 1;
}
