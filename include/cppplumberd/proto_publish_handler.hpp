#pragma once

#include <string>
#include <memory>
#include <typeindex>
#include <unordered_map>
#include <chrono>
#include <array>
#include "cppplumberd/transport_interfaces.hpp"
#include "cppplumberd/message_serializer.hpp"
#include "proto/cqrs.pb.h"
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

#include "proto_frame_buffer.hpp"

namespace cppplumberd {

    class ProtoPublishHandler {
    public:
        explicit ProtoPublishHandler(std::unique_ptr<ITransportPublishSocket> socket)
            : _socket(std::move(socket)) {
            if (!_socket) {
                throw std::invalid_argument("Socket cannot be null");
            }
            _serializer = make_shared<MessageSerializer>();
        }
        explicit ProtoPublishHandler(std::unique_ptr<ITransportPublishSocket> socket, const std::shared_ptr<MessageSerializer>& serializer)
            : _socket(std::move(socket)), _serializer(serializer) {
            if (!_socket) {
                throw std::invalid_argument("Socket cannot be null");
            }
        }
        inline void Start() {
            _socket->Start();
        }

        template<typename TEvent, unsigned int EventId>
        inline void RegisterMessage() { _serializer->RegisterMessage<TEvent, EventId>(); }

        template<typename TEvent>
        inline void Publish(const TEvent& evt) {

			ProtoFrameBuffer<64 * 1024> frameBuffer(_serializer);
			EventHeader header;
            header.set_timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count());
			header.set_event_type(_serializer->GetMessageId<TEvent>());
			frameBuffer.Write<EventHeader, TEvent>(header, evt);

            _socket->Send(frameBuffer.Get(), frameBuffer.Written());

            
        }

    private:
        std::unique_ptr<ITransportPublishSocket> _socket;
		std::shared_ptr<MessageSerializer> _serializer;
        
    };

} // namespace cppplumberd