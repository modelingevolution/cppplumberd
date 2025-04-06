#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>
#include <nngpp/nngpp.h>
#include <nngpp/protocol/sub0.h>
#include "cppplumberd/transport_interfaces.hpp"

namespace cppplumberd {

    using namespace std;

    // NNG implementation for Subscribe socket using nngpp - with API fixes
    class NngSubscribeSocket : public ITransportSubscribeSocket {
    private:
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
                    if (e.get_error() == nng::error::timedout) {
                        // Only handle timeout - it's expected
                        this_thread::sleep_for(chrono::milliseconds(10));
                    }
                    else if (!_running) {
                        // Exit thread if we're shutting down
                        break;
                    }
                    else {
                        // For any other error, fail fast - rethrow to terminate the thread
                        throw;
                    }
                }
            }
        }

    public:
        NngSubscribeSocket() {
            // Open a subscriber socket - will throw on failure
            _socket = nng::sub::open();
            // Subscribe to everything
            nng::sub::set_opt_subscribe(_socket.get(), "");

            // Set a receive timeout
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

        void Connect(const string& url) override {
            if (_connected) {
                throw runtime_error("Socket already connected");
            }

            // Connect to the URL - will throw on failure
            _socket.dial(url.c_str());
            _connected = true;

            // Start receive thread
            _running = true;
            _recvThread = thread(&NngSubscribeSocket::ReceiveLoop, this);
        }
    };
}