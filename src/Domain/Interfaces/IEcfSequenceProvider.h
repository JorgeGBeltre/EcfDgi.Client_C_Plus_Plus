#pragma once

#include <string>

namespace ecf::domain {

class IEcfSequenceProvider {
public:
    virtual ~IEcfSequenceProvider() = default;

    virtual std::string getNext(const std::string& rncEmisor) = 0;
    virtual void release(const std::string& rncEmisor, const std::string& eNcf) = 0;
};

}  // namespace ecf::domain
