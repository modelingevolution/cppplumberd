#include "../../include/plumberd.hpp"
#include "interfaces.hpp"
#include "example.pb.h"
#include "contract.h"
#include <iostream>
#include <mutex>
#include <memory>
#include <string>
#include <map>
#include <set>

using namespace cppplumberd;
using namespace std;

namespace app {

    using ValueType = example::ValueType;
    using SetterCommand = example::SetterCommand;
    using CreateReactiveSubscriptionCommand = example::CreateReactiveSubscription;
    using StartReactiveSubscriptionCommand = example::StartReactiveSubscription;
    using PropertySelector = example::PropertySelector;
    using PropertyChangedEvent = example::PropertyChangedEvent;

    // PropertyInfo implementation with actual data
    class PropertyInfoImpl : public IPropertyInfo {
    private:
        string _name;
        shared_ptr<int> _value;
        IElementInfo* _parent;
        bool _readable;
        bool _writable;

        // Signal for property changes
        signal<void(IPropertyInfo&, int)> _valueChanged;

    public:
        PropertyInfoImpl(const string& name, IElementInfo* parent, bool readable = true, bool writable = true)
            : _name(name), _parent(parent), _readable(readable), _writable(writable) {
            _value = make_shared<int>(0);
        }

        string GetName() const override {
            return _name;
        }

        bool IsReadable() const override {
            return _readable;
        }

        bool IsWritable() const override {
            return _writable;
        }

        shared_ptr<int> GetValue() const override {
            return _value;
        }

        void SetValue(const int& value) override {
            if (!IsWritable()) {
                throw runtime_error("Property is not writable");
            }

            int oldValue = *_value;
            *_value = value;

            if (oldValue != value) {
                // Notify listeners of the change
                _valueChanged(*this, value);
            }
        }

        IElementInfo* GetElementInfo() const override {
            return _parent;
        }

        // Connect to property change signal
        connection ConnectValueChanged(const function<void(IPropertyInfo&, int)>& handler) {
            return _valueChanged.connect(handler);
        }
    };

    // ElementInfo implementation with actual data
    class ElementInfoImpl : public IElementInfo {
    private:
        string _name;
        map<string, shared_ptr<IPropertyInfo>> _properties;

    public:
        ElementInfoImpl(const string& name) : _name(name) {}

        string GetName() const override {
            return _name;
        }

        vector<shared_ptr<IPropertyInfo>> GetProperties() const override {
            vector<shared_ptr<IPropertyInfo>> result;
            for (const auto& pair : _properties) {
                result.push_back(pair.second);
            }
            return result;
        }

        shared_ptr<IPropertyInfo> GetProperty(const string& name) const override {
            auto it = _properties.find(name);
            if (it != _properties.end()) {
                return it->second;
            }
            return nullptr;
        }

        bool HasProperty(const string& name) const override {
            return _properties.find(name) != _properties.end();
        }

        // Create and add a property
        shared_ptr<PropertyInfoImpl> AddProperty(const string& name, bool readable = true, bool writable = true) {
            auto property = make_shared<PropertyInfoImpl>(name, this, readable, writable);
            _properties[name] = property;
            return property;
        }
    };

    // ElementRegistry implementation with actual elements
    class ElementRegistryImpl : public IElementRegistry {
    private:
        map<string, shared_ptr<ElementInfoImpl>> _elements;

    public:
        shared_ptr<IElementInfo> GetElement(const string& element_name) override {
            auto it = _elements.find(element_name);
            if (it != _elements.end()) {
                return it->second;
            }
            return nullptr;
        }

        vector<shared_ptr<IElementInfo>> GetAllElements() override {
            vector<shared_ptr<IElementInfo>> result;
            for (const auto& pair : _elements) {
                result.push_back(pair.second);
            }
            return result;
        }

        // Create and add an element
        shared_ptr<ElementInfoImpl> AddElement(const string& name) {
            auto element = make_shared<ElementInfoImpl>(name);
            _elements[name] = element;
            return element;
        }
    };

    // Handler for setting properties
    class SetterCommandHandler : public ICommandHandler<SetterCommand> {
    private:
        shared_ptr<IElementRegistry> _registry;

    public:
        SetterCommandHandler(shared_ptr<IElementRegistry> registry)
            : _registry(registry) {
        }

        void Handle(const string& stream_id, const SetterCommand& cmd) override {
            cout << "Setting property: " << cmd.element_name() << "." << cmd.property_name() << endl;

            auto element = _registry->GetElement(cmd.element_name());
            if (!element) {
                throw runtime_error("Element not found: " + cmd.element_name());
            }

            auto property = element->GetProperty(cmd.property_name());
            if (!property) {
                throw runtime_error("Property not found: " + cmd.property_name());
            }

            if (cmd.value_type() == ValueType::INT) {
                // Deserialize int value
                int value = 0;
                if (cmd.value_data().size() >= sizeof(int)) {
                    memcpy(&value, cmd.value_data().data(), sizeof(int));
                    cout << "Setting value to: " << value << endl;
                    property->SetValue(value);
                }
                else {
                    throw runtime_error("Invalid value data size");
                }
            }
            else {
                throw runtime_error("Unsupported value type");
            }
        }
    };

    // Property monitor that publishes property changes
    class PropertyMonitorService {
    private:
        shared_ptr<EventStore> _eventStore;
        map<string, map<string, connection>> _connections;  // element -> property -> connection

    public:
        PropertyMonitorService(shared_ptr<EventStore> eventStore)
            : _eventStore(eventStore) {
        }

        void MonitorProperty(shared_ptr<PropertyInfoImpl> property) {
            string elementName = property->GetElementInfo()->GetName();
            string propertyName = property->GetName();

            // Check if already monitoring
            if (_connections.count(elementName) && _connections[elementName].count(propertyName)) {
                return;
            }

            // Connect to property changes
            connection conn = property->ConnectValueChanged(
                [this, elementName, propertyName](IPropertyInfo& prop, int newValue) {
                    this->OnPropertyChanged(elementName, propertyName, newValue);
                });

            // Store the connection
            _connections[elementName][propertyName] = conn;

            cout << "Now monitoring: " << elementName << "." << propertyName << endl;
        }

        void OnPropertyChanged(const string& elementName, const string& propertyName, int newValue) {
            cout << "Property changed: " << elementName << "." << propertyName << " = " << newValue << endl;

            // Create event
            PropertyChangedEvent evt;
            evt.set_element_name(elementName);
            evt.set_property_name(propertyName);
            evt.set_value_type(ValueType::INT);

            // Serialize the value
            string valueData;
            valueData.resize(sizeof(int));
            memcpy(&valueData[0], &newValue, sizeof(int));
            evt.set_value_data(valueData);

            // Publish to all reactive subscriptions
            for (const auto& streamName : _reactiveSubscriptions) {
                _eventStore->Publish(streamName, evt);
            }
        }

        // Add a stream to send property changes to
        void AddReactiveSubscription(const string& streamName) {
            _reactiveSubscriptions.insert(streamName);
        }

    private:
        set<string> _reactiveSubscriptions;
    };

    // Handler for creating reactive subscriptions
    class CreateReactiveSubscriptionHandler : public ICommandHandler<CreateReactiveSubscriptionCommand> {
    private:
        shared_ptr<EventStore> _eventStore;
        shared_ptr<PropertyMonitorService> _propertyMonitor;
        shared_ptr<IElementRegistry> _registry;

    public:
        CreateReactiveSubscriptionHandler(
            shared_ptr<EventStore> eventStore,
            shared_ptr<PropertyMonitorService> propertyMonitor,
            shared_ptr<IElementRegistry> registry)
            : _eventStore(eventStore),
            _propertyMonitor(propertyMonitor),
            _registry(registry) {
        }

        void Handle(const string& stream_id, const CreateReactiveSubscriptionCommand& cmd) override {
            cout << "Creating reactive subscription: " << cmd.name() << endl;

            // Create event stream
            _eventStore->CreateStream(cmd.name());

            // Add to reactive subscriptions
            _propertyMonitor->AddReactiveSubscription(cmd.name());

            // Set up property monitoring for all requested properties
            for (const auto& propSelector : cmd.properties()) {
                auto element = _registry->GetElement(propSelector.element_name());
                if (!element) {
                    cerr << "Element not found: " << propSelector.element_name() << endl;
                    continue;
                }

                auto property = element->GetProperty(propSelector.property_name());
                if (!property) {
                    cerr << "Property not found: " << propSelector.property_name() << endl;
                    continue;
                }

                // Cast to our implementation type since we're using the derived class
                auto elementImpl = dynamic_pointer_cast<ElementInfoImpl>(_registry->GetElement(propSelector.element_name()));
                if (!elementImpl) {
                    cerr << "Element is not of expected implementation type: " << propSelector.element_name() << endl;
                    continue;
                }

                auto propertyImpl = dynamic_pointer_cast<PropertyInfoImpl>(property);
                if (!propertyImpl) {
                    cerr << "Property is not of expected implementation type: " << propSelector.property_name() << endl;
                    continue;
                }

                _propertyMonitor->MonitorProperty(propertyImpl);
            }
        }
    };

    // Handler for starting reactive subscriptions
    class StartReactiveSubscriptionHandler : public ICommandHandler<StartReactiveSubscriptionCommand> {
    private:
        shared_ptr<EventStore> _eventStore;

    public:
        StartReactiveSubscriptionHandler(shared_ptr<EventStore> eventStore)
            : _eventStore(eventStore) {
        }

        void Handle(const string& stream_id, const StartReactiveSubscriptionCommand& cmd) override {
            cout << "Starting reactive subscription: " << cmd.name() << endl;

            
            //_eventStore->CreateStream(cmd.name());
        }
    };
}

int main() {
    cout << "Starting app-server..." << endl;

    // Create socket factory
    auto socketFactory = make_shared<NggSocketFactory>();

    // Create plumber server
    auto plumber = cppplumberd::Plumber::CreateServer(socketFactory);

    // Create element registry with fake elements
    auto registry = make_shared<app::ElementRegistryImpl>();

    // Create fakevideosrc element with bufnumber property
    auto srcElement = registry->AddElement("fakevideosrc");
    auto bufNumberProp = srcElement->AddProperty("num-buffers", true, true);
    bufNumberProp->SetValue(100);  // Initial value

    // Create fakevideosink element with processed property
    auto sinkElement = registry->AddElement("fakevideosink");
    auto processedProp = sinkElement->AddProperty("processed", true, true);
    processedProp->SetValue(0);  // Initial value

    // Create property monitor service
    auto propertyMonitor = make_shared<app::PropertyMonitorService>(plumber->GetEventStore());

    // Set up command handlers
    plumber->AddCommandHandler<app::SetterCommandHandler, app::SetterCommand, app::COMMANDS::SETTER>(registry);
    plumber->AddCommandHandler<app::CreateReactiveSubscriptionHandler, app::CreateReactiveSubscriptionCommand, app::COMMANDS::CREATE_REACTIVE_SUBSCRIPTION>(
        plumber->GetEventStore(), propertyMonitor, registry);
    plumber->AddCommandHandler<app::StartReactiveSubscriptionHandler, app::StartReactiveSubscriptionCommand, app::COMMANDS::START_REACTIVE_SUBSCRIPTION>(
        plumber->GetEventStore());

    // Register message types
    plumber->RegisterMessage<app::PropertyChangedEvent, app::EVENTS::PROPERTY_CHANGED>();

    // Start the server
    plumber->Start();

    cout << "Server started. Press Enter to exit..." << endl;
    cin.get();

    // Stop the server
    plumber->Stop();

    cout << "Server stopped." << endl;
    return 0;
}