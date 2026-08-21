#pragma once

#include "photon_serializer.h"
#include <cstdint>
#include <string>
#include <map>
#include <vector>

namespace ReFix::Photon::Protocol {

    enum class MessageType : uint8_t {
        OperationRequest  = 2,
        OperationResponse = 3,
        Event             = 4,
        InternalOperationRequest = 6,
        InternalOperationResponse = 7
    };

    struct OperationRequest {
        uint8_t opCode = 0;
        std::map<uint8_t, PhotonValue> parameters;

        OperationRequest() = default;
        OperationRequest(uint8_t code) : opCode(code) {}

        void SetParam(uint8_t code, const PhotonValue& val) { parameters[code] = val; }
        bool HasParam(uint8_t code) const { return parameters.find(code) != parameters.end(); }
        PhotonValue GetParam(uint8_t code, const PhotonValue& def = PhotonValue()) const {
            auto it = parameters.find(code);
            return (it != parameters.end()) ? it->second : def;
        }

        std::vector<uint8_t> Serialize() const;
        static bool Deserialize(const std::vector<uint8_t>& buffer, size_t& offset, OperationRequest& outReq);
    };

    struct OperationResponse {
        uint8_t opCode = 0;
        int16_t returnCode = ErrorCode::Ok;
        std::string debugMessage;
        std::map<uint8_t, PhotonValue> parameters;

        OperationResponse() = default;
        OperationResponse(uint8_t code, int16_t retCode = ErrorCode::Ok, const std::string& debug = "")
            : opCode(code), returnCode(retCode), debugMessage(debug) {}

        void SetParam(uint8_t code, const PhotonValue& val) { parameters[code] = val; }
        bool HasParam(uint8_t code) const { return parameters.find(code) != parameters.end(); }
        PhotonValue GetParam(uint8_t code, const PhotonValue& def = PhotonValue()) const {
            auto it = parameters.find(code);
            return (it != parameters.end()) ? it->second : def;
        }

        std::vector<uint8_t> Serialize() const;
        static bool Deserialize(const std::vector<uint8_t>& buffer, size_t& offset, OperationResponse& outResp);
    };

    struct EventData {
        uint8_t code = 0;
        int32_t senderActorNumber = 0;
        std::map<uint8_t, PhotonValue> parameters;

        EventData() = default;
        EventData(uint8_t eventCode, int32_t sender = 0)
            : code(eventCode), senderActorNumber(sender) {
            if (sender != 0) {
                parameters[ParameterCode::ActorNr] = PhotonValue(sender);
            }
        }

        void SetParam(uint8_t paramCode, const PhotonValue& val) {
            parameters[paramCode] = val;
            if (paramCode == ParameterCode::ActorNr) {
                senderActorNumber = val.AsInt(senderActorNumber);
            }
        }
        bool HasParam(uint8_t paramCode) const { return parameters.find(paramCode) != parameters.end(); }
        PhotonValue GetParam(uint8_t paramCode, const PhotonValue& def = PhotonValue()) const {
            auto it = parameters.find(paramCode);
            return (it != parameters.end()) ? it->second : def;
        }

        std::vector<uint8_t> Serialize() const;
        static bool Deserialize(const std::vector<uint8_t>& buffer, size_t& offset, EventData& outEvent);
    };

} // namespace ReFix::Photon::Protocol
