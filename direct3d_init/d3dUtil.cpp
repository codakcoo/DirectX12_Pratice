#include "d3dUtil.h"
#include "d3dx12.h"
#include <comdef.h>
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")

DxException::DxException(HRESULT hr, const std::wstring& functionName,
    const std::wstring& filename, int lineNumber)
    : ErrorCode(hr), FunctionName(functionName),
    Filename(filename), LineNumber(lineNumber)
{
}

std::wstring DxException::ToString() const
{
    _com_error err(ErrorCode);
    std::wstring msg = err.ErrorMessage();
    return FunctionName + L" failed in " + Filename + L"; line "
        + std::to_wstring(LineNumber) + L"; error: " + msg;
}

ComPtr<ID3D12Resource> d3dUtil::CreateDefaultBuffer(ID3D12Device* device, ID3D12GraphicsCommandList* cmdList, const void* initData, UINT64 byteSize, ComPtr<ID3D12Resource>& uploadBuffer)
{
    ComPtr<ID3D12Resource> defaultBuffer;

	// 1. GPU 전용 메모리 확보(Default Heap)
	auto defaultHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto defaultDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
    ThrowIfFailed(device->CreateCommittedResource(
        &defaultHeapProperties,
        D3D12_HEAP_FLAG_NONE,
        &defaultDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
		IID_PPV_ARGS(defaultBuffer.GetAddressOf())));

    // 2. CPU가 쓸 수 있는 중간 업로드 버퍼 확보
	auto uploadHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    ThrowIfFailed(device->CreateCommittedResource(
        &uploadHeapProp,
        D3D12_HEAP_FLAG_NONE,
        &defaultDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr, IID_PPV_ARGS(uploadBuffer.GetAddressOf())));

    // 3. 업로드 버퍼에 CPU 데이터를 채워넣고, GPU 복사를 예약
	D3D12_SUBRESOURCE_DATA subResourceData = {};
	subResourceData.pData = initData;
	subResourceData.RowPitch = byteSize;
	subResourceData.SlicePitch = byteSize;

	auto toCopyDest = CD3DX12_RESOURCE_BARRIER::Transition(
		defaultBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST);
	cmdList->ResourceBarrier(1, &toCopyDest);

	UpdateSubresources<1>(cmdList, defaultBuffer.Get(), uploadBuffer.Get(), 0, 0, 1, &subResourceData);

	auto toRead = CD3DX12_RESOURCE_BARRIER::Transition(
		defaultBuffer.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_COMMON);
	cmdList->ResourceBarrier(1, &toRead);

    return defaultBuffer;
}

ComPtr<ID3DBlob> d3dUtil::CompileShader(
        const std::wstring& filename, 
        const D3D_SHADER_MACRO* defines, 
        const std::string& entrypoint, 
        const std::string& target)
{
    UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	HRESULT hr = S_OK;

	ComPtr<ID3DBlob> byteCode = nullptr;
    ComPtr<ID3DBlob> errors;

	hr = D3DCompileFromFile(
        filename.c_str(), defines, 
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entrypoint.c_str(), target.c_str(),
		compileFlags, 0,
		&byteCode, &errors);

    // 컴파일 에러 메시지를 출력 창에 반드시 띄워야 함
    if (errors != nullptr)
    {
        OutputDebugStringA((char*)errors->GetBufferPointer());
	}

	ThrowIfFailed(hr);

    return byteCode;
}
