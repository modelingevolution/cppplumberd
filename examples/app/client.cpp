#include "interfaces.hpp"
#include "example.pb.h"
#include "../../include/plumberd.hpp"
#include "contract.h"
#include <iostream>

using namespace std;

namespace app {
	using ValueType = example::ValueType;
	using SetterCommand = example::SetterCommand;
	using CreateReactiveSubscriptionCommand = example::CreateReactiveSubscription;
	using StartReactiveSubscriptionCommand = example::StartReactiveSubscription;
	using PropertySelector = example::PropertySelector;
	using PropertyChangedEvent = example::PropertyChangedEvent;

	class ReactivePropertyViewModel : public cppplumberd::EventHandlerBase,
		public cppplumberd::IEventHandler<app::PropertyChangedEvent>
	{
	public:
		ReactivePropertyViewModel()
		{
			this->Map<app::PropertyChangedEvent, EVENTS::PROPERTY_CHANGED>();
		}

	private:
		void Handle(const cppplumberd::Metadata& metadata, const app::PropertyChangedEvent& evt) override
		{
			// Handle the property change event
			cout << "Property changed: " << evt.property_name() << " to " << evt.value_data().size() << " bytes" << endl;
		}
	};
}

int main() {
	auto socketFactory = make_shared<cppplumberd::NggSocketFactory>();
	auto plumber = cppplumberd::PlumberClient::CreateClient(socketFactory, "");

	app::CreateReactiveSubscriptionCommand cmd;
	cmd.set_name("Foo");
	plumber->CommandBus()->Send<app::CreateReactiveSubscriptionCommand>("foo",cmd);

	auto vm = make_shared< app::ReactivePropertyViewModel>();
	auto subscription = plumber->SubscriptionManager()->Subscribe("Foo", vm);

	// Keep the program running
	cout << "Client running. Press Enter to exit..." << endl;
	cin.get();

	return 0;
}