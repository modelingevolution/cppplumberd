#pragma once

#include <string>
#include <memory>
#include <nngpp/nngpp.h>
#include <nngpp/protocol/req0.h>
#include <nngpp/socket_view.h>
#include "cppplumberd/transport_interfaces.hpp"

namespace cppplumberd {

    using namespace std;

    // NNG implementation for Request-Reply client socket using nngpp - with correct API usage
    class NngReqRspClientSocket : public ITransportReqRspClientSocket {
    private:
		string _url;
        nng::socket _socket;
        bool _connected = false;

    public:
        NngReqRspClientSocket(const string &url) : _url(url) {
            // Open a request socket - will throw on failure
            _socket = nng::req::open();
        }
		void Start() override
		{
            Start(_url);
		}
        void Start(const string& url) override {
            if (_connected) {
                throw runtime_error("Socket already connected");
            }
			if (_url != url)
				_url = url;

            _socket.dial(url.c_str());
            _connected = true;
            cout << "connected to: " << url << endl;
        }
        size_t Send(const uint8_t* inBuf, const size_t inSize, uint8_t* outBuf, const size_t outMaxBufSize) override {
            if (!_connected) {
                throw runtime_error("Socket not connected");
            }

            nng::view view(inBuf, inSize);
            _socket.send(view);

            nng::view outView(outBuf, outMaxBufSize);
            auto result= _socket.recv(outView);

            return result;
        }

    };
}