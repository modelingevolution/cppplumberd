#include <gtest/gtest.h>
#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <numeric>
#include <vector>

#include "cppplumberd/stop_watch.hpp"
#include "cppplumberd/transport_interfaces.hpp"
#include "cppplumberd/nng/ngg_socket_factory.hpp"

using namespace std;
using namespace cppplumberd;

// Test fixture for transport tests that need a socket factory
class TransportTest : public ::testing::Test {
protected:
    shared_ptr<ISocketFactory> factory;
    const string req_rsp = "rr"; // Will be transformed into actual URL by the factory
    const string pub_sub = "ps";
    void SetUp() override {
        // Create socket factory - using nng implementation but through the interface
        factory = make_shared<NngSocketFactory>("ipc:///tmp/transport_test");
    }

    void TearDown() override {
        factory.reset();
    }
};
constexpr const char* INPROC_ADDR = "inproc://test-pubsub";

// Test Fixture
TEST(NggppTest, PubSub) {
    std::string test_msg = "test-message";

    // Create PUB and bind
    auto pub = nng::pub::open();
    pub.listen(INPROC_ADDR);

    // Create SUB and dial
    auto sub = nng::sub::open();
    sub.dial(INPROC_ADDR);

    // Must subscribe (even to all)
    nng_setopt(sub.get(), NNG_OPT_SUB_SUBSCRIBE, "", 0);

    // Let sockets handshake
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Send message
    auto buf = nng::make_buffer(test_msg.size());
    std::memcpy(buf.data(), test_msg.data(), test_msg.size());
    auto msg = nng::make_msg(0);
    msg.body().append(std::move(buf));
    pub.send(std::move(msg));

    // Wait for subscriber to receive
    auto received_msg = sub.recv();
    std::string received_str((char*)received_msg.data(), received_msg.size());


    // Verify
    ASSERT_EQ(received_str, test_msg);
}
// Test publish-subscribe pattern
TEST_F(TransportTest, PubSubTest) {
    // Set up a subscriber that will collect messages
    vector<vector<uint8_t>> receivedData;
    vector<string> receivedStrings; // For verification
    std::mutex messagesMutex;
    condition_variable messagesCV;

    auto publisher = factory->CreatePublishSocket(pub_sub);
    publisher->Start();
    this_thread::sleep_for(chrono::milliseconds(100));

    auto subscriber = factory->CreateSubscribeSocket(pub_sub);

    // Set up subscriber callback with buffer-based version
    subscriber->Received.connect([&](const uint8_t* buffer, const size_t size) {
        // Create a string for debugging and verification
        string message(reinterpret_cast<const char*>(buffer), size);
        cout << "message received: " << message << endl;

        {
            lock_guard<std::mutex> lock(messagesMutex);
            // Store both binary and string versions
            vector<uint8_t> data(buffer, buffer + size);
            receivedData.push_back(data);
            receivedStrings.push_back(message);
        }
        messagesCV.notify_one();
        });

    subscriber->Start();

    // Test messages
    const vector<string> testMessages = {
        "Hello, World!",
        "Second Message",
        "Third Message"
    };

    // Send messages using the binary buffer interface
    for (const auto& msg : testMessages) {
        publisher->Send(reinterpret_cast<const uint8_t*>(msg.data()), msg.size());
        cout << "message sent: " << msg << endl;
    }

    this_thread::sleep_for(chrono::milliseconds(350)); // Allow time for delivery

    {
        unique_lock<std::mutex> lock(messagesMutex);
        ASSERT_TRUE(messagesCV.wait_for(lock, chrono::seconds(2),
            [&]() { return receivedData.size() >= testMessages.size(); }));
    }

    // Verify received messages
    ASSERT_EQ(receivedStrings.size(), testMessages.size());
    for (size_t i = 0; i < testMessages.size(); ++i) {
        EXPECT_EQ(receivedStrings[i], testMessages[i]);
    }
}

// Test request-reply pattern
TEST_F(TransportTest, ReqRepTest) {
    auto server = factory->CreateReqRspSrvSocket(req_rsp);

    // Set up the handler for the server to process requests
    uint8_t inBuffer[1024];
    uint8_t outBuffer[1024];

    // Create an echo handler
    auto handler = [&inBuffer, &outBuffer](const size_t requestSize) -> size_t {
        // Echo back the request with "Echo: " prefix
        std::string request(reinterpret_cast<const char*>(inBuffer), requestSize);
        std::string response = "Echo: " + request;

        // Copy the response to the output buffer
        memcpy(outBuffer, response.data(), response.size());

        return response.size();
        };

    // Initialize server with the handler
    server->Initialize(handler, inBuffer, sizeof(inBuffer), outBuffer, sizeof(outBuffer));
    server->Start();

    this_thread::sleep_for(chrono::milliseconds(200));

    auto client = factory->CreateReqRspClientSocket(req_rsp);
    this_thread::sleep_for(chrono::milliseconds(100));
    client->Start();

    StopWatch totalSw = StopWatch::StartNew();

    // Use buffer-based version for the first test
    string requestStr = "Hello, Server!";
    string expectedResponse = "Echo: Hello, Server!";

    // Create buffers for request and response
    vector<uint8_t> requestBuffer(requestStr.begin(), requestStr.end());
    vector<uint8_t> responseBuffer(1024); // Allocate response buffer

    size_t responseSize = client->Send(
        requestBuffer.data(),
        requestBuffer.size(),
        responseBuffer.data(),
        responseBuffer.size()
    );

    string responseStr(reinterpret_cast<char*>(responseBuffer.data()), responseSize);
    EXPECT_EQ(responseStr, expectedResponse);

    // Test multiple request-reply interactions
    for (int i = 1; i <= 5; ++i) {
        string testRequest = "Request " + to_string(i);
        string expectedResponse = "Echo: " + testRequest;

        vector<uint8_t> reqBuffer(testRequest.begin(), testRequest.end());
        vector<uint8_t> respBuffer(1024);

        // Time each individual request
        StopWatch requestSw = StopWatch::StartNew();

        size_t respSize = client->Send(
            reqBuffer.data(),
            reqBuffer.size(),
            respBuffer.data(),
            respBuffer.size()
        );

        string testResponse(reinterpret_cast<char*>(respBuffer.data()), respSize);
        requestSw.Stop();

        requestSw.PrintElapsed("Request " + to_string(i));
        EXPECT_EQ(testResponse, expectedResponse);
    }

    // Stop total timer and print
    totalSw.Stop();
    totalSw.PrintElapsed("Total execution time");
}


int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}