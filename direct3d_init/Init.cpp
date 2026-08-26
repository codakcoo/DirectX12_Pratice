#include "Init.h"
#include <assert.h>

Init* Init::mApp = nullptr;						// 정적 멤버는 .cpp에서 정의 필수
Init* Init::GetApp() { return mApp; }

Init::Init(HINSTANCE hInstance) : mhAppInst(hInstance)
{
	assert(mApp == nullptr);	// 유일한 인스턴스만 존재해야 함
	mApp = this;				// 생성자에서 자신을 등록
}
Init::~Init() { mApp = nullptr; }	// 소멸자에서 자신을 해제

// 클래스 밖의 전역 함수 - 이게 Windows에 넘어감
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return Init::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

LRESULT Init::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool Init::InitWindow(HINSTANCE hInstance)
{
	WNDCLASS wc = {};
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = MainWndProc;								// <- 전역 함수이게 정상 등록
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpszClassName = L"MainWnd";

	if (!RegisterClass(&wc))
	{
		DWORD err = GetLastError();
		wchar_t buf[128];
		swprintf_s(buf, L"RegisterClass Failed. GetLastError = %lu", err);
		MessageBox(nullptr, buf, L"Error", MB_OK);
		return false;
	}

	RECT R = { 0, 0, mClientWidth, mClientHeight };
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, FALSE);	// 클라이언트 영역 기준 보정

	mhMainWnd = CreateWindow(L"MainWnd", L"Direct3D 12 Init", 
			WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 
			R.right - R.left, R.bottom - R.top, 
			nullptr, nullptr, hInstance, nullptr);

	if(!mhMainWnd)
	{
		DWORD err = GetLastError();
		wchar_t buf[128];
		swprintf_s(buf, L"CreateWindow Failed. GetLastError = %lu", err);
		MessageBox(nullptr, buf, L"Error", MB_OK);
		return false;
	}

	ShowWindow(mhMainWnd, SW_SHOW);
	UpdateWindow(mhMainWnd);
	return true;
}

bool Init::Initialize()
{
	if(!InitWindow(mhAppInst)) return false;
	if(!InitD3D()) return false;
	return true;
}
int Init::Run()
{
	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			Draw();
		}
	}
	FlushCommandQueue();
	return static_cast<int>(msg.wParam);
}

bool Init::InitD3D()
{
// 디버깅용
#if defined(DEBUG) || defined(_DEBUG)
{
	ComPtr<ID3D12Debug> debugController;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
	debugController->EnableDebugLayer();
	
}
#endif

	/*
	* 1. 장치 생성
	* DXGI 팩토리 생성, 디바이스 생성, 하드웨어 어댑터 생성
	*/
	// DXGI 팩토리 생성
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&g_dxgiFactory)));
	// 하드웨어 어댑터 자치 생성
	HRESULT hardwareResult = D3D12CreateDevice(
		nullptr,								// nullptr = default adapter = 0
		D3D_FEATURE_LEVEL_11_0,					// 최소 지원 기능 레벨 dx 11.0
		IID_PPV_ARGS(&g_device));				// output device pointer

	// 하드웨어 디바이스 생성 실패 시, WARP 디바이스 생성
	if(FAILED(hardwareResult))
	{
		ComPtr<IDXGIAdapter> pWarpAdapter;
		ThrowIfFailed(g_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&pWarpAdapter)));			// WARP 가져오기
		ThrowIfFailed(D3D12CreateDevice(
			pWarpAdapter.Get(),						// WARP adapter
			D3D_FEATURE_LEVEL_11_0,					// 최소 지원 기능 레벨 dx 11.0
			IID_PPV_ARGS(&g_device)));				// output device pointer
	}

	/*
	* 2. Fence 생성, 서술자 크기 가져오기
	*/
	// Fence 생성
	ThrowIfFailed(g_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_fence)));
	// 서술자 크기 가져오기
	g_rtvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	g_dsvDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
	g_cbvSrvUavDescriptorSize = g_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	/*
	* 3. 4x MSAA 지원 여부 확인
	*/
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	msQualityLevels.Format = mBackBufferFormat;	// 백버퍼 포맷(D3D_DRIVER_TYPE_HARDWARE)
	msQualityLevels.SampleCount = 4;				// 샘플링 카운트
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE; // 멀티샘플링 품질 수준 플래그
	msQualityLevels.NumQualityLevels = 0;		// 지원되는 멀티샘플링 품질 수준 수
	ThrowIfFailed(g_device->CheckFeatureSupport(
		D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,	// 멀티샘플링 품질 수준 확인
		&msQualityLevels,							// 멀티샘플링 품질 수준 구조체
		sizeof(msQualityLevels)));					// 구조체 크기


	m4xMsaaQuality = msQualityLevels.NumQualityLevels;	// 지원되는 멀티샘플링 품질 수준 수
	assert(m4xMsaaQuality > 0 && "Unexpected MSAA quality level.");	// 지원되는 멀티샘플링 품질 수준 수가 0이면 오류

	/*
	* 4. 명령 대기열과 명력 목록 생성
	*/
	CreateCommandObjects();	// 명령 대기열과 명령 목록 생성

	return true;
}

/*
* 4. 명령 대기열과 명력 목록 생성
*/
void Init::CreateCommandObjects()
{
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;		// 명령 대기열 타입
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;		// 명령 대기열 플래그
	ThrowIfFailed(g_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&g_commandQueue)));		// 명령 대기열 생성

	ThrowIfFailed(g_device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,				// 명령 목록 타입
		IID_PPV_ARGS(g_commandAllocator.GetAddressOf())));			// 명령 할당자 생성

	ThrowIfFailed(g_device->CreateCommandList(
		0,											// 노드 마스크
		D3D12_COMMAND_LIST_TYPE_DIRECT,				// 명령 목록 타입
		g_commandAllocator.Get(),					// 명령 할당자
		nullptr,									// 초기 파이프라인 상태 객체
		IID_PPV_ARGS(g_commandList.GetAddressOf())));			// 명령 목록 생성

	// 명령 목록은 생성 시점에 열려있으므로, 닫아야 한다.
	// Reset을 호출하는데, Reset을 호출하려면 명령 목록이 닫혀있어야 한다.
	g_commandList->Close();
}

void Init::FlushCommandQueue()
{
	mCurrBackBuffer++;					
	ThrowIfFailed(g_commandQueue->Signal(g_fence.Get(), mCurrnetFence));

	if (g_fence->GetCompletedValue() < mCurrnetFence)
	{
		// 큐에 적재한 명령들을 eventHandle을 통해 작업을 끝내 SIngle을 보낼때까지 Wait하다 종료.
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
		ThrowIfFailed(g_fence->SetEventOnCompletion(mCurrnetFence, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
}

void Init::Draw()
{
}

/*
* 5. 교환 사슬의 서술과 생성
*/
void Init::CreateSwapChain()
{
	// 새 교환 사슬을 생성하기 전에 먼저 기존 교환 사슬을 해제한다.
	g_swapChain.Reset();

	DXGI_SWAP_CHAIN_DESC sd;
	sd.BufferDesc.Width = mClientWidth;
	sd.BufferDesc.Height = mClientHeight;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = mBackBufferFormat;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	sd.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	sd.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = SwapChainBufferCount;
	sd.OutputWindow = mhMainWnd;
	sd.Windowed = true;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	// 참고: 교환 사슬은 명령 대기열을 이용해서 방출(flush)을 수행한다.
	ThrowIfFailed(g_dxgiFactory->CreateSwapChain(
		g_commandQueue.Get(),
		&sd,
		g_swapChain.GetAddressOf()));
}

/*
* 6. 서술자 힙 생성
*/
void Init::CreateRtvAndDsvDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvHeapDesc.NodeMask = 0;
	ThrowIfFailed(g_device->CreateDescriptorHeap(
		&rtvHeapDesc, IID_PPV_ARGS(g_rtvHeap.GetAddressOf())
	));

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(g_device->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(g_dsvHeap.GetAddressOf())
	));
}


D3D12_CPU_DESCRIPTOR_HANDLE Init::CurrentBackBufferView()const
{
	// 편의를 위해 D3D12_CPU_DESCRIPTOR_HANDLE의 생성자를 사용한다.
	// 이 생성자는 주어진 오프셋에 해당하는 후면 버퍼 RTV의 핸들(D3D12_CPU_DESCRIPTOR_HANDLE)을 돌려준다.
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		g_rtvHeap->GetCPUDescriptorHandleForHeapStart(),		// 첫 핸들
		mCurrBackBuffer,										// 오프셋 색인
		g_rtvDescriptorSize										// 서술자의 바이트 크기
	);
}
D3D12_CPU_DESCRIPTOR_HANDLE Init::DepthStencilView()const
{
	 return g_dsvHeap->GetCPUDescriptorHandleForHeapStart();
}


void Init::OnResize()
{
	/*
	* 7. 렌더 대상 뷰(RTV) 생성
	*/
	assert(g_device);
	assert(g_swapChain);
	assert(g_commandAllocator);

	// 자원을 변경하기전 flush
	FlushCommandQueue();

/*		블록 시작 전에		*/
	ThrowIfFailed(g_commandList->Reset(g_commandAllocator.Get(), nullptr));

	// 다시 생성할 이전 리소스를 해제
	for(int i = 0; i < SwapChainBufferCount; ++i)
		g_SwapChainBuffer[i].Reset();
	g_depthStencilBuffer.Reset();

	// 스왑 체인 사이즈 재조정
	ThrowIfFailed(g_swapChain->ResizeBuffers(
		SwapChainBufferCount,
		mClientWidth, mClientHeight,
		mBackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
	));

	mCurrBackBuffer = 0;

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(
		g_rtvHeap->GetCPUDescriptorHandleForHeapStart());
	for (UINT i = 0; i < SwapChainBufferCount; ++i)
	{
		// 교환 사슬의 i번째 버퍼를 얻는다.
		ThrowIfFailed(g_swapChain->GetBuffer(i, IID_PPV_ARGS(&g_SwapChainBuffer[i])));
		// 그 버퍼에 대한 RTV를 생성한다.
		g_device->CreateRenderTargetView(
			g_SwapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
		// 힙의 다음 항목으로 넘어간다.
		rtvHeapHandle.Offset(i, g_rtvDescriptorSize);
	}

	/*
	* 8. 깊이-스텐실 버퍼와 뷰 생성
	*/
	// 깊이-스텐실 버퍼와 뷰를 생성한다.
	// 리소스 서술 - 깊이 버퍼는 사실 2D 텍스처이다.
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = mClientWidth;
	depthStencilDesc.Height = mClientHeight;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.Format = mDepthStencilFormat;
	depthStencilDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	depthStencilDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	// 클리어 최적화 값 - 이 값으로 지울 거라고 미리 알려줌
	D3D12_CLEAR_VALUE optClear = {};
	optClear.Format = mDepthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;			// 가장 먼 깊이
	optClear.DepthStencil.Stencil = 0;

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
	ThrowIfFailed(g_device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,		// 초기 상태
		&optClear,
		IID_PPV_ARGS(g_depthStencilBuffer.GetAddressOf())));

	// 전체 자원이 밉맵 수준 0에 대한 서술자를, 해당 자원의 픽셀 형식을 적용해서 생성한다.
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = mDepthStencilFormat;
	dsvDesc.Texture2D.MipSlice = 0;
	g_device->CreateDepthStencilView(
		g_depthStencilBuffer.Get(),				// 어떤 리소스를
		&dsvDesc,								// 어떻게 볼지
		DepthStencilView());					// 힙 어디에 쓸지
	
	// 자원을 초기 상태에서 깊이 버퍼로 사용할 수 있는 상태로 전이한다.
	// COMMON으로 만들었으니 깊이 쓰기 용도로 쓰겠다고 알려줌
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		g_depthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_DEPTH_WRITE);
	g_commandList->ResourceBarrier(1, &barrier);

/*		블록 끝난 후		*/
	// 명령 목록을 닫은 후에 목록을 가져와 큐에 실어서 실행해준다.
	ThrowIfFailed(g_commandList->Close());
	ID3D12CommandList* cmdsLists[] = { g_commandList.Get() };
	g_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// 사이즈 조정을 끝낼때까지 대기 후 다음 실행
	FlushCommandQueue();

	/*
	* 9. 뷰포트 설정 (명령 목록을 재설정(Reset)하면 뷰포트들도 재설정 해야함)
	*/
	mScreenViewport.TopLeftX	= 0;
	mScreenViewport.TopLeftY	= 0;
	mScreenViewport.Width		= static_cast<float>(mClientWidth);
	mScreenViewport.Height		= static_cast<float>(mClientHeight);
	mScreenViewport.MinDepth	= 0.0f;
	mScreenViewport.MaxDepth	= 1.0f;

	g_commandList->RSSetViewports(0, &mScreenViewport);

	/*
	* 10. 가위 직사각형 설정 (명령 목록을 재설정(Reset)하면 가위 직사각형들도 재설정 해야함)
	*/
	mScissorRect = { 0, 0, mClientWidth/2, mClientHeight/2};
	g_commandList->RSSetScissorRects(0, &mScissorRect);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	try
	{
		Init theApp(hInstance);
		if (!theApp.Initialize()) return 0;
		return theApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}