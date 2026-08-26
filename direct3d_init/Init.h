#pragma once
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>
#include "d3dx12.h"
#include "d3dUtil.h"


using Microsoft::WRL::ComPtr;



class Init
{
public:
	Init(HINSTANCE hInstance);
	~Init();

	/*
	* 원인은 멤버 함수 포인터와 일반 함수 포인터가 다른 타입이기 때문
	* WndProc을 클래스 멤버로 만들면 컴파일러가 숨겨진 this 매개변수를 하나 더 붙여
	* Windows는 this가 뭔지 모르니까 이걸 호출할 방법이 없음.
	* 그래서 WndProc은 반드시 클래스 밖의 일반 함수이거나 static 멤버여야 함
	*/
	static Init* GetApp();			// 정적 접근자

	bool Initialize();
	int Run();
	// CALLBACK 없음. 그냥 멤버 함수
	LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	
protected:

	bool InitWindow(HINSTANCE hInstance);
	bool InitD3D();
	void CreateCommandObjects();
	void FlushCommandQueue();
	void Draw();

	void CreateSwapChain();
	void CreateRtvAndDsvDescriptorHeaps();

	void OnResize();

	ID3D12Resource* CurrentBackBuffer() const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView()const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()const;

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
};