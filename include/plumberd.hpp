/**
 * @file plumberd.hpp
 * @author ModelingEvolution
 * @brief Main include file for the cppplumberd library
 * @version 0.1.0
 * @date 2025-04-05
 *
 * @copyright Copyright (c) 2025 ModelingEvolution
 * MIT License
 */

#ifndef PLUMBERD_HPP
#define PLUMBERD_HPP

 // Include all components
#include "cppplumberd/transport_interfaces.hpp"
#include "cppplumberd/utils.hpp"
#include "cppplumberd/message_serializer.hpp"
#include "cppplumberd/message_dispatcher.hpp"
#include "cppplumberd/nng/ngg_socket_factory.hpp"
#include "cppplumberd/stop_watch.hpp"
#include "cppplumberd/fault_exception.hpp"
#include "cppplumberd/proto_publish_handler.hpp"
#include "cppplumberd/proto_subscribe_handler.hpp"
#include "cppplumberd/proto_req_rsp_srv_handler.hpp"
#include "cppplumberd/proto_req_rsp_client_handler.hpp"
#include "cppplumberd/command_bus.hpp"
#include "cppplumberd/command_service_handler.hpp"
#include "cppplumberd/event_store.hpp"
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <typeindex>
#include <vector>
#include <map>
#include <set>
#include <chrono>
#include <boost/signals2.hpp>
using namespace std;
using namespace std::chrono;
using namespace boost::signals2;
namespace cppplumberd {

	struct Version {
		static constexpr int major = 0;
		static constexpr int minor = 1;
		static constexpr int patch = 0;

		static constexpr const char* toString() {
			return "0.1.0";
		}
	};



	class EventHandlerBase : public IEventDispatcher {
	public:
		// we expect here that "this" implements IEventHandler<TEvent>
		template<typename TEvent, unsigned int EventType>
		void Map() {}

		void Handle(const Metadata& metadata, unsigned int messageId, MessagePtr msg) override {}
	};


	
	template<typename T>
	concept TException = requires(T t) {
		{ t.ErrorCode() } -> same_as<unsigned short>;
	};


	

    class PlumberClient {
    private:
        class SubscriptionManager : public ISubscriptionManager {
	        
            class Subscription : public ISubscription
            {
            private:
                string _streamName;
                shared_ptr<ClientProtoSubscriptionStream> _stream;
                SubscriptionManager* _parent;
            public:
                Subscription(SubscriptionManager* parent, const string& streamName, const shared_ptr<ClientProtoSubscriptionStream>& stream)
                    : _streamName(streamName), _stream(stream), _parent(parent) {
                }
                void Unsubscribe() override {
					_stream->Stop();
                    _parent->Unsubscribe(this);
                }
            };
            void Unsubscribe(Subscription* subscription)
            {
                auto it = ranges::find(_subscriptions, subscription);
                if (it != _subscriptions.end()) {
                    _subscriptions.erase(it);
                }
            }
        public:
            unique_ptr<ISubscription> Subscribe(const string& streamName, const shared_ptr<IEventDispatcher>& handler)
            {
                CreateStream cmd;
                cmd.set_name(streamName);
                _parent->_commandBus->Send("$", cmd);
                auto sock = _parent->_socketFactory->CreateSubscribeSocket(streamName);
                
				auto stream = make_shared<ClientProtoSubscriptionStream>(std::move(sock),handler,streamName);
				auto sub = make_unique<Subscription>(this, streamName, stream);
                stream->Start();
				_subscriptions.push_back(sub.get());
                return sub;
            }
            
        private:
			vector< Subscription*> _subscriptions;
            PlumberClient* _parent;
            SubscriptionManager(PlumberClient* ptr) : _parent(ptr) {  }
        };
   
        
        shared_ptr<ISocketFactory> _socketFactory;
        string _endpoint;
        shared_ptr<CommandBus> _commandBus;
        shared_ptr<ISubscriptionManager> _subscriptionManager;
        bool _isStarted = false;

    public:
        static unique_ptr<PlumberClient> CreateClient(shared_ptr<ISocketFactory> factory, const string& endpoint) {
            return make_unique<PlumberClient>(factory, endpoint);
        }

        PlumberClient(const shared_ptr<ISocketFactory>& factory, const string& endpoint = "")
            : _socketFactory(factory), _endpoint(endpoint) {

            // Create command bus
            auto clientHandler = make_unique<ProtoReqRspClientHandler>(
                _socketFactory->CreateReqRspClientSocket("commands"));
            _commandBus = make_shared<cppplumberd::CommandBus>(std::move(clientHandler));

        }

        virtual void Start() {
            if (_isStarted) return;

            _commandBus->Start();

            _isStarted = true;
        }

        virtual void Stop() {
            _isStarted = false;
        }

        virtual shared_ptr<CommandBus> CommandBus() {
            return _commandBus;
        }

        virtual shared_ptr<ISubscriptionManager> SubscriptionManager() {
            return _subscriptionManager;
        }

        virtual ~PlumberClient() = default;
    };

    // Server-side implementation
    class Plumber  {
    private:
        shared_ptr<CommandServiceHandler> _commandServiceHandler;
		shared_ptr<EventStore> _eventStore;
        shared_ptr<ISocketFactory> _socketFactory;
		shared_ptr<MessageSerializer> _serializer;
        string _endpoint;
        bool _isStarted;
    public:
        static unique_ptr<Plumber> CreateServer(shared_ptr<ISocketFactory> factory, const string& endpoint) {
            return make_unique<Plumber>(factory, endpoint);
        }

        Plumber(shared_ptr<ISocketFactory> factory, const string& cmdEndpoint = "commands")
    	{
			_serializer = make_shared<MessageSerializer>();
            
            auto srvHandler = make_unique<ProtoReqRspSrvHandler>(_socketFactory->CreateReqRspSrvSocket(cmdEndpoint), _serializer);
            _commandServiceHandler = make_shared<CommandServiceHandler>(move(srvHandler));

			_eventStore = make_shared<cppplumberd::EventStore>(factory, _serializer);
        }

        void Start() {
            if (_isStarted) return;

            _commandServiceHandler->Start();
            _isStarted = true;
        }

        
        void Stop() {
            if (!_isStarted) return;

            _commandServiceHandler->Stop();
            _isStarted = false;
        }
        shared_ptr<cppplumberd::EventStore> EventStore()
        {
			return _eventStore;
        }
       

        shared_ptr<CommandServiceHandler> GetCommandServiceHandler() {
            return _commandServiceHandler;
        }

        // Command handler registration
        template<typename TCommandHandler, typename TCommand, unsigned int MessageId>
        void AddCommandHandler() {
            auto handler = make_shared<TCommandHandler>();
            
            auto typedHandler = static_pointer_cast<ICommandHandler<TCommand>>(handler);
            _commandServiceHandler->RegisterHandler<TCommand, MessageId>(typedHandler);
        }
        template<typename TCommandHandler, typename TCommand, unsigned int MessageId, typename... Args>
        void AddCommandHandler(Args&&... args) {
            auto handler = make_shared<TCommandHandler>(std::forward<Args>(args)...);

            auto typedHandler = static_pointer_cast<ICommandHandler<TCommand>>(handler);
            _commandServiceHandler->RegisterHandler<TCommand, MessageId>(typedHandler);
        }
        template<typename TCommand, unsigned int MessageId>
        void AddCommandHandler(shared_ptr<ICommandHandler<TCommand>> handler) {
            
            _commandServiceHandler->RegisterHandler<TCommand, MessageId>(handler);
        }

        // Event handler registration
        template<typename TEventHandler, typename TEvent, unsigned int MessageId>
        void AddEventHandler() {
            auto handler = make_shared<TEventHandler>();
            
        }
    };
}

#endif // PLUMBERD_HPP