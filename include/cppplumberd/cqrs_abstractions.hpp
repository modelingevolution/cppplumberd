#pragma once

#include <memory>
#include <functional>
#include <string>
#include "cppplumberd/proto_req_rsp_srv_handler.hpp"
#include "cppplumberd/fault_exception.hpp"

namespace cppplumberd {

    template<typename TCommand>
    class ICommandHandler {
    public:
        virtual void Handle(const std::string& stream_id, const TCommand& cmd) = 0;
        virtual ~ICommandHandler() = default;
    };
}