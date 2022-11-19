#include "AmbiLights.hpp"

#include <iostream>


int main() {
	bool success;
	AmbiLights ambiLights;

	success = ambiLights.init();
	if (!success) {
		std::cerr << "Failed to initialize AmbiLights\n";
		std::cerr << " Reason: " << ambiLights.getError() << "\n";
	}

	ambiLights.setBrightness(255);
	ambiLights.run();
	return 0;
}
