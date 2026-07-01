#pragma once
// Produces an enveloped XMLDSig signature (Exclusive C14N + RSA-SHA256) using
// xmlsec1/OpenSSL, and validates that the certificate subject carries the RNC.

#include <string>
#include <vector>

namespace ecf::infra {

class EcfXmlSigner {
public:
    EcfXmlSigner(const std::string& pfxPath, const std::string& pfxPassword);

    // Signs the document and returns the serialized signed XML.
    std::string signXml(const std::string& xmlContent, const std::string& rncEmisor);

    // Extracts the base64 <SignatureValue> content from an already-signed XML.
    std::string extractSignatureValue(const std::string& signedXml);

    // True when the certificate Subject contains the given RNC/Cédula.
    bool validateCertificateSn(const std::string& rncOCedula) const;

private:
    std::vector<unsigned char> pfxBytes_;  // raw PKCS#12, fed to xmlsec
    std::string pfxPassword_;
    std::string certSubject_;              // parsed once via OpenSSL
};

}  // namespace ecf::infra
