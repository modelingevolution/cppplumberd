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
using namespace std;
using namespace std::chrono;

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
		virtual void Connect(const string& url) = 0;
		virtual void Receive(function<void(const string&)>) = 0;
		virtual ~ITransportSubscribeSocket() = 0;
	};
	class ITransportReqRspClientSocket {
	public:
		virtual string Send(const string& data) = 0;
		virtual void Connect(const string& url) = 0;
		virtual ~ITransportReqRspClientSocket() = 0;
	};
	class ITransportReqRspSrvSocket {
	public:
		/* returns a funciton that shall be used to return data back to client. */
		virtual function<void(const string&)> Handle(const string& data) = 0;
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

	template <typename TRsp>
	class MessageDispatcher {
	protected:
		template<typename TMessage, unsigned int MessageId>
		void RegisterHandler(function<TRsp(const TMessage&)> handler);

		template<typename TMessage>
		void Handle(const TMessage& msg);

		template<typename TMessage, unsigned int MessageId>
		void RegisterMessage();

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
		MessageDispatcher<void> _msgDispatcher;
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
		virtual void HandleCommand(const TCommand& cmd) = 0;
		virtual ~ICommandHandler() = default;
	};

	/*
	* Error handling, and exception-to-response translation.Server-Side
	*/
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


	/*
	* Client-Side, translates Command-Response to Exception and throws if needed.
	*/
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
		virtual ~ISocketFactory();
	};
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
}


#endif // PLUMBERD_HPP