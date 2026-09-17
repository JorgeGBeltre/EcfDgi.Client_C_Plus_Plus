#pragma once

#include <memory>

#include "Domain/Interfaces/IEcfDocumentRepository.h"
#include "Infrastructure/Persistence/DbContext.h"

namespace ecf::infra {

class EcfDocumentRepository : public domain::IEcfDocumentRepository {
public:
    explicit EcfDocumentRepository(std::shared_ptr<DbContext> db) : db_(std::move(db)) {}

    std::optional<domain::EcfDocument> getById(const std::string& id) override;
    std::optional<domain::EcfDocument> getByENcf(const std::string& eNcf) override;
    std::optional<domain::EcfDocument> getByTrackId(const std::string& trackId) override;
    std::optional<domain::EcfDocument> getBySourceTxnId(
        const std::string& tenantId,
        const std::string& sourceTxnId,
        const std::optional<std::string>& ambiente = std::nullopt) override;
    std::vector<domain::EcfDocument> getDueForStatusCheck(const std::string& minAgeCutoffIso,
                                                         const std::string& pollDueCutoffIso,
                                                         int limit = 100) override;
    std::vector<domain::EcfDocument> getAll() override;
    void add(const domain::EcfDocument& document) override;
    void update(const domain::EcfDocument& document) override;

private:
    std::shared_ptr<DbContext> db_;
};

}  // namespace ecf::infra
