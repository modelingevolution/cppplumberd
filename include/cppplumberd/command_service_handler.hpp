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

        template<typename TCommand, unsigned int TCommandId>
        inline void RegisterHandler(const std::shared_ptr<ICommandHandler<TCommand>>& handler) {
            if (!handler) {
                throw std::invalid_argument("Command handler cannot be null");
            }

            _handler->RegisterHandlerWithMetadata<TCommand, TCommandId>([handler](const CommandHeader &h, const TCommand& cmd) {
                handler->Handle(h.recipient(), cmd);
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