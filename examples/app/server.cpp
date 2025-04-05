#include "../../include/plumberd.hpp"
#include "interfaces.hpp"
#include "messages.pb.h"
namespace app {
	using namespace cppplumberd;
	enum COMMANDS : unsigned int {
		SETTER = 1,
		
	};
	class ElementRegistry : public IElementRegistry {

	};
	class ElementInfo : public IElementInfo {

	};

	class PropertyInfo : public IPropertyInfo {
	};

	void main() {

		auto socketFactory = make_shared<NngSocketFactory>();
		Host host(socketFactory);
		host.AddCommandHandler<SetterCommand, COMMANDS::SETTER>();

	}
}