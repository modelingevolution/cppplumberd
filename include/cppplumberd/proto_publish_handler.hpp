#pragma once

#include <string>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <chrono>
#include <array>
#include "cppplumberd/transport_interfaces.hpp"
#include "cppplumberd/message_serializer.hpp"
#include "proto/cqrs.pb.h"
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

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

            // Allocate a 64KB buffer on the stack
            std::array<uint8_t, 64 * 1024> buffer;

            // Get a uint32_t pointer to the buffer for easier size field access
            uint32_t* sizePtr = reinterpret_cast<uint32_t*>(buffer.data());

            // Reserve first 8 bytes for header size and payload size
            size_t offset = 8;

            // Prepare header
            EventHeader header;
            header.set_timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
            header.set_event_type(it->second);

            // Serialize header directly to buffer (after the size fields)
            google::protobuf::io::ArrayOutputStream headerArrayStream(buffer.data() + offset, buffer.size() - offset);
            if (!header.SerializeToZeroCopyStream(&headerArrayStream)) {
                throw std::runtime_error("Failed to serialize event header");
            }

            // Get the header size and store in the first 4 bytes
            uint32_t headerSize = static_cast<uint32_t>(headerArrayStream.ByteCount());
            sizePtr[0] = headerSize;

            // Update offset past the serialized header
            offset += headerSize;

            // Serialize payload directly to buffer
            google::protobuf::io::ArrayOutputStream payloadArrayStream(buffer.data() + offset, buffer.size() - offset);
            if (!evt.SerializeToZeroCopyStream(&payloadArrayStream)) {
                throw std::runtime_error("Failed to serialize event payload");
            }

            // Get the payload size and store in the second 4 bytes
            uint32_t payloadSize = static_cast<uint32_t>(payloadArrayStream.ByteCount());
            sizePtr[1] = payloadSize;

            // Check if we've exceeded buffer size
            if (offset + payloadSize > buffer.size()) {
                throw std::runtime_error("Message too large for buffer");
            }

            // Update offset past the serialized payload
            offset += payloadSize;

            // Send the framed message
            std::string message(reinterpret_cast<char*>(buffer.data()), offset);
            _socket->Send(message);
        }

    private:
        std::unique_ptr<ITransportPublishSocket> _socket;
        MessageSerializer _serializer;
        std::unordered_map<std::type_index, unsigned int> _typeToIdMap;
    };

} // namespace cppplumberd