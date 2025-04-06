#pragma once

#include <string>
#include <memory>
#include <stdexcept>
#include <thread>
#include <nng/nng.h>
#include <nng/protocol/pubsub0/pub.h>
#include <nng/protocol/pubsub0/sub.h>
#include <nng/protocol/reqrep0/req.h>
#include <nng/protocol/reqrep0/rep.h>
#include "cppplumberd/transport_interfaces.hpp"

namespace cppplumberd {

    using namespace std;


    // NNG implementation for Subscribe socket
    class NngSubscribeSocket : public ITransportSubscribeSocket {
    private:
        nng_socket _socket;
        bool _connected;
        thread _recvThread;
        atomic<bool> _running;

        void ReceiveLoop() {
            while (_running) {
                void* buf = nullptr;
                size_t size = 0;

                int rv = nng_recv(_socket, &buf, &size, NNG_FLAG_ALLOC);
                if (rv == 0) {
                    // Successfully received message, convert to string and notify
                    string data(static_cast<char*>(buf), size);
                    nng_free(buf, size);

                    // Emit signal to notify subscribers
                    Received(data);
                }
                else if (rv == NNG_ETIMEDOUT) {
                    // Timeout is normal, just continue
                    continue;
                }
                else if (!_running) {
                    // Socket closed due to shutdown
                    break;
                }
                else {
                    // Some error occurred
                    cerr << "Error receiving NNG message: " << nng_strerror(rv) << endl;
                }
            }
        }

    public:
        NngSubscribeSocket() : _connected(false), _running(false) {
            int rv = nng_sub0_open(&_socket);
            if (rv != 0) {
                throw runtime_error("Failed to open NNG sub socket: " + string(nng_strerror(rv)));
            }

            // Subscribe to everything by default
            rv = nng_setopt(_socket, NNG_OPT_SUB_SUBSCRIBE, "", 0);
            if (rv != 0) {
                nng_close(_socket);
                throw runtime_error("Failed to set NNG sub option: " + string(nng_strerror(rv)));
            }

            // Set a receive timeout so we can check _running periodically
            int timeout = 100; // 100ms
            rv = nng_setopt_ms(_socket, NNG_OPT_RECVTIMEO, timeout);
            if (rv != 0) {
                nng_close(_socket);
                throw runtime_error("Failed to set NNG timeout option: " + string(nng_strerror(rv)));
            }
        }

        ~NngSubscribeSocket() override {
            // Stop receive thread if running
            if (_running) {
                _running = false;
                if (_recvThread.joinable()) {
                    _recvThread.join();
                }
            }

            nng_close(_socket);
        }

        void Connect(const string& url) override {
            if (_connected) {
                throw runtime_error("Socket already connected");
            }

            int rv = nng_dial(_socket, url.c_str(), nullptr, 0);
            if (rv != 0) {
                throw runtime_error("Failed to connect NNG sub socket: " + string(nng_strerror(rv)));
            }

            _connected = true;

            // Start receive thread
            _running = true;
            _recvThread = thread(&NngSubscribeSocket::ReceiveLoop, this);
        }
    };
}