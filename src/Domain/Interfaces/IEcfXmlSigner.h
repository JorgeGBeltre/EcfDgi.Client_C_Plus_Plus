#pragma once

#include <string>

namespace ecf::domain {

class IEcfXmlSigner {
public:
    virtual ~IEcfXmlSigner() = default;

    virtual std::string signXml(const std::string& xmlContent, const std::string& rncEmisor) = 0;
    virtual std::string extractSignatureValue(const std::string& signedXml) = 0;
    virtual bool validateCertificateSn(const std::string& rncOCedula) const = 0;
    virtual bool usesFallbackCertificate() const = 0;
};

}  // namespace ecf::domain
