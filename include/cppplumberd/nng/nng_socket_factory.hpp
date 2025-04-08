#pragma once

#include <string>
#include <memory>
#include <filesystem>
#include <nngpp/nngpp.h>
#include "cppplumberd/transport_interfaces.hpp"
#include "nng_publish_socket.hpp"
#include "nng_subscribe_socket.hpp"
#include "nng_req_rsp_client_socket.hpp"
#include "nng_req_rsp_server_socket.hpp"

namespace cppplumberd {

    using namespace std;
    namespace fs = std::filesystem;

    // NNG Socket Factory implementation using nngcpp - fail fast approach
    class NggSocketFactory : public ISocketFactory {
    private:
        string _rootUrl;

        string GetFullUrl(const string& endpoint) const {
            return _rootUrl + "/" + endpoint;
        }

        // Extract directory path from IPC URL and ensure it exists
        void EnsureDirectoryExists(const string& url) {
            
            if (url.find("ipc://") == 0) {
                string path = url.substr(6); 
                fs::create_directories(path);
            }
        }

    public:
        explicit NggSocketFactory(string defaultUrl = "ipc:///tmp/cppplumberd")
            : _rootUrl(std::move(defaultUrl)) {
            EnsureDirectoryExists(_rootUrl);
        }

        unique_ptr<ITransportPublishSocket> CreatePublishSocket(const string& endpoint) override {
            auto socket = make_unique<NngPublishSocket>(GetFullUrl(endpoint));
            return socket;
        }

        unique_ptr<ITransportSubscribeSocket> CreateSubscribeSocket(const string& endpoint) override {
            auto socket = make_unique<NngSubscribeSocket>(GetFullUrl(endpoint));
            return socket;
        }

        unique_ptr<ITransportReqRspClientSocket> CreateReqRspClientSocket(const string& endpoint) override {
            auto socket = make_unique<NngReqRspClientSocket>(GetFullUrl(endpoint));
            return socket;
        }

        unique_ptr<ITransportReqRspSrvSocket> CreateReqRspSrvSocket(const string& endpoint) override {
            auto socket = make_unique<NngReqRspSrvSocket>(GetFullUrl(endpoint));
            return socket;
        }
    };

} // namespace cppplumberd