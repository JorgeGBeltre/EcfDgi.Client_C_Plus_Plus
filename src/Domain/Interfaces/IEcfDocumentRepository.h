#pragma once

#include <optional>
#include <string>
#include <vector>

#include "Domain/Entities/EcfDocument.h"

namespace ecf::domain {

class IEcfDocumentRepository {
public:
    virtual ~IEcfDocumentRepository() = default;

    virtual std::optional<EcfDocument> getById(const std::string& id) = 0;
    virtual std::optional<EcfDocument> getByENcf(const std::string& eNcf) = 0;
    virtual std::optional<EcfDocument> getByTrackId(const std::string& trackId) = 0;
    virtual std::vector<EcfDocument> getAll() = 0;
    virtual void add(const EcfDocument& document) = 0;
    virtual void update(const EcfDocument& document) = 0;
};

}  // namespace ecf::domain
