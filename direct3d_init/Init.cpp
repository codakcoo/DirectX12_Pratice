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
}

void Init::Draw()
{
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