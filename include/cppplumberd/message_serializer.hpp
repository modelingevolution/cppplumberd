#pragma once
#include <string>
#include <stdexcept>
#include <map>
#include <memory>
#include <typeindex>
#include <functional>
#include <typeinfo>
#include <concepts>
#include <google/protobuf/message.h>

#include "message_dispatcher.hpp"

namespace cppplumberd
{
    using namespace std;

    // Concept that requires ParseFromString method
    template<typename T>
    concept HasParseFromString = requires(T t, const string & s) {
        { t.ParseFromString(s) } -> same_as<bool>;
    };
    template<typename T>
    concept HasSerializeToZeroCopyStream = requires(T t, const string & s) {
        { t.SerializeToZeroCopyStream(s) } -> same_as<bool>;
    };

    class MessageSerializer {
    private:
        // Nested class to store message type information
        class MessageTypeInfo {
        public:
            type_index typeIdx;
            function<MessagePtr()> factory;

            // Constructor for direct initialization
            MessageTypeInfo(type_index idx, function<MessagePtr()> func)
                : typeIdx(idx), factory(func) {
            }

            // Default constructor for map compatibility
            MessageTypeInfo()
                : typeIdx(typeid(void)), factory(nullptr) {
            }

            // Copy constructor
            MessageTypeInfo(const MessageTypeInfo& other)
                : typeIdx(other.typeIdx), factory(other.factory) {
            }

            // Move constructor
            MessageTypeInfo(MessageTypeInfo&& other) noexcept
                : typeIdx(other.typeIdx), factory(std::move(other.factory)) {
            }

            // Copy assignment
            MessageTypeInfo& operator=(const MessageTypeInfo& other) {
                typeIdx = other.typeIdx;
                factory = other.factory;
                return *this;
            }

            // Move assignment
            MessageTypeInfo& operator=(MessageTypeInfo&& other) noexcept {
                typeIdx = other.typeIdx;
                factory = std::move(other.factory);
                return *this;
            }

            // Create a new instance of the message
            MessagePtr CreateMessage() const {
                return factory();
            }
        };

    public:
        template<typename TMessage, unsigned int MessageId>
            requires HasParseFromString<TMessage>
        inline void RegisterMessage() {
	        type_index typeIdx = type_index(typeid(TMessage));
            if (_messageIdMap.find(MessageId) != _messageIdMap.end()) {
                throw runtime_error("Message ID already registered");
            }
            _typeIdMap[typeIdx] = MessageId;
            _messageIdMap[MessageId] = MessageTypeInfo(
                typeIdx,
                []() -> MessagePtr { return new TMessage(); }
            );
        }
        inline MessagePtr Deserialize(const void* data, const size_t size, const unsigned int messageId) const {
            auto it = _messageIdMap.find(messageId);
            if (it == _messageIdMap.end()) {
                throw runtime_error("Message ID not registered");
            }
            const auto& typeInfo = it->second;

            MessagePtr msg = typeInfo.CreateMessage();
            if (!msg->ParseFromArray(data, size)) {
                throw runtime_error("Failed to parse message");
            }
            return msg;
        }
        inline MessagePtr Deserialize(const string& data, const unsigned int messageId) const {
            auto it = _messageIdMap.find(messageId);
            if (it == _messageIdMap.end()) {
                throw runtime_error("Message ID not registered");
            }
            const auto& typeInfo = it->second;

            MessagePtr msg = typeInfo.CreateMessage();
            if (!msg->ParseFromString(data)) {
                throw runtime_error("Failed to parse message");
            }
            return msg;
        }
        template<typename TMessage>
        inline unsigned int GetMessageId() { return _typeIdMap[type_index(typeid(TMessage))]; }
      
        template<typename TMessage>
            requires HasSerializeToZeroCopyStream<TMessage>

        inline void Serialize(const TMessage& message, uint8_t* buffer, size_t offset, size_t bufferSize) const {
            google::protobuf::io::ArrayOutputStream arrayStream(buffer + offset, bufferSize - offset);
            
            if (!message.SerializeToZeroCopyStream(&arrayStream)) {
                throw std::runtime_error("Failed to serialize message");
            }
            
        }

        inline void Serialize(const MessagePtr ptr, uint8_t* buffer, size_t offset, size_t size)
        {
            // Create a zero-copy output stream over the buffer starting at the offset
            google::protobuf::io::ArrayOutputStream arrayStream(buffer + offset, size);

            // Serialize the message to the zero-copy stream
            if (!ptr->SerializeToZeroCopyStream(&arrayStream)) {
                throw std::runtime_error("Failed to serialize message");
            }
        }

        inline string Serialize(const MessagePtr ptr) const {
            string result;
            
            if (!ptr->SerializeToString(&result)) {
                throw runtime_error("Failed to serialize message");
            }
            return result;
        }

        template<typename TMessage>
            requires HasParseFromString<TMessage>
        inline string Serialize(const TMessage& message) const {
            string result;
            if (!message.SerializeToString(&result)) {
                throw runtime_error("Failed to serialize message");
            }
            return result;
        }
    private:
        map<unsigned int, MessageTypeInfo> _messageIdMap;
        map< type_index, unsigned int> _typeIdMap;
    };
}