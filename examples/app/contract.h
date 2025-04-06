#pragma once

namespace app {

	enum COMMANDS : unsigned int {
		SETTER = 1,
		CREATE_REACTIVE_SUBSCRIPTION = 2,
		START_REACTIVE_SUBSCRIPTION = 3,
	};
	enum EVENTS : unsigned int {
		PROPERTY_CHANGED = 1,
	};
}