#pragma once
#include <WindowsX.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <memory>
#include <array>
#include <vector>
#include "d3dx12.h"
#include "d3dUtil.h"
#include "GameTimer.h"
#include "UploadBuffer.h"
#include "MathHelper.h"


using Microsoft::WRL::ComPtr;
using namespace DirectX;
//using DirectX::XMFLOAT3;
//using DirectX::XMFLOAT4;

struct Vertex
{
	DirectX::XMFLOAT3 Pos;
	DirectX::XMFLOAT4 Color;
};

struct ObjectConstants
{
	DirectX::XMFLOAT4X4 World = MathHelper::Identity4x4();
};

struct MeshGeometry
{
	ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
	ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;			// GPU 복사 끝날 때까지 살려둬야 함
	ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;
	ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

	UINT VertexByteStride = 0;
	UINT VertexBufferByteSize = 0;
	DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;
	UINT IndexBufferByteSize = 0;

	D3D12_VERTEX_BUFFER_VIEW VertexBufferView()const
	{
		D3D12_VERTEX_BUFFER_VIEW vbv;
		vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
		vbv.StrideInBytes = VertexByteStride;
		vbv.SizeInBytes = VertexBufferByteSize;
		return vbv;
	}

	D3D12_INDEX_BUFFER_VIEW IndexBufferView()const
	{
		D3D12_INDEX_BUFFER_VIEW ibv;
		ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
		ibv.Format = IndexFormat;
		ibv.SizeInBytes = IndexBufferByteSize;
		return ibv;
	}
};

class Init
{

public:

	Init(HINSTANCE hInstance);
	Init(const Init& rhs) = delete;
	Init& operator= (const Init& rhs) = delete;
	virtual ~Init();
	/*
	* 원인은 멤버 함수 포인터와 일반 함수 포인터가 다른 타입이기 때문
	* WndProc을 클래스 멤버로 만들면 컴파일러가 숨겨진 this 매개변수를 하나 더 붙여
	* Windows는 this가 뭔지 모르니까 이걸 호출할 방법이 없음.
	* 그래서 WndProc은 반드시 클래스 밖의 일반 함수이거나 static 멤버여야 함
	*/
	static Init* GetApp();			// 정적 접근자

	int Run();

	bool Initialize();
	// CALLBACK 없음. 그냥 멤버 함수
	LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	
	bool Get4xMsaaState() const;
	void Set4xMsaaState(bool value);


protected:

	bool InitWindow(HINSTANCE hInstance);
	bool InitD3D();
	void Update(const GameTimer& gt);
	void Draw();

	void CreateCommandObjects();
	void FlushCommandQueue();
	void CreateSwapChain();
	void CreateRtvAndDsvDescriptorHeaps();

	void OnResize();

	ID3D12Resource* CurrentBackBuffer() const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView()const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()const;

	void CalculateFrameState();

	virtual void OnMouseDown(WPARAM btnState, int x, int y) { }
	virtual void OnMouseUp(WPARAM btnState, int x, int y) { }
	virtual void OnMouseMove(WPARAM btnState, int x, int y) { }

	void BuildConstantBuffers();
	void BuildDescriptorHeaps();
	void BuildRootSignature();
	void BuildBoxGeometry();
	void BuildShadersAndInputLayout();
	void BuildPSO();

protected:

	static Init* mApp;				// 유일한 인스턴스를 가리킴
	HINSTANCE mhAppInst = nullptr;
	HWND mhMainWnd = nullptr;

	ComPtr<ID3D12Device> g_device;
	ComPtr<IDXGIFactory4> g_dxgiFactory;
	ComPtr<ID3D12Fence> g_fence;
	ComPtr<ID3D12CommandQueue> g_commandQueue;
	ComPtr<ID3D12CommandAllocator> g_commandAllocator;
	ComPtr<ID3D12GraphicsCommandList> g_commandList;
	ComPtr<IDXGISwapChain> g_swapChain;
	ComPtr<ID3D12DescriptorHeap> g_rtvHeap;
	ComPtr<ID3D12DescriptorHeap> g_dsvHeap;
	ComPtr<ID3D12Resource> g_SwapChainBuffer[2];
	ComPtr<ID3D12Resource> g_depthStencilBuffer;

	UINT g_rtvDescriptorSize = 0;
	UINT g_dsvDescriptorSize = 0;
	UINT g_cbvSrvUavDescriptorSize = 0;

	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	int mClientWidth = 800;
	int mClientHeight = 600;

	bool m4xMsaaState = false; // 4X MSAA enabled
	UINT m4xMsaaQuality = 0; // quality level of 4X MSAA

	static const int SwapChainBufferCount = 2;
	int mCurrBackBuffer = 0;
	UINT64 mCurrnetFence = 0;

	D3D12_VIEWPORT	mScreenViewport;
	D3D12_RECT		mScissorRect;

	bool mAppPaused = false;
	bool mMinimized = false;
	bool mMaximized = false;
	bool mResizing = false;
	bool mFullscreenState = false;

	GameTimer mTimer;

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12PipelineState> mPSO = nullptr;

	ComPtr<ID3D10Blob> mvsByteCode = nullptr;
	ComPtr<ID3D10Blob> mpsByteCode = nullptr;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	std::unique_ptr<MeshGeometry> mBoxGeo = nullptr;
	std::unique_ptr<UploadBuffer<ObjectConstants>> mObjectCB = nullptr;
	ComPtr<ID3D12DescriptorHeap> mCbvHeap = nullptr;
};