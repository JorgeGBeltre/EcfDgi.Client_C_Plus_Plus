#include "Application/Services/EcfStatusReconciler.h"

#include <chrono>
#include <string>
#include <spdlog/spdlog.h>

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
        spdlog::error("[EcfStatusReconciler] Error fetching due documents: {}", ex.what());
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
                spdlog::critical(
                    "e-CF {} (RNC {}) lleva {}h sin confirmación definitiva de DGII "
                    "(ventana de {}h agotada); requiere revisión manual.",
                    doc.eNcf, doc.rncEmisor,
                    std::chrono::duration_cast<std::chrono::hours>(age).count(),
                    options_.maxPollingWindowHours);
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

            std::string lowerEstado = estado;
            std::transform(lowerEstado.begin(), lowerEstado.end(), lowerEstado.begin(), ::tolower);

            if (lowerEstado == "aceptado" || lowerEstado == "aceptado condicional") {
                doc.state = "AcceptedByDgii";
                spdlog::info("e-CF {}: DGII confirmó '{}'.", doc.eNcf, estado);
            } else if (lowerEstado == "rechazado") {
                doc.state = "RejectedByDgii";
                spdlog::critical(
                    "ALERTA: e-CF {} (RNC {}) fue aceptado en recepción y luego "
                    "RECHAZADO por DGII tras verificación posterior. Requiere atención — el "
                    "comprobante no tiene validez fiscal.",
                    doc.eNcf, doc.rncEmisor);
            }
            docsRepo_->update(doc);
        } catch (const std::exception& ex) {
            doc.lastStatusCheckAt = nowIso;
            doc.statusCheckAttempts += 1;
            try {
                docsRepo_->update(doc);
            } catch (...) {}
            spdlog::warn("Fallo consultando estado DGII para e-CF {}: {}; se reintentará.", doc.eNcf, ex.what());
        }
    }

    if (processed > 0) {
        try {
            uow_->saveChanges();
        } catch (const std::exception& ex) {
            spdlog::error("[EcfStatusReconciler] Error saving changes: {}", ex.what());
        }
    }

    return processed;
}

}  // namespace ecf::app
