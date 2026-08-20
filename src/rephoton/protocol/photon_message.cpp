#include "photon_message.h"

namespace ReFix::Photon::Protocol {

    // =========================================================================
    // OperationRequest Serialization
    // =========================================================================
    std::vector<uint8_t> OperationRequest::Serialize() const {
        std::vector<uint8_t> buf;
        PhotonSerializer::WriteByte(buf, static_cast<uint8_t>(MessageType::OperationRequest));
        PhotonSerializer::WriteByte(buf, opCode);
        PhotonSerializer::WriteParameterDictionary(buf, parameters);
        return buf;
    }

    bool OperationRequest::Deserialize(const std::vector<uint8_t>& buffer, size_t& offset, OperationRequest& outReq) {
        uint8_t msgType = 0;
        if (!PhotonSerializer::ReadByte(buffer, offset, msgType)) return false;
        if (msgType != static_cast<uint8_t>(MessageType::OperationRequest) &&
            msgType != static_cast<uint8_t>(MessageType::InternalOperationRequest)) return false;

        // Skip Protocol16 type marker (0x71 = 'q' = GpType::OperationRequest) if present
        if (offset < buffer.size() && (buffer[offset] == 0x71 || buffer[offset] == 0x70 || buffer[offset] == 0x65)) {
            offset++;
        }

        if (!PhotonSerializer::ReadByte(buffer, offset, outReq.opCode)) return false;
        return PhotonSerializer::ReadParameterDictionary(buffer, offset, outReq.parameters);
    }

    // =========================================================================
    // OperationResponse Serialization
    // =========================================================================
    std::vector<uint8_t> OperationResponse::Serialize() const {
        std::vector<uint8_t> buf;
        uint8_t msgType = (opCode == 0) ? static_cast<uint8_t>(MessageType::InternalOperationResponse)
                                        : static_cast<uint8_t>(MessageType::OperationResponse);
        PhotonSerializer::WriteByte(buf, msgType);
        PhotonSerializer::WriteByte(buf, opCode);
        PhotonSerializer::WriteShort(buf, returnCode);
        if (returnCode != ErrorCode::Ok && !debugMessage.empty()) {
            PhotonSerializer::WriteByte(buf, GpType::String);
            PhotonSerializer::WriteString(buf, debugMessage);
        } else {
            PhotonSerializer::WriteByte(buf, GpType::Null);
        }
        PhotonSerializer::WriteParameterDictionary(buf, parameters);
        return buf;
    }

    bool OperationResponse::Deserialize(const std::vector<uint8_t>& buffer, size_t& offset, OperationResponse& outResp) {
        uint8_t msgType = 0;
        if (!PhotonSerializer::ReadByte(buffer, offset, msgType)) return false;
        if (msgType != static_cast<uint8_t>(MessageType::OperationResponse)) return false;

        if (!PhotonSerializer::ReadByte(buffer, offset, outResp.opCode)) return false;
        if (!PhotonSerializer::ReadShort(buffer, offset, outResp.returnCode)) return false;

        uint8_t debugType = 0;
        if (!PhotonSerializer::ReadByte(buffer, offset, debugType)) return false;
        if (debugType == GpType::String) {
            if (!PhotonSerializer::ReadString(buffer, offset, outResp.debugMessage)) return false;
        }

        return PhotonSerializer::ReadParameterDictionary(buffer, offset, outResp.parameters);
    }

    // =========================================================================
    // EventData Serialization
    // =========================================================================
    std::vector<uint8_t> EventData::Serialize() const {
        std::vector<uint8_t> buf;
        PhotonSerializer::WriteByte(buf, static_cast<uint8_t>(MessageType::Event));
        PhotonSerializer::WriteByte(buf, code);
        PhotonSerializer::WriteParameterDictionary(buf, parameters);
        return buf;
    }

    bool EventData::Deserialize(const std::vector<uint8_t>& buffer, size_t& offset, EventData& outEvent) {
        uint8_t msgType = 0;
        if (!PhotonSerializer::ReadByte(buffer, offset, msgType)) return false;
        if (msgType != static_cast<uint8_t>(MessageType::Event)) return false;

        if (!PhotonSerializer::ReadByte(buffer, offset, outEvent.code)) return false;

        if (!PhotonSerializer::ReadParameterDictionary(buffer, offset, outEvent.parameters)) return false;

        if (outEvent.parameters.find(ParameterCode::ActorNr) != outEvent.parameters.end()) {
            outEvent.senderActorNumber = outEvent.parameters[ParameterCode::ActorNr].AsInt();
        }
        return true;
    }

} // namespace ReFix::Photon::Protocol
