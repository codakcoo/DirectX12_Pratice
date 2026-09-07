#include "Init.h"
#include <DirectXMath.h>
#include <string>
#include <assert.h>

Init* Init::mApp = nullptr;						// 정적 멤버는 .cpp에서 정의 필수
Init* Init::GetApp() { return mApp; }

Init::Init(HINSTANCE hInstance) : mhAppInst(hInstance)
{
	assert(mApp == nullptr);	// 유일한 인스턴스만 존재해야 함
	mApp = this;				// 생성자에서 자신을 등록
}
Init::~Init() { if(g_device != nullptr) FlushCommandQueue(); mApp = nullptr; }	// 소멸자에서 자신을 해제

// 클래스 밖의 전역 함수 - 이게 Windows에 넘어감
LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	Init* app = Init::GetApp();
	if (app == nullptr)
		return DefWindowProc(hwnd, msg, wParam, lParam);   // 객체 없으면 기본 처리로 넘김

	return app->MsgProc(hwnd, msg, wParam, lParam);
}

bool Init::Get4xMsaaState()const
{
	return m4xMsaaState;
}

void Init::Set4xMsaaState(bool value)
{
	if (m4xMsaaState != value)
	{
		m4xMsaaState = value;

		// Recreate the swapchain and buffers with new multisample settings.
		CreateSwapChain();
		OnResize();
	}
}

LRESULT Init::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			mAppPaused = true;
			mTimer.Stop();
		}
		else
		{
			mAppPaused = false;
			mTimer.Start();
		}
		return 0;

	// WM_ENTERSIZEMOVE는 사용자가 크기 변경 테두리를 잡으면 전달된다.
	case WM_ENTERSIZEMOVE:
		mAppPaused = true;
		mResizing = true;
		mTimer.Stop();
		return 0;

	// WM_EXITSIZEMOVE는 사용자가 크기 변경 테두리를 놓으면 전달된다.
	// 그러면 차으 ㅣ새 크기에 맞게 모든 것을 재설정한다.
	case WM_EXITSIZEMOVE:
		mAppPaused = false;
		mResizing = false;
		mTimer.Start();
		OnResize();
		return 0;

	// WM_DESTROY는 창이 파괴되려 할 때 전달된다.
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	// WM_MENUCHAR 메시지는 메뉴가 활성화되어서 사용자가 키를
	// 눌렀지만 그 키가 그 어떤 니모닉이나 단축키에도 해당하지
	// 않을 때 전달된다.
	case WM_MENUCHAR:
		// Alt-Enter를 눌렀을 때 삐 소리가 나지 않게 한다.
		return MAKELRESULT(0, MNC_CLOSE);

	// 창이 너무 작아지지 않게 하기 위해 이 메시지를 처리한다.
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200;
		return 0;

	// 마우스 입력 처리용 가상 함수들 정의(GET_X_LPARAM, GET_Y_LPARAM 매크로를 사용하기 위해서 Windowsx.h 를 포함시켜야 함)
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_KEYUP:
		if (wParam == VK_ESCAPE)
		{
			PostQuitMessage(0);
		}
		else if ((int)wParam == VK_F2)
			Set4xMsaaState(!m4xMsaaState);

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

	OnResize();

	return true;
}
int Init::Run()
{
	MSG msg = {};
	mTimer.Reset();				// 루피 진입 전 1회

	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			mTimer.Tick();					// 매 프레임
			if (!mAppPaused)
			{
				CalculateFrameState();
				Update(mTimer);
				Draw();
			}
			else
			{
				Sleep(100);
			}
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
	CreateCommandObjects();				// 명령 대기열과 명령 목록 생성
	CreateSwapChain();					// 스왑체인 생성
	CreateRtvAndDsvDescriptorHeaps();	// Rtv(렌더대상), Dsv(딥스텐실뷰) 생성

	ThrowIfFailed(g_commandList->Reset(g_commandAllocator.Get(), nullptr));	// 명령 목록 초기화)

	LoadTextures();
	BuildRootSignature();						// 루트 서명 생성
	BuildDescriptorHeaps();						// 서술자 힙 생성
	//BuildConstantBuffers();						// 상수 버퍼 생성
	BuildSrvHeap();								// LoadTextures() 다음에
	BuildShadersAndInputLayout();				// 쉐이더와 입력 레이아웃 생성
	BuildBoxGeometry();							// 박스 지오메트리 생성, 여기서 정점/인덱스 버퍼 업로드 명령 기록
	BuildFrameResources();						// 디바이스만 있으면 되니 근처 아무데나(g_device만 있으됨)
	//BuildRenderItems();
	BuildPSO();									// 파이프라인 상태 객체 생성

	ThrowIfFailed(g_commandList->Close());	// 명령 목록 닫기
	ID3D12CommandList* cmdsLists[] = { g_commandList.Get() };
	g_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);	// 명령 목록 실행
	FlushCommandQueue();	// 업로드 완료까지 대기 - 업로드 버퍼 해제해도 안전해짐

	return true;
}

void Init::Update(const GameTimer& gt)
{
	// 대기는 여기서....
	// 프레임 링을 순환
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % NumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();

	// 이 프레임 자원이 아직도 GPU에서 사용 중이면(3프레임 전이니 보통은 끝나있음) 대기
	if (mCurrFrameResource->Fence != 0 && g_fence->GetCompletedValue() < mCurrFrameResource->Fence)
	{
		HANDLE eventHandle = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);						// 이벤트 핸들 생성(전부 실행)
		ThrowIfFailed(g_fence->SetEventOnCompletion(mCurrFrameResource->Fence, eventHandle));			// 이벤트 실행
		WaitForSingleObject(eventHandle, INFINITE);														// 이벤트가 시그널을 보낼때까지 INFINITE 대기
		CloseHandle(eventHandle);
	}

	// 뷰/투영은 공통이니 한 번만
	// 뷰 행렬 - 카메라가 고정이면 여기서 한 번만 계산해도 되지만
	// 지금은 이해를 위해 그냥 매 프레임 계산
	XMVECTOR pos = XMVectorSet(0.0f, 3.0f, -15.0f, 1.0f);		// 카메라를 -z에서 원점 바라보게
	XMVECTOR target = XMVectorZero();									// 원점
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);		// y축이 위쪽
	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMMATRIX proj = XMMatrixPerspectiveFovLH(0.25f * XM_PI, (float)mClientWidth / mClientHeight, 1.0f, 1000.0f);	// 투영 행렬
	XMMATRIX viewProj = view * proj;

	PassConstants passCB;
	XMStoreFloat4x4(&passCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat3(&passCB.EyePosW, pos);
	passCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };

	passCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	passCB.Lights[0].Strength = { 0.8f, 0.8f, 0.7f };

	mCurrFrameResource->PassCB->CopyData(0, passCB);						// 패스는 슬롯 1개

	// 물체마다 개별 계산해서 각자의 슬롯(index)에 복사
	for (int i = 0; i < NumObjects; ++i)
	{
		mObjectThetas[i] += gt.DeltaTime() * (1.0f + i * 0.1f);					// 물체마다 속도 다르게
		XMMATRIX baseTranslate = XMLoadFloat4x4(&mObjectWorlds[i]);
		XMMATRIX spin = XMMatrixRotationY(mObjectThetas[i]);
		XMMATRIX world = spin * baseTranslate;									// 자전 후 배치 위치로 이동

		ObjectConstants objConstants;
		XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));	// HLSL은 행우선이므로 전치행렬로 변환
		mCurrFrameResource->ObjectCB->CopyData(i, objConstants);	// i번 슬롯에 상수 버퍼에 복사
	}
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
	mCurrnetFence++;
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
	auto cmdListAlloc = mCurrFrameResource->CmdListAlloc;			// FrameResource의 얼로케이터

	ThrowIfFailed(cmdListAlloc->Reset());							// FrameResource에 있는 얼로케이터를 Reset
	ThrowIfFailed(g_commandList->Reset(cmdListAlloc.Get(), mPSO.Get()));

	// 재설정하기 위해 타입을 변경함.
	// 표현(Present) -> 렌더 대상(Render_Target)
	auto toRT = CD3DX12_RESOURCE_BARRIER::Transition(
		CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	g_commandList->ResourceBarrier(1, &toRT);

	// 명령대기를 재설정(Reset)했기에 뷰포트, 가위를 재설정
	g_commandList->RSSetViewports(1, &mScreenViewport);
	g_commandList->RSSetScissorRects(1, &mScissorRect);

	const float clearColor[] = { 0.68f, 0.77f, 0.87f, 1.0f };			// LightSteelBlue
	g_commandList->ClearRenderTargetView(CurrentBackBufferView(), clearColor, 0, nullptr);
	g_commandList->ClearDepthStencilView(DepthStencilView(),
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	auto rtv = CurrentBackBufferView();
	auto dsv = DepthStencilView();
	g_commandList->OMSetRenderTargets(1, &rtv, true, &dsv);

	g_commandList->SetPipelineState(mPSO.Get());
	g_commandList->SetGraphicsRootSignature(mRootSignature.Get());



	auto vbv = mBoxGeo->VertexBufferView();
	auto ibv = mBoxGeo->IndexBufferView();
	g_commandList->IASetVertexBuffers(0, 1, &vbv);
	g_commandList->IASetIndexBuffer(&ibv);
	g_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//// CBV 힙 대신 FrameResouce의 상수 버퍼 GPU 주소를 직접 넘김
	//D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = mCurrFrameResource->ObjectCB->Resource()->GetGPUVirtualAddress();
	//g_commandList->SetGraphicsRootConstantBufferView(0, objCBAddress);

	//g_commandList->DrawIndexedInstanced(36, 1, 0, 0, 0);

	ID3D12DescriptorHeap* heaps[] = { mSrvHeap.Get() };
	g_commandList->SetDescriptorHeaps(1, heaps);

	g_commandList->SetGraphicsRootDescriptorTable(0, mSrvHeap->GetGPUDescriptorHandleForHeapStart());

	D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = mCurrFrameResource->PassCB->Resource()->GetGPUVirtualAddress();
	g_commandList->SetGraphicsRootConstantBufferView(2, passCBAddress);				// 슬롯 1, 프레임당 한번만

	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	D3D12_GPU_VIRTUAL_ADDRESS objCBBase = mCurrFrameResource->ObjectCB->Resource()->GetGPUVirtualAddress();
	
	for (int i = 0; i < NumObjects; ++i)
	{
		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objCBBase + i * objCBByteSize;			// 물체별 주소
		g_commandList->SetGraphicsRootConstantBufferView(1, objCBAddress);
		g_commandList->DrawIndexedInstanced(36, 1, 0, 0, 0);
	}

	// 재설정을 완료했기에 다시 타입을 바꿈
	// 렌더 대상(Render_Target) -> 표현(Present)
	auto toPresent = CD3DX12_RESOURCE_BARRIER::Transition(
		CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT);
	g_commandList->ResourceBarrier(1, &toPresent);

	ThrowIfFailed(g_commandList->Close());
	ID3D12CommandList* cmdsLists[] = { g_commandList.Get() };
	g_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	ThrowIfFailed(g_swapChain->Present(0,0));
	mCurrBackBuffer = (mCurrBackBuffer+1) % SwapChainBufferCount;

	mCurrFrameResource->Fence = ++mCurrnetFence;
	g_commandQueue->Signal(g_fence.Get(), mCurrnetFence);			// 안기다리고 바로 리턴 -> Update문에 있음....
	//FlushCommandQueue();
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

ID3D12Resource* Init::CurrentBackBuffer() const
{
	return g_SwapChainBuffer[mCurrBackBuffer].Get();
}
D3D12_CPU_DESCRIPTOR_HANDLE Init::CurrentBackBufferView() const
{
	// 편의를 위해 D3D12_CPU_DESCRIPTOR_HANDLE의 생성자를 사용한다.
	// 이 생성자는 주어진 오프셋에 해당하는 후면 버퍼 RTV의 핸들(D3D12_CPU_DESCRIPTOR_HANDLE)을 돌려준다.
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		g_rtvHeap->GetCPUDescriptorHandleForHeapStart(),		// 첫 핸들
		mCurrBackBuffer,										// 오프셋 색인
		g_rtvDescriptorSize);									// 서술자의 바이트 크기
}
D3D12_CPU_DESCRIPTOR_HANDLE Init::DepthStencilView()const
{
	 return g_dsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void Init::CalculateFrameState()
{
	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	if (mTimer.TotalTime() - timeElapsed >= 1.0f)		// 1초마다
	{
		float fps = (float)frameCnt;
		float mfps = 1000.0f / fps;

		std::wstring text = L"Direct3D 12 Init     fps: " + std::to_wstring((int)fps)
							+ L"      mfps: " + std::to_wstring(mfps);
		SetWindowText(mhMainWnd, text.c_str());

		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}

void Init::BuildConstantBuffers()
{
	mObjectCB = std::make_unique<UploadBuffer<ObjectConstants>>(g_device.Get(), 1, true);

	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	D3D12_GPU_VIRTUAL_ADDRESS cbAddress = mObjectCB->Resource()->GetGPUVirtualAddress();

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
	cbvDesc.BufferLocation = cbAddress;
	cbvDesc.SizeInBytes = objCBByteSize;

	g_device->CreateConstantBufferView(&cbvDesc, mCbvHeap->GetCPUDescriptorHandleForHeapStart());
}

void Init::BuildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
	cbvHeapDesc.NumDescriptors = 1;
	cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	g_device->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mCbvHeap));
}

void Init::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);		// t0,개수 1
	
	// cbv의 힙을 사용하지 않고 루트 디스크립터 방식으로 GPU 주소로 바로 때려박기 때문에 heap(공간), table(참조)를 안만들어도 됨.
	CD3DX12_ROOT_PARAMETER slotRootParameter[3];
	slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);			
	slotRootParameter[1].InitAsConstantBufferView(0);			// b0 - 물체별
	slotRootParameter[2].InitAsConstantBufferView(1);			// b1 - 패스별

	// 정적 샘플러 - 지난번 얘기한 그 방식, 별도 힙 불필요
	CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		0, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP);

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter, 1, &linearWrap, 
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if(errorBlob != nullptr)
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	ThrowIfFailed(hr);

	ThrowIfFailed(g_device->CreateRootSignature(0,
		serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&mRootSignature)));
}

void Init::BuildBoxGeometry()
{
	std::array<Vertex, 24> vertices =
	{
		// 앞면
		Vertex({ XMFLOAT3(-1,-1,-1), XMFLOAT3(0,0,-1), XMFLOAT2(0.0f, 1.0f) }),
		Vertex({ XMFLOAT3(-1,+1,-1), XMFLOAT3(0,0,-1), XMFLOAT2(0.0f, 0.0f) }),
		Vertex({ XMFLOAT3(+1,+1,-1), XMFLOAT3(0,0,-1), XMFLOAT2(1.0f, 0.0f) }),
		Vertex({ XMFLOAT3(+1,-1,-1), XMFLOAT3(0,0,-1), XMFLOAT2(1.0f, 1.0f) }),
		// 뒷면
		Vertex({ XMFLOAT3(-1,-1,+1), XMFLOAT3(0,0,1), XMFLOAT2(1.0f, 1.0f) }),
		Vertex({ XMFLOAT3(+1,-1,+1), XMFLOAT3(0,0,1), XMFLOAT2(0.0f, 1.0f) }),
		Vertex({ XMFLOAT3(+1,+1,+1), XMFLOAT3(0,0,1), XMFLOAT2(0.0f, 0.0f) }),
		Vertex({ XMFLOAT3(-1,+1,+1), XMFLOAT3(0,0,1), XMFLOAT2(1.0f, 0.0f) }),
		// 윗면
		Vertex({ XMFLOAT3(-1,+1,-1), XMFLOAT3(0,1,0), XMFLOAT2(0.0f, 1.0f) }),
		Vertex({ XMFLOAT3(-1,+1,+1), XMFLOAT3(0,1,0), XMFLOAT2(0.0f, 0.0f) }),
		Vertex({ XMFLOAT3(+1,+1,+1), XMFLOAT3(0,1,0), XMFLOAT2(1.0f, 0.0f) }),
		Vertex({ XMFLOAT3(+1,+1,-1), XMFLOAT3(0,1,0), XMFLOAT2(1.0f, 1.0f) }),
		// 아랫면
		Vertex({ XMFLOAT3(-1,-1,-1), XMFLOAT3(0,-1,0), XMFLOAT2(0.0f, 1.0f) }),
		Vertex({ XMFLOAT3(+1,-1,-1), XMFLOAT3(0,-1,0), XMFLOAT2(0.0f, 0.0f) }),
		Vertex({ XMFLOAT3(+1,-1,+1), XMFLOAT3(0,-1,0), XMFLOAT2(1.0f, 0.0f) }),
		Vertex({ XMFLOAT3(-1,-1,+1), XMFLOAT3(0,-1,0), XMFLOAT2(1.0f, 1.0f) }),
		// 왼쪽면
		Vertex({ XMFLOAT3(-1,-1,+1), XMFLOAT3(-1,0,0), XMFLOAT2(1.0f, 1.0f) }),
		Vertex({ XMFLOAT3(-1,+1,+1), XMFLOAT3(-1,0,0), XMFLOAT2(0.0f, 1.0f) }),
		Vertex({ XMFLOAT3(-1,+1,-1), XMFLOAT3(-1,0,0), XMFLOAT2(0.0f, 0.0f) }),
		Vertex({ XMFLOAT3(-1,-1,-1), XMFLOAT3(-1,0,0), XMFLOAT2(1.0f, 0.0f) }),
		// 오른쪽면
		Vertex({ XMFLOAT3(+1,-1,-1), XMFLOAT3(1,0,0), XMFLOAT2(0.0f, 1.0f) }),
		Vertex({ XMFLOAT3(+1,+1,-1), XMFLOAT3(1,0,0), XMFLOAT2(0.0f, 0.0f) }),
		Vertex({ XMFLOAT3(+1,+1,+1), XMFLOAT3(1,0,0), XMFLOAT2(1.0f, 0.0f) }),
		Vertex({ XMFLOAT3(+1,-1,+1), XMFLOAT3(1,0,0), XMFLOAT2(1.0f, 1.0f) }),
	};

	std::array<std::uint16_t, 36> indices =
	{
		0,1,2, 0,2,3,       // 앞
		4,5,6, 4,6,7,       // 뒤
		8,9,10, 8,10,11,    // 위
		12,13,14, 12,14,15, // 아래
		16,17,18, 16,18,19, // 왼쪽
		20,21,22, 20,22,23  // 오른쪽
	};

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	mBoxGeo = std::make_unique<MeshGeometry>();

	mBoxGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
		g_device.Get(),
		g_commandList.Get(),
		vertices.data(),
		vbByteSize,
		mBoxGeo->VertexBufferUploader);
	mBoxGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
		g_device.Get(),
		g_commandList.Get(),
		indices.data(),
		ibByteSize,
		mBoxGeo->IndexBufferUploader);

	mBoxGeo->VertexByteStride = sizeof(Vertex);
	mBoxGeo->VertexBufferByteSize = vbByteSize;
	mBoxGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	mBoxGeo->IndexBufferByteSize = ibByteSize;
}

void Init::BuildFrameResources()
{
	for (int i = 0; i < NumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(g_device.Get(), 1, NumObjects));			// 물체 개수 NumObjects개
	}
}

void Init::BuildRenderItems()
{
	mObjectWorlds.resize(NumObjects);
	mObjectThetas.resize(NumObjects);

	int idx = 0;
	for (int x = -1; x <= 1; ++x)
	for (int y = -1; y <= 1; ++y)
	for (int z = -1; z <= 1; ++z)
	{
		XMMATRIX translate = XMMatrixTranslation(x * 3.0f, y * 3.0f, z * 3.0f);			// 3칸 간격
		XMStoreFloat4x4(&mObjectWorlds[idx], translate);
		mObjectThetas[idx] = 0.0f;
		idx++;
	}
}

void Init::BuildShadersAndInputLayout()
{
	mvsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "VS", "vs_5_0");
	mpsByteCode = d3dUtil::CompileShader(L"Shaders\\color.hlsl", nullptr, "PS", "ps_5_0");

	mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(Vertex, Normal), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(Vertex, TexC), D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void Init::BuildPSO()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	psoDesc.pRootSignature = mRootSignature.Get();
	psoDesc.VS = { reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()), mvsByteCode->GetBufferSize() };
	psoDesc.PS = { reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()), mpsByteCode->GetBufferSize() };
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;	// 뒷면 제거
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = mBackBufferFormat;
	psoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	psoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	psoDesc.DSVFormat = mDepthStencilFormat;

	ThrowIfFailed(g_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mPSO)));
}

/*
* 텍스처 로드 함수는 반드시 커맨드 리스트가 열려있을 떄 호출해야 됨.
*/
void Init::LoadTextures()
{
	mBoxTex = std::make_unique<Texture>();
	mBoxTex->name = "boxTex";
	mBoxTex->Filename = L"Textures\\WoodCrate01.dds";		// 확보한 dds 경로/이름 맞추기

	// CreateDDSTextureFromFile12가 내부 업로드 -> 디폴트 힙 복사 명령을
	// 커맨드 리스트에 기록함.
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(
		g_device.Get(), g_commandList.Get(),
		mBoxTex->Filename.c_str(),
		mBoxTex->Resource, mBoxTex->UploadHeap));
}

void Init::BuildSrvHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 1;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;		// 필수
	ThrowIfFailed(g_device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvHeap)));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = mBoxTex->Resource->GetDesc().Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = mBoxTex->Resource->GetDesc().MipLevels;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	g_device->CreateShaderResourceView(mBoxTex->Resource.Get(), &srvDesc,
		mSrvHeap->GetCPUDescriptorHandleForHeapStart());
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
		rtvHeapHandle.Offset(1, g_rtvDescriptorSize);
	}

	/*
	* 8. 깊이-스텐실 버퍼와 뷰 생성
	*/
	// 깊이-스텐실 버퍼와 뷰를 생성한다.
	// 리소스 서술 - 깊이 버퍼는 사실 2D 텍스처이다.
	D3D12_RESOURCE_DESC depthStencilDesc = {};
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


	/*
	* 10. 가위 직사각형 설정 (명령 목록을 재설정(Reset)하면 가위 직사각형들도 재설정 해야함)
	*/
	mScissorRect = { 0, 0, mClientWidth, mClientHeight};
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
