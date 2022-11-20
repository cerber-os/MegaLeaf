#include "AmbiLights.hpp"
#include "ArgsParsing.hpp"

#include <windowsx.h>

#include <iostream>

#pragma comment(lib, "Shell32.lib")


AmbiLights* ambiLight;

/**
 * A bit of GUI stuff
 */
bool useGUI = false;
void displayError(std::string errorMsg) {
	std::cerr << errorMsg << "\n";

	if (useGUI)
		MessageBoxA(NULL, errorMsg.c_str(), "Ambilight error", MB_OK | MB_ICONERROR);
}

/**
 * Daemon handling
 *  We're a bit old-school here - instead of creating proper Windows service,
 *  we're going to just close all windows and detach from parent process.
 *  That has many disadvantages, but allows for easier cmdline control and should
 *  be sufficient for now
 */
#define MY_TRAY_ICON_MESSAGE	 (WM_APP + 2137)

enum TRAY_ICON_CONTEXT_MENU_IDS {
	CONTEXT_MENU_SWITCH = 210,
	CONTEXT_MENU_BRIGHTNESS,
	CONTEXT_MENU_EXIT		
};

bool displayTrayIcon(HWND handle) {
	bool ret = false;
	HICON icon = NULL;
	NOTIFYICONDATA nidata = { 0 };

	icon = ExtractIconA(NULL, "%SystemRoot%\\System32\\SHELL32.dll", -249);
	if (icon == NULL) {
		displayError("Failed to locate TrayIcon ID-249");
		return false;
	}

	nidata.uID = 2137;
	nidata.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
	nidata.hIcon = icon;
	nidata.uCallbackMessage = MY_TRAY_ICON_MESSAGE;
	nidata.hWnd = handle;
	wcscpy(nidata.szTip, L"MegaLeaf AmbiLight Daemon");

	if (!Shell_NotifyIcon(NIM_ADD, &nidata)) {
		displayError("Failed to display TrayIcon");
		DestroyIcon(icon);
		return false;
	}

	DestroyIcon(icon);
	return true;
}

void destroyTrayIcon(HWND hwnd) {
	NOTIFYICONDATA nidata = { 0 };
	nidata.uID = 2137;
	nidata.hWnd = hwnd;

	if (!Shell_NotifyIcon(NIM_DELETE, &nidata)) {
		displayError("Failed to delete TrayIcon");
	}
}

void displayContextMenu(HWND hwnd, POINT pt) {
	int ret;
	AmbiLights::eStates curState = ambiLight->getState();
	int brightness = ambiLight->getBrightness() * 100 / 255;

	HMENU submenu = CreatePopupMenu();
	struct { int value; const char* str; } opts[] = {
		{10, "10%"},
		{40, "40%"},
		{75, "75%"},
		{100, "100%"}
	};
	for (int i = 0; i < sizeof(opts) / sizeof(opts[0]); i++) {
		UINT flags = MF_STRING;
		if (brightness == opts[i].value) flags |= MF_CHECKED;
		AppendMenuA(submenu, flags, opts[i].value, opts[i].str);
	}

	HMENU menu = CreatePopupMenu();
	AppendMenuA(menu, MF_STRING, CONTEXT_MENU_SWITCH, curState == AmbiLights::eStates::AMBI_STATE_OFF ? "Enable" : "Disable");
	AppendMenuA(menu, MF_POPUP, (UINT_PTR)submenu, "Brightness");
	AppendMenuA(menu, MF_SEPARATOR, NULL, NULL);
	AppendMenuA(menu, MF_STRING, CONTEXT_MENU_EXIT, "Exit");
	SetForegroundWindow(hwnd);
	
	ret = TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_RETURNCMD, pt.x, pt.y, 0, hwnd, NULL);

	if (ret == CONTEXT_MENU_EXIT) {
		// Destroy current window. WM_CLOSE will handle stopping AmbiLight thread
		DestroyWindow(hwnd);
	}
	else if (ret == CONTEXT_MENU_SWITCH) {
		// Start/Stop Ambilight thread
		if (curState == AmbiLights::eStates::AMBI_STATE_OFF)
			ambiLight->setState(AmbiLights::eStates::AMBI_STATE_ON);
		else
			ambiLight->setState(AmbiLights::eStates::AMBI_STATE_OFF);
	}
	else if (ret > 0 && ret <= 100) {
		// Set brightness to ret
		ambiLight->setBrightness(ret * 255 / 100);
	}
	else {
		// Just ignore...
	}

	DestroyMenu(menu);
	DestroyMenu(submenu);
}


INT_PTR CALLBACK TaskBarProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	POINT pt;
	LPCSTR notifyMsg = NULL;

	switch (message) {
	case MY_TRAY_ICON_MESSAGE:
		switch (lParam)
		{
		case WM_RBUTTONDOWN:
		case WM_CONTEXTMENU:
			GetCursorPos(&pt);
			displayContextMenu(hWnd, pt);
			break;
		}
		break;

	case WM_CREATE:
		if (!displayTrayIcon(hWnd))
			return -1;
		break;

	case WM_DESTROY:
		destroyTrayIcon(hWnd);
		ambiLight->setState(AmbiLights::AMBI_STATE_SHUTDOWN);
		PostQuitMessage(0);
		return 0;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

HWND createMessageOnlyWindow(void) {

	HWND handle;
	WNDCLASS wc = { 0 };
	
	wc.lpfnWndProc = TaskBarProc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpszClassName = L"DummyMessageOnlyWindow";

	if (!RegisterClass(&wc)) {
		displayError("Failed to register new class for message-only window");
		return NULL;
	}

	handle = CreateWindowEx(0, L"DummyMessageOnlyWindow", L"AmbiLight - message-only window",
			0, 0, 0, 0, 0, 
			HWND_MESSAGE, NULL, NULL, NULL);
	if (!handle) {
		displayError("Failed to create message-only window");
		// fallback
	}
	return handle;
}

DWORD daemon_thread(LPVOID lpParam) {
	MSG msg;
	HWND handle;

	handle = createMessageOnlyWindow();
	if (!handle)
		exit(1);


	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// Clean-up after createMessageOnlyWindow
	UnregisterClass(L"DummyMessageOnlyWindow", GetModuleHandle(NULL));
	return 0;
}

void daemonize(void) {
	HANDLE thread;

	// I want to break free
	FreeConsole();

	thread = CreateThread(NULL, 0, daemon_thread, NULL, 0, NULL);
	if (!thread) {
		displayError("Failed to create GUI thread");
		exit(1);
	}
}

/**
 * Use CreateMutexA to allow only one AmbiLight process to
 *  run at the same time
 */
void limit_to_single_execution(void) {
	HANDLE handle;

	handle = CreateMutexA(NULL, false, "\\Local\\$AmbiLightsSession$");
	if (handle && GetLastError() == ERROR_ALREADY_EXISTS) {
		displayError("AmbiLights is already running in background\n\nKill existing"
			"session before starting a new one");
		exit(1);
	}

	// System should automatically delete this mutex after last instance of this
	//  program exists. But it's Windows, so who the hell knows what's going to 
	//  happen
}

/**
 * Program entry point
 */
int main(int argc, char** argv) {
	bool success;

	// Process cmdline options
	ProgramArgs args;
	args.addBoolParam('g', "gui");
	args.addBoolParam('d', "daemon");
	args.addIntParam('b', "brightness", 255);
	success = args.parse(argc, argv);
	if (!success) {
		displayError("Invalid cmdline options provided\n\nError: " + args.getError());
		return 1;
	}

	auto opts = args.getArgs();
	useGUI = opts["gui"];

	limit_to_single_execution();

	// Setup ambient lighting
	ambiLight = new AmbiLights();
	success = ambiLight->init();
	if (!success) {
		displayError("Failed to initialize AmbiLights\n\nReason: " + ambiLight->getError());
		return 1;
	}
	
	if(opts["brightness"] > 0)
		ambiLight->setBrightness(opts["brightness"]);

	// Run the main task
	if (opts["daemon"])
		daemonize();
	ambiLight->run();

	std::cout << "Done! Closing...\n";

	return 0;
}
