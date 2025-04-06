#include "../../include/plumberd.hpp"
#include "interfaces.hpp"
#include "messages.pb.h"
#include "contract.h"

using namespace cppplumberd;
namespace app {
	
	
	class ElementRegistry : public IElementRegistry {

	};
	class ElementInfo : public IElementInfo {

	};

	class PropertyInfo : public IPropertyInfo {
	};
	class SetterCommandHandler : cppplumberd::ICommandHandler<SetterCommand> {
		inline void cppplumberd::ICommandHandler<SetterCommand>::Handle(const string& stream_id, const SetterCommand& cmd) override
		{
			
		}
	};
	class StartReactiveSubscriptionHandler : cppplumberd::ICommandHandler<StartReactiveSubscriptionCommand>
	{
		inline void cppplumberd::ICommandHandler<SetterCommand>::Handle(const string& stream_id, const StartReactiveSubscriptionCommand& cmd) override
		{
			
		}
		
		shared_ptr< EventStore> _eventStore;
	};
	class ReactiveSubscriptionManager
	{
	public:
		
		set<string> GetReactiveSubscriptionStreams(const string &elementName, const string& propertyName)
		{
			set<string> streams;
			return streams;
		}
		void AddReactiveSubscription(const string& streamName, const string& elementName, const string& propertyName)
		{
			// Add subscription logic here
			auto prop = _elementRegistry->GetElement(elementName)->GetProperty(propertyName);
			if (! IsSubscribed(*prop))
			{
				SubscribeOnPropertyChanged(*prop);
			}
			//TODO: Add the stream to the subscription map
			
		}
	private:
		void PublishPropertyChanges(const IPropertyInfo& property, int value) const
		{
			const string gsId = "gstreamer";
			PropertyChangedEvent evt;
			evt.set_property_name(property.GetElementInfo()->GetName());
			evt.set_element_name(property.GetName());

			_eventStore->Publish<PropertyChangedEvent>(gsId, evt);
		}
		void SubscribeOnPropertyChanged(const IPropertyInfo& property)
		{
			
		}
		bool IsSubscribed(const IPropertyInfo& property) const
		{
			return false;
		}
		shared_ptr<EventStore> _eventStore;
		shared_ptr<IElementRegistry> _elementRegistry;

	};
	class CreateReactiveSubscriptionHandler : public ICommandHandler<CreateReactiveSubscriptionCommand>
	{
		void cppplumberd::ICommandHandler<SetterCommand>::Handle(const string& stream_id, const CreateReactiveSubscriptionCommand& cmd) override
		{
			// we create new stream.
		}
	};


	class ReactiveSubscriptionEventHandler : public IEventHandler<PropertyChangedEvent>
	{
	public:
		void cppplumberd::IEventHandler<PropertyChangedEvent>::Handle(const Metadata& metadata, const PropertyChangedEvent& evt) override
		{
			auto streams = _map->GetReactiveSubscriptionStreams(evt.element_name(), evt.property_name());
			for (const auto& stream : streams)
			{
				_eventStore->Publish<PropertyChangedEvent>(stream, evt);
			}
		}
	private:
		shared_ptr<ReactiveSubscriptionManager> _map;
		shared_ptr<EventStore> _eventStore;
	};
	
}
void main() {

	auto socketFactory = make_shared<NngSocketFactory>();
	auto plumber = cppplumberd::Plumber::CreateServer(socketFactory, "");
	plumber->AddCommandHandler<app::SetterCommandHandler, app::SetterCommand, app::COMMANDS::SETTER>();
	plumber->AddCommandHandler<app::StartReactiveSubscriptionHandler, app::StartReactiveSubscriptionCommand, app::COMMANDS::START_REACTIVE_SUBSCRIPTION>();
	plumber->AddCommandHandler<app::CreateReactiveSubscriptionHandler, app::CreateReactiveSubscriptionCommand, app::COMMANDS::CREATE_REACTIVE_SUBSCRIPTION>();
	plumber->AddEventHandler<app::ReactiveSubscriptionEventHandler, app::PropertyChangedEvent, app::EVENTS::PROPERTY_CHANGED>();


}