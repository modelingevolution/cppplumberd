#pragma once

#include <string>
#include <memory>
#include <nngpp/nngpp.h>
#include "cppplumberd/transport_interfaces.hpp"
#include "ngg_publish_socket.hpp"
#include "ngg_subscribe_socket.hpp"
#include "ngg_req_rsp_client_socket.hpp"
#include "ngg_req_rsp_server_socket.hpp"

namespace cppplumberd {

    using namespace std;

    // NNG Socket Factory implementation using nngcpp - fail fast approach
    class NngSocketFactory : public ISocketFactory {
    private:
        string _defaultUrl;

        string GetFullUrl(const string& endpoint) const {
            return _defaultUrl + "/" + endpoint;
        }

    public:
        explicit NngSocketFactory(string defaultUrl = "ipc:///tmp/cppplumberd")
            : _defaultUrl(std::move(defaultUrl)) {
        }

        unique_ptr<ITransportPublishSocket> CreatePublishSocket(const string& endpoint) override {
            auto socket = make_unique<NngPublishSocket>();
            socket->Bind(GetFullUrl(endpoint));
            return socket;
        }

        unique_ptr<ITransportSubscribeSocket> CreateSubscribeSocket(const string& endpoint) override {
            auto socket = make_unique<NngSubscribeSocket>();
            socket->Connect(GetFullUrl(endpoint));
            return socket;
        }

        unique_ptr<ITransportReqRspClientSocket> CreateReqRspClientSocket(const string& endpoint) override {
            auto socket = make_unique<NngReqRspClientSocket>();
            socket->Connect(GetFullUrl(endpoint));
            return socket;
        }

        unique_ptr<ITransportReqRspSrvSocket> CreateReqRspSrvSocket(const string& endpoint) override {
            auto socket = make_unique<NngReqRspSrvSocket>();
            socket->Bind(GetFullUrl(endpoint));
            return socket;
        }
    };

} // namespace cppplumberd