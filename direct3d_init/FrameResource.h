#pragma once

#include "d3dx12.h"
#include "d3dUtil.h"
#include "UploadBuffer.h"
#include "GeometryTypes.h"
#include <wrl.h>
#include <memory>

///*
//* 기존의 경우 얼로케이터 하나뿐이기에, 
//* 기존 GPU가 이전 프레임 명령을 아직 실행 중이라면, 
//* CPU가 같은 얼로케이터에 기록할 수 없어서 GPU가 끝날 때까지 기다려야 했다.
//* mCurrentFence 버그이 이유가 이것이다.
//*/
struct FrameResource
{
public:
	FrameResource(ID3D12Device* device, UINT objectCount);
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator=(const FrameResource& rhs) = delete;
	~FrameResource() = default;

	// 프레임마다 독립된 얼로케이터 - GPU가 프레임 N-2를 실행 중이어도
	// CPU는 프레임 N용 얼로케이터에 안전하게 기록 가능
	ComPtr<ID3D12CommandAllocator> CmdListAlloc;

	// 프레임마다 독립된 상수 버퍼 - 마찬가지 이유
	std::unique_ptr<UploadBuffer<ObjectConstants>> ObjectCB = nullptr;

	// 이 프레임의 명령이 GPU에서 완료됐는지 확인할 펜스 값
	UINT64 Fence = 0;
};