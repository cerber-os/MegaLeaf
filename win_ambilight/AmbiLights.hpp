#pragma once

#include "DesktopDuplicationAPI.h"
#include "MLFProtoLib.hpp"

#include <stdlib.h>

class AmbiLights {

	DesktopDuplication* duo;
	MLFProtoLib* controller;
	std::vector<int> screen, colors;

	int topLeds, bottomLeds;

	std::string err;
	void setError(std::string _err);

	void applyColorCorrections(long from, long to, size_t ledsCount);

public:
	AmbiLights();
	~AmbiLights();

	bool init();

	void setBrightness(int level);
	bool runFrame(void);
	void run(void);

	std::string& getError(void);
};
