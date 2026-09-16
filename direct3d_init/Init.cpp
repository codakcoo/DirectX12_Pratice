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
	BuildConstantBuffers();						// 상수 버퍼 생성
	BuildSrvHeap();								// LoadTextures() 다음에
	BuildShadersAndInputLayout();				// 쉐이더와 입력 레이아웃 생성
	BuildBoxGeometry();							// 박스 지오메트리 생성, 여기서 정점/인덱스 버퍼 업로드 명령 기록
	BuildFloorGeometry();						// quad 지오메트리 생성, 여기서 정점/인덱스 버퍼 업로드 명령 기록(반사용)
	BuildFrameResources();						// 디바이스만 있으면 되니 근처 아무데나(g_device만 있으됨)
	BuildRenderItems();
	BuildPSO();									// 파이프라인 상태 객체 생성
	
	BuildOffscreenResources();
	BuildOffscreenViews();
	BuildBlurResources();
	BuildBlurDescriptorHeap();
	BuildBlurRootSignature();
	BuildBlurPSO();

	BuildComputeRootSignature();				// Compute 루트 서명 생성
	BuildComputePSO();
	RunComputeTest();

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
	XMVECTOR pos = XMVectorSet(0.0f, 15.0f, -15.0f, 1.0f);		// 카메라를 -z에서 원점 바라보게
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

	// 바닥 (인덱스 27) - 회전 없이 고정
	{
		ObjectConstants floorConstants;
		XMStoreFloat4x4(&floorConstants.World, XMMatrixTranspose(XMMatrixIdentity()));
		mCurrFrameResource->ObjectCB->CopyData(27, floorConstants);
	}

	// 반사 큐브 (인덱스 28) - 0번 큐브를 y=-2 평면에 대해 반사
	{
		XMVECTOR mirrorPlane = XMVectorSet(0.0f, 1.0f, 0.0f, 2.0f);		// y=-2 평면 (ax+by+cz+d=0 -> y+2=0)
		XMMATRIX R = XMMatrixReflect(mirrorPlane);

		XMMATRIX cube0world = XMLoadFloat4x4(&mObjectWorlds[0]);		// 0번 큐브의 현재 월드
		XMMATRIX spin = XMMatrixRotationY(mObjectThetas[0]);
		XMMATRIX world = spin * cube0world * R;							// 큐브 변환 후 반사

		ObjectConstants reflectConstants;
		XMStoreFloat4x4(&reflectConstants.World, XMMatrixTranspose(world));
		mCurrFrameResource->ObjectCB->CopyData(28, reflectConstants);
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
	ThrowIfFailed(g_commandList->Reset(cmdListAlloc.Get(), mOpaquePSO.Get()));

	g_commandList->RSSetViewports(1, &mScreenViewport);
	g_commandList->RSSetScissorRects(1, &mScissorRect);

	// -- (A) 오프스크린 텍스처를 덴더 타켓 상태로 전이 --
	auto toRT = CD3DX12_RESOURCE_BARRIER::Transition(
		mOffscreenTex.Get(),
		D3D12_RESOURCE_STATE_COMMON,
		D3D12_RESOURCE_STATE_RENDER_TARGET);
	g_commandList->ResourceBarrier(1, &toRT);

	// -- (B) 오프스크린을 렌더 타켓으로 설정하고 씬 그리기 --
	auto offscreenRtv = OffscreenRtv();
	auto dsv = DepthStencilView();

	const float clearColor[] = { 0.68f, 0.77f, 0.87f, 1.0f };			// LightSteelBlue
	g_commandList->ClearRenderTargetView(offscreenRtv, clearColor, 0, nullptr);
	g_commandList->ClearDepthStencilView(dsv,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	g_commandList->OMSetRenderTargets(1, &offscreenRtv, true, &dsv);		// <- 백버퍼 아님!

	g_commandList->SetGraphicsRootSignature(mRootSignature.Get());

	ID3D12DescriptorHeap* heaps[] = { mSrvHeap.Get() };
	g_commandList->SetDescriptorHeaps(1, heaps);
	g_commandList->SetGraphicsRootDescriptorTable(0, mSrvHeap->GetGPUDescriptorHandleForHeapStart());

	D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = mCurrFrameResource->PassCB->Resource()->GetGPUVirtualAddress();
	g_commandList->SetGraphicsRootConstantBufferView(2, passCBAddress);				// 슬롯 1, 프레임당 한번만
	
	UINT objCBByteSize = d3dUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
	D3D12_GPU_VIRTUAL_ADDRESS objCBBase = mCurrFrameResource->ObjectCB->Resource()->GetGPUVirtualAddress();

	// 큐브들 그리기 (기존 그대로  불투명/반투명)
	auto vbv = mBoxGeo->VertexBufferView();
	auto ibv = mBoxGeo->IndexBufferView();
	g_commandList->IASetVertexBuffers(0, 1, &vbv);
	g_commandList->IASetIndexBuffer(&ibv);
	g_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	g_commandList->SetPipelineState(mOpaquePSO.Get());
	for (int i = 0; i < NumObjects; ++i)
	{
		if (mObjectTransparent[i]) continue;
		D3D12_GPU_VIRTUAL_ADDRESS addr = objCBBase + i * objCBByteSize;			// 물체별 주소
		g_commandList->SetGraphicsRootConstantBufferView(1, addr);
		g_commandList->DrawIndexedInstanced(36, 1, 0, 0, 0);
	}
	// 반투명 나중
	g_commandList->SetPipelineState(mTransparentPSO.Get());
	for (int i = 0; i < NumObjects; ++i)
	{
		if (!mObjectTransparent[i]) continue;
		D3D12_GPU_VIRTUAL_ADDRESS addr = objCBBase + i * objCBByteSize;			// 물체별 주소
		g_commandList->SetGraphicsRootConstantBufferView(1, addr);
		g_commandList->DrawIndexedInstanced(36, 1, 0, 0, 0);
	}

	// -- (C) 백퍼버로 복사하기 위한 상태 전이
	// 오프스크린: RENDER_TARGET -> COPY_SOURCE
	auto offToCopy = CD3DX12_RESOURCE_BARRIER::Transition(
		mOffscreenTex.Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_COPY_SOURCE);
	g_commandList->ResourceBarrier(1, &offToCopy);
	
	// 백버퍼: PRESENT -> COPY_DEST
	auto backToCopy = CD3DX12_RESOURCE_BARRIER::Transition(
		CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_COPY_DEST);
	g_commandList->ResourceBarrier(1, &backToCopy);

	// -- (D) 복사 --
	g_commandList->CopyResource(CurrentBackBuffer(), mOffscreenTex.Get());

	// -- (E) 백버퍼: COPY_DEST -> PRESENT --
	auto backToPresent = CD3DX12_RESOURCE_BARRIER::Transition(
		CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_COPY_DEST,
		D3D12_RESOURCE_STATE_PRESENT);
	g_commandList->ResourceBarrier(1, &backToPresent);

	// 오프스크린을 다음 프레임을 위해 COMMON으로
	auto offToCommon = CD3DX12_RESOURCE_BARRIER::Transition(
		mOffscreenTex.Get(),
		D3D12_RESOURCE_STATE_COPY_SOURCE,
		D3D12_RESOURCE_STATE_COMMON);
	g_commandList->ResourceBarrier(1, &offToCommon);


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

void Init::BuildComputeRootSignature()
{
	CD3DX12_ROOT_PARAMETER slotRootParameter[2];
	slotRootParameter[0].InitAsShaderResourceView(0);			// t0 - 입력 SRV
	slotRootParameter[1].InitAsUnorderedAccessView(0);			// u0 - 출력 UAV

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(2, slotRootParameter, 0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_NONE);

	ComPtr<ID3D10Blob> serializedRootSig = nullptr;
	ComPtr<ID3D10Blob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if(errorBlob != nullptr)
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	ThrowIfFailed(hr);

	ThrowIfFailed(g_device->CreateRootSignature(0,
		serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&mComputeRootSignature)));
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

void Init::BuildFloorGeometry()
{
	std::array<Vertex, 4> vertices =
	{
		Vertex({ XMFLOAT3(-10.0f, -2.0f, -10.0f), XMFLOAT3(0,1,0), XMFLOAT2(0.0f, 1.0f) }),
		Vertex({ XMFLOAT3(-10.0f, -2.0f, +10.0f), XMFLOAT3(0,1,0), XMFLOAT2(0.0f, 0.0f) }),
		Vertex({ XMFLOAT3(+10.0f, -2.0f, +10.0f), XMFLOAT3(0,1,0), XMFLOAT2(1.0f, 0.0f) }),
		Vertex({ XMFLOAT3(+10.0f, -2.0f, -10.0f), XMFLOAT3(0,1,0), XMFLOAT2(1.0f, 1.0f) }),
	};

	std::array<std::uint16_t, 6> indices = { 0, 1, 2, 0, 2, 3 };

	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	mFloorGeo = std::make_unique<MeshGeometry>();

	mFloorGeo->VertexBufferGPU = d3dUtil::CreateDefaultBuffer(
		g_device.Get(), g_commandList.Get(),
		vertices.data(), vbByteSize, mFloorGeo->VertexBufferUploader);


	mFloorGeo->IndexBufferGPU = d3dUtil::CreateDefaultBuffer(
		g_device.Get(), g_commandList.Get(),
		indices.data(), ibByteSize, mFloorGeo->IndexBufferUploader);

	mFloorGeo->VertexByteStride = sizeof(Vertex);
	mFloorGeo->VertexBufferByteSize = vbByteSize;
	mFloorGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	mFloorGeo->IndexBufferByteSize = ibByteSize;
}

void Init::BuildFrameResources()
{
	for (int i = 0; i < NumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(g_device.Get(), 1, NumObjects));			// 물체 개수 NumObjects개
	}
}

// 포멧을 mBackBufferFormat으로 맞춘 게 중요.
// 오프스크린 텍스처를 블러맵으로 복사하거나 읽을 때 포맷이 같아야 함.
void Init::BuildBlurResources()
{
	auto texDesc = mBoxTex->Resource->GetDesc();		// 원본과 같은 크기/포맷

	D3D12_RESOURCE_DESC blurTexDesc = {};
	blurTexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	blurTexDesc.Width = mClientWidth;
	blurTexDesc.Height = mClientHeight;
	blurTexDesc.DepthOrArraySize = 1;
	blurTexDesc.MipLevels = 1;
	blurTexDesc.Format = mBackBufferFormat;		// UAV 지원 포맷
	blurTexDesc.SampleDesc.Count = 1;
	blurTexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;		// UAV 필수

	auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	ThrowIfFailed(g_device->CreateCommittedResource(
		&defaultHeap, D3D12_HEAP_FLAG_NONE, &blurTexDesc,
		D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&mBlurMap0)));

	ThrowIfFailed(g_device->CreateCommittedResource(
		&defaultHeap, D3D12_HEAP_FLAG_NONE, &blurTexDesc,
		D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&mBlurMap1)));
}

void Init::BuildRenderItems()
{
	mObjectWorlds.resize(NumObjects);
	mObjectThetas.resize(NumObjects);
	mObjectTransparent.resize(NumObjects);
	for (int i = 0; i < NumObjects; ++i)
		mObjectTransparent[i] = (i % 3 == 0);			// 세 개 중 하나는 반투명

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
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc = {};
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS = { reinterpret_cast<BYTE*>(mvsByteCode->GetBufferPointer()), mvsByteCode->GetBufferSize() };
	opaquePsoDesc.PS = { reinterpret_cast<BYTE*>(mpsByteCode->GetBufferPointer()), mpsByteCode->GetBufferSize() };
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;	// 뒷면 제거
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.SampleDesc.Count = m4xMsaaState ? 4 : 1;
	opaquePsoDesc.SampleDesc.Quality = m4xMsaaState ? (m4xMsaaQuality - 1) : 0;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;

	ThrowIfFailed(g_device->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mOpaquePSO)));


	// 반투명 PSO - 위 설정을 복사해서 블렌드만 교체
	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;
	
	D3D12_RENDER_TARGET_BLEND_DESC transparentBlendDesc = {};
	transparentBlendDesc.BlendEnable = true;
	transparentBlendDesc.LogicOpEnable = false;
	transparentBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	transparentBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparentBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparentBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparentBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparentBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparentBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	transparentPsoDesc.BlendState.RenderTarget[0] = transparentBlendDesc;

	ThrowIfFailed(g_device->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&mTransparentPSO)));

	// -- (1) 스텐실 마킹 PSO - 바닥을 그리되 색/깊이는 안 남기고 스텐실만 1로 마킹
	D3D12_GRAPHICS_PIPELINE_STATE_DESC markPsoDesc = opaquePsoDesc;

	// 색 안 씀 (렌더타겟에 안 그림)
	CD3DX12_BLEND_DESC markBlend(D3D12_DEFAULT);
	markBlend.RenderTarget[0].RenderTargetWriteMask = 0;			// RGBA 다 안 씀
	markPsoDesc.BlendState = markBlend;

	// 깊이는 테스트하되 쓰진 않음, 스텐실은 항상 통과 + 참조값으로 교체
	CD3DX12_DEPTH_STENCIL_DESC markDS(D3D12_DEFAULT);
	markDS.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;			// 깊이 안 씀
	markDS.StencilEnable = true;
	markDS.StencilReadMask = 0xff;
	markDS.StencilWriteMask = 0xff;
	markDS.FrontFace.StencilFailOp		= D3D12_STENCIL_OP_KEEP;
	markDS.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	// StencilPassOp = REPLACE + StencilFunc = ALWAYS -> 이 픽셀을 그릴 때 스텐실 버퍼에 참조값을 무조건 써 넣어라 (마킹)
	markDS.FrontFace.StencilPassOp		= D3D12_STENCIL_OP_REPLACE;			// 통과하면 참조값으로 교체
	markDS.FrontFace.StencilFunc		= D3D12_COMPARISON_FUNC_ALWAYS;		// 항상 통과
	markDS.BackFace						= markDS.FrontFace;					// 뒷면도 동일하게
	markPsoDesc.DepthStencilState = markDS;

	ThrowIfFailed(g_device->CreateGraphicsPipelineState(&markPsoDesc, IID_PPV_ARGS(&mMarkStencilPSO)));

	// --  (2) 반사 PSO - 스텐실 값이 참조값과 같은 곳에만 그림
	D3D12_GRAPHICS_PIPELINE_STATE_DESC reflectPsoDesc = opaquePsoDesc;

	CD3DX12_DEPTH_STENCIL_DESC reflectDS(D3D12_DEFAULT);
	reflectDS.StencilEnable = true;
	reflectDS.StencilReadMask = 0xff;
	reflectDS.StencilWriteMask = 0xff;
	reflectDS.FrontFace.StencilFailOp		= D3D12_STENCIL_OP_KEEP;
	reflectDS.FrontFace.StencilDepthFailOp	= D3D12_STENCIL_OP_KEEP;
	reflectDS.FrontFace.StencilPassOp		= D3D12_STENCIL_OP_KEEP;
	// StencilFunc = EQUAL -> 스텐실 버퍼 값이 참조값과 같은 픽셀만 그려라 (마스킹)
	reflectDS.FrontFace.StencilFunc			= D3D12_COMPARISON_FUNC_EQUAL;		// 스텐실 == 참조값 일 때만
	reflectDS.BackFace = reflectDS.FrontFace;
	reflectPsoDesc.DepthStencilState = reflectDS;

	// 반사는 지오메트리가 뒤집혀서 winding이 반대가 되므로 컬링 방향도 뒤집어야 함
	CD3DX12_RASTERIZER_DESC reflectRS(D3D12_DEFAULT);
	// FrontCounterClockwise = true 는 반사 때문에 필요함.
	// 물체를 거울로 반전시키면 삼각형 감는 순서가 뒤짐힘.
	// 원래 앞면이던 게 뒷면 취급을 받아 컬링돼 사라지기 때문에, 컬링 기준을 반대로 뒤집어 줘야함.
	reflectRS.FrontCounterClockwise = true;				// 감기 방향 반전 대응
	reflectPsoDesc.RasterizerState = reflectRS;

	ThrowIfFailed(g_device->CreateGraphicsPipelineState(&reflectPsoDesc, IID_PPV_ARGS(&mReflectionPSO)));
}

void Init::BuildComputePSO()
{
	mComputeByteCode = d3dUtil::CompileShader(L"Shaders\\compute.hlsl", nullptr, "CS", "cs_5_0");

	D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
	computePsoDesc.pRootSignature = mComputeRootSignature.Get();
	computePsoDesc.CS = {
		reinterpret_cast<BYTE*>(mComputeByteCode->GetBufferPointer()),
		mComputeByteCode->GetBufferSize()
	};
	computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	ThrowIfFailed(g_device->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&mComputePSO)));
}

void Init::BuildOffscreenResources()
{
	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Width = mClientWidth;
	texDesc.Height = mClientHeight;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = mBackBufferFormat;
	texDesc.SampleDesc.Count = 1;
	// 이 텍스처는 두 역할을 함.
	// 씬을 그릴 때는 렌더 타켓 (RTV)
	// 블러할 때는 컴퓨트 입력 (SRV)
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET |
					D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;		// RTV + UAV 둘 다

	auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

	// 클리어 값 (렌더 타켓이니 지정)
	D3D12_CLEAR_VALUE optClear = {};
	optClear.Format = mBackBufferFormat;
	const float clearColor[] = { 0.68f, 0.77f, 0.87f, 1.0f };
	memcpy(optClear.Color, clearColor, sizeof(clearColor));

	ThrowIfFailed(g_device->CreateCommittedResource(
		&defaultHeap, D3D12_HEAP_FLAG_NONE, &texDesc,
		D3D12_RESOURCE_STATE_COMMON, &optClear,
		IID_PPV_ARGS(&mOffscreenTex)));
}

void Init::BuildOffscreenViews()
{
	// 오프스크린 RTV 힙 (1개)
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = 1;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(g_device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mOffscreenRtvHeap)));

	// 오프스크린 텍스처에 대한 RTV 생성
	g_device->CreateRenderTargetView(mOffscreenTex.Get(), nullptr,
		mOffscreenRtvHeap->GetCPUDescriptorHandleForHeapStart());
}

/*
* 블러에 필요한 뷰들
* 오프스크린 텍스처의 SRV (블러 입력으로 읽기; 1개)
* 블러맵0의 SRV, UAV	(2개)
* 블러맵1의 SRV, UAV	(2개)
*/
void Init::BuildBlurDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.NumDescriptors = 5;						// 오프스크린 SRV, 블러0 SRV/UAV, 블러1 SRV/UAV
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(g_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mBlurHeap)));

	UINT descSize = g_cbvSrvUavDescriptorSize;
	auto cpuStart = mBlurHeap->GetCPUDescriptorHandleForHeapStart();

	// 각 뷰를 힙의 순서대로 배치
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = mBackBufferFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = mBackBufferFormat;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	CD3DX12_CPU_DESCRIPTOR_HANDLE handle(cpuStart);

	// 슬롯 0: 오프스크린 SRV
	g_device->CreateShaderResourceView(mOffscreenTex.Get(), &srvDesc, handle);
	handle.Offset(1, descSize);

	// 슬롯 1: 블러맵0 SRV
	g_device->CreateShaderResourceView(mBlurMap0.Get(), &srvDesc, handle);
	handle.Offset(1, descSize);

	// 슬롯 2: 블러맵0 UAV
	g_device->CreateUnorderedAccessView(mBlurMap0.Get(), nullptr, &uavDesc, handle);
	handle.Offset(1, descSize);

	// 슬롯 3: 블러맵1 SRV
	g_device->CreateShaderResourceView(mBlurMap1.Get(), &srvDesc, handle);
	handle.Offset(1, descSize);

	// 슬롯 4: 블러맵1 UAV
	g_device->CreateUnorderedAccessView(mBlurMap1.Get(), nullptr, &uavDesc, handle);
}

/*
* 블러 루트 시그니처 
* 블러 셰이더가 받는 것:
* 블러 설정 상수(b0)
* 입력 SRV 테이블(t0),
* 출력 UAV 테이블(u0)
*/
void Init::BuildBlurRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE srvTable;
	srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);		// t0

	CD3DX12_DESCRIPTOR_RANGE uavTable;
	uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);		// u0

	CD3DX12_ROOT_PARAMETER slotRootParameter[3];
	// 루트 파라미터 종류 중 루트 상수.
	// 작은 값(int 1개 + float 11ro = 12 DWORD)을 루트 시그니처에 직접 박아넣는 방식.
	// SetComputeRoot32BitConstants로 넘길것
	slotRootParameter[0].InitAsConstants(12, 0);				// b0: 블러 설정 (int 1 + float 11 = 12개)
	slotRootParameter[1].InitAsDescriptorTable(1, &srvTable);	// t0: 입력 SRV
	slotRootParameter[2].InitAsDescriptorTable(1, &uavTable);	// u0: 출력 UAV

	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if(errorBlob != nullptr)
		OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	ThrowIfFailed(hr);

	ThrowIfFailed(g_device->CreateRootSignature(0,
		serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(&mBlurRootSignature)));
}

void Init::BuildBlurPSO()
{
	mHorzBlurByteCode = d3dUtil::CompileShader(L"Shaders\\blur.hlsl", nullptr, "HorzBlurCS", "cs_5_0");
	mVertBlurByteCode = d3dUtil::CompileShader(L"Shaders\\blur.hlsl", nullptr, "VertBlurCS", "cs_5_0");

	// 가로 블러 PSO
	D3D12_COMPUTE_PIPELINE_STATE_DESC horzPsoDesc = {};
	horzPsoDesc.pRootSignature = mBlurRootSignature.Get();
	horzPsoDesc.CS = 
	{
		reinterpret_cast<BYTE*>(mHorzBlurByteCode->GetBufferPointer()),
		mHorzBlurByteCode->GetBufferSize()
	};
	horzPsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(g_device->CreateComputePipelineState(&horzPsoDesc, IID_PPV_ARGS(&mHorzBlurPSO)));
	

	// 세로 블러 PSO
	D3D12_COMPUTE_PIPELINE_STATE_DESC vertPsoDesc = {};
	vertPsoDesc.pRootSignature = mBlurRootSignature.Get();
	vertPsoDesc.CS =
	{
		reinterpret_cast<BYTE*>(mVertBlurByteCode->GetBufferPointer()),
		mVertBlurByteCode->GetBufferSize()
	};
	vertPsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(g_device->CreateComputePipelineState(&vertPsoDesc, IID_PPV_ARGS(&mVertBlurPSO)));
}

void Init::BlurExecute(int blurCount)
{
	// 가우시안 가중치 계산
	auto weights = CalcGaussWeights(2.5f);
	int blurRadius = (int)weights.size() / 2;

	g_commandList->SetComputeRootSignature(mBlurRootSignature.Get());

	// 블러 힙 바인딩
	ID3D12DescriptorHeap* heaps[] = { mBlurHeap.Get() };
	g_commandList->SetDescriptorHeaps(1, heaps);

	UINT descSize = g_cbvSrvUavDescriptorSize;
	auto gpuStart = mBlurHeap->GetGPUDescriptorHandleForHeapStart();

	// 힙 슬롯별 GPU 핸들 (BuildBlurDescriptorHeap 순서와 일치)
	CD3DX12_GPU_DESCRIPTOR_HANDLE offscreenSrv(gpuStart, 0, descSize);			// 슬롯0
	CD3DX12_GPU_DESCRIPTOR_HANDLE blur0Srv(gpuStart, 1, descSize);			// 슬롯1
	CD3DX12_GPU_DESCRIPTOR_HANDLE blur0Uav(gpuStart, 2, descSize);			// 슬롯2
	CD3DX12_GPU_DESCRIPTOR_HANDLE blur1Srv(gpuStart, 3, descSize);			// 슬롯3
	CD3DX12_GPU_DESCRIPTOR_HANDLE blur1Uav(gpuStart, 4, descSize);			// 슬롯4

	// 블러 설정 상수 (b0) - 루트 상수로 직접 전달
	g_commandList->SetComputeRoot32BitConstants(0, 1, &blurRadius, descSize);
	g_commandList->SetComputeRoot32BitConstants(0, (UINT)weights.size(), weights.data(), 1);

	// -- 입력 준비: 오프스크린을 SRV로 읽을 수 읽게, 블러맵0을 UAV로 쓸 수 있게 --
	// 오프스크린: COPY_SOURCE(Draw에서 온 상태) -> GENERIC_READ
	// * 여기서 오프스크린 상태를 Draw()와 맞춰야 함 (아래 4단계에서 조정)

	for (int i = 0; i < blurCount; ++i)
	{
		// -- 가로 블러: 오프스크린(또는 이전 결과) -> 블러맵0 --
		g_commandList->SetPipelineState(mHorzBlurPSO.Get());

		g_commandList->SetComputeRootDescriptorTable(1, (i==0) ? offscreenSrv : blur1Srv);			// 입력
		g_commandList->SetComputeRootDescriptorTable(2, blur0Uav);									// 출력

		// 가로: N개 스레드로 가로 한 줄씩, 세로는 픽셀 개수만큼 그룹
		UINT numGroupX = (UINT)ceilf(mClientWidth / 256.0f);
		g_commandList->Dispatch(numGroupX, mClientHeight, 1);

		// 블러맵0: UAV(방금 씀) -> SRV(다음에 읽음), 블러맵1: SRV -> UAV
		auto b0ToSrv = CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap0.Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
		auto b1ToUav = CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap1.Get(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		g_commandList->ResourceBarrier(1, &b0ToSrv);
		g_commandList->ResourceBarrier(1, &b1ToUav);

		// -- 세로 블러: 블러맵0 -> 블러맵1 --
		g_commandList->SetPipelineState(mVertBlurPSO.Get());

		g_commandList->SetComputeRootDescriptorTable(1, blur0Srv);									// 입력
		g_commandList->SetComputeRootDescriptorTable(1, blur1Uav);									// 출력

		UINT numGroupY = (UINT)ceilf(mClientHeight / 256.0f);
		g_commandList->Dispatch(mClientWidth, numGroupY, 1);

		// 다음 반복을 위해 되돌림: 블러맵0 SRV->UAV, 블러맵1 UAV->SRV
		auto b0ToUav = CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap0.Get(),
			D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		auto b1ToSrv = CD3DX12_RESOURCE_BARRIER::Transition(mBlurMap1.Get(),
			D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ);
		g_commandList->ResourceBarrier(1, &b0ToUav);
		g_commandList->ResourceBarrier(1, &b1ToSrv);
	}
	// 최종 결과는 블러맵1에 (마지막이 세로 블러 -> 블러맵1, 근데 위에서 b1ToSrv 했으니 SRV 상태)
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

void Init::RunComputeTest()
{
	const int numElements = 128;
	const UINT byteSize = numElements * sizeof(float);

	// 입력 데이터 준비 (1, 2, 3, ... 128)
	std::vector<float> inputData(numElements);
	for(int i = 0; i < numElements; ++i)
		inputData[i] = (float)(i + 1);

	// (1) 입력 버퍼 - DEFAULT 힙 + 업로드
	mInputBuffer = d3dUtil::CreateDefaultBuffer(
		g_device.Get(), g_commandList.Get(),
		inputData.data(), byteSize, mInputUploadBuffer);

	// (2) 출력 버퍼 - DEFAULT 힙 + UAV 플래그
	auto defaultHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	auto outputDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize,
		D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);		// <- UAV 플래그 필수
	ThrowIfFailed(g_device->CreateCommittedResource(
		&defaultHeap, D3D12_HEAP_FLAG_NONE, &outputDesc,
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, nullptr,
		IID_PPV_ARGS(&mOutputBuffer)));

	// (3) readback 버퍼 - READBACK 힙 (CPU가 읽기)
	auto readbackHeap = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
	auto readbackDesc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
	ThrowIfFailed(g_device->CreateCommittedResource(
		&readbackHeap, D3D12_HEAP_FLAG_NONE, &readbackDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, nullptr,
		IID_PPV_ARGS(&mReadbackBuffer)));

	// -- 컴퓨트 실행 --
	g_commandList->SetPipelineState(mComputePSO.Get());
	g_commandList->SetComputeRootSignature(mComputeRootSignature.Get());		// Compute!

	g_commandList->SetComputeRootShaderResourceView(0, mInputBuffer->GetGPUVirtualAddress());
	g_commandList->SetComputeRootUnorderedAccessView(1, mOutputBuffer->GetGPUVirtualAddress());

	int numGroups = numElements / 64;		// 128 / 64 = 2 그룹
	g_commandList->Dispatch(numGroups, 1, 1);

	// 출력 버퍼 -> readback 버퍼 복사
	auto toCopySrc = CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffer.Get(),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	g_commandList->ResourceBarrier(1, &toCopySrc);

	g_commandList->CopyResource(mReadbackBuffer.Get(), mOutputBuffer.Get());

	// -- 명령 실행 후 대기 --
	ThrowIfFailed(g_commandList->Close());
	ID3D12CommandList* cmdsLists[] = { g_commandList.Get() };
	g_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	FlushCommandQueue();			// GPU 계산 끌날 때까지 대기

	// --결과 읽기--
	float* mappedData = nullptr;
	ThrowIfFailed(mReadbackBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mappedData)));

	OutputDebugStringA("===== Compute Test Result =====\n");
	for (int i = 0; i < numElements; ++i)
	{
		std::string line = std::to_string(inputData[i]) + " -> " + std::to_string(mappedData[i]) + "\n";
		OutputDebugStringA(line.c_str());
	}
	mReadbackBuffer->Unmap(0, nullptr);

	// 다음 작업을 위해 커맨드 리스트 다시 읽기
	ThrowIfFailed(g_commandList->Reset(g_commandAllocator.Get(), nullptr));
}

// sigma가 클수록 더 흐려짐
// sigma = 2.5f 정도로 시작하면 반경 5짜리 적당한 블러가 나옴
std::vector<float> Init::CalcGaussWeights(float sigma)
{
	float twoSigma2 = 2.0f * sigma * sigma;			// 2 * sigma^2
	int blurRadius = (int)ceil(2.0f * sigma);		// 보통 반경 = 2*sigma

	assert(blurRadius <= 5);						// gMaxBlurRadius = 5

	std::vector<float> weights(2 * blurRadius + 1);
	float weightSum = 0.0f;

	for (int i = -blurRadius; i <= blurRadius; ++i)
	{
		float x = (float)i;
		weights[i + blurRadius] = expf(-x*x / twoSigma2);
		weightSum += weights[i + blurRadius];
	}

	// 합이 1이 되도록 정규화 (안하면 이미자가 밝아지거나 어두워짐)
	for(int i = 0; i < weights.size(); ++i)
		weights[i] /= weightSum;

	return weights;
}

D3D12_CPU_DESCRIPTOR_HANDLE Init::OffscreenRtv() const
{
	return mOffscreenRtvHeap->GetCPUDescriptorHandleForHeapStart();
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
