#pragma once

#include <chrono>
#include <string>
#include <vector>

#include "Domain/Interfaces/IEcfXmlSigner.h"

namespace ecf::infra {

class EcfXmlSigner : public domain::IEcfXmlSigner {
public:
    EcfXmlSigner();
    EcfXmlSigner(const std::string& pfxPath, const std::string& pfxPassword);
    EcfXmlSigner(const std::vector<unsigned char>& pfxBytes, const std::string& pfxPassword);

    std::string signXml(const std::string& xmlContent, const std::string& rncEmisor) override;

    std::string extractSignatureValue(const std::string& signedXml) override;

    bool validateCertificateSn(const std::string& rncOCedula) const override;

    bool usesFallbackCertificate() const override { return usesFallbackCertificate_; }

    std::chrono::system_clock::time_point notBefore() const { return notBefore_; }
    std::chrono::system_clock::time_point notAfter() const { return notAfter_; }

private:
    std::vector<unsigned char> pfxBytes_;
    std::string pfxPassword_;
    std::string certSubject_;
    std::string certIssuer_;
    std::string certSan_;
    std::chrono::system_clock::time_point notBefore_{};
    std::chrono::system_clock::time_point notAfter_{};
    bool isSelfSigned_ = false;
    bool usesFallbackCertificate_ = false;
};

}
