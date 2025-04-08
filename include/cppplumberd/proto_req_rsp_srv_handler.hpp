#pragma once

#include <string>
#include <memory>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <chrono>
#include "cppplumberd/transport_interfaces.hpp"
#include "cppplumberd/message_serializer.hpp"
#include "cppplumberd/message_dispatcher.hpp"
#include "cppplumberd/proto_frame_buffer.hpp"
#include "proto/cqrs.pb.h"

namespace cppplumberd {

    using namespace std;

    class ProtoReqRspSrvHandler {
    private:
        unique_ptr<ITransportReqRspSrvSocket> _socket;
        shared_ptr<MessageSerializer> _serializer;
        MessageDispatcher<size_t, CommandHeader> _dispatcher;
        bool _running = false;

        // Process incoming messages with direct buffer access
        inline size_t HandleRequest(const size_t requestSize) {
            _inBuffer->AckWritten(requestSize);
            _outBuffer->Reset();
            auto responseTypeSelector = [](const CommandHeader& header) -> unsigned int {
                // Return 0 if no response type (for void responses) or the actual response type
                return header.command_type();
                };
            MessagePtr payload;
            unique_ptr<CommandHeader> header = this->_inBuffer->Read<CommandHeader>(responseTypeSelector, payload);
            CommandResponse rsp;
            try {
                size_t retSize = _dispatcher.Handle(*header, header->command_type(), payload);
                return retSize;
            }
            catch (const FaultException &f)
            {
                rsp.set_error_message(f.what());
                rsp.set_status_code(f.ErrorCode());
                rsp.set_response_type(f.MessageTypeId());
                // we need to serialize exception and return;
                _outBuffer->Write(rsp, f.Get());
                return _outBuffer->Written();
            }
        }
        unique_ptr<ProtoFrameBuffer<64 * 1024>> _inBuffer;
        unique_ptr<ProtoFrameBuffer<64 * 1024>> _outBuffer;
        inline bool OnStart()
        {
            if (_running) return true;

            auto replyFunc = [this](const size_t requestSize) -> size_t {
                return this->HandleRequest(requestSize);
                };
            _inBuffer = make_unique<ProtoFrameBuffer<64 * 1024>>(_serializer);
            _outBuffer = make_unique<ProtoFrameBuffer<64 * 1024>>(_serializer);
            _socket->Initialize(replyFunc, _inBuffer->Get(), _inBuffer->FreeBytes(), _outBuffer->Get(), _outBuffer->FreeBytes());
            return false;
        }
    public:
        inline ProtoReqRspSrvHandler(unique_ptr<ITransportReqRspSrvSocket> socket, shared_ptr< MessageSerializer> serializer)
            : _socket(move(socket)), _serializer(serializer) {
            if (!_socket) {
                throw invalid_argument("Socket cannot be null");
            }
        }

        inline ProtoReqRspSrvHandler(unique_ptr<ITransportReqRspSrvSocket> socket)
            : ProtoReqRspSrvHandler(move(socket),make_shared<MessageSerializer>()){
        }
        
        // Old string-based Initialize method (throws not supported exception)
        inline void Initialize(function<string(const string&)> handler) {
            throw runtime_error("String-based handlers are no longer supported. Use buffer-based initialization.");
        }

        template<typename TReq, unsigned int ReqId, typename TRsp, unsigned int RspId>
        inline void RegisterHandler(function<TRsp(const TReq&)> handler) {
            // Register message types with serializer
            _serializer->RegisterMessage<TReq, ReqId>();
            _serializer->RegisterMessage<TRsp, RspId>();

            // Register handler with MessageDispatcher
            _dispatcher.RegisterHandler<TReq, ReqId>(
                [this, handler](const CommandHeader& header, const TReq& request) -> size_t {
                    // Call the handler - let exceptions propagate up
                    TRsp response = handler(request);
                    CommandResponse rsp;
                    rsp.set_status_code(200);
                    return _outBuffer->Write(rsp,&response);
                }
            );
        }
        template<typename TReq, unsigned int ReqId>
        inline void RegisterHandlerWithMetadata(function<void(const CommandHeader &header, const TReq&)> handler) {
            // Register message types with serializer
            _serializer->RegisterMessage<TReq, ReqId>();
            _dispatcher.RegisterHandler<TReq, ReqId>(
                [this, handler](const CommandHeader& header, const TReq& request) -> size_t {
                    // Call the handler - let exceptions propagate up
                    handler(header,request);
                    CommandResponse rsp;
                    rsp.set_status_code(200);
                    return _outBuffer->Write(rsp);
                }
            );
        }
        template<typename TReq, unsigned int ReqId>
        inline void RegisterHandler(function<void(const TReq&)> handler) {
            // Register message types with serializer
            _serializer->RegisterMessage<TReq, ReqId>();
            _dispatcher.RegisterHandler<TReq, ReqId>(
                [this, handler](const CommandHeader& header, const TReq& request) -> size_t {
                    // Call the handler - let exceptions propagate up
                    handler(request);
                    CommandResponse rsp;
                    rsp.set_status_code(200);
                    return _outBuffer->Write(rsp);
                }
            );
        }

        template<typename TError, unsigned int MessageId>
        inline void RegisterError() {
            _serializer->RegisterMessage<TError, MessageId>();
        }


       
        inline void Start(const string& url) {
            if (OnStart()) return;
            _socket->Start(url);
            _running = true;
        }
        inline void Start() {
            if (OnStart()) return;
            _socket->Start();
            _running = true;
        }

        inline void Stop() {
            _running = false;
        }

        inline ~ProtoReqRspSrvHandler() {
            
        }
    };
}