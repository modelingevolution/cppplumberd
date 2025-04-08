#pragma once

#include <string>
#include <memory>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include "cppplumberd/transport_interfaces.hpp"
#include "cppplumberd/message_serializer.hpp"
#include "cppplumberd/fault_exception.hpp"
#include "cppplumberd/proto_frame_buffer.hpp"
#include "proto/cqrs.pb.h"

namespace cppplumberd {

	class ProtoReqRspClientHandler {
	private:
		std::unique_ptr<ITransportReqRspClientSocket> _socket;
		std::shared_ptr<MessageSerializer> _serializer;
		bool _connected = false;

		// Map of message types to exception factories
		std::unordered_map<unsigned int, std::function<void(const std::string&, MessagePtr, unsigned int)>> _exceptionFactories;

		
		unique_ptr<CommandResponse> OnResponse(const ProtoFrameBuffer<64 * 1024>& frameBuffer, MessagePtr& payloadPtr)
		{
			if (frameBuffer.Written() < 8) {
				throw std::runtime_error("Response too short");
			}
			// Use a selector function that extracts the message type from the response header
			auto responseTypeSelector = [](const CommandResponse& header) -> unsigned int {
				// Return 0 if no response type (for void responses) or the actual response type
				return header.response_type();
			};

			// Parse using ProtoFrameBuffer
			payloadPtr = nullptr;

			auto response = frameBuffer.Read<CommandResponse>(responseTypeSelector, payloadPtr);
			if (!response) {
				delete payloadPtr;
				throw std::runtime_error("Failed to parse command response header");
			}
			return response;
		}
		void ProcessResponse(const ProtoFrameBuffer<64 * 1024>& frameBuffer, size_t received)
		{
			MessagePtr payloadPtr;
			auto response = OnResponse(frameBuffer, payloadPtr);
			if (response->status_code() >= 300 || response->status_code() < 200) {

				std::string errorMsg = response->error_message();
				auto status = response->status_code();

				if (payloadPtr != nullptr) {
					// Find the exception factory for this error type
					auto factoryIt = _exceptionFactories.find(response->response_type());
					if (factoryIt != _exceptionFactories.end()) {
						factoryIt->second(errorMsg, payloadPtr, status);
						// Should never reach here
					}

					delete payloadPtr;
					payloadPtr = nullptr;
				}

				throw FaultException(errorMsg, status);
			}
			// fail fast.
			if (payloadPtr != nullptr) {
				
				// Clean up
				delete payloadPtr;

				throw std::runtime_error("Response type mismatch");
			}
		}
		// Helper method to process response using ProtoFrameBuffer
		template<typename TRsp>
		TRsp ProcessResponse(const ProtoFrameBuffer<64 * 1024>& frameBuffer, size_t received) {
			
			MessagePtr payloadPtr;
			auto response = OnResponse(frameBuffer, payloadPtr);

			// Check for errors
			if (response->status_code() >= 300 || response->status_code() < 200) {

				std::string errorMsg = response->error_message();
				auto status = response->status_code();

				if (payloadPtr != nullptr) {
					// Find the exception factory for this error type
					auto factoryIt = _exceptionFactories.find(response->response_type());
					if (factoryIt != _exceptionFactories.end()) {
						factoryIt->second(errorMsg, payloadPtr, status);
						// Should never reach here
					}

					delete payloadPtr;
					payloadPtr = nullptr;
				}

				throw FaultException(errorMsg, status);
			}

			// Process successful response
			if (payloadPtr != nullptr) {
				// Try to cast to the expected response type
				TRsp* typedResponse = dynamic_cast<TRsp*>(payloadPtr);
				if (!typedResponse) {
					delete payloadPtr;
					throw std::runtime_error("Response type mismatch");
				}

				// Make a copy of the response before deleting the pointers
				TRsp result = *typedResponse;

				// Clean up
				delete payloadPtr;

				return result;
			}
			else {
				// Empty response (void)
				return TRsp();
			}

		}

	public:
		ProtoReqRspClientHandler(std::unique_ptr<ITransportReqRspClientSocket> socket)
			: _socket(std::move(socket)), _serializer(std::make_shared<MessageSerializer>()) {
			if (!_socket) {
				throw std::invalid_argument("Socket cannot be null");
			}
		}
		template<typename TReq, unsigned int ReqId>
		void RegisterRequest() {
			_serializer->RegisterMessage<TReq, ReqId>();
			
		}
		// Register request-response pair
		template<typename TReq, unsigned int ReqId, typename TRsp, unsigned int RspId>
		void RegisterRequestResponse() {
			_serializer->RegisterMessage<TReq, ReqId>();
			_serializer->RegisterMessage<TRsp, RspId>();
		}

		// Register error type with exception factory
		template<typename TError, unsigned int MessageId>
		void RegisterError() {
			_serializer->RegisterMessage<TError, MessageId>();

			// Register the exception factory that creates and throws the appropriate exception
			_exceptionFactories[MessageId] = [](const std::string& message, MessagePtr errorDetails, unsigned int errorCode) {
				// Create specific error from the generic message
				TError* typedError = dynamic_cast<TError*>(errorDetails);
				if (!typedError) {
					delete errorDetails;
					throw std::runtime_error("Error type mismatch");
				}

				throw TypedFaultException<TError>(MessageId, errorCode, message, typedError);
				};
		}
		template<typename TReq>
		void Send(const TReq& request)
		{
			ProtoFrameBuffer<64 * 1024> outBuf(_serializer);
			size_t received;
			OnSend<TReq>(request, outBuf, received);
			// Process the response
			return ProcessResponse(outBuf, received);
		}

		template <typename TReq>
		void OnSend(const TReq& request, ProtoFrameBuffer<64 * 1024>& outBuf, size_t& received)
		{
			// Ensure connected
			if (!_connected) {
				_socket->Start();
				_connected = true;
			}

			// Create a local frame buffer instance for thread safety
			ProtoFrameBuffer<64 * 1024> inBuf(_serializer);
			outBuf = _serializer;

			// Create command header
			CommandHeader header;
			unsigned int reqId = _serializer->GetMessageId<TReq>();
			header.set_command_type(reqId);

			// Use frame buffer to create the framed message
			inBuf.Write<CommandHeader, TReq>(header, request);
			outBuf.Reset();

			received = _socket->Send(inBuf.Get(), inBuf.Written(), outBuf.Get(), outBuf.FreeBytes());
			outBuf.AckWritten(received);
		}

		// Send request and receive response
		template<typename TReq, typename TRsp>
		TRsp Send(const TReq& request) {
			ProtoFrameBuffer<64 * 1024> outBuf(_serializer);
			size_t received;
			OnSend<TReq>(request, outBuf, received);
			// Process the response
			return ProcessResponse<TRsp>(outBuf, received);
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