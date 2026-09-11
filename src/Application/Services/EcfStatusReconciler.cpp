#include "Application/Services/EcfStatusReconciler.h"

#include <chrono>
#include <iostream>
#include <string>

#include "Shared/Common/Sys.h"

namespace ecf::app {

EcfStatusReconciler::EcfStatusReconciler(std::shared_ptr<domain::IEcfDocumentRepository> docsRepo,
                                        std::shared_ptr<domain::IUnitOfWork> uow,
                                        std::shared_ptr<domain::IEcfClient> ecfClient,
                                        domain::EcfStatusPollingOptions options)
    : docsRepo_(std::move(docsRepo)),
      uow_(std::move(uow)),
      ecfClient_(std::move(ecfClient)),
      options_(options) {}

int EcfStatusReconciler::reconcile() {
    if (!docsRepo_ || !uow_ || !ecfClient_) return 0;

    auto now = std::chrono::system_clock::now();
    auto minAgeCutoff = now - std::chrono::minutes(options_.minDocumentAgeMinutes);
    auto pollDueCutoff = now - std::chrono::minutes(options_.pollingIntervalMinutes);

    std::string minAgeIso = sys::toIsoUtc(minAgeCutoff);
    std::string pollDueIso = sys::toIsoUtc(pollDueCutoff);

    std::vector<domain::EcfDocument> due;
    try {
        due = docsRepo_->getDueForStatusCheck(minAgeIso, pollDueIso, 100);
    } catch (const std::exception& ex) {
        std::cerr << "[EcfStatusReconciler] Error fetching due documents: " << ex.what() << std::endl;
        return 0;
    }

    int processed = 0;
    std::string nowIso = sys::toIsoUtc(now);

    for (auto& doc : due) {
        processed++;

        if (doc.sentToDgiiAt.has_value()) {
            auto sentTime = sys::parseIsoUtc(*doc.sentToDgiiAt);
            auto age = now - sentTime;
            if (age >= std::chrono::hours(options_.maxPollingWindowHours)) {
                doc.state = "RequiresManualReview";
                doc.lastStatusCheckAt = nowIso;
                try {
                    docsRepo_->update(doc);
                } catch (...) {}
                continue;
            }
        }

        try {
            auto response = ecfClient_->consultarEstado(
                doc.rncEmisor, doc.eNcf, doc.rncComprador, doc.securityCode);

            doc.lastStatusCheckAt = nowIso;
            doc.statusCheckAttempts += 1;

            std::string estado = response.estado;
            while (!estado.empty() && std::isspace(static_cast<unsigned char>(estado.front()))) estado.erase(estado.begin());
            while (!estado.empty() && std::isspace(static_cast<unsigned char>(estado.back()))) estado.pop_back();

            if (estado == "Aceptado" || estado == "Aceptado condicional") {
                doc.state = "AcceptedByDgii";
            } else if (estado == "Rechazado") {
                doc.state = "RejectedByDgii";
                std::cerr << "[EcfStatusReconciler] CRITICAL: e-CF " << doc.eNcf
                          << " (RNC " << doc.rncEmisor << ") RECHAZADO tras verificacion posterior." << std::endl;
            }
            docsRepo_->update(doc);
        } catch (const std::exception& ex) {
            doc.lastStatusCheckAt = nowIso;
            doc.statusCheckAttempts += 1;
            try {
                docsRepo_->update(doc);
            } catch (...) {}
            std::cerr << "[EcfStatusReconciler] Error polling e-CF " << doc.eNcf << ": " << ex.what() << std::endl;
        }
    }

    if (processed > 0) {
        try {
            uow_->saveChanges();
        } catch (const std::exception& ex) {
            std::cerr << "[EcfStatusReconciler] Error saving changes: " << ex.what() << std::endl;
        }
    }

    return processed;
}

}  // namespace ecf::app
