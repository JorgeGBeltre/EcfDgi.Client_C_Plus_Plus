#pragma once
// Ports of the Ecf commands/queries.

#include <optional>
#include <string>

#include "Domain/Entities/Rfce.h"

namespace ecf::app {

struct SendEcfCommand {
    std::string xmlContent;
    std::string fileName;
    std::string rncEmisor;
    std::string eNcf;
    std::optional<std::string> rncComprador;
    double totalAmount = 0;
    double itbisAmount = 0;
};

struct SendRfceCommand {
    domain::Rfce rfceModel;
};

struct GetEcfStatusQuery {
    std::string rncEmisor;
    std::string eNcf;
};

}  // namespace ecf::app
