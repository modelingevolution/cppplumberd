#pragma once

#include <string>
#include <iostream>
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
        string _url;
        bool _bound = false;

    public:
        NngPublishSocket(const string &url) : _url(url) {
            // Open a publisher socket - will throw on failure
            _socket = nng::pub::open();
            
        }
        void Start() override
        {
            Start(_url);
        }
        void Start(const string& url) override {
            if (_bound) {
                throw runtime_error("Socket already bound");
            }
            if (_url != url)
                _url = url;

            _socket.listen(url.c_str());
            
            cout << "listening at: " << url << endl;
            _bound = true;
        }

        void Send(const uint8_t* buffer, const size_t size) override {
            if (!_bound) {
                throw runtime_error("Socket not bound");
            }

            // Using view to send data directly
            nng::view view(buffer, size);
            _socket.send(view);
        }
    };
}