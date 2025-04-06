#pragma once

#include <string>
#include <memory>
#include <nngpp/nngpp.h>
#include <nngpp/protocol/pub0.h>
#include <nngpp/socket_view.h>
#include "cppplumberd/transport_interfaces.hpp"

namespace cppplumberd {

    using namespace std;

    // NNG implementation for Publish socket using nngpp - with correct API usage
    class NngPublishSocket : public ITransportPublishSocket {
    private:
        nng::socket _socket;
        bool _bound = false;

    public:
        NngPublishSocket() {
            // Open a publisher socket - will throw on failure
            _socket = nng::pub::open();
        }

        void Bind(const string& url) override {
            if (_bound) {
                throw runtime_error("Socket already bound");
            }
            // Listen on the URL - will throw on failure
            _socket.listen(url.c_str());
            _bound = true;
        }

        void Send(const string& data) override {
            if (!_bound) {
                throw runtime_error("Socket not bound");
            }

            // Using view to send data directly
            nng::view view(data.data(), data.size());
            _socket.send(view);
        }
    };
}