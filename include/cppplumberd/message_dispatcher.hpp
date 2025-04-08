#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <typeindex>
#include <functional>
#include <stdexcept>
#include <google/protobuf/message.h>

namespace cppplumberd
{
    using namespace std;
    typedef google::protobuf::Message* MessagePtr;

    template <typename TRsp, typename TMeta>
    class MessageDispatcher {
    public:
        // Maps message IDs to handler functions that take a Message* and return TRsp
        unordered_map<unsigned int, function<TRsp(const TMeta&, MessagePtr)>> _handlers;

        // Maps message IDs to their type_index for type checking
        unordered_map<unsigned int, type_index> _messageTypes;

        // Maps type_index to message IDs for reverse lookup
        unordered_map<type_index, unsigned int> _typeToIdMap;

        template<typename TMessage, unsigned int MessageId>
        void RegisterHandler(function<TRsp(const TMeta&, const TMessage&)> handler) {
            /*static_assert(is_base_of<MessagePtr, TMessage>::value,
                "TMessage must derive from MessagePtr");*/

            // Store the message type for runtime type checking
            /*_messageTypes[MessageId] = type_index(typeid(TMessage));
            _typeToIdMap[type_index(typeid(TMessage))] = MessageId;*/
            _messageTypes.insert_or_assign(MessageId, type_index(typeid(TMessage)));
            _typeToIdMap.insert_or_assign(type_index(typeid(TMessage)), MessageId);

            // Create an adapter function that downcasts the Message* to TMessage*
            _handlers[MessageId] = [handler](const TMeta& metadata, MessagePtr msg) -> TRsp {
                // Ensure the message is of the expected type
                const TMessage* typedMsg = dynamic_cast<const TMessage*>(msg);
                if (!typedMsg) {
                    throw runtime_error("Message type mismatch");
                }

                // Call the handler with the properly typed message
                return handler(metadata, *typedMsg);
                };
        }

        TRsp Handle(const TMeta& metadata, unsigned int messageId, MessagePtr msg) {
            // Find the handler for this message ID
            auto handlerIt = _handlers.find(messageId);
            if (handlerIt == _handlers.end()) {
                throw runtime_error("No handler registered for message ID: " + to_string(messageId));
            }

            // Check that the message is not null
            if (!msg) {
                throw runtime_error("Null message pointer");
            }

            // Call the handler
            return handlerIt->second(metadata, msg);
        }

        template<typename TMessage>
        TRsp Handle(const TMeta& metadata, const TMessage& msg) {
            static_assert(is_base_of<MessagePtr, TMessage>::value,
                "TMessage must derive from MessagePtr");

            // Find the message ID for this message type
            auto typeIdx = type_index(typeid(TMessage));
            auto idIt = _typeToIdMap.find(typeIdx);
            if (idIt == _typeToIdMap.end()) {
                throw runtime_error("Message type not registered");
            }

            // Get the message ID
            unsigned int messageId = idIt->second;

            // Call the handler with the message ID and message pointer
            return Handle(metadata, messageId, &msg);
        }

        template<typename TMessage, unsigned int MessageId>
        void RegisterMessage() {
            static_assert(is_base_of<MessagePtr, TMessage>::value,
                "TMessage must derive from MessagePtr");

            // Register the message type without registering a handler
            _messageTypes[MessageId] = type_index(typeid(TMessage));
            _typeToIdMap[type_index(typeid(TMessage))] = MessageId;
        }
    };
}