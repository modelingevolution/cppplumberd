#pragma once

#include <string>
#include <memory>
#include <functional>
#include <typeindex>
#include <unordered_map>
#include <chrono>

#include "proto_frame_buffer.hpp"
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
            _socket->Received.connect([this](uint8_t* buffer, size_t size) {
                this->OnMessageReceived(buffer, size);
                });
        }

        ~ProtoSubscribeHandler() {
            Stop();
        }

        template<typename TEvent, unsigned int EventId>
            requires HasParseFromString<TEvent>
        void RegisterHandler(std::function<void(const time_point<system_clock>&, const TEvent&)> handler) {
            
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
		shared_ptr<MessageSerializer> _serializer = std::make_shared<MessageSerializer>();
        std::unordered_map<unsigned int,
            std::function<void(const time_point<system_clock>&, const MessagePtr)>> _eventHandlers;
        bool _running;
        MessageDispatcher<void, Metadata> _msgDispatcher;

        void OnMessageReceived(uint8_t* buffer, size_t size) {
            if (!_running) return;

            try {
                ProtoFrameBufferView v(_serializer, buffer, size);
                v.AckWritten(size);
                auto responseTypeSelector = [](const EventHeader& header) -> unsigned int { return header.event_type(); };
				MessagePtr payloadBytes;
                auto header = v.Read<EventHeader>(responseTypeSelector, payloadBytes);
                
                auto handlerIt = _eventHandlers.find(header->event_type());
                if (handlerIt == _eventHandlers.end()) {
                    return;
                }

                time_point<system_clock> timestamp = system_clock::time_point(
                    milliseconds(header->timestamp()));

                handlerIt->second(timestamp, payloadBytes);

                
                delete payloadBytes;
            }
            catch (const std::exception& ex) {
                // Log error but continue processing other messages
                std::cerr << "Error processing message: " << ex.what() << std::endl;
            }
        }
    };

} // namespace cppplumberd