#pragma once

#include "DesktopDuplicationAPI.h"
#include "MLFProtoLib.hpp"

#include <stdlib.h>

class AmbiLights {

public:
	enum eStates {
		AMBI_STATE_OFF = 0,
		AMBI_STATE_ON,
		AMBI_STATE_SHUTDOWN,

		AMBI_STATE_MAX
	};

private:
	DesktopDuplication* duo;
	MLFProtoLib* controller;
	std::vector<int> screen, colors;

	int topLeds, bottomLeds;

	std::string err;
	void setError(std::string _err);

	void applyColorCorrections(long from, long to, size_t ledsCount);
	void handleEvents(void);
	bool getFrame(void);
	bool runFrame(void);
	void turnLEDsOn(void);
	void turnLEDsOff(void);

	/* Multi-thread support */ 
	HANDLE mutexHandle;
	volatile eStates state = AMBI_STATE_ON;

public:
	AmbiLights();
	~AmbiLights();

	bool init();

	void setBrightness(int level);
	int getBrightness(void);
	void setState(eStates state);
	eStates getState(void);

	void run(void);

	std::string& getError(void);
};
