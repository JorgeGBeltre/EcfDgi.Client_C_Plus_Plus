#include "Infrastructure/Dgii/EcfEnvironmentConfig.h"

#include <stdexcept>

namespace ecf::infra {

using domain::AmbienteEnum;

EcfEnvironmentConfig EcfEnvironmentConfig::getConfig(AmbienteEnum ambiente) {
    EcfEnvironmentConfig c;
    switch (ambiente) {
        case AmbienteEnum::PreCertificacion:
            c.autenticacionUrl = "https://ecf.dgii.gov.do/testecf/autenticacion";
            c.recepcionUrl = "https://ecf.dgii.gov.do/testecf/recepcion";
            c.recepcionFcUrl = "https://fc.dgii.gov.do/testecf/recepcionfc";
            c.consultaResultadoUrl = "https://ecf.dgii.gov.do/testecf/consultaresultado";
            c.consultaEstadoUrl = "https://ecf.dgii.gov.do/testecf/consultaestado";
            c.consultaTrackIdsUrl = "https://ecf.dgii.gov.do/testecf/consultatrackids";
            c.consultaRfceUrl = "https://fc.dgii.gov.do/testecf/consultarfce";
            c.aprobacionComercialUrl = "https://ecf.dgii.gov.do/testecf/aprobacioncomercial";
            c.anulacionRangosUrl = "https://ecf.dgii.gov.do/testecf/anulacionrangos";
            c.directorioUrl = "https://ecf.dgii.gov.do/testecf/consultadirectorio";
            c.timbreUrl = "https://ecf.dgii.gov.do/testecf/consultatimbre";
            c.timbreFcUrl = "https://fc.dgii.gov.do/testecf/consultatimbrefc";
            return c;
        case AmbienteEnum::Certificacion:
            c.autenticacionUrl = "https://ecf.dgii.gov.do/certecf/autenticacion";
            c.recepcionUrl = "https://ecf.dgii.gov.do/certecf/recepcion";
            c.recepcionFcUrl = "https://fc.dgii.gov.do/certecf/recepcionfc";
            c.consultaResultadoUrl = "https://ecf.dgii.gov.do/certecf/consultaresultado";
            c.consultaEstadoUrl = "https://ecf.dgii.gov.do/certecf/consultaestado";
            c.consultaTrackIdsUrl = "https://ecf.dgii.gov.do/certecf/consultatrackids";
            c.consultaRfceUrl = "https://fc.dgii.gov.do/certecf/consultarfce";
            c.aprobacionComercialUrl = "https://ecf.dgii.gov.do/certecf/aprobacioncomercial";
            c.anulacionRangosUrl = "https://ecf.dgii.gov.do/certecf/anulacionrangos";
            c.directorioUrl = "https://ecf.dgii.gov.do/certecf/consultadirectorio";
            c.timbreUrl = "https://ecf.dgii.gov.do/certecf/consultatimbre";
            c.timbreFcUrl = "https://fc.dgii.gov.do/certecf/consultatimbrefc";
            return c;
        case AmbienteEnum::Produccion:
            c.autenticacionUrl = "https://ecf.dgii.gov.do/ecf/autenticacion";
            c.recepcionUrl = "https://ecf.dgii.gov.do/ecf/recepcion";
            c.recepcionFcUrl = "https://fc.dgii.gov.do/ecf/recepcionfc";
            c.consultaResultadoUrl = "https://ecf.dgii.gov.do/ecf/consultaresultado";
            c.consultaEstadoUrl = "https://ecf.dgii.gov.do/ecf/consultaestado";
            c.consultaTrackIdsUrl = "https://ecf.dgii.gov.do/ecf/consultatrackids";
            c.consultaRfceUrl = "https://fc.dgii.gov.do/ecf/consultarfce";
            c.aprobacionComercialUrl = "https://ecf.dgii.gov.do/ecf/aprobacioncomercial";
            c.anulacionRangosUrl = "https://ecf.dgii.gov.do/ecf/anulacionrangos";
            c.directorioUrl = "https://ecf.dgii.gov.do/ecf/consultadirectorio";
            c.timbreUrl = "https://ecf.dgii.gov.do/ecf/consultatimbre";
            c.timbreFcUrl = "https://fc.dgii.gov.do/ecf/consultatimbrefc";
            return c;
    }
    throw std::invalid_argument("Ambiente no soportado");
}

}  // namespace ecf::infra
