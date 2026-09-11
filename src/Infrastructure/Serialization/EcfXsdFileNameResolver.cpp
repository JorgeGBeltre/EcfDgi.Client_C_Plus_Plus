#include "Infrastructure/Serialization/EcfXsdFileNameResolver.h"

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <cstring>
#include <algorithm>

namespace ecf::infra {

std::string EcfXsdFileNameResolver::resolve(const std::string& xmlContent) {
    if (xmlContent.empty()) return "";

    xmlDocPtr doc = xmlReadMemory(xmlContent.c_str(), static_cast<int>(xmlContent.size()),
                                  "doc.xml", nullptr, XML_PARSE_NOERROR | XML_PARSE_NOWARNING);
    if (!doc) return "";

    xmlNodePtr root = xmlDocGetRootElement(doc);
    if (!root || !root->name) {
        xmlFreeDoc(doc);
        return "";
    }

    std::string rootName = reinterpret_cast<const char*>(root->name);

    if (rootName == "RFCE") {
        xmlFreeDoc(doc);
        return "RFCE 32 v.1.0.xsd";
    }
    if (rootName == "ACECF") {
        xmlFreeDoc(doc);
        return "ACECF v.1.0.xsd";
    }
    if (rootName == "ARECF") {
        xmlFreeDoc(doc);
        return "ARECF v1.0.xsd";
    }
    if (rootName == "ANECF" || rootName == "Anulacion") {
        xmlFreeDoc(doc);
        return "ANECF v.1.0.xsd";
    }
    if (rootName == "SemillaModel") {
        xmlFreeDoc(doc);
        return "Semilla v.1.0.xsd";
    }

    if (rootName == "ECF") {
        // Find //Encabezado/IdDoc/TipoeCF
        xmlXPathContextPtr xpathCtx = xmlXPathNewContext(doc);
        if (xpathCtx) {
            xmlXPathObjectPtr xpathObj = xmlXPathEvalExpression(
                reinterpret_cast<const xmlChar*>("//*[local-name()='Encabezado']/*[local-name()='IdDoc']/*[local-name()='TipoeCF']"),
                xpathCtx);
            if (xpathObj && xpathObj->nodesetval && xpathObj->nodesetval->nodeNr > 0) {
                xmlChar* content = xmlNodeGetContent(xpathObj->nodesetval->nodeTab[0]);
                if (content) {
                    std::string tipo = reinterpret_cast<const char*>(content);
                    xmlFree(content);
                    // trim
                    size_t first = tipo.find_first_not_of(" \t\r\n");
                    size_t last = tipo.find_last_not_of(" \t\r\n");
                    if (first != std::string::npos && last != std::string::npos) {
                        tipo = tipo.substr(first, last - first + 1);
                    }
                    xmlXPathFreeObject(xpathObj);
                    xmlXPathFreeContext(xpathCtx);
                    xmlFreeDoc(doc);
                    return "e-CF " + tipo + " v.1.0.xsd";
                }
            }
            if (xpathObj) xmlXPathFreeObject(xpathObj);
            xmlXPathFreeContext(xpathCtx);
        }
    }

    xmlFreeDoc(doc);
    return "";
}

}  // namespace ecf::infra
