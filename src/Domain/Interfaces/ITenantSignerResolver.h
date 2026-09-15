#pragma once

#include <memory>
#include <string>

#include "Domain/Interfaces/IEcfXmlSigner.h"

namespace ecf::domain {

/// <summary>
/// Resuelve dinámicamente el firmador XML (IEcfXmlSigner) correspondiente
/// a un contribuyente / tenant a partir de su RNC o identificador.
/// </summary>
class ITenantSignerResolver {
public:
    virtual ~ITenantSignerResolver() = default;

    /// <summary>
    /// Obtiene la instancia de IEcfXmlSigner configurada con el certificado digital activo
    /// para el RNC especificado (buscando en base de datos de tenants o disco).
    /// Si no se encuentra un certificado específico para el RNC o ante errores temporales,
    /// retorna el firmador por defecto del sistema (fallback seguro).
    /// </summary>
    /// <param name="rnc">RNC o Cédula del contribuyente (con o sin guiones).</param>
    /// <returns>Instancia de IEcfXmlSigner lista para firmar.</returns>
    virtual std::shared_ptr<IEcfXmlSigner> resolveSigner(const std::string& rnc) = 0;
};

}  // namespace ecf::domain
