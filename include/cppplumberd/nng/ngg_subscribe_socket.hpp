#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <nngpp/nngpp.h>
#include <nngpp/protocol/sub0.h>
#include <nngpp/protocol/req0.h>
#include "cppplumberd/transport_interfaces.hpp"

namespace cppplumberd {

    using namespace std;

    // NNG implementation for Subscribe socket using nngpp - with API fixes
    class NngSubscribeSocket : public ITransportSubscribeSocket {
    private:
        string _url;
        nng::socket _socket;
        bool _connected = false;
        thread _recvThread;
        atomic<bool> _running{ false };

        void ReceiveLoop() {
            while (_running) {
                try {
                    nng::buffer buf = _socket.recv();
                    string data(static_cast<const char*>(buf.data()), buf.size());
                    Received(data);
                }
                catch (const nng::exception& e) {
                    if (!_running) {
                        break;
                    }
                    if (e.get_error() == nng::error::timedout) {
                        continue;
                    }
                	throw;
                }
            }
        }

    public:
        NngSubscribeSocket(const string &url) : _url(url) {
            
            _socket = nng::sub::open();
            //_socket = nng::req::open();
            // Subscribe to everything
            //nng::sub::set_opt_subscribe(_socket, "");
            nng_setopt(_socket.get(), NNG_OPT_SUB_SUBSCRIBE, "", 0);
            _socket.set_opt_ms(nng::to_name(nng::option::recv_timeout), 100);
        }

        ~NngSubscribeSocket() override {
            if (_running) {
                _running = false;
                if (_recvThread.joinable()) {
                    _recvThread.join();
                }
            }
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
            

            cout << "connected to: " << url << endl;

            _connected = true;

            // Start receive thread
            _running = true;
            _recvThread = thread(&NngSubscribeSocket::ReceiveLoop, this);
        }
    };
}