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
		virtual void Send(const uint8_t* buffer, const size_t size) = 0;
	};
	class ITransportSubscribeSocket : public ISocket {
	public:
		using ReceivedSignal = signal<void(uint8_t* buffer, size_t size)>;
		ReceivedSignal Received;

	};
	class ITransportReqRspClientSocket : public ISocket {
	public:
		virtual size_t Send(const uint8_t* inBuf, const size_t inSize, uint8_t* outBuf, const size_t outMaxBufSize) = 0;
	};
	class ITransportReqRspSrvSocket : public ISocket {
	public:
		
		virtual void Initialize(function<size_t(const size_t)> replFunc, uint8_t* inBuf, size_t inMaxBufSize, uint8_t* outBuf, size_t outMaxBufSize) = 0;
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