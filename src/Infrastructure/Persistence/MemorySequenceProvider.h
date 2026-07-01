#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>

#include "Domain/Interfaces/IEcfSequenceProvider.h"

namespace ecf::infra {

class MemorySequenceProvider : public domain::IEcfSequenceProvider {
public:
    std::string getNext(const std::string& rncEmisor) override;
    void release(const std::string& rncEmisor, const std::string& eNcf) override;

private:
    std::map<std::string, std::int64_t> sequences_;
    std::mutex mutex_;
};

}  // namespace ecf::infra
