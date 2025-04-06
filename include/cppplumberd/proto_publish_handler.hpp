#pragma once

#include <string>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <chrono>
#include "cppplumberd/transport_interfaces.hpp"
#include "cppplumberd/message_serializer.hpp"
#include "proto/cqrs.pb.h"

namespace cppplumberd {

    class ProtoPublishHandler {
    public:
        explicit ProtoPublishHandler(std::unique_ptr<ITransportPublishSocket> socket)
            : _socket(std::move(socket)) {

            if (!_socket) {
                throw std::invalid_argument("Socket cannot be null");
            }
        }

        void Start() {
            _socket->Start();
        }

        template<typename TEvent, unsigned int EventId>
            requires HasParseFromString<TEvent>
        void RegisterMessage() {
            _serializer.RegisterMessage<TEvent, EventId>();
            _typeToIdMap[std::type_index(typeid(TEvent))] = EventId;
        }

        template<typename TEvent>
            requires HasParseFromString<TEvent>
        void Publish(const TEvent& evt) {
            auto typeIdx = std::type_index(typeid(TEvent));
            auto it = _typeToIdMap.find(typeIdx);

            if (it == _typeToIdMap.end()) {
                throw std::runtime_error("Event type not registered: " + std::string(typeid(TEvent).name()));
            }

            // Serialize the event payload
            std::string eventPayload = _serializer.Serialize(evt);

            // Prepare header
            EventHeader header;
            header.set_timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            header.set_event_type(it->second);

            // Serialize header
            std::string headerData;
            if (!header.SerializeToString(&headerData)) {
                throw std::runtime_error("Failed to serialize event header");
            }

            // Combine header and payload
            std::string message = headerData + eventPayload;

            // Send the message
            _socket->Send(message);
        }

    private:
        std::unique_ptr<ITransportPublishSocket> _socket;
        MessageSerializer _serializer;
        std::unordered_map<std::type_index, unsigned int> _typeToIdMap;
    };

} // namespace cppplumberd