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



	

	class IEventDispatcher
	{
	public:
		virtual void Handle(const Metadata&, unsigned int messageId, MessagePtr msg) = 0;
		virtual ~IEventDispatcher() = default;
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


	/* Can be used on Client side or Server side */
	class ISubscriptionManager
	{
	public:
		class ISubscription
		{
		public:
			virtual void Unsubscribe() = 0;
			virtual ~ISubscription() = default;
		};

		template<typename TEvent>
		class IEventPublisher
		{
		public:
			virtual void Publish(const Metadata& m, const TEvent& evt) = 0;
			virtual ~IEventPublisher() = default;
		};

		virtual unique_ptr<ISubscription> Subscribe(const string& streamName, const IEventDispatcher& handler) = 0;

		// For client this would be low level adapters for sub-handler-> socket-subscribers.
		// For server this would be used by IEventStore to push events to low-level publish-handler->publish-socket
		template<typename TEvent>
		vector<shared_ptr<IEventPublisher<TEvent>>> GetPublishers() { return {}; }

		virtual ~ISubscriptionManager() = default;
	};

	// Server-side
	class EventStore
	{
	public:
		template<typename TMessage, unsigned int MessageId>
		void RegisterMessage() {} // event

		template<typename TEvent> // pushes events to local ISubscriptionManager 
		void Publish(const string& streamName, const TEvent& evt) {}
	};

	

	

	class HandlerFactory {
	public:
		HandlerFactory(shared_ptr<ISocketFactory> socketFactory) : _socketFactory(socketFactory) {}

		unique_ptr<ProtoPublishHandler> CreatePublishHandler(const string& endpoint) { return nullptr; }
		unique_ptr<ProtoSubscribeHandler> CreateSubscribeHandler(const string& endpoint) { return nullptr; }
		unique_ptr<ProtoReqRspClientHandler> CreateReqRspClientHandler(const string& endpoint) { return nullptr; }
		unique_ptr<ProtoReqRspSrvHandler> CreateReqRspSrvHandler(const string& endpoint) { return nullptr; }
	private:
		shared_ptr<ISocketFactory> _socketFactory;
	};

	

    class PlumberClient {
    protected:
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
    class Plumber : public PlumberClient {
    private:
        shared_ptr<CommandServiceHandler> _commandServiceHandler;
        

    public:
        static unique_ptr<Plumber> CreateServer(shared_ptr<ISocketFactory> factory, const string& endpoint) {
            return make_unique<Plumber>(factory, endpoint);
        }

        Plumber(shared_ptr<ISocketFactory> factory, const string& endpoint = "")
            : PlumberClient(factory, endpoint) {

            // Create the command service handler
            auto srvHandler = make_unique<ProtoReqRspSrvHandler>(
                _socketFactory->CreateReqRspSrvSocket("commands"));
            _commandServiceHandler = make_shared<CommandServiceHandler>(move(srvHandler));
        }

        // Override to provide server-specific start behavior
        void Start() override {
            if (_isStarted) return;

            _commandServiceHandler->Start();
            _isStarted = true;
        }

        // Override to provide server-specific stop behavior
        void Stop() override {
            if (!_isStarted) return;

            _commandServiceHandler->Stop();
            _isStarted = false;
        }

        // Override CommandBus to throw an exception since in-proc communication is not yet implemented
        shared_ptr<cppplumberd::CommandBus> CommandBus() override {
            throw runtime_error("In-process CommandBus not implemented for server-side. Use CommandServiceHandler directly.");
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