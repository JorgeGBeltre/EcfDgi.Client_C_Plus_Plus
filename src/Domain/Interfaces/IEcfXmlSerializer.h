#pragma once
// Serializes the RFCE model to XML and parses the XML-form DGII responses.

#include <string>

#include "Domain/Entities/ResponseModels.h"
#include "Domain/Entities/Rfce.h"

namespace ecf::domain {

class IEcfXmlSerializer {
public:
    virtual ~IEcfXmlSerializer() = default;

    virtual std::string serialize(const Rfce& model) = 0;

    virtual EcfRecepcionResponse deserializeEcfRecepcion(const std::string& xml) = 0;
    virtual RfceRecepcionResponse deserializeRfceRecepcion(const std::string& xml) = 0;
    virtual ConsultaResultadoResponse deserializeConsultaResultado(const std::string& xml) = 0;

    virtual std::string getFileName(const std::string& rncEmisor,
                                    const std::string& eNcf) = 0;
    virtual std::string escapeAlfanum(const std::string& value) = 0;
};

}  // namespace ecf::domain
