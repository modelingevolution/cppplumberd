#include <gtest/gtest.h>
#include <string>
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <vector>
#include "cppplumberd/transport_interfaces.hpp"
#include "cppplumberd/nng/ngg_socket_factory.hpp"

using namespace std;
using namespace cppplumberd;

// Test fixture for transport tests that need a socket factory
class TransportTest : public ::testing::Test {
protected:
    shared_ptr<ISocketFactory> factory;
    const string testEndpoint = "test"; // Will be transformed into actual URL by the factory

    void SetUp() override {
        // Create socket factory - using nng implementation but through the interface
        factory = make_shared<NngSocketFactory>("ipc:///tmp/transport_test");
    }

    void TearDown() override {
        factory.reset();
    }
};

// Test publish-subscribe pattern
TEST_F(TransportTest, PubSubTest) {
    // Set up a subscriber that will collect messages
    vector<string> receivedMessages;
    std::mutex messagesMutex;
    condition_variable messagesCV;

    // Create subscriber
    auto subscriber = factory->CreateSubscribeSocket(testEndpoint);

    // Set up subscriber callback
    subscriber->Received.connect([&](const string& message) {
        {
            lock_guard<std::mutex> lock(messagesMutex);
            receivedMessages.push_back(message);
        }
        messagesCV.notify_one();
        });

    // Allow time for connection to establish
    this_thread::sleep_for(chrono::milliseconds(100));

    // Create publisher
    auto publisher = factory->CreatePublishSocket(testEndpoint);

    // Allow time for binding
    this_thread::sleep_for(chrono::milliseconds(100));

    // Test messages
    const vector<string> testMessages = {
        "Hello, World!",
        "Second Message",
        "Third Message"
    };

    // Send messages
    for (const auto& msg : testMessages) {
        publisher->Send(msg);
        this_thread::sleep_for(chrono::milliseconds(50)); // Allow time for delivery
    }

    // Wait for all messages to be received (with timeout)
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
    // Create a server that echoes messages back
    auto server = factory->CreateReqRspSrvSocket(testEndpoint);

    // Set up server to echo messages
    server->Initialize([](const string& message) -> string { return "Echo: " + message; });
    server->Start();

    this_thread::sleep_for(chrono::milliseconds(100));

    auto client = factory->CreateReqRspClientSocket(testEndpoint);

    // Allow time for connection
    this_thread::sleep_for(chrono::milliseconds(100));

    const string request = "Hello, Server!";
    string response = client->Send(request);

    // Verify response
    EXPECT_EQ(response, "Echo: " + request);

    // Test multiple request-reply interactions
    for (int i = 1; i <= 5; ++i) {
        string testRequest = "Request " + to_string(i);
        string testResponse = client->Send(testRequest);
        EXPECT_EQ(testResponse, "Echo: " + testRequest);

        // Add a small delay between requests
        this_thread::sleep_for(chrono::milliseconds(50));
    }
}

// Parameterized test to verify send-receive with different message sizes
class MessageSizeTest : public TransportTest, public ::testing::WithParamInterface<size_t> {
protected:
    // Helper to generate a string of a specific size
    string GenerateString(size_t size) {
        string result;
        result.reserve(size);
        for (size_t i = 0; i < size; ++i) {
            result.push_back(static_cast<char>('A' + (i % 26)));
        }
        return result;
    }
};

// Test that messages of different sizes can be sent and received properly
TEST_P(MessageSizeTest, PubSubDifferentSizes) {
    size_t messageSize = GetParam();

    // Set up subscriber
    atomic<bool> messageReceived{ false };
    string receivedMessage;
    std::mutex messageMutex;
    condition_variable messageCV;

    auto subscriber = factory->CreateSubscribeSocket(testEndpoint);
    subscriber->Received.connect([&](const string& message) {
        {
            lock_guard<std::mutex> lock(messageMutex);
            receivedMessage = message;
            messageReceived = true;
        }
        messageCV.notify_one();
        });

    // Allow time for connection
    this_thread::sleep_for(chrono::milliseconds(100));

    // Create publisher
    auto publisher = factory->CreatePublishSocket(testEndpoint);

    // Allow time for binding
    this_thread::sleep_for(chrono::milliseconds(100));

    // Generate and send a message of the specified size
    string testMessage = GenerateString(messageSize);
    publisher->Send(testMessage);

    // Wait for message to be received
    {
        unique_lock<std::mutex> lock(messageMutex);
        ASSERT_TRUE(messageCV.wait_for(lock, chrono::seconds(2), [&]() { return messageReceived.load(); }));
    }

    // Verify message
    EXPECT_EQ(receivedMessage.size(), messageSize);
    EXPECT_EQ(receivedMessage, testMessage);
}

// Test request-response with different message sizes
TEST_P(MessageSizeTest, ReqRepDifferentSizes) {
    size_t messageSize = GetParam();

    // Create server
    auto server = factory->CreateReqRspSrvSocket(testEndpoint);
    server->Initialize([](const string& message) -> string {
        return "Echo: " + message;
        });

    // Allow time for binding
    this_thread::sleep_for(chrono::milliseconds(100));

    // Create client
    auto client = factory->CreateReqRspClientSocket(testEndpoint);

    // Allow time for connection
    this_thread::sleep_for(chrono::milliseconds(100));

    // Generate test message
    string testMessage = GenerateString(messageSize);

    // Send request and get response
    string response = client->Send(testMessage);

    // Verify response
    EXPECT_EQ(response, "Echo: " + testMessage);
}

// Different message sizes to test
INSTANTIATE_TEST_SUITE_P(
    VaryingSizes,
    MessageSizeTest,
    ::testing::Values(
        1,          // Single byte
        100,        // Small message
        1000,       // Medium message
        10000,      // Larger message
        100000      // Very large message
    )
);

// Test error handling and edge cases
TEST_F(TransportTest, ErrorHandlingTest) {
    // Test binding to same endpoint twice
    auto server1 = factory->CreateReqRspSrvSocket("duplicate_endpoint");
    server1->Initialize([](const string& message) -> string {
        return "Echo: " + message;
        });

    // Allow time for the first binding to complete
    this_thread::sleep_for(chrono::milliseconds(100));

    // Binding to the same endpoint should throw
    auto server2 = factory->CreateReqRspSrvSocket("duplicate_endpoint");
    server2->Initialize([](const string& message) -> string {
        return "Echo: " + message;
        });

    // This should throw an exception
    EXPECT_THROW({
        server2->Start("ipc:///tmp/transport_test/duplicate_endpoint");
        }, std::exception);

    // Test sending without binding
    auto unboundPublisher = factory->CreatePublishSocket("unbound");
    EXPECT_THROW({
        unboundPublisher->Send("This should fail");
        }, std::exception);

    // Test receiving without connecting
    auto unconnectedClient = factory->CreateReqRspClientSocket("unconnected");
    EXPECT_THROW({
        unconnectedClient->Send("This should fail");
        }, std::exception);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}