#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <chrono>
#include <memory>
#include <string>
#include "plumberd.hpp"
#include "test_msgs.pb.h"




using namespace cppplumberd;
using namespace std;
using namespace testing;
using namespace app::testing;

// Mock for ITransportPublishSocket
class MockTransportPublishSocket : public ITransportPublishSocket {
public:
    MOCK_METHOD(void, Start, (), (override));
    MOCK_METHOD(void, Start, (const string& url), (override));
    MOCK_METHOD(void, Send, (const string& data), (override));
};

// Test fixture
class ProtoPublishHandlerTest : public Test {
protected:
    void SetUp() override {
        // Create the mock
        mockSocket = make_shared<MockTransportPublishSocket>();

        // Create the handler with the mock
        handler = make_unique<ProtoPublishHandler>(unique_ptr<ITransportPublishSocket>(mockSocket.get()));

        // Register PropertyChangedEvent message type
        handler->RegisterMessage<app::testing::PropertyChangedEvent, PROPERTY_CHANGED_EVENT_ID>();
    }

    void TearDown() override {
        // Prevent deletion of the mock by the unique_ptr in handler
        // by releasing it before handler's destructor runs
        if (handler) {
            handler.release();
        }
    }

    // Constants for test
    static constexpr unsigned int PROPERTY_CHANGED_EVENT_ID = 1; // From examples/app/contract.h EVENTS enum

    // Test objects
    shared_ptr<MockTransportPublishSocket> mockSocket;
    unique_ptr<ProtoPublishHandler> handler;
};

TEST_F(ProtoPublishHandlerTest, StartCallsSocketStart) {
    // Expect Start to be called on the socket
    EXPECT_CALL(*mockSocket, Start()).Times(1);

    // Call Start on the handler
    handler->Start();
}

TEST_F(ProtoPublishHandlerTest, PublishSerializesAndSendsMessage) {
    // Create a PropertyChangedEvent from the app
    app::testing::PropertyChangedEvent event;
    event.set_element_name("TestElement");
    event.set_property_name("TestProperty");
    event.set_value_type(ValueType::INT);

    // Serialize an int value and set it
    int testValue = 42;
    string valueData;
    valueData.assign(reinterpret_cast<const char*>(&testValue), sizeof(testValue));
    event.set_value_data(valueData);

    // Set up expectations
    EXPECT_CALL(*mockSocket, Send(_)).WillOnce([](const string& data) {
        // First part of the message should be the EventHeader
        EventHeader header;
        string headerPart = data.substr(0, header.ByteSize());
        EXPECT_TRUE(header.ParseFromString(headerPart));

        // Event type should match our registered ID
        EXPECT_EQ(header.event_type(), 1); // PROPERTY_CHANGED_EVENT_ID

        // Timestamp should be set
        EXPECT_GT(header.timestamp(), 0);

        // The rest of the data should be our serialized PropertyChangedEvent
        string eventPart = data.substr(headerPart.size());
        PropertyChangedEvent receivedEvent;
        EXPECT_TRUE(receivedEvent.ParseFromString(eventPart));

        // Verify the event fields
        EXPECT_EQ(receivedEvent.element_name(), "TestElement");
        EXPECT_EQ(receivedEvent.property_name(), "TestProperty");
        EXPECT_EQ(receivedEvent.value_type(), ValueType::INT);

        // Verify the value
        int receivedValue;
        memcpy(&receivedValue, receivedEvent.value_data().data(), sizeof(int));
        EXPECT_EQ(receivedValue, 42);
        });

    // Call Publish on the handler
    handler->Publish(event);
}

TEST_F(ProtoPublishHandlerTest, PublishThrowsWhenMessageTypeNotRegistered) {
    // Create a different, unregistered event type (using SetterCommand as another message type)
    SetterCommand cmd;
    cmd.set_element_name("TestElement");
    cmd.set_property_name("TestProperty");

    // Expect no calls to Send
    EXPECT_CALL(*mockSocket, Send(_)).Times(0);

    // Expect an exception when trying to publish an unregistered event type
    EXPECT_THROW(handler->Publish(cmd), runtime_error);
}