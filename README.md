# cppplumberd

A modern C++ library implementing **CQRS (Command Query Responsibility Segregation)** pattern with event-driven architecture, built on top of [NNG (nanomsg-next-gen)](https://github.com/nanomsg/nng) for high-performance messaging.

[![Version](https://img.shields.io/badge/version-0.1.0-blue.svg)](https://github.com/modelingevolution/streamer)
[![License](https://img.shields.io/badge/license-MIT-green.svg)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://isocpp.org/)

## 🎯 Overview

**cppplumberd** provides a robust foundation for building distributed, event-driven applications using the CQRS pattern. It separates command handling (write operations) from event subscriptions (read operations), enabling scalable, maintainable architectures with clear separation of concerns.

### Key Features

- **🏗️ CQRS Architecture**: Clean separation between commands and events
- **📡 Real-time Messaging**: Built on NNG for high-performance, low-latency communication  
- **🔄 Event Sourcing**: Built-in event store for reliable event persistence and replay
- **🌐 Network Transparency**: Client-server communication over TCP/IPC
- **🧵 Thread-Safe**: Concurrent command processing and event handling
- **📋 Protocol Buffers**: Efficient binary serialization for messages
- **🎭 Type-Safe**: Template-based APIs for compile-time type checking
- **🔌 Reactive Subscriptions**: Subscribe to specific event streams with filtering

## 🏛️ Architecture

```
┌─────────────────┐    Commands     ┌─────────────────┐
│                 │ ──────────────→ │                 │
│  PlumberClient  │                 │    Plumber      │
│                 │ ←────────────── │   (Server)      │
└─────────────────┘    Events       └─────────────────┘
         │                                   │
         │                                   │
    ┌────▼────┐                         ┌────▼────┐
    │ Command │                         │ Command │
    │   Bus   │                         │Handlers │
    └─────────┘                         └─────────┘
         │                                   │
    ┌────▼────┐                         ┌────▼────┐
    │Event    │                         │Event    │
    │Subscr.  │                         │ Store   │
    └─────────┘                         └─────────┘
```

### Core Components

#### 🎯 **Command Side (Write)**
- **Command Bus**: Routes commands to appropriate handlers
- **Command Handlers**: Process business logic and generate events
- **Command Validation**: Type-safe command processing

#### 📊 **Query Side (Read)** 
- **Event Store**: Persists and replays events
- **Event Subscriptions**: Real-time event streaming
- **Event Handlers**: Process events for read model updates

#### 🌐 **Transport Layer**
- **NNG Sockets**: REQ/REP for commands, PUB/SUB for events
- **Protocol Buffers**: Efficient message serialization
- **Socket Factory**: Abstract socket creation for testability

## 🚀 Quick Start

### Prerequisites

- **C++20** compatible compiler (GCC 11+, Clang 12+, MSVC 2022+)
- **CMake 3.20+**
- **vcpkg** package manager
- **NNG** and **nngpp** libraries
- **Protocol Buffers**
- **Boost.Signals2**

### Installation

#### Using vcpkg

```bash
# Install dependencies
vcpkg install nng:x64-linux nngpp:x64-linux protobuf:x64-linux boost-signals2:x64-linux

# Build cppplumberd
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake ..
make -j$(nproc)
```

#### CMake Integration

```cmake
find_package(cppplumberd REQUIRED)
target_link_libraries(your_target PRIVATE cppplumberd::cppplumberd)
```

### Basic Usage

#### 1. Define Your Messages (Protocol Buffers)

```protobuf
// messages.proto
syntax = "proto3";

message SetPropertyCommand {
    string element_name = 1;
    string property_name = 2;
    bytes value_data = 3;
}

message PropertyChangedEvent {
    string element_name = 1;
    string property_name = 2;
    bytes value_data = 3;
    uint64 timestamp = 4;
}
```

#### 2. Server Setup

```cpp
#include <plumberd.hpp>
#include "messages.pb.h"

using namespace cppplumberd;

// Command Handler
class PropertyCommandHandler : public ICommandHandler<SetPropertyCommand> {
public:
    void Handle(const std::string& stream_id, const SetPropertyCommand& cmd) override {
        // Process the command
        std::cout << "Setting property: " << cmd.property_name() 
                  << " on element: " << cmd.element_name() << std::endl;
        
        // Publish resulting event
        PropertyChangedEvent event;
        event.set_element_name(cmd.element_name());
        event.set_property_name(cmd.property_name());
        event.set_value_data(cmd.value_data());
        event.set_timestamp(std::chrono::system_clock::now().time_since_epoch().count());
        
        // Event store will handle publishing
        eventStore->AppendEvent(stream_id, event);
    }
    
private:
    std::shared_ptr<EventStore> eventStore;
};

int main() {
    // Create server
    auto socketFactory = std::make_shared<NggSocketFactory>();
    auto server = Plumber::CreateServer(socketFactory, "tcp://localhost:5555");
    
    // Register message types
    server->RegisterMessage<SetPropertyCommand, 100>();
    server->RegisterMessage<PropertyChangedEvent, 200>();
    
    // Register command handler
    server->AddCommandHandler<PropertyCommandHandler, SetPropertyCommand, 100>();
    
    // Start server
    server->Start();
    
    std::cout << "Server running on tcp://localhost:5555" << std::endl;
    std::cin.get(); // Wait for input
    
    server->Stop();
    return 0;
}
```

#### 3. Client Setup

```cpp
#include <plumberd.hpp>
#include "messages.pb.h"

using namespace cppplumberd;

// Event Handler
class PropertyEventHandler : public EventHandlerBase,
                            public IEventHandler<PropertyChangedEvent> {
public:
    PropertyEventHandler() {
        this->Map<PropertyChangedEvent, 200>();
    }
    
private:
    void Handle(const Metadata& metadata, const PropertyChangedEvent& evt) override {
        std::cout << "Property changed event received: " 
                  << evt.property_name() << " = " << evt.value_data().size() 
                  << " bytes" << std::endl;
    }
};

int main() {
    // Create client
    auto socketFactory = std::make_shared<NggSocketFactory>();
    auto client = PlumberClient::CreateClient(socketFactory, "tcp://localhost:5555");
    
    // Register message types
    client->RegisterMessage<SetPropertyCommand, 100>();
    client->RegisterMessage<PropertyChangedEvent, 200>();
    
    // Start client
    client->Start();
    
    // Send commands
    SetPropertyCommand cmd;
    cmd.set_element_name("videosrc");
    cmd.set_property_name("bitrate");
    cmd.set_value_data("1000000");
    
    client->CommandBus()->Send("element-stream", cmd);
    
    // Subscribe to events
    auto eventHandler = std::make_shared<PropertyEventHandler>();
    auto subscription = client->SubscriptionManager()->Subscribe("element-stream", eventHandler);
    
    std::cout << "Client running. Press Enter to exit..." << std::endl;
    std::cin.get();
    
    client->Stop();
    return 0;
}
```

## 📚 Advanced Features

### Stream-Based Event Organization

Events are organized into **streams** - logical groupings of related events:

```cpp
// Different streams for different contexts
client->CommandBus()->Send("user-123", userCommand);      // User domain
client->CommandBus()->Send("order-456", orderCommand);    // Order domain
client->CommandBus()->Send("inventory", stockCommand);    // Inventory domain
```

### Reactive Subscriptions

Subscribe to specific event streams with filtering:

```cpp
// Subscribe to specific stream
auto subscription = client->SubscriptionManager()->Subscribe("user-events", handler);

// Multiple subscriptions
auto userSub = client->SubscriptionManager()->Subscribe("user-events", userHandler);
auto orderSub = client->SubscriptionManager()->Subscribe("order-events", orderHandler);

// Unsubscribe when done
subscription->Unsubscribe();
```

### Event Store Operations

```cpp
auto eventStore = server->GetEventStore();

// Append events to stream
eventStore->AppendEvent("stream-1", myEvent);

// Read events from stream
auto events = eventStore->ReadEvents("stream-1", 0, 100);

// Stream management
eventStore->EnsureStreamCreated("new-stream");
```

### Error Handling

```cpp
class MyCommandHandler : public ICommandHandler<MyCommand> {
public:
    void Handle(const std::string& stream_id, const MyCommand& cmd) override {
        try {
            // Business logic
            processCommand(cmd);
        }
        catch (const BusinessLogicException& e) {
            // Handle business rule violations
            throw FaultException(400, e.what());
        }
        catch (const std::exception& e) {
            // Handle unexpected errors
            throw FaultException(500, "Internal server error");
        }
    }
};
```

## 🔧 Configuration

### Socket Factory Configuration

```cpp
class CustomSocketFactory : public ISocketFactory {
public:
    std::unique_ptr<ITransportReqRspSrvSocket> CreateReqRspSrvSocket(const std::string& endpoint) override {
        auto socket = std::make_unique<NngReqRspSrvSocket>();
        socket->SetTimeout(5000);  // 5 second timeout
        socket->SetBufferSize(1024 * 1024);  // 1MB buffer
        return socket;
    }
    // ... implement other methods
};

auto customFactory = std::make_shared<CustomSocketFactory>();
auto server = Plumber::CreateServer(customFactory, "tcp://0.0.0.0:5555");
```

### Message Registration

```cpp
// Register messages with unique IDs
constexpr uint32_t SET_PROPERTY_CMD = 100;
constexpr uint32_t PROPERTY_CHANGED_EVT = 200;
constexpr uint32_t CREATE_STREAM_CMD = 300;

server->RegisterMessage<SetPropertyCommand, SET_PROPERTY_CMD>();
server->RegisterMessage<PropertyChangedEvent, PROPERTY_CHANGED_EVT>();
server->RegisterMessage<CreateStreamCommand, CREATE_STREAM_CMD>();
```

## 🧪 Testing

### Unit Testing Support

```cpp
#include <gtest/gtest.h>
#include <gmock/gmock.h>

class MockSocketFactory : public ISocketFactory {
    MOCK_METHOD(std::unique_ptr<ITransportReqRspSrvSocket>, CreateReqRspSrvSocket, 
                (const std::string&), (override));
    // ... other mock methods
};

TEST(PlumberTest, CommandHandling) {
    auto mockFactory = std::make_shared<MockSocketFactory>();
    auto server = Plumber::CreateServer(mockFactory);
    
    // Set up expectations
    EXPECT_CALL(*mockFactory, CreateReqRspSrvSocket(_))
        .WillOnce(Return(std::make_unique<MockReqRspSocket>()));
    
    // Test command handling
    // ...
}
```

### Integration Testing

```cpp
TEST(PlumberIntegrationTest, EndToEndCommandFlow) {
    // Start server
    auto server = CreateTestServer();
    server->Start();
    
    // Connect client
    auto client = CreateTestClient();
    client->Start();
    
    // Send command and verify response
    TestCommand cmd;
    cmd.set_data("test-data");
    
    auto response = client->CommandBus()->Send("test-stream", cmd);
    EXPECT_EQ(response.status_code(), 0);
    
    // Verify event was published
    auto eventHandler = std::make_shared<TestEventHandler>();
    auto subscription = client->SubscriptionManager()->Subscribe("test-stream", eventHandler);
    
    EXPECT_TRUE(eventHandler->WaitForEvent(1000)); // Wait 1 second
    EXPECT_EQ(eventHandler->GetReceivedEvent().data(), "test-data");
}
```

## 🚀 Performance Considerations

### Throughput Optimization

- **Connection Pooling**: Reuse connections for multiple commands
- **Batch Processing**: Send multiple commands in batches
- **Async Operations**: Use non-blocking I/O where possible

```cpp
// Batch command sending
std::vector<SetPropertyCommand> commands = {cmd1, cmd2, cmd3};
for (const auto& cmd : commands) {
    client->CommandBus()->Send("batch-stream", cmd);
}
```

### Memory Management

- **Smart Pointers**: Automatic memory management
- **Buffer Pooling**: Reuse message buffers
- **Stream Cleanup**: Properly close subscriptions

```cpp
// RAII pattern for subscriptions
class SubscriptionManager {
    std::vector<std::unique_ptr<ISubscription>> subscriptions_;
    
public:
    ~SubscriptionManager() {
        for (auto& sub : subscriptions_) {
            sub->Unsubscribe();  // Automatic cleanup
        }
    }
};
```

## 🛠️ Integration Examples

### GStreamer Integration

This library is designed to work seamlessly with GStreamer pipelines:

```cpp
// GStreamer element property control
class GStreamerPropertyHandler : public ICommandHandler<SetPropertyCommand> {
public:
    void Handle(const std::string& stream_id, const SetPropertyCommand& cmd) override {
        // Find GStreamer element
        GstElement* element = gst_bin_get_by_name(GST_BIN(pipeline), 
                                                  cmd.element_name().c_str());
        if (!element) {
            throw FaultException(404, "Element not found");
        }
        
        // Set property
        g_object_set_property(G_OBJECT(element), 
                            cmd.property_name().c_str(), 
                            &gvalue);
        
        // Publish property changed event
        PropertyChangedEvent event;
        event.set_element_name(cmd.element_name());
        event.set_property_name(cmd.property_name());
        eventStore->AppendEvent(stream_id, event);
        
        gst_object_unref(element);
    }
};
```

## 📈 Monitoring & Debugging

### Built-in Diagnostics

```cpp
// Enable debug logging
#define CPPPLUMBERD_DEBUG 1

// Performance metrics
auto stats = client->GetStatistics();
std::cout << "Commands sent: " << stats.commands_sent << std::endl;
std::cout << "Events received: " << stats.events_received << std::endl;
std::cout << "Average latency: " << stats.avg_latency_ms << "ms" << std::endl;
```

### Tracing Support

```cpp
class TracingCommandHandler : public ICommandHandler<MyCommand> {
public:
    void Handle(const std::string& stream_id, const MyCommand& cmd) override {
        auto start = std::chrono::high_resolution_clock::now();
        
        try {
            // Process command
            processCommand(cmd);
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            
            logger->info("Command processed in {}μs", duration.count());
        }
        catch (const std::exception& e) {
            logger->error("Command failed: {}", e.what());
            throw;
        }
    }
};
```

## 🤝 Contributing

We welcome contributions! Please see our [Contributing Guidelines](CONTRIBUTING.md) for details.

### Development Setup

```bash
# Clone with submodules
git clone --recursive https://github.com/modelingevolution/streamer.git
cd streamer/src/cppplumberd

# Install dependencies via vcpkg
vcpkg install --triplet=x64-linux nng nngpp protobuf boost-signals2

# Build with tests
mkdir build && cd build
cmake -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake \
      -DCPPPLUMBERD_BUILD_TESTS=ON ..
make -j$(nproc)

# Run tests
ctest -V
```

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## 🙏 Acknowledgments

- **NNG**: High-performance messaging library
- **Protocol Buffers**: Efficient serialization
- **Boost.Signals2**: Type-safe callbacks
- **GoogleTest**: Testing framework

---

**Built with ❤️ by [ModelingEvolution](https://github.com/modelingevolution)**