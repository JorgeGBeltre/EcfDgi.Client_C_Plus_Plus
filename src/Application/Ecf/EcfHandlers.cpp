#include "Application/Ecf/EcfHandlers.h"

#include <exception>

#include "Domain/Entities/EcfDocument.h"
#include "Shared/Common/Sys.h"

namespace ecf::app {

using namespace ecf::domain;
using shared::ResultT;

ResultT<EcfRecepcionResponse> SendEcfCommandHandler::handle(const SendEcfCommand& cmd) {
    try {
        auto response = client_->sendEcf(cmd.xmlContent, cmd.fileName);
        const bool hasError = response.error.has_value() && !response.error->empty();

        EcfDocument doc;
        doc.eNcf = cmd.eNcf;
        doc.rncEmisor = cmd.rncEmisor;
        doc.rncComprador = cmd.rncComprador;
        doc.trackId = response.trackId;
        doc.state = hasError ? "Rechazado" : "Recibido";
        doc.totalAmount = cmd.totalAmount;
        doc.itbisAmount = cmd.itbisAmount;
        doc.xmlContent = cmd.xmlContent;
        doc.receiptDate = sys::utcNowIso();

        docRepo_->add(doc);
        uow_->saveChanges();

        if (hasError) {
            const std::string msg =
                response.mensaje.value_or(response.error.value_or("Error"));
            return ResultT<EcfRecepcionResponse>::Failure(msg);
        }
        return ResultT<EcfRecepcionResponse>::Success(std::move(response));
    } catch (const std::exception& ex) {
        return ResultT<EcfRecepcionResponse>::Failure(
            std::string("Failed to send e-CF: ") + ex.what());
    }
}

ResultT<RfceRecepcionResponse> SendRfceCommandHandler::handle(SendRfceCommand cmd) {
    try {
        Rfce& rfce = cmd.rfceModel;
        auto response = client_->sendRfce(rfce);

        const std::string xmlContent = serializer_->serialize(rfce);

        EcfDocument doc;
        doc.eNcf = rfce.encabezado.idDoc.eNcf;
        doc.rncEmisor = rfce.encabezado.emisor.rncEmisor;
        if (rfce.encabezado.comprador.has_value())
            doc.rncComprador = rfce.encabezado.comprador->rncComprador;
        doc.trackId = std::nullopt;  // RFCE responses return code/estado, not a TrackId
        doc.state = response.estado;
        doc.totalAmount = rfce.encabezado.totales.montoTotal;
        doc.itbisAmount = rfce.encabezado.totales.totalITBIS.value_or(0);
        doc.securityCode = rfce.encabezado.totales.codigoSeguridadeCF;
        doc.xmlContent = xmlContent;
        doc.receiptDate = sys::utcNowIso();

        docRepo_->add(doc);
        uow_->saveChanges();

        if (response.estado == "Rechazado") {
            const std::string msg = !response.mensajes.empty()
                                        ? response.mensajes[0].valor
                                        : "Rechazado por DGII";
            return ResultT<RfceRecepcionResponse>::Failure(msg);
        }
        return ResultT<RfceRecepcionResponse>::Success(std::move(response));
    } catch (const std::exception& ex) {
        return ResultT<RfceRecepcionResponse>::Failure(
            std::string("Failed to send RFCE: ") + ex.what());
    }
}

ResultT<ConsultaEstadoResponse> GetEcfStatusQueryHandler::handle(const GetEcfStatusQuery& q) {
    try {
        auto localDoc = docRepo_->getByENcf(q.eNcf);

        if (localDoc.has_value() && localDoc->state == "Aceptado") {
            ConsultaEstadoResponse local;
            local.codigo = 0;
            local.estado = localDoc->state;
            local.rncEmisor = localDoc->rncEmisor;
            local.ncfElectronico = localDoc->eNcf;
            local.montoTotal = localDoc->totalAmount;
            local.totalITBIS = localDoc->itbisAmount;
            local.rncComprador = localDoc->rncComprador.value_or("");
            local.codigoSeguridad = localDoc->securityCode.value_or("");
            local.fechaEmision = localDoc->createdAt;
            return ResultT<ConsultaEstadoResponse>::Success(std::move(local));
        }

        std::optional<std::string> rncComprador =
            localDoc.has_value() ? localDoc->rncComprador : std::nullopt;
        std::optional<std::string> secCode =
            localDoc.has_value() ? localDoc->securityCode : std::nullopt;

        auto live = client_->consultarEstado(q.rncEmisor, q.eNcf, rncComprador, secCode);

        if (localDoc.has_value() && localDoc->state != live.estado) {
            localDoc->state = live.estado;
            docRepo_->update(*localDoc);
            uow_->saveChanges();
        }

        return ResultT<ConsultaEstadoResponse>::Success(std::move(live));
    } catch (const std::exception& ex) {
        return ResultT<ConsultaEstadoResponse>::Failure(
            std::string("Failed to retrieve status: ") + ex.what());
    }
}

}  // namespace ecf::app
