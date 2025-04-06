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
        function<string(const string&)> _handler;

       

        void ReceiveLoop() {
            while (_running) {
                try {
                    nng::buffer buffer = _socket.recv();

                    // Convert buffer to string
                    string request(static_cast<const char*>(buffer.data()), buffer.size());
                    
                    auto result = _handler(request);
					// Send the response back
					nng::view view(result.data(), result.size());
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

        void ITransportReqRspSrvSocket::Initialize(function<string(const string&)> handler) override {
            _handler = handler;
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