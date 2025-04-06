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

    // NNG implementation for Request-Reply server socket
    class NngReqRspSrvSocket : public ITransportReqRspSrvSocket {
    private:
        nng_socket _socket;
        bool _bound;
        thread _recvThread;
        atomic<bool> _running;
        function<IResponse(const string&)> _handler;

        class NngResponse : public IResponse {
        private:
            nng_aio* _aio;
            nng_msg* _msg;
            bool _replied;

        public:
            NngResponse(nng_aio* aio, nng_msg* reqMsg) : _aio(aio), _replied(false) {
                // Create new message for response
                int rv = nng_msg_alloc(&_msg, 0);
                if (rv != 0) {
                    throw runtime_error("Failed to allocate NNG response message: " + string(nng_strerror(rv)));
                }

                // Copy request headers to response
                rv = nng_msg_header_append(_msg, nng_msg_header(_reqMsg), nng_msg_header_len(_reqMsg));
                if (rv != 0) {
                    nng_msg_free(_msg);
                    throw runtime_error("Failed to copy NNG message headers: " + string(nng_strerror(rv)));
                }
            }

            ~NngResponse() override {
                if (!_replied) {
                    // Free message if not sent
                    nng_msg_free(_msg);
                }
            }

            void Return(const string& data) override {
                if (_replied) {
                    throw runtime_error("Response already sent");
                }

                // Copy data to message
                int rv = nng_msg_append(_msg, data.data(), data.size());
                if (rv != 0) {
                    throw runtime_error("Failed to append data to NNG message: " + string(nng_strerror(rv)));
                }

                // Send response
                nng_aio_set_msg(_aio, _msg);
                nng_ctx_send(nullptr, _aio);

                _replied = true;
            }
        };

        static void HandleRequest(void* arg) {
            auto* self = static_cast<NngReqRspSrvSocket*>(arg);
            nng_aio* aio = nullptr;

            // Get the AIO
            aio = self->_aio;

            // Get the message
            nng_msg* msg = nng_aio_get_msg(aio);
            if (msg == nullptr) {
                // No message, just return
                return;
            }

            // Extract data from message
            const void* data = nng_msg_body(msg);
            size_t size = nng_msg_len(msg);
            string request(static_cast<const char*>(data), size);

            try {
                // Create response object
                auto response = self->_handler(request);

                // Response is sent by the IResponse implementation
            }
            catch (const exception& ex) {
                cerr << "Error handling NNG request: " << ex.what() << endl;

                // Send error response
                nng_msg_clear(msg);
                string errorMsg = "Error: " + string(ex.what());
                nng_msg_append(msg, errorMsg.data(), errorMsg.size());
                nng_aio_set_msg(aio, msg);
                nng_ctx_send(nullptr, aio);
            }
        }

        nng_aio* _aio;

    public:
        NngReqRspSrvSocket() : _bound(false), _running(false), _aio(nullptr) {
            int rv = nng_rep0_open(&_socket);
            if (rv != 0) {
                throw runtime_error("Failed to open NNG rep socket: " + string(nng_strerror(rv)));
            }

            // Create AIO
            rv = nng_aio_alloc(&_aio, HandleRequest, this);
            if (rv != 0) {
                nng_close(_socket);
                throw runtime_error("Failed to allocate NNG AIO: " + string(nng_strerror(rv)));
            }
        }

        ~NngReqRspSrvSocket() override {
            // Stop receive thread if running
            if (_running) {
                _running = false;
                if (_recvThread.joinable()) {
                    _recvThread.join();
                }
            }

            if (_aio != nullptr) {
                nng_aio_free(_aio);
            }

            nng_close(_socket);
        }

        void Initialize(function<IResponse(const string&)> handler) override {
            _handler = move(handler);
        }

        void Bind(const string& url) override {
            if (_bound) {
                throw runtime_error("Socket already bound");
            }

            int rv = nng_listen(_socket, url.c_str(), nullptr, 0);
            if (rv != 0) {
                throw runtime_error("Failed to bind NNG rep socket: " + string(nng_strerror(rv)));
            }

            _bound = true;

            // Start receive thread
            _running = true;
            nng_ctx_recv(nullptr, _aio);
        }
    };
}