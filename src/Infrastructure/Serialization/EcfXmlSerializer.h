#pragma once
// Builds the RFCE XML payload and parses the XML-form DGII responses.

#include <string>

#include "Domain/Interfaces/IEcfXmlSerializer.h"

namespace ecf::infra {

class EcfXmlSerializer : public domain::IEcfXmlSerializer {
public:
    std::string serialize(const domain::Rfce& model) override;

    domain::EcfRecepcionResponse deserializeEcfRecepcion(const std::string& xml) override;
    domain::RfceRecepcionResponse deserializeRfceRecepcion(const std::string& xml) override;
    domain::ConsultaResultadoResponse deserializeConsultaResultado(
        const std::string& xml) override;

    std::string getFileName(const std::string& rncEmisor,
                            const std::string& eNcf) override;
    std::string escapeAlfanum(const std::string& value) override;
};

}  // namespace ecf::infra
