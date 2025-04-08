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
    class ProtoFrameBufferView
    {
    private:
        uint8_t* _buffer;
        size_t _capacity;
        size_t _written = 0;
        shared_ptr<MessageSerializer> _serializer;


    public:
        inline ProtoFrameBufferView(shared_ptr<MessageSerializer> s, uint8_t* buffer, size_t capacity) :_serializer(s) {
			_buffer = buffer;
			_capacity = capacity;
			_written = 0;
        }
        inline size_t Written() const { return _written; }
        inline uint8_t* Get() const { return (uint8_t*)_buffer; }
        inline size_t FreeBytes() const { return _capacity - _written; }
        inline shared_ptr<MessageSerializer> Serializer() { return _serializer; }

        template<typename THeader, typename TPayload>
        inline size_t Write(const THeader& header, const TPayload& payload)
        {
            size_t offset = _written + 8;
            // Reserve first 8 bytes for header size and payload size
            uint32_t* sizePtr = reinterpret_cast<uint32_t*>(_buffer);

            // Serialize header directly to buffer
            if (!header.SerializeToArray(_buffer + offset, _capacity - offset)) {
                throw std::runtime_error("Failed to serialize header");
            }

            // Store the serialized header size
            uint32_t headerSize = static_cast<uint32_t>(header.ByteSizeLong());
            sizePtr[0] = headerSize;

            // Update offset past the serialized header
            offset += headerSize;

            // Serialize payload directly to buffer
            if (!payload.SerializeToArray(_buffer + offset, _capacity - offset)) {
                throw std::runtime_error("Failed to serialize message payload");
            }

            // Store the serialized payload size
            uint32_t payloadSize = static_cast<uint32_t>(payload.ByteSizeLong());
            sizePtr[1] = payloadSize;

            // Calculate total size
            _written = offset + payloadSize;

            // Validate the buffer size
            if (_written > _capacity) {
                throw std::runtime_error("Message too large for buffer");
            }

            return _written;
        }
        template<typename THeader>
        inline size_t Write(const THeader& header, MessagePtr ptr)
        {
            size_t offset = _written + 8;

            // Reserve first 8 bytes for header size and payload size
            uint32_t* sizePtr = reinterpret_cast<uint32_t*>(_buffer);

            // Serialize header directly to buffer
            if (!header.SerializeToArray(_buffer + offset, _capacity - offset)) {
                throw std::runtime_error("Failed to serialize header");
            }

            // Store the serialized header size
            uint32_t headerSize = static_cast<uint32_t>(header.ByteSizeLong());
            sizePtr[0] = headerSize;

            // Update offset past the serialized header
            offset += headerSize;

            if (ptr == nullptr)
            {
                _written = offset;
                return _written;
            }

            // Serialize payload directly to buffer
            if (!ptr->SerializeToArray(_buffer + offset, _capacity - offset)) {
                throw std::runtime_error("Failed to serialize message payload");
            }

            // Store the serialized payload size
            uint32_t payloadSize = static_cast<uint32_t>(ptr->ByteSizeLong());
            sizePtr[1] = payloadSize;

            // Calculate total size
            _written = offset + payloadSize;

            // Validate the buffer size
            if (_written > _capacity) {
                throw std::runtime_error("Message too large for buffer");
            }
            return _written;
        }
        inline void AckWritten(size_t size)
        {
            _written = size;
        }

        template<typename THeader>
        inline unique_ptr<THeader> Read(function<unsigned int(THeader&)> payloadMessageIdSelector, MessagePtr& msgPtr, size_t offset = 0)
        {
            // Check if buffer is large enough for header and payload sizes
            if (_written < 8) {
                throw std::runtime_error("Buffer too small");
            }

            // Get pointer to the first 8 bytes containing sizes
            const uint32_t* sizePtr = reinterpret_cast<const uint32_t*>(_buffer + offset);

            // Extract header and payload sizes
            uint32_t headerSize = sizePtr[0];
            uint32_t payloadSize = sizePtr[1];

            size_t totalExpectedSize = 8 + headerSize + payloadSize + offset;
			if (_written < totalExpectedSize) {
				throw std::runtime_error("Buffer too small for header and payload");
			}
            //const char* headerBytes = reinterpret_cast<const char*>(_buffer+offset + 8);
            auto headerBytes = _buffer + offset + 8;
            unique_ptr<THeader> typedHeader = make_unique<THeader>();
            if (!typedHeader->ParseFromArray(headerBytes, headerSize)) {
                throw std::runtime_error("Failed to parse header");
            }

            unsigned int payloadType = payloadMessageIdSelector(*typedHeader);
            auto payloadBytes = headerBytes + headerSize;

            msgPtr = _serializer->Deserialize(payloadBytes, payloadSize, payloadType);
            return typedHeader;
        }

        inline void Reset()
        {
            _written = 0;
        }
    };
    template<unsigned long long size>
    class ProtoFrameBuffer : public ProtoFrameBufferView
    {
    public: ProtoFrameBuffer(shared_ptr<MessageSerializer> s) : ProtoFrameBufferView(s, _buffer, size) {}
    public: ProtoFrameBuffer() : ProtoFrameBuffer(make_shared<MessageSerializer>(), _buffer, size) {}
    private:
        uint8_t _buffer[size];
    };

    
    
}