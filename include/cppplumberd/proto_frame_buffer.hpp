#pragma once

#include <string>
#include <memory>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include "cppplumberd/transport_interfaces.hpp"
#include "cppplumberd/message_serializer.hpp"
#include "cppplumberd/fault_exception.hpp"
#include "proto/cqrs.pb.h"

namespace cppplumberd {

    template<unsigned long long size>
    class ProtoFrameBuffer
    {
    private:
        uint8_t _buffer[size];
        size_t _size = 0;
        size_t _offset = 0;
        unique_ptr<MessageSerializer> _serializer;
    public:
        inline size_t Size() const { return _size; }
        inline uint8_t* Get() const { return _buffer; }

        template<typename THeader, unsigned int THeaderType, typename TMsg, unsigned int TMsgType>
        inline void Write(const THeader& header, const TMsg& msg)
        {
            // Reset buffer
            _size = 0;
            _offset = 0;

            // Reserve first 8 bytes for header size and payload size
            uint32_t* sizePtr = reinterpret_cast<uint32_t*>(_buffer);
            _offset = 8;

            // Serialize header directly to buffer
            if (!header.SerializeToArray(_buffer + _offset, size - _offset)) {
                throw std::runtime_error("Failed to serialize header");
            }

            // Store the serialized header size
            uint32_t headerSize = static_cast<uint32_t>(header.ByteSizeLong());
            sizePtr[0] = headerSize;

            // Update offset past the serialized header
            _offset += headerSize;

            // Serialize payload directly to buffer
            if (!msg.SerializeToArray(_buffer + _offset, size - _offset)) {
                throw std::runtime_error("Failed to serialize message payload");
            }

            // Store the serialized payload size
            uint32_t payloadSize = static_cast<uint32_t>(msg.ByteSizeLong());
            sizePtr[1] = payloadSize;

            // Calculate total size
            _size = _offset + payloadSize;

            // Validate the buffer size
            if (_size > size) {
                throw std::runtime_error("Message too large for buffer");
            }
        }


        template<typename THeader>
        inline void Read(const uint8_t* externalBuffer, size_t bufferSize,
            function<unsigned int(THeader&)> payloadMessageIdSelector,
            MessagePtr& headerPtr, MessagePtr& msgPtr)
        {
            // Check if buffer is large enough for header and payload sizes
            if (bufferSize < 8) {
                throw std::runtime_error("Buffer too small: " + std::to_string(bufferSize) + " bytes");
            }

            // Get pointer to the first 8 bytes containing sizes
            const uint32_t* sizePtr = reinterpret_cast<const uint32_t*>(externalBuffer);

            // Extract header and payload sizes
            uint32_t headerSize = sizePtr[0];
            uint32_t payloadSize = sizePtr[1];

            // Validate total size
            size_t totalExpectedSize = 8 + headerSize + payloadSize;
            if (bufferSize < totalExpectedSize) {
                throw std::runtime_error("Buffer truncated: expected " +
                    std::to_string(totalExpectedSize) +
                    " bytes, got " +
                    std::to_string(bufferSize));
            }

            // Extract header bytes (after the 8-byte size header)
            const char* headerBytes = reinterpret_cast<const char*>(externalBuffer + 8);

            // First create and parse header to determine payload type
            THeader typedHeader;
            if (!typedHeader.ParseFromArray(headerBytes, headerSize)) {
                throw std::runtime_error("Failed to parse header");
            }

            // Use the selector function to determine payload message type
            unsigned int payloadType = payloadMessageIdSelector(typedHeader);

            // Now create the headerPtr from the parsed header
            headerPtr = new THeader(typedHeader);

            // Extract payload bytes (after header)
            const char* payloadBytes = headerBytes + headerSize;

            // Parse payload using MessageSerializer with the determined type
            msgPtr = _serializer->Deserialize(payloadBytes, payloadSize, payloadType);
        }
    };
}