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
        shared_ptr<MessageSerializer> _serializer ;


    public:
        inline ProtoFrameBuffer(shared_ptr<MessageSerializer> s) :_serializer(s) {
        }
        inline size_t Size() const { return _size; }
        inline uint8_t* Get() const { return (uint8_t*)_buffer; }
        inline size_t FreeBytes() const { return size - _size; }
        

        template<typename THeader>
        inline size_t Write(const THeader& header, MessagePtr ptr)
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

            if (ptr == nullptr)
            {
                _size = _offset;
                return _size;
            }

            // Serialize payload directly to buffer
            if (!ptr->SerializeToArray(_buffer + _offset, size - _offset)) {
                throw std::runtime_error("Failed to serialize message payload");
            }

            // Store the serialized payload size
            uint32_t payloadSize = static_cast<uint32_t>(ptr->ByteSizeLong());
            sizePtr[1] = payloadSize;

            // Calculate total size
            _size = _offset + payloadSize;

            // Validate the buffer size
            if (_size > size) {
                throw std::runtime_error("Message too large for buffer");
            }
            return _size;
        }
        inline void AckWritten(size_t size)
        {
            _size = size;
        }

        template<typename THeader>
        inline unique_ptr<THeader> Read(function<unsigned int(THeader&)> payloadMessageIdSelector, MessagePtr& msgPtr)
        {
            // Check if buffer is large enough for header and payload sizes
            if (_size < 8) {
                throw std::runtime_error("Buffer too small");
            }

            // Get pointer to the first 8 bytes containing sizes
            const uint32_t* sizePtr = reinterpret_cast<const uint32_t*>(_buffer);

            // Extract header and payload sizes
            uint32_t headerSize = sizePtr[0];
            uint32_t payloadSize = sizePtr[1];

            // Validate total size
            size_t totalExpectedSize = 8 + headerSize + payloadSize;
            if (_size < totalExpectedSize) {
                throw std::runtime_error("Buffer truncated: expected " +
                    std::to_string(totalExpectedSize) +
                    " bytes, got " +
                    std::to_string(_size));
            }

            // Extract header bytes (after the 8-byte size header)
            const char* headerBytes = reinterpret_cast<const char*>(_size + 8);

            // First create and parse header to determine payload type
            unique_ptr<THeader> typedHeader = make_unique<THeader>();
            if (!typedHeader->ParseFromArray(headerBytes, headerSize)) {
                throw std::runtime_error("Failed to parse header");
            }

            // Use the selector function to determine payload message type
            unsigned int payloadType = payloadMessageIdSelector(*typedHeader);


            // Extract payload bytes (after header)
            const char* payloadBytes = headerBytes + headerSize;

            // Parse payload using MessageSerializer with the determined type
            msgPtr = _serializer->Deserialize(payloadBytes, payloadSize, payloadType);
            return typedHeader;
        }

        inline void Reset()
        {
            _size = 0;
            _offset = 0;
        }
    };
}