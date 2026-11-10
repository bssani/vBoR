# M1 마커 정렬 시험대 — 카메라로 볼트 위 마커 4개를 읽어 4점 정렬과 비교

| 항목 | 값 |
|---|---|
| 단계 | 50% 범위 밖 실험(사용자 2026-11-11 "새 레벨에서 먼저 시험하고 통과하면 main 레벨에 추가"). 시험 계획 T-01(데이터·Buck 정렬)의 보조 |
| 결정 근거 | **사용자 결정(2026-11-11):** 실물 Buck의 좌석 볼트 4개(= `BP_Car_Core` 정렬점 1–4, 사용자 확인)에 마커를 붙여 카메라로 읽는 정렬을 시험한다. 카메라는 설치 정렬 시험에서만 쓰고 화면에 패스스루를 띄우지 않는다(AGENTS.md 실험 예외). 근거·조사: 조사 기록 2026-11-11 "마커 정렬 — Quest 카메라·OpenCV" |
| 베이스 브랜치 | `main` |
| 작업 브랜치 | `codex/M1-marker-alignment-probe` |
| 리뷰 파일 | `Docs/Reviews/M1-marker-alignment-probe.md` (첫 리뷰 때 생김) |
| 상태 | 명세 작성 |
| 에디터 작업 | **있음** — §6 레벨 하나(Codex가 MCP로). `L_Main`·`Content/Main` 에셋은 고치지 않는다 |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **시험대만, 저장 없음** | 새 Actor `AM1MarkerAlignProbe`(`Source/vBoR/M1Probe/`)와 새 레벨 `Content/Development/M1/L_M1_MarkerAlign`. 마커로 구한 정렬은 **보여 주고 비교만** 한다. `BuckEye.json`을 쓰지 않고, `UVBoRVehicleAlignmentComponent`·`FVBoRBuckAlignment`·저장소 코드를 고치지 않는다. 쓸 수 있는 공개 API만 쓴다: `FVBoRBuckAlignment::FitPoints`(`VBoRBuckAlignment.h:59`), `UVBoRAlignPointComponent::Order`·`ShowPoint`(`VBoRAlignPointComponent.h:14-16`), 정렬 컴포넌트의 `FloorWorldZCm`·`PointSpreadLimitMm`·`PointFitLimitCm`·`PointHand`·`State`(`VBoRVehicleAlignmentComponent.h:57-69`), `AVBoRHandRigPawn::ReadRawIndexTip`(`VBoRHandRigPawn.h:42`) |
| D2 | **마커 = ArUco `DICT_4X4_50`, ID n = 정렬점 Order n** | 엔진 OpenCV 도우미 `FOpenCVHelper::IdentifyArucoMarkers`(`Engine/Plugins/Runtime/OpenCV/Source/OpenCVHelper/Public/OpenCVHelper.h:147`)는 ArUco 사전만 받는다(`EArucoDictionary`, `:48-68`, AprilTag 없음). 가장 단순하게 이걸 쓴다. 인쇄본: `Docs/Markers/vBoR_markers_aruco4x4_ID1-4.pdf`(검은 사각형 60 mm·80 mm, 벡터, 재인식 확인함). 마커 한 변 `MarkerSizeMm`(기본 60) 프로퍼티. 차의 `Order` 집합에 없는 ID는 무시한다 |
| D3 | **카메라 = MRUK Passthrough Camera Access, 왼쪽 눈, CPU 읽기** | `UMRUKPassthroughCameraAccess`(`Plugins/MetaXR/Source/MRUtilityKit/Public/MRUKPassthroughCameraAccess.h:130-223`)와 `UMRUKPassthroughCameraAccessTexture`(`:226-292`)를 **런타임에 `NewObject`로** 만든다(에셋 없음). 해상도는 `GetSupportedResolutions`(`:63`)에서 4:3 하나를 고르고(가장 큰 것을 무조건 고르지 않음 — Meta 문서 "Incorrect resolution handling"), 1280×960이 있으면 그것. Link(Win64)에서 플러그인은 CPU 이미지를 텍스처에 올린다(`MRUKPassthroughCameraAccess.cpp:417-427`). **`FRenderTarget::ReadPixels`는 쓰지 않는다** — 이 리소스는 `RenderTargetTextureRHI`를 채우지 않는다(`:704-734`). 렌더 스레드에서 `GetResource()->GetTexture2DRHI()`를 `RHICmdList.ReadSurfaceData`(`Engine/Source/Runtime/RHI/Public/RHICommandList.h:4850`)로 읽는다. 픽셀 형식 `PF_R8G8B8A8`(`MRUKPassthroughCameraAccess.h:291`). 기록 중에만 읽는다(평상시 비용 없음) |
| D4 | **프레임마다 영상과 그 순간의 카메라 자세** | `GetTimestamp()`(`:201`)가 지난 프레임과 같으면 그 프레임은 건너뛴다(같은 영상 두 번 세지 않음). 카메라 자세는 `GetCameraPose`(`:177`) — 영상 시각의 헤드셋 자세 + 렌즈 위치(`MRUKPassthroughCameraAccess.cpp:218-241`). 이 값에는 TrackingToWorld를 곱하지 않는다(레이캐스트는 곱함 `MRUtilityKitSubsystem.cpp:917-943`) → **추적 공간 좌표로 보고 vBoR의 Stage(cm)로 그대로 쓴다.** 같은지 여부가 이 실험의 첫 측정이다(D8, Q2) |
| D5 | **카메라 모델과 자기 검사** | 순수 구조체 `FM1CameraModel`(카메라 Stage 자세 + 영상 픽셀 기준 fx·fy·cx·cy)로 투영·역투영·PnP를 한다. MRUK 보정값은 센서 해상도 기준이고 영상은 잘라 낸 영역이다(`CalcSensorCropRegion`, private). 그래서 영상 픽셀 기준 값은 **공개 함수 `ViewportPointToWorldSpaceRay`(`:189`)로 몇 점의 광선을 받아** 구한다(방법은 구현자, §5). **카메라 시작 직후 자기 검사:** 카메라 앞 임의의 점 3개를 `FM1CameraModel`로 투영한 픽셀과 MRUK `WorldToViewportPoint`(`:195`)×영상 크기가 0.5 px 안에서 같아야 한다. 다르면 기록을 거부하고 `Camera model mismatch <px>`를 보인다(축·부호 실수를 기기에서 바로 잡음). 왜곡 계수는 없다(`FMRUKCameraIntrinsics` `:33-47`) — 0으로 두고 재투영 오차로 본다 |
| D6 | **마커 중심 구하기** | 프레임마다 `IdentifyArucoMarkers`로 모서리 4개 → 마커 평면 위 정사각형(한 변 `MarkerSizeMm`)과 PnP(`FOpenCVHelper::SolvePnP` `:165` 또는 같은 일을 하는 OpenCV 호출) → 마커 중심의 Stage 위치(cm). 그 프레임의 **재투영 오차**(모서리 4개 평균, px)와 **영상 속 한 변 길이**(px)를 같이 낸다. 받는 조건: 재투영 오차 ≤ `MarkerReprojLimitPx`(기본 2.0), 한 변 ≥ `MarkerMinSidePx`(기본 24). 안 맞는 프레임의 그 마커는 버리고 버린 수를 센다 |
| D7 | **기록 세션 (`M`)** | `M`을 누르면 세션 시작(카메라가 꺼져 있으면 켬). 마커 ID마다 받은 중심을 모은다. 마커마다 `MarkerMinFrames`(기본 30)개가 모이면 그 마커는 끝. 모든 정렬점의 마커가 끝나거나 `MarkerSessionSeconds`(기본 30 s, 실제 시간)가 지나면 세션 끝.<br>- 마커마다: 평균 = Stage 위치, 흩어짐 = 표본 사이 최대 거리(mm, 지금 손끝 기록과 같은 정의 `VBoRPointAlignment.cpp:121-126`). 흩어짐 > 정렬 컴포넌트 `PointSpreadLimitMm`이면 그 마커 거부.<br>- 시간이 지나도 모자란 마커가 있으면 결과는 `Incomplete — missing ID 3 (12/30 frames)`처럼 빠진 ID와 수를 보인다. 맞추기는 하지 않는다.<br>- 세션 중 다시 `M` = 취소.<br>- 설치자는 앉지 않고 서서 마커를 하나씩 봐도 된다(카메라 자세가 추적되므로 같은 Stage에 모인다) |
| D8 | **결과 — 4점 정렬과 같은 계산** | 모든 마커가 받아지면, 마커 Stage 중심과 차 정렬점 world 위치(`Order` 순)로 `FitPoints`(정렬 컴포넌트의 `FloorWorldZCm`)를 부른다. 보일 것:<br>- 점마다 XY 차이(cm), 높이 차이(cm, `MarkerHeightOffsetCm`(기본 0)을 뺀 값 — 볼트 캡·판 두께 보정), 최대 XY와 `PointFitLimitCm` 비교(넘으면 `over limit` 표시, 거부는 아님 — 시험이므로 숫자를 본다)<br>- **지금 Pawn(파일에서 적용된 수동 정렬)과의 차이:** ΔXY(cm), ΔYaw(deg), 그리고 Yaw 차이가 앞 70 cm에서 만드는 거리(cm). 정렬 컴포넌트 `State`가 `Applied`가 아니면 그 상태를 같이 적는다<br>- 마커마다 흩어짐(mm)·받은 수·버린 수·평균 재투영 오차(px), 세션 시간(s)<br>- 같은 내용을 로그 한 줄씩(범주 `LogM1Marker`, R5 D8처럼 줄마다 따로) |
| D9 | **잠깐 적용·되돌리기 (`G` / `H`)** | `G`: 마커 맞춤 Pawn을 `SetActorTransform`(TeleportPhysics)으로 적용한다. **저장하지 않는다.** 처음 `G` 전의 Pawn 자세를 기억한다. `H`: 그 자세로 되돌린다. 맞춤 결과가 없으면 `G` 거부(`No marker fit`). 적용 중이면 차 정렬점 표시를 켠다(`ShowPoint(true)`) — 손끝을 대 보며 확인 |
| D10 | **손끝 비교 (`T`)** | 1.0 s 동안 `PointHand` 검지 끝 **원본**(`ReadRawIndexTip`)을 모아 평균 Stage 위치(Pawn 역변환, 지금 손끝 기록과 같은 변환 `VBoRPointAlignment.cpp:116-117`)를 구한다. 가장 가까운 **마지막으로 받은 마커 중심**과의 차이를 XY·높이·3D(mm)로 보인다. 손이 한 프레임이라도 추적되지 않으면 그 비교는 취소. 마커 1장만 붙인 첫 시험(Q2)이 이것만 쓴다 — 차·CAD 없이 카메라 좌표 = Stage인지 본다 |
| D11 | **정렬에 안 쓴 확인점** | 차 Actor의 Scene 컴포넌트 중 태그 `Align.Check`인 것을 확인점으로 쓴다(사용자가 손잡이 등에 둠, 없으면 줄 없음). `G` 적용 중에는 손끝 원본에서 가장 가까운 확인점까지의 XY·높이 차이(cm)와 그 이름을 화면에 계속 보인다 |
| D12 | **MRUK World Lock 끔** | `UMRUKSubsystem::Tick`은 `EnableWorldLock`이면 Pawn 위치를 덮어쓸 수 있다(`MRUtilityKitSubsystem.cpp:1081-1097`, 기본값 true `OculusXRHMDRuntimeSettings.h:275`). 프로젝트 설정은 바꾸지 않는다. 시험대 BeginPlay에서 `GetGameInstance()->GetSubsystem<UMRUKSubsystem>()->EnableWorldLock = false`(런타임 값, `MRUtilityKitSubsystem.h:131-132`), EndPlay에서 원래 값으로 |
| D13 | **표시** | 진행자: `GEngine` 화면 메시지(PC 창) + 로그. 설치자(VR): 받는 중인 마커 중심에 작은 구(노랑 = 모으는 중, 초록 = 끝, 빨강 = 거부). 패스스루 영상은 화면에 띄우지 않는다 |
| D14 | **키** | 시험대 전용: `M` 기록·취소, `T` 손끝 비교, `G` 잠깐 적용, `H` 되돌리기. 기존 진행자 키(`VBoROperatorPanel.cpp:18-32`: R O 1 2 A I F6 F U P S X E)와 겹치지 않는다. 이 레벨에는 Runner를 두지 않는다 |
| D15 | **카메라 끄기** | EndPlay·레벨 종료에서 `Stop()`. 세션이 끝나도 카메라는 켜 둔다(`T`·다음 `M`이 바로 되게). 시작 실패(`Play` false — 권한·Link 설정·버전)면 `Camera start failed — check Link settings (Passthrough over Link, camera API permission)`를 보이고 다른 키는 동작한다 |

## 1. 왜 하는가

- 지금 Buck 정렬은 설치자가 헤드셋을 쓴 채 보이지 않는 볼트를 손끝으로 더듬어 짚는다(조사 기록 2026-10-20). 마커를 카메라로 읽으면 사람 손 오차와 시간이 줄어들 수 있다.
- 바꾸기 전에 숫자로 비교한다: 카메라 좌표가 Stage와 같은지, 반복성, 수동 정렬과의 차이, 확인점 오차.
- 통과하면 다음 명세(M2)에서 진행자 창·`L_Main`·저장(정렬 입력 원천)으로 옮긴다. 이번에는 하지 않는다.

## 2. 범위

### 만들 것

- D1–D15. 의존성: `vBoR.Build.cs`에 `MRUtilityKit`(private), Win64일 때만 `OpenCVHelper`·`OpenCV`. `vBoR.uproject`에 `OpenCV` 플러그인 켬(Win64). OpenCV 코드는 `WITH_OPENCV`로 감싼다.
- 시험 `vBoR.M1.*`(§4). 레벨 하나(§6).

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| `BuckEye.json` 쓰기, 정렬 컴포넌트·`FitPoints`·저장소·진행자 창 변경 | D1. 다음 명세(M2) |
| `L_Main`·`Content/Main` 에셋 변경, Runner 배치 | 산출물 영향 없음. Development는 Main을 참조해도 되고 반대는 안 됨(AGENTS.md) |
| 패스스루 화면 표시(`OculusXRPassthrough` 레이어 등) | AGENTS.md 실험 예외는 카메라 데이터뿐 |
| 프로젝트 설정 변경(MRUK World Lock 등), RHI 변경(DX12 그대로) | D12. Vulkan은 Android 경로에서만 요구(`MRUKPassthroughCameraAccess.cpp:129-136`) |
| AprilTag 직접 구현, 학습 모델, 외부 서비스 | D2. 영상은 PC 밖으로 나가지 않는다 |
| 마커 한 장으로 6DoF 정렬, 높이 맞추기 | 지금 정렬과 같게 XY·Yaw만, 높이는 바닥(U-17) |
| 컨트롤러 사용 | AGENTS.md(정렬에 컨트롤러 안 씀) |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- `Source/vBoR/Interaction/` 파일은 고치지 않는다(새 파일도 `M1Probe/`에). 수동 4점 정렬(P·S·X), 저장 형식 version 1·2, `CheckEnvironment`, 재중심 처리 그대로.
- 시험대가 있어도 정렬 컴포넌트의 자동 적용(BeginPlay에 파일 적용)은 그대로 일어난다. 시험대는 그 결과를 **기준**으로 비교만 한다.
- `G` 없이 Pawn을 움직이지 않는다. World Lock은 시험대가 있는 동안만 끈다.
- **기존 시험 감사(Claude 2026-11-11):** 새 Actor·새 파일·새 의존성만 더한다. 기존 코드 경로가 바뀌지 않으므로 `vBoR.*` 기존 단언은 그대로다. `OpenCV` 플러그인을 켜면 시험 명령(Agent-Workflow §2.4)에서 플러그인이 하나 더 로드될 뿐이다. **고쳐도 되는 단언: 없음.** 기존 시험이 깨지면 멈추고 리뷰 파일 "질문"에 적는다.

## 4. 시험 — `vBoR.M1.*`

카메라 없이 돈다. 카메라 읽기는 가상 함수(`ReadCameraFrame` 같은 이름 — 영상·크기·`FM1CameraModel`·시각을 돌려줌) 뒤에 두고, 시험용 하위 클래스가 합성 프레임을 준다(P4h의 `ReadPointTip` 대체와 같은 방식, `VBoRVehicleAlignmentComponent.h:78-81`). **합성 영상:** `FM1CameraModel`로 마커 평면을 픽셀마다 역투영해 그린다(검은 테두리 + `DICT_4X4_50` 비트, 흰 여백 1칸). 비트는 OpenCV 사전에서 얻는다.

| ID | 상황 | 확인 |
|---|---|---|
| E1 | `FM1CameraModel` 투영·역투영 왕복, 자세 3가지(정면, Yaw 30°+Pitch −40°, Roll 15° 포함) | 왕복 오차 < 0.01 px |
| E2 | 합성 영상(1280×960, 마커 60 mm, 거리 0.6 m·1.2 m, 위 자세 3가지)에서 D6 | ID 맞음, 중심 오차 ≤ 2 mm(0.6 m)·≤ 4 mm(1.2 m), 재투영 오차 ≤ 1 px |
| E3 | D5 자기 검사: 시험용 MRUK 대역이 같은 모델 / Y축 부호를 뒤집은 모델 | 같으면 통과, 뒤집으면 `Camera model mismatch`로 `M` 거부 |
| E4 | D7: 같은 시각 프레임 두 번 | 한 번만 센다 |
| E5 | D7: 마커 4개 중 ID 3이 끝까지 안 보임(세션 시간 짧게) | `Incomplete — missing ID 3 (n/30 frames)`, 맞추기 없음, Pawn 그대로 |
| E6 | D7: ID 2 표본에 ±8 mm 흔들림(흩어짐 16 mm > 10 mm) | ID 2 거부 사유(흩어짐 값), 맞추기 없음 |
| E7 | D6: 재투영 오차 큰 프레임(모서리 하나를 5 px 옮김), 너무 작은 마커(한 변 < 24 px) | 그 프레임 버림, 버린 수 증가 |
| E8 | 차 `Order` 집합 {1,2,3,4}, 영상에 ID 7도 있음 | ID 7 무시, 결과 줄 없음 |
| E9 | D8: 마커 중심 = 차 정렬점을 알려진 (ΔX 3 cm, ΔY −2 cm, Yaw 1.5°)로 옮긴 위치(Stage) | `FitPoints` 결과와 같고, 지금 Pawn과의 ΔXY·ΔYaw가 그 값(±0.01 cm, ±0.01°), 70 cm 거리 환산이 70·tan(1.5°) |
| E10 | D8 높이: `MarkerHeightOffsetCm` = 1.5, 마커가 정렬점보다 1.5 cm 위 | 높이 차이 0(±0.01) |
| E11 | D9: `G` → `H` / 결과 없이 `G` | Pawn이 맞춤 자세 → 처음 자세로 정확히 돌아옴. 결과 없으면 거부, Pawn 그대로 |
| E12 | 저장 없음: 세션·`G`·`H`·`T` 전체 전후 | `SavedAlignmentPath()` 파일 바이트(있으면)·수정 시각 같음, 없으면 계속 없음. 정렬 컴포넌트 `State`·`Reason` 같음 |
| E13 | D10: 손끝 1 s 창(시험 Pawn이 손끝 제공), 창 중 한 프레임 추적 끊김 | 정상: 차이 = 알려진 값. 끊김: 그 비교 취소 |
| E14 | 세션 중 `FCoreDelegates::VRHeadsetRecenter` | 세션 취소(`Recenter — marker capture canceled`), 모은 표본 버림 |
| E15 | D12: BeginPlay 뒤·EndPlay 뒤 `UMRUKSubsystem::EnableWorldLock` | false → 원래 값(서브시스템이 없으면 건너뛰고 통과) |
| E16 | D11: `Align.Check` 컴포넌트 2개, `G` 적용 중 손끝이 그중 하나에서 (1, 0, 0.5) cm | 그 이름과 XY 1.0·높이 0.5 |
| E17 | D15: 카메라 시작 실패(대역이 false) | 안내 문구, `M` 거부, `T`·`G`·`H`는 동작 |

## 5. 구현자가 정할 것

- `FM1CameraModel`의 fx·fy·cx·cy를 `ViewportPointToWorldSpaceRay`로 구하는 방법(예: 시야 가운데·모서리 몇 점의 광선을 카메라 축으로 돌려 기울기에서 선형으로). 단 D5 자기 검사는 반드시.
- PnP를 `FOpenCVHelper::SolvePnP`로 할지 OpenCV `solvePnP`(`SOLVEPNP_IPPE_SQUARE`)를 직접 부를지. **축·부호 변환(OpenCV ↔ UE ↔ MRUK 뷰포트 원점)이 가장 흔한 실수 자리다** — E1–E3이 덮게 한다.
- 렌더 스레드 읽기 방식(`ENQUEUE_RENDER_COMMAND` + 동기 대기 또는 `FRHIGPUTextureReadback`). 기록 중에만 읽는다.
- 프로퍼티 이름·기본값(위 기본값은 임시, 평가 합격 수치가 아님), 화면 문구(영어), 구 크기·색.
- 새 파일 구성(`M1Probe/` 안).

## 6. 에디터 작업 (Codex, MCP — Agent-Workflow §2.10 순서)

C++ 빌드 → 에디터 열기 → 아래 → 저장 → 에디터 닫기 → 전체 시험.

1. `Content/Development/M1/L_M1_MarkerAlign` 새 레벨(빈 레벨).
2. World Settings의 GameMode Override = `/Game/Main/Core/BP_vBoRGameMode`(`L_Main`과 같음 — 다르면 `L_Main` 값을 따르고 보고).
3. `BP_Car_Core`를 `L_Main`에 놓인 것과 **같은 Transform**으로 하나 놓는다(값은 `L_Main`에서 읽어 옮김, `L_Main`은 고치지 않음). 정렬 컴포넌트·정렬점은 BP에 이미 있다.
4. `AM1MarkerAlignProbe` 하나를 놓는다(위치 무관).
5. 바닥이 필요하면 큰 평면 하나(근사 형상, 이름에 `Mock`). CAD 메시는 넣지 않는다.
6. 보고: 레벨 경로, Car Transform 값, GameMode 이름. 맵 로드 확인(Agent-Workflow §2.5).

## 7. 완료 조건

1. D1–D15 구현, §3 조건 지킴, §2 범위 밖 없음.
2. `vBoR.M1.E1`–`E17` 통과, 기존 시험 변경 없이 통과.
3. non-unity(`-Module=vBoR`)·unity 빌드 성공, `vBoR.*` 전체 통과(작업 끝에 한 번).
4. §6 레벨 저장, 맵 로드 경고 없음.
5. 새 주석·로그·표시 이름 영어(AGENTS.md).

## 8. 로컬에서 확인할 것

- 빌드·시험: 위 2–3.
- 에디터(사용자): `L_M1_MarkerAlign` 열기 → Car가 `L_Main`과 같은 자리인지.
- **Quest 3 (케이블 Link, VR Preview) — 모두 `not-tested`로 시작, T-01 보조.** 숫자는 `Saved/Logs/vBoR.log`의 `LogM1Marker`에서 뽑는다. 조건: Link 앱 v201 이상(지금 v208), "Link를 통한 패스스루"·"패스스루 카메라 API 권한" 켬, 설정을 켠 뒤 에디터 재시작.

| ID | 할 일 | 볼 것 |
|---|---|---|
| Q1 | 레벨 Play | 카메라 시작 로그(해상도), D5 자기 검사 통과, 60 s 동안 Pawn이 움직이지 않음(World Lock) |
| Q2 | **마커 1장**(바닥이나 책상, 60 mm) → `M`(1장만 있으면 Incomplete지만 중심은 보임) → 검지로 마커 중심 누르고 `T`. 거리 0.5·1.0 m, 각 5회 | 카메라 중심 − 손끝 차이: 일정한 방향으로 쏠리면 좌표계 차이(멈추고 보고), 1 cm 안팎 무작위면 다음 |
| Q3 | 마커 4장을 바닥에 CAD 볼트 간격대로(줄자) → `M` | 점마다 XY 차이(4장 상대 위치가 맞는지), 세션 시간 |
| Q4 | 회사 Buck: 볼트 캡 위 마커 4장 → `M` → 결과 | 수동 정렬과의 ΔXY·ΔYaw, 점별 차이, 높이 차이(캡 두께 보정 후) |
| Q5 | Q4를 헤드셋 벗었다 쓰고 5회 | ΔXY·ΔYaw의 흩어짐(반복성). 수동 4점 5회와 비교 |
| Q6 | `G` → 손잡이 등 `Align.Check` 점에 손끝 | 확인점 XY·높이 차이 |
| Q7 | 조명(형광등만·어둡게), 마커 일부 가림(손·다리) | 받은 수·버린 수, 세션 끝나는지 |
| Q8 | 회사 PC에서 카메라 켬/끔 | 프레임 시간(`stat unit`), 기록 중 끊김 |
| Q9 | Q2·Q4를 Air Link(PrismXR Puppis)로 | 카메라 시작 여부, 받은 수, 결과 차이 |

**합격 기준:** 프로젝트에 절대 정렬 정확도 요구값이 없다 → **평가 담당자와 합의할 값.** M2로 넘어갈 임시 판단(사용자와 정함): Q2에 설명 안 되는 쏠림이 없고, Q5 반복성이 수동 4점보다 나쁘지 않고, Q4 세션 시간이 수동보다 짧을 것.
