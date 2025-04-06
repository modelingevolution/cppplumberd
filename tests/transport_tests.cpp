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
    vector<string> receivedMessages;
    std::mutex messagesMutex;
    condition_variable messagesCV;

    auto publisher = factory->CreatePublishSocket(pub_sub);
    publisher->Start();
    this_thread::sleep_for(chrono::milliseconds(100));

    auto subscriber = factory->CreateSubscribeSocket(pub_sub);
    
    // Set up subscriber callback
    subscriber->Received.connect([&](const string& message) {
        cout << "message received: " << message << endl;
        {
            lock_guard<std::mutex> lock(messagesMutex);
            receivedMessages.push_back(message);
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

    // Send messages
    for (const auto& msg : testMessages) {
        publisher->Send(msg);
        cout << "message send: " << msg << endl;
    }
    this_thread::sleep_for(chrono::milliseconds(350)); // Allow time for delivery
    {
        unique_lock<std::mutex> lock(messagesMutex);
        ASSERT_TRUE(messagesCV.wait_for(lock, chrono::seconds(2),
            [&]() { return receivedMessages.size() >= testMessages.size(); }));
    }

    // Verify received messages
    ASSERT_EQ(receivedMessages.size(), testMessages.size());
    for (size_t i = 0; i < testMessages.size(); ++i) {
        EXPECT_EQ(receivedMessages[i], testMessages[i]);
    }
}

// Test request-reply pattern
TEST_F(TransportTest, ReqRepTest) {
    
    auto server = factory->CreateReqRspSrvSocket(req_rsp);

    
    server->Initialize([](const string& message) -> string { return "Echo: " + message; });
    server->Start();

    this_thread::sleep_for(chrono::milliseconds(200));

    auto client = factory->CreateReqRspClientSocket(req_rsp);
    this_thread::sleep_for(chrono::milliseconds(100));
    client->Start();
    
    StopWatch totalSw = StopWatch::StartNew();
    string response = client->Send("Hello, Server!");
    EXPECT_EQ(response, "Echo: Hello, Server!");

    // Test multiple request-reply interactions
    for (int i = 1; i <= 5; ++i) {
        string testRequest = "Request " + to_string(i);

        // Time each individual request
        StopWatch requestSw = StopWatch::StartNew();
        string testResponse = client->Send(testRequest);
        requestSw.Stop();

        requestSw.PrintElapsed("Request " + to_string(i));
        EXPECT_EQ(testResponse, "Echo: " + testRequest);
    }

    // Stop total timer and print
    totalSw.Stop();
    totalSw.PrintElapsed("Total execution time");
}




int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}