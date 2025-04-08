#pragma once

#include <memory>
#include <functional>
#include <string>
#include "cppplumberd/proto_req_rsp_srv_handler.hpp"
#include "cppplumberd/fault_exception.hpp"
#include "cqrs_abstractions.hpp"

namespace cppplumberd {


    class CommandServiceHandler {
    public:
        inline CommandServiceHandler(std::unique_ptr<ProtoReqRspSrvHandler> handler)
            : _handler(std::move(handler)) {
            if (!_handler) {
                throw std::invalid_argument("Handler cannot be null");
            }
        }

        template<typename TCommand>
        inline void RegisterHandler(const std::shared_ptr<ICommandHandler<TCommand>>& handler) {
            if (!handler) {
                throw std::invalid_argument("Command handler cannot be null");
            }

            // Register a handler for the command type
            _handler->RegisterHandlerWithMetadata<TCommand>([handler](const CommandHeader &h, const TCommand& cmd) {
                // Here we would normally pass the stream_id, but for simplicity
                // we're using an empty string since we don't have access to it
                handler->Handle(h.Recipient(), cmd);
                });
        }

        template<typename TException, unsigned int MessageId>
        inline void RegisterError() {
            _handler->RegisterError<TException, MessageId>();
        }

        inline void Start(const std::string& url) {
            _handler->Start(url);
        }

        inline void Start() {
            _handler->Start();
        }

        inline void Stop() {
            _handler->Stop();
        }

    private:
        std::unique_ptr<ProtoReqRspSrvHandler> _handler;
    };

} // namespace cppplumberd