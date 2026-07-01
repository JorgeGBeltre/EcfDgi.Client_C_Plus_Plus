#include "Infrastructure/Security/EcfXmlSigner.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include <openssl/pkcs12.h>
#include <openssl/x509.h>

#include <xmlsec/crypto.h>
#include <xmlsec/templates.h>
#include <xmlsec/xmldsig.h>
#include <xmlsec/xmlsec.h>
#include <xmlsec/xmltree.h>

#include <cstring>
#include <fstream>
#include <mutex>
#include <stdexcept>

#include "Domain/Exceptions/EcfException.h"

namespace ecf::infra {

using domain::EcfException;
using domain::EcfSigningException;

namespace {

// One-time initialisation of libxml2 + xmlsec (+ the OpenSSL engine).
void ensureXmlSecInit() {
    static std::once_flag flag;
    static bool ok = false;
    std::call_once(flag, [] {
        xmlInitParser();
        LIBXML_TEST_VERSION
        if (xmlSecInit() < 0) return;
        if (xmlSecCheckVersion() != 1) return;
        if (xmlSecCryptoAppInit(nullptr) < 0) return;
        if (xmlSecCryptoInit() < 0) return;
        ok = true;
    });
    if (!ok) throw EcfSigningException("No se pudo inicializar xmlsec/OpenSSL.");
}

std::vector<unsigned char> readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Certificado no encontrado: " + path);
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(f)),
                                      std::istreambuf_iterator<char>());
}

std::string parseSubject(const std::vector<unsigned char>& pfx,
                         const std::string& password) {
    const unsigned char* p = pfx.data();
    PKCS12* p12 = d2i_PKCS12(nullptr, &p, static_cast<long>(pfx.size()));
    if (!p12) throw EcfSigningException("PKCS#12 inválido: no se pudo leer el certificado.");

    EVP_PKEY* pkey = nullptr;
    X509* cert = nullptr;
    STACK_OF(X509)* ca = nullptr;
    std::string subject;
    if (PKCS12_parse(p12, password.c_str(), &pkey, &cert, &ca) && cert) {
        char buf[512];
        X509_NAME_oneline(X509_get_subject_name(cert), buf, sizeof(buf));
        subject = buf;
    }
    if (pkey) EVP_PKEY_free(pkey);
    if (cert) X509_free(cert);
    if (ca) sk_X509_pop_free(ca, X509_free);
    PKCS12_free(p12);

    if (subject.empty())
        throw EcfSigningException("No se pudo extraer el Subject del certificado (¿clave incorrecta?).");
    return subject;
}

}  // namespace

EcfXmlSigner::EcfXmlSigner(const std::string& pfxPath, const std::string& pfxPassword)
    : pfxPassword_(pfxPassword) {
    ensureXmlSecInit();
    pfxBytes_ = readFile(pfxPath);
    certSubject_ = parseSubject(pfxBytes_, pfxPassword_);
}

bool EcfXmlSigner::validateCertificateSn(const std::string& rncOCedula) const {
    return certSubject_.find(rncOCedula) != std::string::npos;
}

std::string EcfXmlSigner::signXml(const std::string& xmlContent,
                                  const std::string& rncEmisor) {
    if (!validateCertificateSn(rncEmisor))
        throw EcfSigningException(
            "El RNC del certificado no coincide con el emisor: " + rncEmisor);

    xmlDocPtr doc = xmlReadMemory(xmlContent.c_str(),
                                  static_cast<int>(xmlContent.size()),
                                  "doc.xml", nullptr, 0);
    if (!doc || !xmlDocGetRootElement(doc)) {
        if (doc) xmlFreeDoc(doc);
        throw EcfSigningException("Documento XML a firmar inválido.");
    }

    xmlNodePtr signNode = nullptr;
    xmlSecDSigCtxPtr dsigCtx = nullptr;
    xmlSecKeyPtr key = nullptr;
    std::string result;

    try {
        // <Signature> template: Exclusive C14N + RSA-SHA256.
        signNode = xmlSecTmplSignatureCreate(doc, xmlSecTransformExclC14NId,
                                             xmlSecTransformRsaSha256Id, nullptr);
        if (!signNode) throw EcfSigningException("No se pudo crear la plantilla de firma.");

        // Enveloped signature over the whole document (URI="").
        xmlNodePtr refNode = xmlSecTmplSignatureAddReference(
            signNode, xmlSecTransformSha256Id, nullptr,
            reinterpret_cast<const xmlChar*>(""), nullptr);
        if (!refNode) throw EcfSigningException("No se pudo crear la referencia de firma.");
        if (!xmlSecTmplReferenceAddTransform(refNode, xmlSecTransformEnvelopedId))
            throw EcfSigningException("No se pudo agregar la transformación enveloped.");
        if (!xmlSecTmplReferenceAddTransform(refNode, xmlSecTransformExclC14NId))
            throw EcfSigningException("No se pudo agregar la transformación ExcC14N.");

        // KeyInfo/X509Data so the certificate travels with the signature.
        xmlNodePtr keyInfoNode = xmlSecTmplSignatureEnsureKeyInfo(signNode, nullptr);
        if (!keyInfoNode || !xmlSecTmplKeyInfoAddX509Data(keyInfoNode))
            throw EcfSigningException("No se pudo agregar KeyInfo/X509Data.");

        // Append template to the document root => enveloped.
        xmlAddChild(xmlDocGetRootElement(doc), signNode);

        // Load the signing key + certificate from the in-memory PKCS#12.
        key = xmlSecCryptoAppKeyLoadMemory(
            pfxBytes_.data(), pfxBytes_.size(), xmlSecKeyDataFormatPkcs12,
            pfxPassword_.c_str(), nullptr, nullptr);
        if (!key) throw EcfSigningException("No se pudo cargar la clave del certificado.");

        dsigCtx = xmlSecDSigCtxCreate(nullptr);
        if (!dsigCtx) throw EcfSigningException("No se pudo crear el contexto de firma.");
        dsigCtx->signKey = key;
        key = nullptr;  // owned by dsigCtx now

        if (xmlSecDSigCtxSign(dsigCtx, signNode) < 0)
            throw EcfSigningException("Fallo al calcular la firma digital.");

        xmlChar* out = nullptr;
        int size = 0;
        xmlDocDumpMemory(doc, &out, &size);
        if (out) {
            result.assign(reinterpret_cast<const char*>(out), static_cast<size_t>(size));
            xmlFree(out);
        }
    } catch (...) {
        if (dsigCtx) xmlSecDSigCtxDestroy(dsigCtx);
        if (key) xmlSecKeyDestroy(key);
        xmlFreeDoc(doc);
        throw;
    }

    xmlSecDSigCtxDestroy(dsigCtx);
    xmlFreeDoc(doc);
    return result;
}

std::string EcfXmlSigner::extractSignatureValue(const std::string& signedXml) {
    xmlDocPtr doc = xmlReadMemory(signedXml.c_str(),
                                  static_cast<int>(signedXml.size()),
                                  "signed.xml", nullptr, 0);
    if (!doc) throw EcfException("El XML firmado es inválido.");

    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);
    xmlXPathRegisterNs(ctx, reinterpret_cast<const xmlChar*>("ds"),
                       reinterpret_cast<const xmlChar*>("http://www.w3.org/2000/09/xmldsig#"));
    xmlXPathObjectPtr obj = xmlXPathEvalExpression(
        reinterpret_cast<const xmlChar*>("//ds:SignatureValue"), ctx);

    std::string value;
    if (obj && obj->nodesetval && obj->nodesetval->nodeNr > 0) {
        xmlChar* content = xmlNodeGetContent(obj->nodesetval->nodeTab[0]);
        if (content) {
            value = reinterpret_cast<const char*>(content);
            xmlFree(content);
        }
    }

    if (obj) xmlXPathFreeObject(obj);
    if (ctx) xmlXPathFreeContext(ctx);
    xmlFreeDoc(doc);

    // trim
    size_t a = value.find_first_not_of(" \t\r\n");
    size_t b = value.find_last_not_of(" \t\r\n");
    if (a == std::string::npos)
        throw EcfException(
            "El XML no contiene un nodo SignatureValue. ¿Fue firmado correctamente?");
    return value.substr(a, b - a + 1);
}

}  // namespace ecf::infra
