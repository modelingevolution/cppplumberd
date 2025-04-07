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
#include "proto/cqrs.pb.h"

namespace cppplumberd {

    using namespace std::chrono;

    class ProtoReqRspSrvHandler {
    private:
        unique_ptr<ITransportReqRspSrvSocket> _socket;
        MessageSerializer _serializer;
        MessageDispatcher<string, CommandHeader> _dispatcher;
        bool _running = false;

        // Process incoming messages with proper framing
        string ProcessMessage(const string& requestData) {
            try {
                // Message must be at least 8 bytes (for header and payload sizes)
                if (requestData.size() < 8) {
                    throw runtime_error("Message too short");
                }

                // Extract sizes
                const uint32_t* sizePtr = reinterpret_cast<const uint32_t*>(requestData.data());
                uint32_t headerSize = sizePtr[0];
                uint32_t payloadSize = sizePtr[1];

                // Validate total size
                if (requestData.size() < 8 + headerSize + payloadSize) {
                    throw runtime_error("Message truncated");
                }

                // Parse command header
                CommandHeader header;
                if (!header.ParseFromArray(requestData.data() + 8, headerSize)) {
                    throw runtime_error("Failed to parse command header");
                }

                // Extract payload
                const char* payloadData = requestData.data() + 8 + headerSize;
                string payload(payloadData, payloadSize);

                try {
                    // Deserialize the payload using the message serializer
                    MessagePtr requestMsg = _serializer.Deserialize(payload, header.command_type());

                    // Process the command using MessageDispatcher
                    string responsePayload = _dispatcher.Handle(header, header.command_type(), requestMsg);

                    // Delete the message pointer (it was allocated by Deserialize)
                    delete requestMsg;

                    // Create success response
                    return CreateResponse(0, "", 0, responsePayload);
                }
                catch (const FaultException& ex) {
                    // Create error response with exception details
                    return CreateResponse(ex.ErrorCode(), ex.what(), ex.MessageTypeId(), ex.Serialize());
                }
            }
            catch (const exception& ex) {
                // Create standard error response
                return CreateResponse(1, ex.what(), 0, "");
            }
        }

        // Helper to create framed response with consistent format
        string CreateResponse(unsigned int statusCode,
            const string& errorMessage,
            unsigned int responseType,
            const string& payload) {
            // Create and serialize command response header
            CommandResponse response;
            response.set_status_code(statusCode);
            response.set_error_message(errorMessage);
            response.set_response_type(responseType);

            string headerData;
            response.SerializeToString(&headerData);

            // Allocate buffer for framed message
            vector<uint8_t> framedMessage(8 + headerData.size() + payload.size());
            uint32_t* sizePtr = reinterpret_cast<uint32_t*>(framedMessage.data());
            sizePtr[0] = headerData.size();  // NOLINT(clang-diagnostic-shorten-64-to-32)
            sizePtr[1] = payload.size(); // NOLINT(clang-diagnostic-shorten-64-to-32)

            // Copy header and payload data
            memcpy(framedMessage.data() + 8, headerData.data(), headerData.size());
            if (!payload.empty()) {
                memcpy(framedMessage.data() + 8 + headerData.size(), payload.data(), payload.size());
            }

            return string(reinterpret_cast<char*>(framedMessage.data()), framedMessage.size());
        }

    public:
        ProtoReqRspSrvHandler(unique_ptr<ITransportReqRspSrvSocket> socket)
            : _socket(move(socket)) {
            if (!_socket) {
                throw invalid_argument("Socket cannot be null");
            }
        }

        template<typename TReq, unsigned int ReqId, typename TRsp, unsigned int RspId>
        void RegisterHandler(function<TRsp(const TReq&)> handler) {
            // Register message types with serializer
            _serializer.RegisterMessage<TReq, ReqId>();
            _serializer.RegisterMessage<TRsp, RspId>();

            // Register handler with MessageDispatcher
            _dispatcher.RegisterHandler<TReq, ReqId>(
                [this, handler](const CommandHeader& header, const TReq& request) -> string {
                    // Call the handler - let exceptions propagate up
                    TRsp response = handler(request);

                    // Serialize the response
                    string responseData;
                    if (!response.SerializeToString(&responseData)) {
                        throw runtime_error("Failed to serialize response");
                    }

                    return responseData;
                }
            );
        }

        // Base class for fault exceptions
        

        template<typename TError, unsigned int MessageId>
        void RegisterError() {
            _serializer.RegisterMessage<TError, MessageId>();
        }

        void Start() {
            if (_running) return;

            // Initialize the socket with our message processing callback
            _socket->Initialize([this](const string& request) -> string {
                return this->ProcessMessage(request);
                });

            _socket->Start();
            _running = true;
        }

        void Stop() {
            _running = false;
        }
    };
}