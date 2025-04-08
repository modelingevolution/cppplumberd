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

// Test command that will be sent through the system
class TestCommand {
public:
    string message;
};

// Test command handler that will process the command
class TestCommandHandler : public ICommandHandler<SetterCommand> {
public:
    void Handle(const string& stream_id, const SetterCommand& cmd) override {
        lock_guard<std::mutex> lock(mtx);
        receivedCommand = cmd;
        commandReceived = true;
        commandProcessed.notify_all();
    }

    bool WaitForCommand(int timeoutMs = 1000) {
        unique_lock<std::mutex> lock(mtx);
        return commandProcessed.wait_for(lock, chrono::milliseconds(timeoutMs),
            [this]() { return commandReceived; });
    }

    SetterCommand GetReceivedCommand() {
        lock_guard<std::mutex> lock(mtx);
        return receivedCommand;
    }

    void Reset() {
        lock_guard<std::mutex> lock(mtx);
        commandReceived = false;
    }

private:
    std::mutex mtx;
    condition_variable commandProcessed;
    bool commandReceived = false;
    SetterCommand receivedCommand;
};

// Test Fixture for Command Flow Tests
class CommandFlowTest : public Test {
protected:
    void SetUp() override {
        // Create socket factory
        socketFactory = make_shared<NngSocketFactory>("ipc:///tmp/command_flow_test");

        // Create server and client
        server = Plumber::CreateServer(socketFactory, "x");
        client = PlumberClient::CreateClient(socketFactory, "x");

        // Register command handler on server
        commandHandler = make_shared<TestCommandHandler>();
        server->AddCommandHandler<SetterCommand, COMMANDS::SETTER>(commandHandler);

        // Register command on client
        client->CommandBus()->RegisterMessage<SetterCommand, COMMANDS::SETTER>();

        // Start server and client
        server->Start();

        // Give the server time to start
        this_thread::sleep_for(chrono::milliseconds(100));

        client->Start();
    }

    void TearDown() override {
        // Stop client and server
        client->Stop();
        server->Stop();

        // Reset the command handler
        commandHandler->Reset();
    }

    // Helper function to create a test command
    SetterCommand CreateTestCommand(const string& element, const string& property, int value) {
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
    shared_ptr<TestCommandHandler> commandHandler;
};

// Test basic command flow
TEST_F(CommandFlowTest, BasicCommandFlowTest) {
    // Create a test command
    int testValue = 42;
    auto cmd = CreateTestCommand("TestElement", "TestProperty", testValue);

    // Send the command through the client
    client->CommandBus()->Send(cmd);

    // Wait for the command to be processed by the handler
    ASSERT_TRUE(commandHandler->WaitForCommand())
        << "Command was not received by handler within timeout";

    // Verify the command was processed correctly
    auto receivedCmd = commandHandler->GetReceivedCommand();
    EXPECT_EQ(receivedCmd.element_name(), cmd.element_name());
    EXPECT_EQ(receivedCmd.property_name(), cmd.property_name());
    EXPECT_EQ(receivedCmd.value_type(), cmd.value_type());

    // Extract and verify the value
    int receivedValue;
    ASSERT_EQ(receivedCmd.value_data().size(), sizeof(int));
    memcpy(&receivedValue, receivedCmd.value_data().data(), sizeof(int));
    EXPECT_EQ(receivedValue, testValue);
}

// Test multiple sequential commands
TEST_F(CommandFlowTest, MultipleSequentialCommandsTest) {
    // Send several commands in sequence
    for (int i = 0; i < 5; i++) {
        // Reset for each command
        commandHandler->Reset();

        // Create a command with unique value
        int testValue = 100 + i;
        auto cmd = CreateTestCommand("Element" + to_string(i), "Property", testValue);

        // Send the command
        client->CommandBus()->Send(cmd);

        // Wait for processing
        ASSERT_TRUE(commandHandler->WaitForCommand())
            << "Command " << i << " was not received by handler within timeout";

        // Verify the command
        auto receivedCmd = commandHandler->GetReceivedCommand();
        EXPECT_EQ(receivedCmd.element_name(), cmd.element_name());

        // Extract and verify the value
        int receivedValue;
        ASSERT_EQ(receivedCmd.value_data().size(), sizeof(int));
        memcpy(&receivedValue, receivedCmd.value_data().data(), sizeof(int));
        EXPECT_EQ(receivedValue, testValue);
    }
}

// Add to your test/ directory and add to your CMakeLists.txt