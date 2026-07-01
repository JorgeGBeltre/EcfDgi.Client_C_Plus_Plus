#include "Infrastructure/Persistence/MemorySequenceProvider.h"

#include <cstdio>

namespace ecf::infra {

std::string MemorySequenceProvider::getNext(const std::string& rncEmisor) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& seq = sequences_[rncEmisor];
    seq += 1;  // starts at 1 on first use
    char buf[32];
    std::snprintf(buf, sizeof(buf), "E31%010lld", static_cast<long long>(seq));
    return std::string(buf);
}

void MemorySequenceProvider::release(const std::string&, const std::string&) {
    // no-op
}

}  // namespace ecf::infra
