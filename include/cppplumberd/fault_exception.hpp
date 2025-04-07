#pragma once 
#include <string>
#include <memory>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include "cppplumberd/transport_interfaces.hpp"
#include "cppplumberd/message_serializer.hpp"
#include "proto/cqrs.pb.h"

namespace cppplumberd {
    class FaultException : public runtime_error {
    public:
        FaultException(const string& message, unsigned int errorCode = 0)
            : runtime_error(message), _errorCode(errorCode) {
        }

        virtual ~FaultException() = default;

        virtual MessagePtr Get() const { return nullptr; }
        virtual unsigned int MessageTypeId() const { return 0; }

        unsigned int ErrorCode() const { return _errorCode; }

    private:
        unsigned int _errorCode;
    };

    // Typed fault exception
    template<typename TMsg>
    class TypedFaultException : public FaultException {
    public:
        template<typename... Args>
        TypedFaultException(const string& message,
            unsigned int messageTypeId,
            unsigned int errorCode,
            Args&&... args)
            : FaultException(message, errorCode),
            _messageTypeId(messageTypeId) {
            _errorDetails = new TMsg(std::forward<Args>(args)...);
        }
        TypedFaultException(unsigned int messageTypeId,
            unsigned int errorCode,
            const string& message,
            TMsg* details)
            : FaultException(message, errorCode),
            _messageTypeId(messageTypeId) {
            _errorDetails = details;
        }

        const TMsg& ErrorDetails() const { return _errorDetails; }

        ~TypedFaultException() override {
            delete _errorDetails;
        }

        unsigned int MessageTypeId() const override {
            return _messageTypeId;
        }

    private:
        TMsg* _errorDetails;
        unsigned int _messageTypeId;
    };
}