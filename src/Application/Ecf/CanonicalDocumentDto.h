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
    std::optional<std::string> correoComprador;
    // ISO 8601 (yyyy-MM-dd) or DGII format (dd-MM-yyyy)
    std::string fechaEmision;
};

struct CanonicalLineDto {
    int lineNumber = 0;
    std::string itemName;
    double quantity = 0.0;
    double unitPrice = 0.0;
    double amount = 0.0;
};

struct CanonicalTaxBucketDto {
    int rate = 18;       // 18, 16, 0
    double base = 0.0;
    double tax = 0.0;
};

struct CanonicalTotalsDto {
    double montoSubtotal = 0.0;
    std::optional<double> montoGravadoTotal;
    std::optional<double> montoExento;
    std::vector<CanonicalTaxBucketDto> taxBuckets;
    double montoItbis = 0.0;
    double montoTotal = 0.0;
};

struct CanonicalReferencesDto {
    std::string correctsTxnId;
    std::string correctsENcf;
    std::optional<int> codigoModificacion;
    std::optional<std::string> razonModificacion;
    std::optional<std::string> fechaNcfModificado;
    std::optional<std::string> rncOtroContribuyente;
};

struct CanonicalRetentionDto {
    int indicadorAgenteRetencionoPercepcion = 1;
    double montoItbisRetenido = 0.0;
    std::optional<double> montoIsrRetenido;
};

struct CanonicalCertificateDto {
    std::optional<std::string> certificateBase64;
    std::optional<std::string> password;
    std::optional<std::string> certificatePath;
};

struct CanonicalDocumentDto {
    std::optional<std::string> tenantId;
    std::optional<std::string> environment;
    std::optional<CanonicalCertificateDto> certificate;
    std::optional<std::string> ncf;
    SourceReferenceDto sourceReference;
    std::string documentKind = "Invoice"; // Invoice, CreditNote, DebitNote, Bill
    std::string tipoComprobante = "E31"; // Default Factura de Crédito Fiscal
    CanonicalHeaderDto header;
    std::vector<CanonicalLineDto> lines;
    CanonicalTotalsDto totals;
    CanonicalReferencesDto references;
    std::optional<CanonicalRetentionDto> retention;
};

} // namespace ecf::app
