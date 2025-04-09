#pragma once

namespace app {

	enum COMMANDS : unsigned int {
		SETTER = 0xFF + 1,
		CREATE_REACTIVE_SUBSCRIPTION = 0xFF + 2,
		START_REACTIVE_SUBSCRIPTION = 0xFF + 3,
	};
	enum EVENTS : unsigned int {
		PROPERTY_CHANGED = 0xFFFF + 1,
	};
}