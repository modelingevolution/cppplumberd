#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include "plumberd.hpp"
#include "test_msgs.pb.h"
#include "contract.h"

using namespace cppplumberd;
using namespace std;
using namespace testing;
using namespace app::testing;

class TestCommandPublishingHandler : public ICommandHandler<SetterCommand>
{
	shared_ptr<EventStore> _eventStore;
public:
	TestCommandPublishingHandler(shared_ptr<EventStore> es) :_eventStore(es) {  }
    void Handle(const string& recipient, const SetterCommand& cmd) override {
        PropertyChangedEvent evt;
        evt.set_property_name(cmd.property_name());
		evt.set_element_name(cmd.element_name());
		evt.set_value_type(cmd.value_type());
		evt.set_value_data(cmd.value_data());

        
        _eventStore->Publish("foo", evt);

    }

};

// Test Event handler that will process the Event
class TestReadModel :
public EventHandlerBase,
public cppplumberd::IEventHandler<PropertyChangedEvent> {
public:
	TestReadModel()
	{
		Map<PropertyChangedEvent, app::testing::EVENTS::PROPERTY_CHANGED>();
	}

    void Handle(const Metadata& m, const PropertyChangedEvent& evt) override {
        lock_guard<std::mutex> lock(mtx);
        receivedEvent = evt;
        EventReceived = true;
        EventProcessed.notify_all();
    }

    bool WaitForEvent(int timeoutMs = 1000) {
        unique_lock<std::mutex> lock(mtx);
        return EventProcessed.wait_for(lock, chrono::milliseconds(timeoutMs),
            [this]() { return EventReceived; });
    }

    PropertyChangedEvent GetReceivedEvent() {
        lock_guard<std::mutex> lock(mtx);
        return receivedEvent;
    }

    void Reset() {
        lock_guard<std::mutex> lock(mtx);
        EventReceived = false;
    }

private:
    std::mutex mtx;
    condition_variable EventProcessed;
    bool EventReceived = false;
    PropertyChangedEvent receivedEvent;
};

// Test Fixture for Event Flow Tests
class EventFlowTest : public Test {
protected:
	unique_ptr<ISubscription> _sub;

    void SetUp() override {
        // Create socket factory
        socketFactory = make_shared<NggSocketFactory>("ipc:///tmp/Event_flow_test");
		_testModel = make_shared<TestReadModel>();
        // Create server and client
        server = Plumber::CreateServer(socketFactory);
        client = PlumberClient::CreateClient(socketFactory);

        server->AddCommandHandler<TestCommandPublishingHandler, SetterCommand, app::testing::COMMANDS::SETTER>(server->GetEventStore());
        server->RegisterMessage<PropertyChangedEvent, app::testing::EVENTS::PROPERTY_CHANGED>();
        server->Start();

        this->_sub = client->SubscriptionManager()->Subscribe("foo", _testModel);
        client->RegisterMessage<SetterCommand, app::testing::COMMANDS::SETTER>();
        client->RegisterMessage<PropertyChangedEvent, app::testing::EVENTS::PROPERTY_CHANGED>();
        // Give the server time to start
        this_thread::sleep_for(chrono::milliseconds(100));

        client->Start();
    }

    void TearDown() override {
        // Stop client and server
        client->Stop();
        server->Stop();

        // Reset the Event handler
        _testModel->Reset();
    }

    // Helper function to create a test Event
    SetterCommand CreateSetterCommand(const string& element, const string& property, int value) {
        SetterCommand cmd;
        cmd.set_element_name(element);
        cmd.set_property_name(property);
        cmd.set_value_type(ValueType::INT);

        // Serialize the value into a binary string
        string valueData;
        valueData.resize(sizeof(int));
        memcpy(&valueData[0], &value, sizeof(int));
        cmd.set_value_data(valueData);

        return cmd;
    }

    // Test objects
    shared_ptr<ISocketFactory> socketFactory;
    unique_ptr<Plumber> server;
    unique_ptr<PlumberClient> client;
    shared_ptr<TestReadModel> _testModel;
};

// Test basic Event flow
TEST_F(EventFlowTest, BasicEventFlowTest) {
    // Create a test Event
    int testValue = 42;
    auto cmd = CreateSetterCommand("TestElement", "TestProperty", testValue);

    // Send the Event through the client
    client->CommandBus()->Send("foo", cmd);

    // Wait for the Event to be processed by the handler
    ASSERT_TRUE(_testModel->WaitForEvent()) << "Event was not received by handler within timeout";

    // Verify the Event was processed correctly
    auto receivedCmd = _testModel->GetReceivedEvent();
    EXPECT_EQ(receivedCmd.element_name(), cmd.element_name());
    EXPECT_EQ(receivedCmd.property_name(), cmd.property_name());
    EXPECT_EQ(receivedCmd.value_type(), cmd.value_type());

    // Extract and verify the value
    int receivedValue;
    ASSERT_EQ(receivedCmd.value_data().size(), sizeof(int));
    memcpy(&receivedValue, receivedCmd.value_data().data(), sizeof(int));
    EXPECT_EQ(receivedValue, testValue);
}

// Test multiple sequential Events
TEST_F(EventFlowTest, MultipleSequentialEventsTest) {
    // Send several Events in sequence
    for (int i = 0; i < 5; i++) {
        // Reset for each Event
        _testModel->Reset();

        // Create a Event with unique value
        int testValue = 100 + i;
        auto cmd = CreateSetterCommand("Element" + to_string(i), "Property", testValue);

        // Send the Event
        client->CommandBus()->Send("foo",cmd);

        // Wait for processing
        ASSERT_TRUE(_testModel->WaitForEvent())
            << "Event " << i << " was not received by handler within timeout";

        // Verify the Event
        auto receivedCmd = _testModel->GetReceivedEvent();
        EXPECT_EQ(receivedCmd.element_name(), cmd.element_name());

        // Extract and verify the value
        int receivedValue;
        ASSERT_EQ(receivedCmd.value_data().size(), sizeof(int));
        memcpy(&receivedValue, receivedCmd.value_data().data(), sizeof(int));
        EXPECT_EQ(receivedValue, testValue);
    }
}

// Add to your test/ directory and add to your CMakeLists.txt