#pragma once
#include <string>
#include <stdexcept>
#include <map>
#include <memory>
#include <typeindex>
#include <functional>
#include <typeinfo>
#include <tuple>
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

    class MessageSerializer {
    public:
        template<typename TMessage, unsigned int MessageId>
            requires HasParseFromString<TMessage>
        inline void RegisterMessage() {
            auto typeIdx = type_index(typeid(TMessage));
            if (_messageIdMap.find(MessageId) != _messageIdMap.end()) {
                throw runtime_error("Message ID already registered");
            }

            _messageIdMap[MessageId] = make_tuple(
                typeIdx,
                []() -> MessagePtr { return new TMessage(); }
            );
        }

        inline MessagePtr Deserialize(const string& data, const unsigned int messageId) const {
            auto it = _messageIdMap.find(messageId);
            if (it == _messageIdMap.end()) {
                throw runtime_error("Message ID not registered");
            }
            const auto& r = it->second;

            MessagePtr msg = get<1>(r)();
            if (!msg->ParseFromString(data)) {
                throw runtime_error("Failed to parse message");
            }
            return msg;
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
        map<unsigned int, tuple<type_index, function<MessagePtr()>>> _messageIdMap;
    };
}
