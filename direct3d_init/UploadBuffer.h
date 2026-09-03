#pragma once
#include "d3dx12.h"
#include <wrl.h>


// 256바이트 배수로 맞춰야 하는 상수 버퍼를 위한 업로드 버퍼
// 실제 ObjectConstants여도 256바이트 차지함.
template<typename T>
class UploadBuffer
{
public:
	UploadBuffer(ID3D12Device* device, UINT elementCount, bool isConstantBuffer) : mIsConstantBuffer(isConstantBuffer)
	{
		mElementByteSize = sizeof(T);

		// 상수 버퍼는 반드시 256바이트 배수여야 함
		if (isConstantBuffer)
			mElementByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(T));
		auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(mElementByte * elementCount);

		device->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(mUploadBuffer.GetAddressOf()));

		mUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData));
		// Unmap은 소멸자에서만 - 매 프레임 CPU가 계속 쓸 수 있게 열어둠
	}

	~UploadBuffer()
	{
		if (mUploadBuffer != nullptr)
			mUploadBuffer->Unmap(0, nullptr);
		mMappedData = nullptr;
	}

	UploadBuffer(const UploadBuffer& rhs) = delete;
	UploadBuffer& operator =(const UploadBuffer& rhs) = delete;

	ID3D12Resource* Resource() const { return mUploadBuffer.Get(); }

	void CopyData(int elementIndex, const T& data)
	{
		memcpy(&mMappedData[elementIndex * mElementByteSize], &data, sizeof(T));
	}

private:
	Microsoft::WRL::ComPtr<ID3D12Resource> mUploadBuffer;
	BYTE* mMappedData = nullptr;
	UINT mElementByteSize = 0;
	bool mIsConstantBuffer = false;
};