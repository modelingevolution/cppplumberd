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
#include "cppplumberd/utils.hpp"
#include "cppplumberd/calculator.hpp"
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <functional>
#include <typeindex>
#include <vector>
#include <map>
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


	class ITransportPublishSocket {
	public:
		virtual void Bind(const string& url) = 0;
		virtual void Send(const string& data) = 0;
		virtual ~ITransportPublishSocket() = 0;
	};
	class ITransportSubscribeSocket {
	public:
		using ReceivedSignal = signal<void(const string&)>;
		ReceivedSignal Received;

		virtual void Connect(const string& url) = 0;
		
		virtual ~ITransportSubscribeSocket() = 0;
	};
	class ITransportReqRspClientSocket {
	public:
		virtual string Send(const string& data) = 0;
		virtual void Connect(const string& url) = 0;
		virtual ~ITransportReqRspClientSocket() = 0;
	};
	class ITransportReqRspSrvSocket {
		class IResponse
		{
			virtual void Return(const string&);
			virtual ~IResponse() = 0;
		};
	public:
		virtual void Initialize(function<IResponse(const string&)>) = 0;
		virtual void Bind(const string& url) = 0;
		virtual ~ITransportReqRspSrvSocket() = 0;
	};

	class MessageSerializer {
	public:
		template<typename TMessage, unsigned int MessageId>
		void RegisterMessage();

		template<typename TMessage>
		TMessage Deserialize(const string& data) const;

		template<typename TMessage>
		string Serialize(const TMessage& msg) const;
	};

	typedef void* MessagePtr;

	

	template <typename TRsp, typename TMeta>
	class MessageDispatcher {
	protected:
		template<typename TMessage, unsigned int MessageId>
		void RegisterHandler(function<TRsp(const TMeta&, const TMessage&)> handler);

		void Handle(const TMeta&, unsigned int messageId, MessagePtr msg);

		template<typename TMessage>
		void Handle(const TMeta &, const TMessage& msg);

		template<typename TMessage, unsigned int MessageId>
		void RegisterMessage();

	};
	class Metadata
	{

	};
	template<typename TEvent>
	class IEventHandler {
	public:
		virtual void Handle(const Metadata& metadata, const TEvent& evt) = 0;
		virtual ~IEventHandler() = default;
	};
	class IEventDispatcher
	{
		virtual void Handle(const Metadata&, unsigned int messageId, MessagePtr msg) = 0;
	};

	class EventHandlerBase : public IEventDispatcher {
	public:
		// we expect here that "this" implements IEventHandler<TEvent>
		template<typename TEvent, unsigned int EventType>
		void Map()
		{
			
		}

		inline void IEventDispatcher::Handle(const Metadata& metadata, unsigned int messageId, MessagePtr msg) override
		{
			
		}
	

	};

	
	/*
	* Used by server to publish events.
	*/
	class ProtoPublishHandler {
	public:
		ProtoPublishHandler(unique_ptr<ITransportPublishSocket> socket);

		template<typename TEvent>
		void Publish(const TEvent& evt);
	private:
		unique_ptr<ITransportPublishSocket> _socket;
		MessageSerializer _serializer;
	};
	/*
	* Used by client to subscribe to events published.
	*/
	class ProtoSubscribeHandler {
	public:
		ProtoSubscribeHandler(unique_ptr<ITransportSubscribeSocket> socket);


		template<typename TEvent, unsigned int EventId>
		void RegisterHandler(function<void(const time_point<system_clock>&, const TEvent&)> handler);

		void Start();
		void Stop();
	private:
		unique_ptr<ITransportSubscribeSocket> _socket;
		MessageDispatcher<void, Metadata> _msgDispatcher;
	};
	/*
	* Used by client to send requests to server and receive responses.
	*/
	class ProtoReqRspClientHandler {
	public:
		ProtoReqRspClientHandler(unique_ptr<ITransportReqRspClientSocket>);

		template<typename TReq, unsigned int ReqId, typename TRsp, unsigned int RspId>
		void RegisterRequestResponse();

		template<typename TReq, typename TRsp>
		TRsp Send(const TReq& evt);
	private:
		MessageSerializer _serializer;
		unique_ptr<ITransportReqRspClientSocket> _socket;
	};
	/*
	* Used by server to handle requests from clients and send responses.
	*/
	class ProtoReqRspSrvHandler {
	public:
		ProtoReqRspSrvHandler(unique_ptr<ITransportReqRspSrvSocket>);
		template<typename TReq, unsigned int ReqId, typename TRsp, unsigned int RspId>
		void RegisterHandler(const TReq& evt, function<TRsp(const TReq&)> f);

		void Start();
		void Stop();
	private:
		unique_ptr<ITransportReqRspSrvSocket> _socket;
	};

	template<typename T>
	concept TException = requires(T t) {
		{ t.ErrorCode() } -> same_as<unsigned short>;
	};
	

	template<typename TCommand>
	class ICommandHandler {
	public:
		virtual void Handle(const string& stream_id, const TCommand& cmd) = 0;
		virtual ~ICommandHandler() = default;
	};
	
	
	class CommandServiceHandler {
	public:
		template<typename TCommand>
		void RegisterHandler(const shared_ptr<ICommandHandler<TCommand>>& handler);

		template<TException T, unsigned int MessageId>
		void RegisterError();
		void Start();
		void Stop();
	private:
		unique_ptr<ProtoReqRspSrvHandler> _handler;
	};
	/* Can be used on Client side or Server side */
	class ISubscriptionManager
	{
	public:
		class ISubscription
		{
			virtual void Unsubscribe() = 0;
			virtual ~ISubscription() = 0;
		};
		template<typename TEvent>
		class IEventPublisher
		{
			virtual void Publish(const Metadata& m, const TEvent&);
		};

		unique_ptr<ISubscription> Subscribe(const string& streamName,const IEventDispatcher &handler);

		// For client this would be low level adapters for sub-handler-> socket-subscribers.
		// For server this would be used by IEventStore to push events to low-level publish-handler->publish-socket
		template<typename TEvent>
		vector<shared_ptr<IEventPublisher<TEvent>>> GetPublishers(); // 
	};

	// Server-side
	class EventStore
	{
	public:
		template<typename TMessage, unsigned int MessageId>
		void RegisterMessage(); // event

		template<typename TEVent> // pushes events to local ISubscriptionManager 
		void Publish(const string &streamName, const TEVent& evt);
	private:

	};
	
	class CommandBus {
	public:
		CommandBus(shared_ptr<CommandServiceHandler> handler);

		template<typename TCommand>
		void Send(const TCommand& cmd);

		template<typename TMessage, unsigned int MessageId>
		void RegisterMessage(); // error or command.
	private:
		unique_ptr<ProtoReqRspClientHandler> _handler;
	};
	
	class ISocketFactory {
	public:
		virtual unique_ptr<ITransportPublishSocket> CreatePublishSocket(const string& endpoint) = 0;
		virtual unique_ptr<ITransportSubscribeSocket> CreateSubscribeSocket(const string& endpoint) = 0;
		virtual unique_ptr<ITransportReqRspClientSocket> CreateReqRspClientSocket(const string& endpoint) = 0;
		virtual unique_ptr<ITransportReqRspSrvSocket> CreateReqRspSrvSocket(const string& endpoint) = 0;
		virtual ~ISocketFactory() = 0;
	};
	inline ISocketFactory::~ISocketFactory() {}

	class HandlerFactory {
	public:
		HandlerFactory(shared_ptr<ISocketFactory> socketFactory);
		unique_ptr<ProtoPublishHandler> CreatePublishHandler(const string& endpoint);
		unique_ptr<ProtoSubscribeHandler> CreateSubscribeHandler(const string& endpoint);
		unique_ptr<ProtoReqRspClientHandler> CreateReqRspClientHandler(const string& endpoint);
		unique_ptr<ProtoReqRspSrvHandler> CreateReqRspSrvHandler(const string& endpoint);
	private:
		shared_ptr<ISocketFactory> _socketFactory;
	};

	class NngSocketFactory : public ISocketFactory
	{
	public:
		inline unique_ptr<ITransportPublishSocket> ISocketFactory::CreatePublishSocket(const string& endpoint) override { return nullptr; }
		inline unique_ptr<ITransportSubscribeSocket> ISocketFactory::CreateSubscribeSocket(const string& endpoint) override { return nullptr; }
		inline unique_ptr<ITransportReqRspClientSocket> ISocketFactory::CreateReqRspClientSocket(const string& endpoint) override { return nullptr; }
		inline unique_ptr<ITransportReqRspSrvSocket> ISocketFactory::CreateReqRspSrvSocket(const string& endpoint) override { return nullptr; }
		
		inline ~NngSocketFactory() override = default;
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
		
		virtual void Start();
		virtual void Stop();
		virtual shared_ptr<CommandBus> CommandBus()
		{
			return nullptr;
		}
		virtual shared_ptr<ISubscriptionManager> SubscriptionManager()
		{

		}
	};
	class Plumber : public PlumberClient
	{
		
	public:
		static unique_ptr<Plumber> CreateServer(shared_ptr<ISocketFactory> factory, const string& endpoint)
		{
			return make_unique<Plumber>(factory);
		}
		
		Plumber(shared_ptr<ISocketFactory> factory) : PlumberClient(factory) {
			
		}
		template<typename TCommandHandler, typename TCommand, unsigned int MessageId>
		void AddCommandHandler()
		{
			
		}
		template<typename TEventHandler, typename TEvent, unsigned int MessageId>
		void AddEventHandler()
		{

		}
		template<typename TCommand, unsigned int MessageId>
		void AddCommandHandler(shared_ptr<ICommandHandler<TCommand>> cmd);

		
	};
	

}


#endif // PLUMBERD_HPP