#include "interfaces.hpp"
#include "messages.pb.h"
#include "../../include/plumberd.hpp"
#include "contract.h"
namespace app {
	class ReactivePropertyViewModel : public cppplumberd::EventHandlerBase,
	                                  cppplumberd::IEventHandler<app::PropertyChangedEvent>
	{
	public:
		ReactivePropertyViewModel()
		{
			this->Map<app::PropertyChangedEvent, EVENTS::PROPERTY_CHANGED>();
		}

	private:
		void cppplumberd::IEventHandler<app::PropertyChangedEvent>::Handle(const cppplumberd::Metadata& metadata, const app::PropertyChangedEvent& evt) override
		{
			// Handle the property change event
			cout << "Property changed: " << evt.property_name() << " to " << evt.value_data() << endl;
		}
	};
}
void main() {
	auto socketFactory = make_shared<cppplumberd::NngSocketFactory>();
	auto plumber = cppplumberd::Plumber::CreateClient(socketFactory, "");

	app::CreateReactiveSubscriptionCommand cmd;
	cmd.set_name("Foo");
	plumber->CommandBus()->Send<app::CreateReactiveSubscriptionCommand>(cmd);

	app::ReactivePropertyViewModel vm;
	plumber->SubscriptionManager()->Subscribe("Foo", vm);
}