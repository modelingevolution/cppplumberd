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
#include "ngg_publish_socket.hpp"
#include "ngg_subscribe_socket.hpp"
#include "ngg_req_rsp_client_socket.hpp"
#include "ngg_req_rsp_server_socket.hpp"

namespace cppplumberd {

using namespace std;


// NNG Socket Factory implementation
class NngSocketFactory : public ISocketFactory {
private:
    string _defaultUrl;
    
    string AppendEndpointToUrl(const string& url, const string& endpoint) const {
        if (url.empty()) {
            return _defaultUrl + "/" + endpoint;
        }
        return url + "/" + endpoint;
    }

public:
    NngSocketFactory(string defaultUrl = "ipc:///tmp/cppplumberd") : _defaultUrl(move(defaultUrl)) {}

    unique_ptr<ITransportPublishSocket> CreatePublishSocket(const string& endpoint) override {
        auto socket = make_unique<NngPublishSocket>();
        socket->Bind(AppendEndpointToUrl(_defaultUrl, endpoint));
        return socket;
    }

    unique_ptr<ITransportSubscribeSocket> CreateSubscribeSocket(const string& endpoint) override {
        auto socket = make_unique<NngSubscribeSocket>();
        socket->Connect(AppendEndpointToUrl(_defaultUrl, endpoint));
        return socket;
    }

    unique_ptr<ITransportReqRspClientSocket> CreateReqRspClientSocket(const string& endpoint) override {
        auto socket = make_unique<NngReqRspClientSocket>();
        socket->Connect(AppendEndpointToUrl(_defaultUrl, endpoint));
        return socket;
    }

    unique_ptr<ITransportReqRspSrvSocket> CreateReqRspSrvSocket(const string& endpoint) override {
        auto socket = make_unique<NngReqRspSrvSocket>();
        socket->Bind(AppendEndpointToUrl(_defaultUrl, endpoint));
        return socket;
    }
};

} // namespace cppplumberd