#include "AmbiLights.hpp"

#include <iostream>

#include <stdlib.h>
#include <time.h>

void AmbiLights::setError(std::string _err) {
	err = _err;
}

AmbiLights::AmbiLights() {
	duo = NULL;
	controller = NULL;
	topLeds = bottomLeds = 0;
}

AmbiLights::~AmbiLights() {
	delete controller;
	delete duo;

	controller = NULL;
	duo = NULL;
}

bool AmbiLights::init() {
	bool success;

	// Setup communication with MegaLeaf controller
	try {
		controller = new MLFProtoLib();
		controller->getLedsCount(topLeds, bottomLeds);
	} catch(MLFException& ex) {
		err = ex.what();
		delete controller;
		return false;
	}

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
	try {
		controller->setBrightness(level);
	}
	catch (MLFException& ex) {
		err = ex.what();
	}
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

bool AmbiLights::runFrame(void) {
	bool success;
	size_t screenWidth;

	success = duo->getScreen(screen);
	if (!success) {
		err = duo->getError();
		return false;
	}
	screenWidth = screen.size() / 2;

	colors.clear();
	applyColorCorrections(screenWidth, screenWidth * 2 - 1, bottomLeds);
	applyColorCorrections(0, screenWidth - 1, topLeds);

	try {
		controller->setColors(colors.data(), colors.size());
	}
	catch (MLFException& ex) {
		err = ex.what();
		return false;
	}
	return true;
}

void AmbiLights::run(void) {
	size_t processedFrames = 0;
	time_t t = time(NULL);

	while (1) {
		runFrame();

		processedFrames++;
		if (time(NULL) - t) {
			std::cout << "Processed " << processedFrames << " frames" << std::endl;
			processedFrames = 0;
			t = time(NULL);
		}
	}
}

std::string& AmbiLights::getError(void) {
	return err;
}
