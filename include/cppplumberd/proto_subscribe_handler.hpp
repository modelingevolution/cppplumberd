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

    // Forward declaration
    class Metadata;

    class ProtoSubscribeHandler {
    public:
        explicit ProtoSubscribeHandler(std::unique_ptr<ITransportSubscribeSocket> socket)
            : _socket(std::move(socket)), _running(false) {

            if (!_socket) {
                throw std::invalid_argument("Socket cannot be null");
            }

            // Connect to the socket's Received signal
            _socket->Received.connect([this](const std::string& message) {
                this->OnMessageReceived(message);
                });
        }

        ~ProtoSubscribeHandler() {
            Stop();
        }

        template<typename TEvent, unsigned int EventId>
            requires HasParseFromString<TEvent>
        void RegisterHandler(std::function<void(const time_point<system_clock>&, const TEvent&)> handler) {
            // Register the event type with the serializer
            _serializer.RegisterMessage<TEvent, EventId>();

            // Register the handler
            _eventHandlers[EventId] = [handler](const time_point<system_clock>& timestamp, const MessagePtr msg) {
                const TEvent* typedEvent = dynamic_cast<const TEvent*>(msg);
                if (!typedEvent) {
                    throw std::runtime_error("Event type mismatch in handler");
                }
                handler(timestamp, *typedEvent);
                };
        }

        void Start() {
            _running = true;
            _socket->Start();
        }

        void Stop() {
            _running = false;
        }

    private:
        std::unique_ptr<ITransportSubscribeSocket> _socket;
        MessageSerializer _serializer;
        std::unordered_map<unsigned int,
            std::function<void(const time_point<system_clock>&, const MessagePtr)>> _eventHandlers;
        bool _running;
        MessageDispatcher<void, Metadata> _msgDispatcher;

        void OnMessageReceived(const std::string& message) {
            if (!_running) return;

            try {
                // Message must be at least 8 bytes (for header size and payload size)
                if (message.size() < 8) {
                    throw std::runtime_error("Message too short: " + std::to_string(message.size()) + " bytes");
                }

                // Get pointer to the first 8 bytes containing sizes
                const uint32_t* sizePtr = reinterpret_cast<const uint32_t*>(message.data());

                // Extract header and payload sizes
                uint32_t headerSize = sizePtr[0];
                uint32_t payloadSize = sizePtr[1];

                // Validate sizes
                size_t totalExpectedSize = 8 + headerSize + payloadSize;
                if (message.size() < totalExpectedSize) {
                    throw std::runtime_error("Message truncated: expected " +
                        std::to_string(totalExpectedSize) +
                        " bytes, got " +
                        std::to_string(message.size()));
                }

                // Extract header bytes
                const char* headerBytes = message.data() + 8;

                // Parse header
                EventHeader header;
                if (!header.ParseFromArray(headerBytes, headerSize)) {
                    throw std::runtime_error("Failed to parse event header");
                }

                // Extract payload bytes
                const char* payloadBytes = headerBytes + headerSize;

                // Find the handler for this event type
                auto handlerIt = _eventHandlers.find(header.event_type());
                if (handlerIt == _eventHandlers.end()) {
                    // No handler registered for this event type - silently ignore
                    return;
                }

                // Deserialize the payload
                MessagePtr eventMsg = _serializer.Deserialize(
                    std::string(payloadBytes, payloadSize),
                    header.event_type()
                );

                // Create timestamp from header
                time_point<system_clock> timestamp = system_clock::time_point(
                    milliseconds(header.timestamp()));

                // Call the handler
                handlerIt->second(timestamp, eventMsg);

                // Clean up
                delete eventMsg;
            }
            catch (const std::exception& ex) {
                // Log error but continue processing other messages
                std::cerr << "Error processing message: " << ex.what() << std::endl;
            }
        }
    };

} // namespace cppplumberd