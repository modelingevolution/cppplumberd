#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <memory>
#include <string>
#include "plumberd.hpp"
#include "test_msgs.pb.h"
#include <vector>
#include <mutex>
#include <condition_variable>
#include "cppplumberd/proto_publish_handler.hpp"
#include "cppplumberd/proto_subscribe_handler.hpp"
#include "cppplumberd/message_serializer.hpp"
#include "contract.h"

using namespace cppplumberd;
using namespace std;
using namespace testing;
using namespace std::chrono;
using namespace app::testing;

// Mock for ITransportPublishSocket
class MockTransportPublishSocket : public ITransportPublishSocket {
public:
    MOCK_METHOD(void, Start, (), (override));
    MOCK_METHOD(void, Start, (const string& url), (override));
    MOCK_METHOD(void, Send, (const uint8_t* data, const size_t size), (override));
};

// Mock for ITransportSubscribeSocket
class MockTransportSubscribeSocket : public ITransportSubscribeSocket {
public:
    MOCK_METHOD(void, Start, (), (override));
    MOCK_METHOD(void, Start, (const string& url), (override));

    // Method to simulate receiving a message
    void SimulateReceive(const uint8_t* data, size_t size) {
        // The Received signal expects non-const pointers, but we can cast away const
        // since we're only reading the data
        Received(const_cast<uint8_t*>(data), size);
    }
};

// Test fixture for integrated tests
class PublishSubscribeIntegrationTest : public Test {
protected:
    void SetUp() override {
        // Create mocks
        mockPubSocket = make_shared<MockTransportPublishSocket>();
        mockSubSocket = make_shared<MockTransportSubscribeSocket>();

        // Create handlers with mocks
        publisher = make_unique<ProtoPublishHandler>(unique_ptr<ITransportPublishSocket>(mockPubSocket.get()));
        subscriber = make_unique<ProtoSubscribeHandler>(unique_ptr<ITransportSubscribeSocket>(mockSubSocket.get()));

        // Register message types
        publisher->RegisterMessage<PropertyChangedEvent, EVENTS::PROPERTY_CHANGED>();

        // Set up condition variables for synchronization
        receivedEvent = false;
    }

    void TearDown() override {
        // Release mocks before handlers are destroyed
        if (publisher) {
            publisher.release();
        }
        if (subscriber) {
            subscriber.release();
        }
    }

    // Helpers
    void WaitForEvent(int timeoutMs = 1000) {
        unique_lock<std::mutex> lock(mtx);
        ASSERT_TRUE(cv.wait_for(lock, milliseconds(timeoutMs), [this]() { return receivedEvent; }));
    }

    // Test objects
    shared_ptr<MockTransportPublishSocket> mockPubSocket;
    shared_ptr<MockTransportSubscribeSocket> mockSubSocket;
    unique_ptr<ProtoPublishHandler> publisher;
    unique_ptr<ProtoSubscribeHandler> subscriber;

    // Event reception tracking
    std::mutex mtx;
    condition_variable cv;
    bool receivedEvent;
    PropertyChangedEvent lastReceivedEvent;
    system_clock::time_point lastReceivedTimestamp;
};

TEST_F(PublishSubscribeIntegrationTest, PublishEventIsReceivedBySubscriber) {
    // 1. Set up event handler in subscriber
    subscriber->RegisterHandler<PropertyChangedEvent, EVENTS::PROPERTY_CHANGED>(
        [this](const system_clock::time_point& timestamp, const PropertyChangedEvent& evt) {
            lock_guard<std::mutex> lock(mtx);
            lastReceivedEvent = evt;
            lastReceivedTimestamp = timestamp;
            receivedEvent = true;
            cv.notify_one();
        }
    );

    // 2. Create a property changed event
    PropertyChangedEvent sentEvent;
    sentEvent.set_element_name("TestElement");
    sentEvent.set_property_name("TestProperty");
    sentEvent.set_value_type(ValueType::INT);

    // Serialize an int value
    int testValue = 42;
    string valueData;
    valueData.assign(reinterpret_cast<const char*>(&testValue), sizeof(testValue));
    sentEvent.set_value_data(valueData);

    // 3. Set up the publish socket mock to capture the message
    // Use a vector to store the captured data
    vector<uint8_t> capturedData;
    size_t capturedSize = 0;

    // Use a custom action to copy the data to our vector
    // This fixes the issue with SaveArg not being able to directly assign to a vector
    ON_CALL(*mockPubSocket, Send(_, _))
        .WillByDefault(DoAll(
            [&capturedData, &capturedSize](const uint8_t* data, size_t size) {
                capturedData.assign(data, data + size);
                capturedSize = size;
            },
            Return()
        ));

    EXPECT_CALL(*mockPubSocket, Send(_, _))
        .Times(1);

    // 4. Start the handlers
    EXPECT_CALL(*mockPubSocket, Start()).Times(1);
    EXPECT_CALL(*mockSubSocket, Start()).Times(1);

    publisher->Start();
    subscriber->Start();

    // 5. Publish the event
    publisher->Publish(sentEvent);

    // 6. Wait for message to be sent
    Mock::VerifyAndClearExpectations(mockPubSocket.get());
    ASSERT_GT(capturedSize, 0) << "No message was captured from the publish handler";

    // 7. Feed the captured message to the subscribe socket
    mockSubSocket->SimulateReceive(capturedData.data(), capturedSize);

    // 8. Wait for the event to be received and processed
    WaitForEvent();

    // 9. Verify the received event matches what was sent
    EXPECT_EQ(lastReceivedEvent.element_name(), sentEvent.element_name());
    EXPECT_EQ(lastReceivedEvent.property_name(), sentEvent.property_name());
    EXPECT_EQ(lastReceivedEvent.value_type(), sentEvent.value_type());

    // 10. Verify the value data
    ASSERT_EQ(lastReceivedEvent.value_data().size(), sizeof(int));
    int receivedValue;
    memcpy(&receivedValue, lastReceivedEvent.value_data().data(), sizeof(int));
    EXPECT_EQ(receivedValue, testValue);

    // 11. Verify timestamp is set and reasonable
    auto now = system_clock::now();
    EXPECT_LE(lastReceivedTimestamp, now);
    EXPECT_GE(lastReceivedTimestamp, now - minutes(1)); // Should be recent
    cout << "DONE" << endl;
}

TEST_F(PublishSubscribeIntegrationTest, HandlesMultipleEvents) {
    // Similar to the test above, but publish multiple events in sequence
    // and verify they're all received correctly

    vector<PropertyChangedEvent> sentEvents;
    vector<PropertyChangedEvent> receivedEvents;
    std::mutex eventsLock;

    // Set up handler to collect all received events
    subscriber->RegisterHandler<PropertyChangedEvent, EVENTS::PROPERTY_CHANGED>(
        [&](const system_clock::time_point& timestamp, const PropertyChangedEvent& evt) {
            lock_guard<std::mutex> lock(eventsLock);
            receivedEvents.push_back(evt);

            // Signal when we've received all events
            if (receivedEvents.size() == sentEvents.size()) {
                lock_guard<std::mutex> cvLock(mtx);
                receivedEvent = true;
                cv.notify_one();
            }
        }
    );

    // Create and stage multiple events
    for (int i = 0; i < 5; i++) {
        PropertyChangedEvent evt;
        evt.set_element_name("Element" + to_string(i));
        evt.set_property_name("Property" + to_string(i));
        evt.set_value_type(ValueType::INT);

        int value = i * 100;
        string valueData;
        valueData.assign(reinterpret_cast<const char*>(&value), sizeof(value));
        evt.set_value_data(valueData);

        sentEvents.push_back(evt);
    }

    // Set up to capture all messages
    vector<vector<uint8_t>> capturedMessages;

    // Use a custom action to copy each message
    ON_CALL(*mockPubSocket, Send(_, _))
        .WillByDefault([&capturedMessages](const uint8_t* data, size_t size) {
        vector<uint8_t> message(data, data + size);
        capturedMessages.push_back(message);
            });

    EXPECT_CALL(*mockPubSocket, Send(_, _))
        .Times(sentEvents.size());

    // Start handlers
    EXPECT_CALL(*mockPubSocket, Start()).Times(1);
    EXPECT_CALL(*mockSubSocket, Start()).Times(1);

    publisher->Start();
    subscriber->Start();

    // Publish all events
    for (const auto& evt : sentEvents) {
        publisher->Publish(evt);
    }

    // Wait for all messages to be sent
    Mock::VerifyAndClearExpectations(mockPubSocket.get());
    ASSERT_EQ(capturedMessages.size(), sentEvents.size());

    // Feed all captured messages to the subscriber
    for (const auto& message : capturedMessages) {
        mockSubSocket->SimulateReceive(message.data(), message.size());
    }

    // Wait for all events to be processed
    WaitForEvent();

    // Verify all events were received correctly
    ASSERT_EQ(receivedEvents.size(), sentEvents.size());

    // Sort both vectors for comparison (since order might not be preserved)
    auto sortByElementName = [](const PropertyChangedEvent& a, const PropertyChangedEvent& b) {
        return a.element_name() < b.element_name();
        };

    sort(sentEvents.begin(), sentEvents.end(), sortByElementName);
    sort(receivedEvents.begin(), receivedEvents.end(), sortByElementName);

    // Compare each event
    for (size_t i = 0; i < sentEvents.size(); i++) {
        EXPECT_EQ(receivedEvents[i].element_name(), sentEvents[i].element_name());
        EXPECT_EQ(receivedEvents[i].property_name(), sentEvents[i].property_name());
        EXPECT_EQ(receivedEvents[i].value_type(), sentEvents[i].value_type());

        int sentValue, receivedValue;
        memcpy(&sentValue, sentEvents[i].value_data().data(), sizeof(int));
        memcpy(&receivedValue, receivedEvents[i].value_data().data(), sizeof(int));
        EXPECT_EQ(receivedValue, sentValue);
    }
    cout << "DONE" << endl;
}

// Additional test case ideas:
// - Test handling of large messages that approach buffer limits
// - Test error conditions like malformed messages
// - Test message type validation