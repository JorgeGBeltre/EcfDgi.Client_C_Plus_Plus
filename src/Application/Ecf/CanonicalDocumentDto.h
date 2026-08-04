#pragma once

#include <string>
#include <vector>
#include <optional>

namespace ecf::app {

struct SourceReferenceDto {
    std::string provider = "QuickBooksDesktop";
    std::string txnId;
    std::string editSequence;
};

struct CanonicalHeaderDto {
    std::string rncEmisor;
    std::string razonSocialEmisor;
    std::string rncComprador;
    std::string razonSocialComprador;
    std::string fechaEmision;
};

struct CanonicalLineDto {
    int lineNumber = 0;
    std::string itemName;
    double quantity = 0;
    double unitPrice = 0;
    double amount = 0;
};

struct CanonicalTotalsDto {
    double montoSubtotal = 0;
    double montoItbis = 0;
    double montoTotal = 0;
};

struct CanonicalReferencesDto {
    std::string correctsTxnId;
    std::string correctsENcf;
};

struct CanonicalDocumentDto {
    std::optional<std::string> ncf;
    SourceReferenceDto sourceReference;
    std::string documentKind = "Invoice"; // Invoice, CreditNote, DebitNote, Bill
    std::string tipoComprobante = "E31"; // Default Factura de Crédito Fiscal
    CanonicalHeaderDto header;
    std::vector<CanonicalLineDto> lines;
    CanonicalTotalsDto totals;
    CanonicalReferencesDto references;
};

} // namespace ecf::app
