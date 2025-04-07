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
#include "proto_frame_buffer.hpp"

namespace cppplumberd {

    
    class ProtoReqRspClientHandler {
    private:
        std::unique_ptr<ITransportReqRspClientSocket> _socket;
        MessageSerializer _serializer;
        bool _connected = false;

        // Map of message types to exception factories
        std::unordered_map<unsigned int, std::function<void(const std::string&, MessagePtr, unsigned int)>> _exceptionFactories;

        // Helper method to create a framed request message
        std::string CreateRequestMessage(unsigned int commandType, const std::string& payload) {
            // Create command header
            CommandHeader header;
            header.set_command_type(commandType);

            // Serialize header
            std::string headerData;
            header.SerializeToString(&headerData);

            // Create framed message (8 bytes for sizes + header + payload)
            std::vector<uint8_t> framedMessage(8 + headerData.size() + payload.size());
            uint32_t* sizePtr = reinterpret_cast<uint32_t*>(framedMessage.data());
            sizePtr[0] = headerData.size();
            sizePtr[1] = payload.size();

            // Copy header and payload data
            std::memcpy(framedMessage.data() + 8, headerData.data(), headerData.size());
            if (!payload.empty()) {
                std::memcpy(framedMessage.data() + 8 + headerData.size(), payload.data(), payload.size());
            }

            return std::string(reinterpret_cast<char*>(framedMessage.data()), framedMessage.size());
        }

        // Parse and process response
        template<typename TRsp>
        TRsp ProcessResponse(const std::string& responseData) {
            // Response must be at least 8 bytes (for header and payload sizes)
            if (responseData.size() < 8) {
                throw std::runtime_error("Response too short");
            }

            // Extract sizes
            const uint32_t* sizePtr = reinterpret_cast<const uint32_t*>(responseData.data());
            uint32_t headerSize = sizePtr[0];
            uint32_t payloadSize = sizePtr[1];

            // Validate total size
            if (responseData.size() < 8 + headerSize + payloadSize) {
                throw std::runtime_error("Response truncated");
            }

            // Parse command response header
            CommandResponse response;
            if (!response.ParseFromArray(responseData.data() + 8, headerSize)) {
                throw std::runtime_error("Failed to parse command response");
            }

            // Check for errors
            if (response.status_code() != 0) {
                // Extract error payload if available
                if (response.response_type() > 0 && payloadSize > 0) {
                    // There's a typed error to deserialize
                    const char* payloadData = responseData.data() + 8 + headerSize;
                    std::string payload(payloadData, payloadSize);

                    // Use serializer to deserialize the error
                    MessagePtr errorMsg = _serializer.Deserialize(payload, response.response_type());

                    // Find the exception factory for this error type
                    auto factoryIt = _exceptionFactories.find(response.response_type());
                    if (factoryIt != _exceptionFactories.end()) {
                        // Create and throw the appropriate exception
                        factoryIt->second(response.error_message(), errorMsg, response.status_code());

                        // Should never reach here, but just in case
                        delete errorMsg;
                    }

                    // No factory found, clean up and throw generic exception
                    delete errorMsg;
                }

                // Generic error with no details
                throw std::runtime_error("Server error: " + response.error_message());
            }

            // Process successful response
            if (payloadSize > 0) {
                // Extract payload
                const char* payloadData = responseData.data() + 8 + headerSize;
                std::string payload(payloadData, payloadSize);

                // Deserialize to the expected response type
                TRsp responsObj;
                if (!responsObj.ParseFromString(payload)) {
                    throw std::runtime_error("Failed to parse response payload");
                }

                return responsObj;
            }
            else {
                // Empty response (void)
                return TRsp();
            }
        }

    public:
        ProtoReqRspClientHandler(std::unique_ptr<ITransportReqRspClientSocket> socket)
            : _socket(std::move(socket)) {
            if (!_socket) {
                throw std::invalid_argument("Socket cannot be null");
            }
        }

        // Register request-response pair
        template<typename TReq, unsigned int ReqId, typename TRsp, unsigned int RspId>
        void RegisterRequestResponse() {
            _serializer.RegisterMessage<TReq, ReqId>();
            _serializer.RegisterMessage<TRsp, RspId>();
        }

        // Register error type with exception factory
        template<typename TError, unsigned int MessageId>
        void RegisterError() {
            _serializer.RegisterMessage<TError, MessageId>();

            // Register the exception factory that creates and throws the appropriate exception
            _exceptionFactories[MessageId] = [](const std::string& message, MessagePtr errorDetails, unsigned int errorCode) {
                // Create specific error from the generic message
                TError* typedError = dynamic_cast<TError*>(errorDetails);
                if (!typedError) {
                    delete errorDetails;
                    throw std::runtime_error("Error type mismatch");
                }

                // Throw typed exception
                throw TypedFaultException<TError>(message, MessageId, errorCode, *typedError);
                };
        }

        // Send request and receive response
        template<typename TReq, typename TRsp>
        TRsp Send(const TReq& request) {
            // Ensure connected
            if (!_connected) {
                _socket->Start();
                _connected = true;
            }

            // Get message type ID and serialize request
            unsigned int reqId = _serializer.GetMessageId<TReq>();
            std::string requestPayload = _serializer.Serialize(request);

            // Send request and receive response
            std::string responseData = _socket->Send(CreateRequestMessage(reqId, requestPayload));

            // Process response
            return ProcessResponse<TRsp>(responseData);
        }

        // Start the client
        void Start(const std::string& url) {
            if (!_connected) {
                _socket->Start(url);
                _connected = true;
            }
        }

        void Start() {
            if (!_connected) {
                _socket->Start();
                _connected = true;
            }
        }
    };

} // namespace cppplumberd