#pragma once
// Produces an enveloped XMLDSig signature (Exclusive C14N + RSA-SHA256) using
// xmlsec1/OpenSSL, and validates that the certificate subject carries the RNC.

#include <string>
#include <vector>

#include "Domain/Interfaces/IEcfXmlSigner.h"

namespace ecf::infra {

class EcfXmlSigner : public domain::IEcfXmlSigner {
public:
    EcfXmlSigner(const std::string& pfxPath, const std::string& pfxPassword);

    // Signs the document and returns the serialized signed XML.
    std::string signXml(const std::string& xmlContent, const std::string& rncEmisor) override;

    // Extracts the base64 <SignatureValue> content from an already-signed XML.
    std::string extractSignatureValue(const std::string& signedXml) override;

    // True when the certificate Subject contains the given RNC/Cédula.
    bool validateCertificateSn(const std::string& rncOCedula) const override;

    // True when a dummy/fallback self-signed certificate was generated.
    bool usesFallbackCertificate() const override { return usesFallbackCertificate_; }

private:
    std::vector<unsigned char> pfxBytes_;  // raw PKCS#12, fed to xmlsec
    std::string pfxPassword_;
    std::string certSubject_;              // parsed once via OpenSSL
    bool usesFallbackCertificate_ = false;
};

}  // namespace ecf::infra
