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

    // NNG implementation for Request-Reply client socket
    class NngReqRspClientSocket : public ITransportReqRspClientSocket {
    private:
        nng_socket _socket;
        bool _connected;

    public:
        NngReqRspClientSocket() : _connected(false) {
            int rv = nng_req0_open(&_socket);
            if (rv != 0) {
                throw runtime_error("Failed to open NNG req socket: " + string(nng_strerror(rv)));
            }
        }

        ~NngReqRspClientSocket() override {
            nng_close(_socket);
        }

        void Connect(const string& url) override {
            if (_connected) {
                throw runtime_error("Socket already connected");
            }

            int rv = nng_dial(_socket, url.c_str(), nullptr, 0);
            if (rv != 0) {
                throw runtime_error("Failed to connect NNG req socket: " + string(nng_strerror(rv)));
            }

            _connected = true;
        }

        string Send(const string& data) override {
            if (!_connected) {
                throw runtime_error("Socket not connected");
            }

            // Prepare request message
            void* reqBuf = nullptr;
            size_t reqSize = data.size();
            int rv = nng_alloc(&reqBuf, reqSize);
            if (rv != 0) {
                throw runtime_error("Failed to allocate memory for NNG request: " + string(nng_strerror(rv)));
            }

            // Copy data to buffer
            memcpy(reqBuf, data.data(), reqSize);

            // Send request and receive response
            void* repBuf = nullptr;
            size_t repSize = 0;

            rv = nng_send(_socket, reqBuf, reqSize, NNG_FLAG_ALLOC);
            if (rv != 0) {
                nng_free(reqBuf, reqSize);
                throw runtime_error("Failed to send NNG request: " + string(nng_strerror(rv)));
            }

            rv = nng_recv(_socket, &repBuf, &repSize, NNG_FLAG_ALLOC);
            if (rv != 0) {
                throw runtime_error("Failed to receive NNG response: " + string(nng_strerror(rv)));
            }

            // Convert response to string
            string response(static_cast<char*>(repBuf), repSize);
            nng_free(repBuf, repSize);

            return response;
        }
    };
}