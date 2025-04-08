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
#include "cppplumberd/proto_req_rsp_client_handler.hpp"
#include "cppplumberd/proto_req_rsp_srv_handler.hpp"
#include "cppplumberd/message_serializer.hpp"
#include "contract.h"

// Define the error type ID
const unsigned int ERROR_TYPE_ID = 999;

using namespace cppplumberd;
using namespace std;
using namespace testing;
using namespace std::chrono;
using namespace app::testing;

// Mock for ITransportReqRspClientSocket
class MockTransportReqRspClientSocket : public ITransportReqRspClientSocket {
public:
    MOCK_METHOD(void, Start, (), (override));
    MOCK_METHOD(void, Start, (const string& url), (override));
    MOCK_METHOD(size_t, Send, (const uint8_t* inBuf, const size_t inSize, uint8_t* outBuf, const size_t outMaxBufSize), (override));
};

// Mock for ITransportReqRspSrvSocket
class MockTransportReqRspSrvSocket : public ITransportReqRspSrvSocket {
public:
    MOCK_METHOD(void, Start, (), (override));
    MOCK_METHOD(void, Start, (const string& url), (override));
    MOCK_METHOD(void, Initialize, (function<size_t(const size_t)> handler, uint8_t* inBuf, size_t inMaxBufSize, uint8_t* outBuf, size_t outMaxBufSize), (override));

    // Method to capture and store the handler function
    void CaptureHandler(function<size_t(const size_t)> handler, uint8_t* inBuf, size_t inMaxBufSize, uint8_t* outBuf, size_t outMaxBufSize) {
        _handler = handler;
        _inBuffer = inBuf;
        _inBufferSize = inMaxBufSize;
        _outBuffer = outBuf;
        _outBufferSize = outMaxBufSize;
    }

    // Method to simulate receiving a request and generating a response
    size_t SimulateRequest(const uint8_t* requestData, size_t requestSize) {
        if (!_handler) {
            throw runtime_error("Handler not initialized");
        }

        // Copy request data to input buffer
        if (requestSize > _inBufferSize) {
            throw runtime_error("Request size exceeds buffer capacity");
        }
        memcpy(_inBuffer, requestData, requestSize);

        // Call the handler to process the request and get response size
        return _handler(requestSize);
    }

    // Make these public for easier access from test functions
    function<size_t(const size_t)> _handler;
    uint8_t* _inBuffer = nullptr;
    size_t _inBufferSize = 0;
    uint8_t* _outBuffer = nullptr;
    size_t _outBufferSize = 0;
};

// Test fixture for Request-Response integration tests
class ReqRspIntegrationTest : public Test {
protected:
    void SetUp() override {
        // Create mocks
        mockClientSocket = make_shared<MockTransportReqRspClientSocket>();
        mockServerSocket = make_shared<MockTransportReqRspSrvSocket>();

        // Create handlers with mocks
        clientHandler = make_unique<ProtoReqRspClientHandler>(unique_ptr<ITransportReqRspClientSocket>(mockClientSocket.get()));
        serverHandler = make_unique<ProtoReqRspSrvHandler>(unique_ptr<ITransportReqRspSrvSocket>(mockServerSocket.get()));

        // Register message types
        clientHandler->RegisterRequest<SetterCommand, COMMANDS::SETTER>();
        clientHandler->RegisterError<TestError, ERROR_TYPE_ID>();
        serverHandler->RegisterError<TestError, ERROR_TYPE_ID>();

        // Default handler implementation
        SetDefaultCommandHandler();

        // Setup expectations for handler initialization
        SetupServerSocketInitializeMock();

        // Setup expectations for client socket's Send method
        SetupClientSocketSendMock();
    }

    void TearDown() override {
        // Release mocks before handlers are destroyed
        if (clientHandler) {
            clientHandler.release();
        }
        if (serverHandler) {
            serverHandler.release();
        }
    }

    // Helper methods for setting up mocks
    void SetDefaultCommandHandler() {
        serverHandler->RegisterHandler<SetterCommand, COMMANDS::SETTER>([this](const SetterCommand& cmd) {
            // Store the last received command
            lock_guard<std::mutex> lock(mtx);
            lastReceivedCommand = cmd;
            commandReceived = true;
            cv.notify_one();
            });
    }

    void SetupServerSocketInitializeMock() {
        ON_CALL(*mockServerSocket, Initialize(_, _, _, _, _))
            .WillByDefault(Invoke([this](function<size_t(const size_t)> handler,
                uint8_t* inBuf, size_t inMaxBufSize,
                uint8_t* outBuf, size_t outMaxBufSize) {
                    mockServerSocket->CaptureHandler(handler, inBuf, inMaxBufSize, outBuf, outMaxBufSize);
                }));
    }

    void SetupClientSocketSendMock() {
        ON_CALL(*mockClientSocket, Send(_, _, _, _))
            .WillByDefault(Invoke([this](const uint8_t* inBuf, const size_t inSize,
                uint8_t* outBuf, const size_t outMaxBufSize) -> size_t {
                    // Simulate server processing the request
                    size_t responseSize = mockServerSocket->SimulateRequest(inBuf, inSize);

                    // Copy response back to client's output buffer
                    if (responseSize > 0) {
                        memcpy(outBuf, mockServerSocket->_outBuffer, responseSize);
                    }

                    return responseSize;
                }));
    }

    void SetupErrorThrowingCommandHandler() {
        serverHandler->RegisterHandler<SetterCommand, COMMANDS::SETTER>(
            [](const SetterCommand& cmd) -> void {
                throw FaultException("Test error", 400);
            });
    }

    

    // Helpers
    void WaitForCommand(int timeoutMs = 1000) {
        unique_lock<std::mutex> lock(mtx);
        ASSERT_TRUE(cv.wait_for(lock, milliseconds(timeoutMs), [this]() { return commandReceived; }));
    }

    // Helper for creating a test command with an integer value
    SetterCommand CreateTestCommand(const string& element, const string& property, int value) {
        SetterCommand cmd;
        cmd.set_element_name(element);
        cmd.set_property_name(property);
        cmd.set_value_type(ValueType::INT);

        // Serialize an int value
        string valueData;
        valueData.assign(reinterpret_cast<const char*>(&value), sizeof(value));
        cmd.set_value_data(valueData);

        return cmd;
    }

    // Test objects
    shared_ptr<MockTransportReqRspClientSocket> mockClientSocket;
    shared_ptr<MockTransportReqRspSrvSocket> mockServerSocket;
    unique_ptr<ProtoReqRspClientHandler> clientHandler;
    unique_ptr<ProtoReqRspSrvHandler> serverHandler;

    // Command reception tracking
    std::mutex mtx;
    condition_variable cv;
    bool commandReceived = false;
    SetterCommand lastReceivedCommand;
};

// Additional test for multiple command execution
TEST_F(ReqRspIntegrationTest, MultipleCommandExecutionTest) {
    // Set expectations for multiple calls
    EXPECT_CALL(*mockServerSocket, Initialize(_, _, _, _, _)).Times(1);
    EXPECT_CALL(*mockClientSocket, Send(_, _, _, _)).Times(3);
    EXPECT_CALL(*mockClientSocket, Start()).Times(1);
    EXPECT_CALL(*mockServerSocket, Start(_)).Times(1);

    serverHandler->Start("test-url");

    // Send multiple commands with different values
    vector<int> testValues = { 42, 100, 255 };
    

    for (int value : testValues) {
        commandReceived = false; // Reset for each command
        SetterCommand cmd = CreateTestCommand("Element", "Property" + to_string(value), value);

        // Send command and capture response
        clientHandler->Send<SetterCommand>(cmd);
        

        // Wait for each command to be processed
        WaitForCommand();

        // Verify each command was processed correctly
        EXPECT_EQ(lastReceivedCommand.element_name(), cmd.element_name());
        EXPECT_EQ(lastReceivedCommand.property_name(), cmd.property_name());

        int receivedValue;
        memcpy(&receivedValue, lastReceivedCommand.value_data().data(), sizeof(int));
        EXPECT_EQ(receivedValue, value);

    }

}

TEST_F(ReqRspIntegrationTest, SendCommandIsProcessedByServer) {
    // Set expectations for the socket initialization and start methods
    EXPECT_CALL(*mockServerSocket, Initialize(_, _, _, _, _)).Times(1);
    EXPECT_CALL(*mockClientSocket, Send(_, _, _, _)).Times(1);
    EXPECT_CALL(*mockClientSocket, Start()).Times(1);
    EXPECT_CALL(*mockServerSocket, Start(_)).Times(1);

    // Start the server handler
    serverHandler->Start("test-url");

    // Create test command with helper method
    int testValue = 42;
    SetterCommand cmd = CreateTestCommand("TestElement", "TestProperty", testValue);

    // Send the command
    clientHandler->Send<SetterCommand>(cmd);

    // Wait for the command to be processed
    WaitForCommand();

    // Verify the received command matches what was sent
    EXPECT_EQ(lastReceivedCommand.element_name(), cmd.element_name());
    EXPECT_EQ(lastReceivedCommand.property_name(), cmd.property_name());
    EXPECT_EQ(lastReceivedCommand.value_type(), cmd.value_type());

    // Verify the value data
    ASSERT_EQ(lastReceivedCommand.value_data().size(), sizeof(int));
    int receivedValue;
    memcpy(&receivedValue, lastReceivedCommand.value_data().data(), sizeof(int));
    EXPECT_EQ(receivedValue, testValue);

}

TEST_F(ReqRspIntegrationTest, ErrorHandlingTest) {
    // Configure error-throwing handler
    SetupErrorThrowingCommandHandler();

    // Set expectations for the socket initialization and start methods
    EXPECT_CALL(*mockServerSocket, Initialize(_, _, _, _, _)).Times(1);
    EXPECT_CALL(*mockClientSocket, Send(_, _, _, _)).Times(1);
    EXPECT_CALL(*mockClientSocket, Start()).Times(1);
    EXPECT_CALL(*mockServerSocket, Start(_)).Times(1);

    serverHandler->Start("test-url");

    // Create test command
    SetterCommand cmd = CreateTestCommand("TestElement", "TestProperty", 42);

    // Expect the Send call to throw FaultException with correct error code
    try {
        CommandResponse response = clientHandler->Send<SetterCommand, CommandResponse>(cmd);
        FAIL() << "Expected FaultException to be thrown";
    }
    catch (const FaultException& e) {
        EXPECT_EQ(e.ErrorCode(), 400);
        EXPECT_STREQ(e.what(), "Test error");
    }
    catch (...) {
        FAIL() << "Expected FaultException, got different exception";
    }
}

