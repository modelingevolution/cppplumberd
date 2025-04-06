#pragma once

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

	class ISocket
	{
	public:
		virtual void Start(const string& url) = 0;
		virtual void Start() = 0;
		virtual ~ISocket() = default;
	};
	class ITransportPublishSocket : public ISocket {
	public:
		virtual void Send(const string& data) = 0;
	};
	class ITransportSubscribeSocket : public ISocket {
	public:
		using ReceivedSignal = signal<void(const string&)>;
		ReceivedSignal Received;

	};
	class ITransportReqRspClientSocket : public ISocket {
	public:
		virtual string Send(const string& data) = 0;
	};
	class ITransportReqRspSrvSocket : public ISocket {
	public:
		virtual void Initialize(function<string(const string&)>) = 0;
	};
	class ISocketFactory {
	public:
		virtual unique_ptr<ITransportPublishSocket> CreatePublishSocket(const string& endpoint) = 0;
		virtual unique_ptr<ITransportSubscribeSocket> CreateSubscribeSocket(const string& endpoint) = 0;
		virtual unique_ptr<ITransportReqRspClientSocket> CreateReqRspClientSocket(const string& endpoint) = 0;
		virtual unique_ptr<ITransportReqRspSrvSocket> CreateReqRspSrvSocket(const string& endpoint) = 0;
		virtual ~ISocketFactory() = default;
	};
}