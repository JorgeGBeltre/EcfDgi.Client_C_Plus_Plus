#pragma once

#include <string>
#include <vector>
#include <optional>
#include <sstream>

#include "Application/Ecf/CanonicalDocumentDto.h"

namespace ecf::app {

struct ProcessedLineItem {
    int lineNumber = 0;
    std::string name;
    std::string description;
    double quantity = 1.0;
    double unitPrice = 0.0;
    double discountAmount = 0.0;
    double montoItem = 0.0;
};

std::string escapeXml(const std::string& value);

std::string normalizeFechaDgii(const std::string& value);

std::vector<ProcessedLineItem> normalizeCanonicalLines(const std::vector<CanonicalLineDto>& lines, double defaultTotal = 0.0);

void appendRetencion(std::ostringstream& ss, const std::optional<CanonicalRetentionDto>& retention, const std::string& tipoEcf);

std::string buildXmlFromCanonical(const CanonicalDocumentDto& dto,
                                  const std::string& eNcf,
                                  const std::string& emisorRnc,
                                  const std::string& emisorRazonSocial);

} // namespace ecf::app
