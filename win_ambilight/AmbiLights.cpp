#include "AmbiLights.hpp"

void AmbiLights::setError(std::string _err) {
	err = _err;
}

AmbiLights::AmbiLights() {
	duo = NULL;
	controller = NULL;
	topLeds = bottomLeds = 0;

	mutexHandle = CreateMutexA(NULL, false, NULL);
	if (mutexHandle == NULL)
		throw std::runtime_error("Failed to create mutex for AmbiLight class");
}

AmbiLights::~AmbiLights() {
	delete controller;
	delete duo;

	controller = NULL;
	duo = NULL;

	CloseHandle(mutexHandle);
}

bool AmbiLights::init() {
	bool success;

	// Setup communication with MegaLeaf controller
	WaitForSingleObject(mutexHandle, INFINITE);
	try {
		controller = new MLFProtoLib();
		controller->getLedsCount(topLeds, bottomLeds);
	} catch(MLFException& ex) {
		err = ex.what();
		delete controller;

		ReleaseMutex(mutexHandle);
		return false;
	}
	ReleaseMutex(mutexHandle);

	if (topLeds == 0 || bottomLeds == 0) {
		setError("Failed to acquire number of leds from MLF controller");
		return false;
	}

	// Setup Desktop Duplication API
	duo = new DesktopDuplication();
	success = duo->init();
	if (!success) {
		err = duo->getError();
		return false;
	}
	return true;
}

void AmbiLights::setBrightness(int level) {
	WaitForSingleObject(mutexHandle, INFINITE);
	
	try {
		if(controller)
			controller->setBrightness(level);
	}
	catch (MLFException& ex) {
		err = ex.what();
	}
	
	ReleaseMutex(mutexHandle);
}

int AmbiLights::getBrightness(void) {
	int result = -1;
	WaitForSingleObject(mutexHandle, INFINITE);

	try {
		//if (controller)
			//result = controller->getBrightness();
		result = 255;
	}
	catch (MLFException& ex) {
		err = ex.what();
	}

	ReleaseMutex(mutexHandle);
	return result;
}

void AmbiLights::setState(eStates _state) {
	if(_state >= 0 && _state < AMBI_STATE_MAX)
		state = _state;
}

AmbiLights::eStates AmbiLights::getState(void) {
	return state;
}

void AmbiLights::applyColorCorrections(long from, long to, size_t ledsCount) {
	const long scopeWidth = to - from;

	long bucketSize = (scopeWidth / ledsCount) + 1;
	int r, g, b, color = 0;
	long long sumR, sumG, sumB;
	size_t addedColors = 0;

	for (long i = to - bucketSize; i >= from; i -= bucketSize) {
		sumR = sumG = sumB = 0;


		for (long j = 0; j < bucketSize; j++) {
			r = screen[i + j] & 0xff;
			g = (screen[i + j] >> 8) & 0xff;
			b = (screen[i + j] >> 16) & 0xff;
			sumR += r;
			sumG += g;
			sumB += b;
		}

		r = sumR / bucketSize;
		g = sumG / bucketSize;
		b = sumB / bucketSize;

		constexpr int max_diff = 0x10;
		if (r < 0x70 && g < 0x70 && b < 0x70 &&
			abs(r - g) < max_diff && abs(r - b) < max_diff && abs(g - b) < max_diff) {
			r /= 3;
			g /= 3;
			b /= 3;
		}

		color = r | (g << 8) | (b << 16);
		colors.push_back(color);
		addedColors++;
	}

	// Fill remaining LEDs with last color
	while (addedColors < ledsCount) {
		colors.push_back(color);
		addedColors++;
	}
}

bool AmbiLights::getFrame(void) {
	bool success;
	int maxRetries = 5;

	do {
		success = duo->getScreen(screen);
		if (success)
			return true;
		
		// Try to reconfigure whole DesktopDuplication API
		duo->deinit();
		success = duo->init();
		if (!success)
			break;
	} while (maxRetries-- > 0);

	// sth went so wrong that we cannot clone image frame even
	//  after reconfiguring DDUAPI. Just switch to disable mode
	//  at this point
	setState(AMBI_STATE_OFF);

	err = duo->getError();
	return false;
}

bool AmbiLights::runFrame(void) {
	size_t screenWidth;
	bool ret = true;

	if (!getFrame())
		return false;
	screenWidth = screen.size() / 2;

	colors.clear();
	applyColorCorrections(screenWidth, screenWidth * 2 - 1, bottomLeds);
	applyColorCorrections(0, screenWidth - 1, topLeds);

	WaitForSingleObject(mutexHandle, INFINITE);
	try {
		controller->setColors(colors.data(), colors.size());
	}
	catch (MLFException& ex) {
		err = ex.what();
		ret = false;
	}
	ReleaseMutex(mutexHandle);

	return ret;
}

void AmbiLights::turnLEDsOn(void) {
	WaitForSingleObject(mutexHandle, INFINITE);
	try {
		if(controller)
			controller->turnOn();
	}
	catch (MLFException& ex) {
		err = ex.what();
	}
	ReleaseMutex(mutexHandle);
}

void AmbiLights::turnLEDsOff(void) {
	WaitForSingleObject(mutexHandle, INFINITE);
	try {
		if(controller)
			controller->turnOff();
	}
	catch (MLFException& ex) {
		err = ex.what();
	}
	ReleaseMutex(mutexHandle);
}

void AmbiLights::run(void) {
	bool success;
	eStates lState = AMBI_STATE_OFF, prevState;

	while (1) {
		prevState = lState;
		lState = state;
		if (lState == AMBI_STATE_OFF) {
			if (prevState == AMBI_STATE_ON) {
				duo->deinit();
				turnLEDsOff();
			}

			Sleep(500);
			continue;
		}
		else if (lState == AMBI_STATE_SHUTDOWN) {
			break;
		}
		else if (lState == AMBI_STATE_ON) {
			if (prevState == AMBI_STATE_OFF) {
				success = duo->init();
				if (!success)
					setState(AMBI_STATE_OFF);
				turnLEDsOn();
			}
		}

		runFrame();
	}

	turnLEDsOff();
}

std::string& AmbiLights::getError(void) {
	return err;
}
