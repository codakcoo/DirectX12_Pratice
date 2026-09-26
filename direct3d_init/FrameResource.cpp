#include "FrameResource.h"



FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT instanceCount)
{
	ThrowIfFailed(device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));

	PassCB = std::make_unique<UploadBuffer<PassConstants>> (device, passCount, true);
	InstanceBuffer = std::make_unique<UploadBuffer<InstanceData>>(device, instanceCount, false);
	VisibleIndexBuffer = std::make_unique<UploadBuffer<UINT>>(device, 2*instanceCount, false);
}
