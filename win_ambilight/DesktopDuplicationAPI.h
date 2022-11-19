/**
 * Main class implementing average color computation using
 *  Windows' Desktop Duplication API.
 */
#pragma once

#include <d3d11.h>
#include <dxgi1_6.h>

#include <string>
#include <vector>


/**
 * Class responsible for handling Desktop Duplication API,
 *  running shader and returning result to caller
 */
class DesktopDuplication {
	ID3D11Device* pD3DDevice;
	ID3D11DeviceContext* pD3DDeviceContext;
	IDXGIOutput6* pDXGIOutput;
	IDXGIOutputDuplication* pDXGIOutputDupl;
	ID3D11ComputeShader* pCompShader;

	bool holdsFrame = false;
	std::string errorMessage = "";

	unsigned long long screenWidth, screenHeight;

	const LPCWSTR shaderFilePath = L"ComputeRowsAvgColors.hlsl";
	const char* shaderEntry = "main";

	/* Can be safely called (and even is) multiple times, whenever current
	 *  DDUAPI handle becomes invalid
	 */
	bool initOutputDuplication(void);
	
	bool CreateComputeShader(void);
	void SetError(std::string msg, HRESULT error = 0);

public:
	DesktopDuplication();
	~DesktopDuplication();

	/* Must be called once after constructor. Not a part of constructor,
	 *  as it might easily fail (for instance, when currently running app
	 *  is using legacy DirectX API) and it's much easier to handle errors
	 *  here than throught C++ exceptions
	 */
	bool init(void);

	/* Clear provided vector and fill it with average colors. The output
	 *  vector has a size of screenWidth * 2 (i.e. stores average color of
	 *  each half of each column on screen).
	 * 
	 * Will block while waiting for new frame on screen
	 */
	bool getScreen(std::vector<int>& screen);

	/* Retrieve error message for the last failed function
	 */
	std::string& getError(void);
};
