# DirectX 12 Rendering & Compute Study

Frank Luna의 *『DirectX 12를 이용한 3D 게임 프로그래밍 입문』* 을 기반으로,
프레임워크에 의존하지 않고 **디바이스 초기화부터 컴퓨트 후처리·인스턴싱·프러스텀 컬링까지
밑바닥부터 직접 구현**한 학습 프로젝트입니다.

단순히 예제를 따라 친 것이 아니라, 각 기능을 **"왜 이 순서로, 왜 이 구조로"** 이해하는 데 초점을 두었고,
저수준 DirectX 12와 **UE5 RDG(Render Dependency Graph)** 추상화가 어떻게 대응되는지를 정리했습니다.

---

## 구현 기능

| 영역 | 구현 내용 |
| --- | --- |
| **초기화 파이프라인** | 디바이스, 커맨드 큐/얼로케이터/리스트, 스왑체인, 깊이-스텐실 버퍼, 펜스 동기화 |
| **렌더링 기본** | 정점/인덱스 버퍼, HLSL 런타임 컴파일, 루트 시그니처, PSO, 상수 버퍼 |
| **FrameResource** | 프레임별 얼로케이터/상수버퍼 링(3중)으로 CPU-GPU 병렬성 확보 |
| **조명** | 노멀 기반 디렉셔널 라이트, 패스/오브젝트 상수 버퍼 분리 |
| **텍스처링** | DDS 로드, SRV 힙, 정적 샘플러 |
| **블렌딩** | 알파 블렌딩, 불투명/반투명 PSO 분리 |
| **스텐실** | 평면 반사 (마킹 + 마스킹 PSO, 반사 행렬, 컬링 방향 반전) |
| **컴퓨트 셰이더** | GPU 병렬 연산, UAV, READBACK 힙 회수 |
| **컴퓨트 후처리** | 오프스크린 렌더 → 분리 가능 가우시안 블러(가로/세로 2패스, `groupshared` 캐싱, 핑퐁) → 백버퍼 합성 |
| **인스턴싱** | 구조화 버퍼 + `SV_InstanceID` 로 물체 다수를 **드로우콜 1회**에 렌더 |
| **프러스텀 컬링** | `BoundingFrustum` × `BoundingBox` 교차 검사로 시야 밖 물체 제외 |
| **카메라 조작** | 마우스 궤도(orbit) 회전 + 줌 (구면 좌표계) |

토글 키: `F2` MSAA · `F3` 블러 · `F4` 프러스텀 컬링

---

## DirectX 12 저수준 ↔ UE5 RDG 대응

이 프로젝트의 핵심 목표는 **"엔진이 자동화한 것을 밑바닥에서 직접 만들어보는 것"** 이었습니다.
상용 UE5 플러그인([GaussianSplatLab](https://www.fab.com/))에서 RDG로 렌더 패스를 작성해 본 경험을
저수준 API 관점에서 재구성했습니다.

| DirectX 12 (직접 구현) | UE5 RDG (추상화) |
| --- | --- |
| 리소스 상태 전이 (`ResourceBarrier` 수동 관리) | RDG 자동 배리어 삽입 |
| 컴퓨트 디스패치 + UAV 배리어 순서 제어 | `AddPass` + 자동 의존성 그래프 |
| 오프스크린 RT → 블러 → 백버퍼 합성 | PostProcess 패스 체인 |
| 디스크립터 힙 / 루트 시그니처 수동 구성 | `SHADER_PARAMETER_STRUCT` 매크로 |
| PSO 수동 생성 및 상태 조합 관리 | `TShaderMapRef` + PSO 캐시 |
| 펜스 기반 프레임 동기화 (`FrameResource`) | `FRHICommandListExecutor` |

---

## 컴퓨트 후처리 파이프라인

```
씬 → 오프스크린 텍스처 (RTV)
   → 가로 블러 CS (오프스크린 SRV → 블러맵0 UAV)
   → 세로 블러 CS (블러맵0 SRV → 블러맵1 UAV)   ← groupshared 캐싱, 핑퐁
   → 백버퍼로 복사
```

- **분리 가능(separable) 블러**: 2D 블러(반경 5 → 121 샘플)를 1D 두 번(22 샘플)으로 분해해 연산량 대폭 절감
- **`groupshared` + `GroupMemoryBarrierWithGroupSync`**: 스레드 그룹 공유 메모리로 이웃 픽셀 캐싱 (CUDA `__shared__` / `__syncthreads()` 와 동일 개념)
- **핑퐁 텍스처**: 동일 리소스를 읽으며 동시에 쓸 수 없으므로 두 텍스처를 번갈아 사용
- **리소스 상태 전이**: `RENDER_TARGET → SRV → UAV → COPY_SOURCE` 를 한 프레임 안에서 수동으로 순서 제어

---

## 컴퓨트 / CUDA 대응

컴퓨트 셰이더를 CUDA 병렬 모델과 1:1로 매핑해 이해했습니다.

| DirectX 12 컴퓨트 | CUDA |
| --- | --- |
| `[numthreads(64,1,1)]` | `blockDim = 64` |
| `Dispatch(n,1,1)` | `<<<n, 64>>>` |
| `SV_DispatchThreadID` | `blockIdx * blockDim + threadIdx` |
| `groupshared` | `__shared__` |
| `GroupMemoryBarrierWithGroupSync()` | `__syncthreads()` |
| `RWStructuredBuffer` (UAV) | device 포인터 |
| READBACK 힙 + `Map` | `cudaMemcpy(host, device)` |

---

## 주요 문제 해결 사례

밑바닥부터 구현하며 겪은 실전 디버깅 경험들입니다. (컴파일은 통과하지만 런타임에만 드러나는 유형이 많았습니다.)

- **함수 반환값 오류로 인한 검은 화면** — `CompileShader`가 계산한 바이트코드 대신 빈 `ComPtr`을 반환. 컴파일·예외 모두 정상이라 호출부에서 널 포인터로만 드러남 → 반환 경로 추적으로 해결
- **디스크립터 테이블 슬롯 오배치** — 컴퓨트 블러에서 출력 UAV를 입력 슬롯에 바인딩 → RenderDoc 없이 리소스 상태 전이를 역추적해 특정
- **CPU-GPU 동기화 실수** — 펜스 값 증가 변수를 잘못 지정해 `FlushCommandQueue`가 사실상 무동작 → GPU가 실행 중인 얼로케이터를 CPU가 리셋하여 디바이스 제거(device removed) 발생
- **후처리 경로 분기별 상태 관리** — 블러 on/off에 따라 오프스크린 텍스처의 최종 상태(`GENERIC_READ` vs `RENDER_TARGET`)가 달라지는 문제를 경로별 배리어로 분리
- **Z-파이팅** — 스텐실 반사 실습에서 바닥 평면과 물체가 같은 깊이에서 충돌 → 씬 배치가 원인임을 문제 분리로 규명

---

## 최적화에 대한 통찰

프러스텀 컬링을 구현한 뒤 **물체 1000개 환경에서 컬링 on/off 시 FPS 변화가 없음**을 확인했습니다.
`visibleCount`는 정상적으로 감소했으므로 컬링 로직 자체는 올바르게 동작했으나:

- 물체 수가 적거나 렌더링이 가벼우면 **CPU 컬링 검사 비용이 GPU 절약분과 상쇄**됨
- 후처리 블러처럼 **해상도에 비례하는 고정 비용**이 병목일 때는 물체 수를 줄여도 프레임 시간이 변하지 않음

→ **최적화는 프로파일링으로 병목을 먼저 특정한 뒤 적용해야 효과가 있다**는 원칙을 실측으로 체득했습니다.
"기법을 넣었으니 빨라진다"는 가정이 성립하지 않는 경우를 직접 확인한 것이 가장 큰 수확이었습니다.

---

## 빌드 환경

- Visual Studio 2022, Windows SDK 10, C++17
- 준수 모드 해제 (`/permissive-`) — `DDSTextureLoader` 호환
- 빌드 후 이벤트로 `Shaders/`, `Textures/` 폴더를 출력 디렉터리에 자동 복사

```
프로젝트/
├── Shaders/        color.hlsl, blur.hlsl, compute.hlsl
├── Textures/       WoodCrate01.dds
└── Common/         d3dUtil, UploadBuffer, GameTimer, MathHelper, DDSTextureLoader, d3dx12.h
```

각 챕터의 구현 단계는 Git 태그(`ch4-complete` ~ `ch15-complete`)로 구분되어 있어
단계별 진행 과정을 커밋 히스토리로 확인할 수 있습니다.

---

## 학습 범위 (챕터별)

| 챕터 | 내용 |
| --- | --- |
| 4 | Direct3D 초기화 — 디바이스, 커맨드 객체, 스왑체인, 펜스 |
| 6 | 렌더링 파이프라인 — 정점 버퍼, 셰이더, 루트 시그니처, PSO |
| 7 | FrameResource — CPU-GPU 병렬성 |
| 8~9 | 조명, 텍스처링 |
| 10~11 | 블렌딩, 스텐실(평면 반사) |
| 13 | 컴퓨트 셰이더 + 후처리 블러 |
| 15 | 인스턴싱, 프러스텀 컬링, 카메라 조작 |
