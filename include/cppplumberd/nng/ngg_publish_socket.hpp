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


    // NNG implementation for Publish socket
    class NngPublishSocket : public ITransportPublishSocket {
    private:
        nng_socket _socket;
        bool _bound;

    public:
        NngPublishSocket() : _bound(false) {
            int rv = nng_pub0_open(&_socket);
            if (rv != 0) {
                throw runtime_error("Failed to open NNG pub socket: " + string(nng_strerror(rv)));
            }
        }

        ~NngPublishSocket() override {
            nng_close(_socket);
        }

        void Bind(const string& url) override {
            if (_bound) {
                throw runtime_error("Socket already bound");
            }

            int rv = nng_listen(_socket, url.c_str(), nullptr, 0);
            if (rv != 0) {
                throw runtime_error("Failed to bind NNG pub socket: " + string(nng_strerror(rv)));
            }

            _bound = true;
        }

        void Send(const string& data) override {
            if (!_bound) {
                throw runtime_error("Socket not bound");
            }

            // NNG requires a separate buffer to send - we can't just send data directly
            void* buf = nullptr;
            size_t size = data.size();
            int rv = nng_alloc(&buf, size);
            if (rv != 0) {
                throw runtime_error("Failed to allocate memory for NNG message: " + string(nng_strerror(rv)));
            }

            // Copy data to buffer
            memcpy(buf, data.data(), size);

            // Send buffer
            rv = nng_send(_socket, buf, size, NNG_FLAG_ALLOC);
            if (rv != 0) {
                nng_free(buf, size);
                throw runtime_error("Failed to send NNG message: " + string(nng_strerror(rv)));
            }
            // No need to free buf as NNG takes ownership with NNG_FLAG_ALLOC
        }
    };
}