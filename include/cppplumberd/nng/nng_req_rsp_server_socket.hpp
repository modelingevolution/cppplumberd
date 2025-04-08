#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <iostream>
#include <nngpp/nngpp.h>
#include <nngpp/protocol/rep0.h>
#include <nngpp/socket_view.h>
#include "cppplumberd/transport_interfaces.hpp"

namespace cppplumberd {

    using namespace std;

    // NNG implementation for Request-Reply server socket using nngpp - with correct API usage
    class NngReqRspSrvSocket : public ITransportReqRspSrvSocket {
    private:
        string _url;
        nng::socket _socket;
        bool _bound = false;
        thread _recvThread;
        atomic<bool> _running{ false };
        function<size_t(const size_t)> _handler;
        uint8_t* _inBuffer;
        size_t _inBufferSize;
        uint8_t* _outBuffer;
        size_t _outBufferSize;
       

        void ReceiveLoop() {
            while (_running) {
                try {
	                nng::view out(_inBuffer, _inBufferSize);
                    auto reqSize = _socket.recv(out);

                    auto rspSize = _handler(reqSize);
					
					nng::view view(_outBuffer, rspSize);
					_socket.send(view);
                }
                catch (const nng::exception& e) {
                    if (e.get_error() == nng::error::timedout) {
                        continue;
                    }
                    if (!_running) {
	                    break;
                    }
                    // For any other error, fail fast - rethrow to terminate the thread
                    throw;
                }
            }
        }

    public:
        NngReqRspSrvSocket(const string &url) : _url(url) {
            // Open a reply socket - will throw on failure 
            _socket = nng::rep::open();
            _socket.set_opt_ms(nng::to_name(nng::option::recv_timeout), 1000);
        }

        ~NngReqRspSrvSocket() override {
            if (_running) {
                _running = false;
                if (_recvThread.joinable()) {
                    _recvThread.join();
                }
            }
        }

        

        void Initialize(function<size_t(const size_t)> handler, uint8_t* inBuf, size_t inMaxBufSize, uint8_t* outBuf, size_t outMaxBufSize)
    {
            _handler = handler;
            _inBuffer = inBuf;
            _inBufferSize = inMaxBufSize;
            _outBuffer = outBuf;
            _outBufferSize = outMaxBufSize;
    }
        
        void Start() override
        {
            Start(_url);
        }
        void Start(const string& url) override {
            if (_bound) {
                throw runtime_error("Socket already bound");
            }

            // Check if handler is initialized
            if (!_handler) {
                throw runtime_error("Handler not initialized");
            }
            if (_url != url)
                _url = url;
            // Bind the socket - will throw on failure
            _socket.listen(url.c_str());
            
            _bound = true;

            // Start receive thread
            _running = true;
            _recvThread = thread(&NngReqRspSrvSocket::ReceiveLoop, this);
            cout << "listening at: " << url << ". receive loop created." << endl;
        }
    };
}