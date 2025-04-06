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
        nng::socket _socket;
        bool _connected = false;

    public:
        NngReqRspClientSocket() {
            // Open a request socket - will throw on failure
            _socket = nng::req::open();
        }

        void Connect(const string& url) override {
            if (_connected) {
                throw runtime_error("Socket already connected");
            }

            // Connect to the URL - will throw on failure
            _socket.dial(url.c_str());
            _connected = true;
        }

        string Send(const string& data) override {
            if (!_connected) {
                throw runtime_error("Socket not connected");
            }

            // Send the request data with view
            nng::view view(data.data(), data.size());
            _socket.send(view);

            // Receive response as a buffer
            nng::buffer buffer = _socket.recv();

            // Convert buffer to string and return
            return string(static_cast<const char*>(buffer.data()), buffer.size());
        }
    };
}