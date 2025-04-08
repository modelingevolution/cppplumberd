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

	

	class PlumberClient
	{
	protected:
		shared_ptr<ISocketFactory> _socketFactory;
	public:

		static unique_ptr<PlumberClient> CreateClient(shared_ptr<ISocketFactory> factory, const string& endpoint)
		{
			return make_unique<PlumberClient>(factory);
		}

		PlumberClient(const shared_ptr<ISocketFactory>& factory) {
			_socketFactory = factory;
		}

		virtual void Start() {}
		virtual void Stop() {}

		virtual shared_ptr<CommandBus> CommandBus()
		{
			return nullptr;
		}

		virtual shared_ptr<ISubscriptionManager> SubscriptionManager()
		{
			return nullptr;
		}

		virtual ~PlumberClient() = default;
	};

	class Plumber : public PlumberClient
	{
	public:
		static unique_ptr<Plumber> CreateServer(shared_ptr<ISocketFactory> factory, const string& endpoint)
		{
			return make_unique<Plumber>(factory);
		}

		Plumber(shared_ptr<ISocketFactory> factory) : PlumberClient(factory) {}

		template<typename TCommandHandler, typename TCommand, unsigned int MessageId>
		void AddCommandHandler() {}

		template<typename TEventHandler, typename TEvent, unsigned int MessageId>
		void AddEventHandler() {}

		template<typename TCommand, unsigned int MessageId>
		void AddCommandHandler(shared_ptr<ICommandHandler<TCommand>> cmd) {}
	};
}

#endif // PLUMBERD_HPP