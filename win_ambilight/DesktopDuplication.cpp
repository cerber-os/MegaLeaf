#include <windows.h>
#include <comdef.h>
#include <format>
#include <d3dcompiler.h>

#pragma comment(lib,"d3dcompiler.lib")

#include "DesktopDuplicationAPI.h"


/**
 * Generic class for creating and accessing shared buffer between
 *  CPU app and GPU shader
 */
class ShaderOutputBuffer {
	UINT width;
	ID3D11Device* pD3DDevice;
	ID3D11DeviceContext* pD3DDeviceContext;

	ID3D11Buffer* outputShaderBuffer, * outputSystemBuffer;
	ID3D11UnorderedAccessView* outShaderView;

public:
	ShaderOutputBuffer(ID3D11Device* device, ID3D11DeviceContext* context, size_t _width);
	bool Init();
	ID3D11Buffer* getSystemBuffer(void);
	ID3D11UnorderedAccessView* getShaderView(void);
	~ShaderOutputBuffer();
};

ShaderOutputBuffer::ShaderOutputBuffer(ID3D11Device* device, ID3D11DeviceContext* context, size_t _width) {
	pD3DDevice = device;
	pD3DDeviceContext = context;
	width = (UINT)_width;

	outputShaderBuffer = NULL;
	outputSystemBuffer = NULL;
	outShaderView = NULL;
}

bool ShaderOutputBuffer::Init(void) {
	HRESULT res;

	D3D11_BUFFER_DESC outputDesc;
	outputDesc.Usage = D3D11_USAGE_DEFAULT;
	outputDesc.ByteWidth = width*4;
	outputDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS | D3D11_BIND_SHADER_RESOURCE;
	outputDesc.CPUAccessFlags = 0;
	outputDesc.StructureByteStride = 4;
	outputDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	res = pD3DDevice->CreateBuffer(&outputDesc, 0, &outputShaderBuffer);
	if (FAILED(res))
		return false;

	D3D11_BUFFER_DESC systemDesc;
	outputShaderBuffer->GetDesc(&systemDesc);
	systemDesc.Usage = D3D11_USAGE_STAGING;
	systemDesc.BindFlags = 0;
	systemDesc.MiscFlags = 0;
	systemDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	res = pD3DDevice->CreateBuffer(&systemDesc, 0, &outputSystemBuffer);
	if (FAILED(res))
		return false;

	D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.Flags = 0;
	uavDesc.Buffer.NumElements = width;
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	res = pD3DDevice->CreateUnorderedAccessView(outputShaderBuffer, &uavDesc, &outShaderView);
	if (FAILED(res))
		return false;
	return true;
}

ID3D11Buffer* ShaderOutputBuffer::getSystemBuffer(void) {
	pD3DDeviceContext->CopyResource(outputSystemBuffer, outputShaderBuffer);
	return outputSystemBuffer;
}

ID3D11UnorderedAccessView* ShaderOutputBuffer::getShaderView(void) {
	return outShaderView;
}

ShaderOutputBuffer::~ShaderOutputBuffer(void) {
	if (outShaderView)
		outShaderView->Release();
	if (outputShaderBuffer)
		outputShaderBuffer->Release();
	if (outputSystemBuffer)
		outputSystemBuffer->Release();
}


/**
 * The main class responsible of handling Desktop Duplication API,
 *  running shader and returning result to caller
 */
DesktopDuplication::DesktopDuplication(void) {
	pD3DDevice = nullptr;
	pD3DDeviceContext = nullptr;
	pDXGIOutput = nullptr;
	pDXGIOutputDupl = nullptr;
	pCompShader = nullptr;
	screenWidth = screenHeight = 0;
}

DesktopDuplication::~DesktopDuplication(void) {
	deinit();
}



bool DesktopDuplication::init(void) {
	HRESULT res;
	bool success;

	res = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &pD3DDevice, nullptr, &pD3DDeviceContext);
	if (FAILED(res)) {
		SetError("Failed to create Direct3D 11 device", res);
		return false;
	}

	IDXGIDevice4* pDXGIDevice;
	res = pD3DDevice->QueryInterface(__uuidof(IDXGIDevice4), (void**)&pDXGIDevice);
	if (FAILED(res)) {
		SetError("Failed to query DXGI Device interface", res);
		return false;
	}

	IDXGIAdapter4* pDXGIAdapter;
	res = pDXGIDevice->GetParent(__uuidof(IDXGIAdapter4), (void**)&pDXGIAdapter);
	pDXGIDevice->Release();
	if(FAILED(res)) {
		SetError("Failed to query DXGI Adapter interface", res);
		return false;
	}

	res = pDXGIAdapter->EnumOutputs(0, (IDXGIOutput**)&pDXGIOutput);
	pDXGIAdapter->Release();
	if (FAILED(res)) {
		SetError("Failed to query DXGI Output interface", res);
		return false;
	}

	DXGI_OUTPUT_DESC pDesc;
	res = pDXGIOutput->GetDesc(&pDesc);
	if (FAILED(res)) {
		pDXGIOutput->Release();
		SetError("Failed to query output description", res);
		return false;
	}

	screenWidth = pDesc.DesktopCoordinates.right - pDesc.DesktopCoordinates.left;
	screenHeight = pDesc.DesktopCoordinates.bottom - pDesc.DesktopCoordinates.top;

	success = initOutputDuplication();
	if (!success)
		return false;

	return CreateComputeShader();
}

void DesktopDuplication::deinit(void) {
	if (pCompShader)
		pCompShader->Release();
	pCompShader = NULL;

	if (pDXGIOutputDupl)
		pDXGIOutputDupl->Release();
	pDXGIOutputDupl = NULL;

	if (pDXGIOutput)
		pDXGIOutput->Release();
	pDXGIOutput = NULL;

	if (pD3DDeviceContext)
		pD3DDeviceContext->Release();
	pD3DDeviceContext = NULL;

	if (pD3DDevice)
		pD3DDevice->Release();
	pD3DDevice = NULL;
}

bool DesktopDuplication::initOutputDuplication(void) {
	HRESULT res;

	if (pDXGIOutputDupl)
		pDXGIOutputDupl->Release();

	res = pDXGIOutput->DuplicateOutput(pD3DDevice, &pDXGIOutputDupl);
	if (FAILED(res)) {
		SetError("Failed to create DuplicateOutput instance", res);
		return false;
	}

	return true;
}

bool DesktopDuplication::CreateComputeShader(void) {
	HRESULT res;
	ID3DBlob* shaderBlob, *errorBlob;
	UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;

	LPCSTR profile = (pD3DDevice->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0) ? "cs_5_0" : "cs_4_0";
	res = D3DCompileFromFile(shaderFilePath, nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		shaderEntry, profile, flags, 0, &shaderBlob, &errorBlob);

	if (FAILED(res)) {
		SetError("Failed to compile shader from FILE. Refer to stdout for details.", res);
		if (errorBlob) {
			OutputDebugStringA((char*)errorBlob->GetBufferPointer());
			errorBlob->Release();
		}

		if (shaderBlob)
			shaderBlob->Release();
		return false;
	}

	res = pD3DDevice->CreateComputeShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), 
			nullptr, &pCompShader);
	shaderBlob->Release();
	if (FAILED(res)) {
		SetError("Failed to create ComputeShader", res);
		return false;
	}

	return true;
}

void DesktopDuplication::SetError(std::string msg, HRESULT error) {
	errorMessage = msg;
	if (error != 0) {
		_com_error err(error);
		errorMessage += " Error code: " + std::format("{:x}", (unsigned long)error);
		errorMessage += " [";
		errorMessage += (const char*) err.ErrorMessage();
		errorMessage += "]";
	}
}

bool DesktopDuplication::getScreen(std::vector<int>& screen) {
	int maxRetries = 3;
	HRESULT res;
	bool success;

	if (pDXGIOutputDupl == nullptr) {
		SetError("Failed to getScreen - DuplicateOutput instance hasn't been created yet");
		return false;
	}
	
	// https://docs.microsoft.com/en-us/windows/win32/api/dxgi1_2/nf-dxgi1_2-idxgioutputduplication-releaseframe
	// For performance reasons, we recommend that you release the frame just before you call the
	//  IDXGIOutputDuplication::AcquireNextFrame method to acquire the next frame
	if (holdsFrame) {
		pDXGIOutputDupl->ReleaseFrame();
		holdsFrame = false;
	}

	IDXGIResource* pDXGIRes;
	DXGI_OUTDUPL_FRAME_INFO DXGIFrameInfo;
	do {
		res = pDXGIOutputDupl->AcquireNextFrame(INFINITE, &DXGIFrameInfo, &pDXGIRes);
		if (SUCCEEDED(res))
			break;
		else if (res == DXGI_ERROR_ACCESS_LOST) {
			// DuplicateOutput is no longer valid - reinitialize to regain control
			if (!initOutputDuplication())
				return false;
		}
	} while (maxRetries-- > 0);

	if (FAILED(res)) {
		SetError("Failed to acquire next frame from DuplicateOutput", res);
		return false;
	}
	holdsFrame = true;

	ID3D11Texture2D* pGPUTex;
	res = pDXGIRes->QueryInterface(__uuidof(ID3D11Texture2D), (void**)&pGPUTex);
	pDXGIRes->Release();
	if (FAILED(res)) {
		SetError("Failed to query ID3D11Texture2D for GPU Texture", res);
		return false;
	}

	// Provide screen frame to compute shader
	ID3D11ShaderResourceView* pSharedRes;
	res = pD3DDevice->CreateShaderResourceView(pGPUTex, nullptr, &pSharedRes);
	if (FAILED(res)) {
		pGPUTex->Release();
		SetError("Failed to create shader resource view for aquired screen frame", res);
		return false;
	}

	// Setup output buffer
	ShaderOutputBuffer outputBuffer(pD3DDevice, pD3DDeviceContext, screenWidth * 2);
	if (!outputBuffer.Init()) {
		SetError("Failed to setup outputBuffer");
		pGPUTex->Release();
		return false;
	}
	const ID3D11UnorderedAccessView* view = outputBuffer.getShaderView();

	// Setup const buffer
	ID3D11Buffer* pConstBuffer;
	struct VS_CONSTANT_BUFFER {
		unsigned int width;
		unsigned int height;
		unsigned int targetWidth;
		unsigned int unused;
	};

	struct VS_CONSTANT_BUFFER VsConstantBuf;
	VsConstantBuf.width = screenWidth;
	VsConstantBuf.height = screenHeight;
	VsConstantBuf.targetWidth = screenWidth;

	D3D11_BUFFER_DESC cbDesc;
	cbDesc.ByteWidth = sizeof(VS_CONSTANT_BUFFER);
	cbDesc.Usage = D3D11_USAGE_DYNAMIC;
	cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	cbDesc.MiscFlags = 0;
	cbDesc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA InitData;
	InitData.pSysMem = &VsConstantBuf;
	InitData.SysMemPitch = 0;
	InitData.SysMemSlicePitch = 0;

	res = pD3DDevice->CreateBuffer(&cbDesc, &InitData, &pConstBuffer);
	if (FAILED(res)) {
		SetError("Failed to create const buffer", res);
		pGPUTex->Release();
		return false;
	}

	// Dispatch our pour shader
	pD3DDeviceContext->CSSetShader(pCompShader, nullptr, 0);
	pD3DDeviceContext->CSSetShaderResources(0, 1, &pSharedRes);
	pD3DDeviceContext->CSSetUnorderedAccessViews(0, 1, (ID3D11UnorderedAccessView* const*) &view, 0);
	pD3DDeviceContext->CSSetConstantBuffers(0, 1, &pConstBuffer);
	pD3DDeviceContext->Dispatch((int)screenWidth / 512, 1, 1);
	pD3DDeviceContext->CSSetShader(nullptr, nullptr, 0);

	D3D11_MAPPED_SUBRESOURCE subResource;
	ID3D11Buffer* pDestBuffer = outputBuffer.getSystemBuffer();
	res = pD3DDeviceContext->Map(pDestBuffer, 0, D3D11_MAP_READ, 0, &subResource);
	if (SUCCEEDED(res)) {
		screen.resize(screenWidth * 2);
		memcpy(screen.data(), subResource.pData, screenWidth * 2 * 4);
		pD3DDeviceContext->Unmap(pDestBuffer, 0);
		success = true;
	}
	else {
		SetError("Failed to map CPU texture", res);
		success = false;
	}

	pConstBuffer->Release();
	pSharedRes->Release();
	pGPUTex->Release();

	return success;
}

std::string& DesktopDuplication::getError(void) {
	return errorMessage;
}
