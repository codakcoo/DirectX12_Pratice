#pragma once
#include <WindowsX.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <wrl.h>
#include <DirectXMath.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include <memory>
#include <array>
#include <vector>
#include <unordered_map>
#include <string>
#include "d3dx12.h"
#include "d3dUtil.h"
#include "GameTimer.h"
#include "UploadBuffer.h"
#include "MathHelper.h"
#include "FrameResource.h"
#include "GeometryTypes.h"
#include "DDSTextureLoader.h"
#include "Camera.h"


using namespace std;
using namespace DirectX;
using Microsoft::WRL::ComPtr;
//using DirectX::XMFLOAT3;
//using DirectX::XMFLOAT4;



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

	virtual void OnMouseDown(WPARAM btnState, int x, int y);
	virtual void OnMouseUp(WPARAM btnState, int x, int y);
	virtual void OnMouseMove(WPARAM btnState, int x, int y);
	void OnKeyboardInput(const GameTimer& gt);

	void BuildRootSignature();						// 컬러 셰이더 레이아웃 빌드
	void BuildBoxGeometry();
	void BuildFrameResources();
	void BuildBlurResources();
	void BuildRenderItems();
	void BuildShadersAndInputLayout();
	void BuildPSO();

	void BuildOffscreenResources();					// 오프스크린용 텍스처 생성
	void BuildOffscreenViews();						// 오프스크린용 RTV
	void BuildBlurDescriptorHeap();					// 블러 디스크립터 힙 생성
	void BuildBlurRootSignature();					// 블러 루트 시그니처 생성
	void BuildBlurPSO();							// 블러 파이프라인 스테이트 오브젝트 생성

	void BlurExecute(int blurCount);				// 블러 실행 함수 (블러의 하이라이트; 핑퐁, UAV배리어)

	void BuildShadowMapResource();							// 새도우 리소스 생성
	CD3DX12_CPU_DESCRIPTOR_HANDLE ShadowDsv() const;		// 디스크립터 생성

	void LoadTextures();
	void BuildSrvHeap();

	std::vector<float> CalcGaussWeights(float sigma);

	D3D12_CPU_DESCRIPTOR_HANDLE OffscreenRtv() const;			// 핸들 접근용 헬퍼

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
	ComPtr<ID3D12PipelineState> mOpaquePSO = nullptr;

	std::unordered_map<string, ComPtr<ID3D10Blob>> mShaders;

	ComPtr<ID3D10Blob> mvsByteCode;
	ComPtr<ID3D10Blob> mpsByteCode;
	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;

	std::unique_ptr<MeshGeometry> mBoxGeo = nullptr;


	// FrameResource(Allocator 다중화를 위함)
	static const int NumFrameResources = 3;

	std::vector<std::unique_ptr<FrameResource>> mFrameResources;
	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	// 월드 좌표
	float mTheta = 1.5f * XM_PI;
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();
	XMFLOAT4X4 mWorld = MathHelper::Identity4x4();

	static const int NumObjects = 1000;				// 예: 3x3x3 격자 큐브 27개, 바닥 1개, 반사 큐브 1개
	std::vector<XMFLOAT4X4> mObjectWorlds;			// 각 물체의 월드 행렬 
	std::vector<float> mObjectThetas;				// 각 물체의 회전 속도용 각도

	// 텍스처
	std::unique_ptr<Texture> mBoxTex = nullptr;
	ComPtr<ID3D12DescriptorHeap> mSrvHeap = nullptr;

	// 블렌딩
	ComPtr<ID3D12PipelineState> mTransparentPSO = nullptr;
	

	// 컴퓨트
	ComPtr<ID3D12Resource> mBlurMap0 = nullptr;		// 텍스처A
	ComPtr<ID3D12Resource> mBlurMap1 = nullptr;		// 텍스처B

	ComPtr<ID3D12Resource> mOffscreenTex = nullptr;		// 씬을 그릴 오프스크린 텍스처
	

	// 오프 스크린용 RTV, SRV를 담을 힙 (기존 g_rtvHeap와 별도로 관리하거나 확장)
	ComPtr<ID3D12DescriptorHeap> mOffscreenRtvHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> mOffscreenUavHeap = nullptr;
	
	ComPtr<ID3D12DescriptorHeap> mBlurHeap = nullptr;

	ComPtr<ID3D12RootSignature> mBlurRootSignature = nullptr;
	ComPtr<ID3D12PipelineState> mHorzBlurPSO = nullptr;
	ComPtr<ID3D12PipelineState> mVertBlurPSO = nullptr;
	ComPtr<ID3DBlob> mHorzBlurByteCode = nullptr;
	ComPtr<ID3DBlob> mVertBlurByteCode = nullptr;
	
	bool mBlurEnabled = false;

	// 카메라
	Camera mCamera;
	POINT mLastMousePos		= { 0, 0 };

	// 컬링
	DirectX::BoundingFrustum mCameraFrustum;				// 카메라 절두체 (뷰 공간)
	int mVisibleCount = 0;									// 이번 프레임에 보이는 큐브 수
	bool mFrustumCullingEnabled = true;						// 컬링 on/off 토글용

	// 큐브맵 
	std::unique_ptr<Texture> mSkyTex = nullptr;
	ComPtr<ID3D12PipelineState> mSkyPSO = nullptr;

	// 노멀맵
	std::unique_ptr<Texture> mNormalTex = nullptr;

	// 섀도
	ComPtr<ID3D12PipelineState> mShadowPSO = nullptr;

	static const UINT shadowMapSize = 2048;
	ComPtr<ID3D12Resource> mShadowMap = nullptr;
	D3D12_VIEWPORT mShadowViewport;
	D3D12_RECT mShadowScissor;

	int mShadowCount = 0;									// 섀도 패스에서 그릴 개수
};