# P0-b 리뷰 기록

명세: [Docs/Specs/P0-b-poke-button-bench.md](../Specs/P0-b-poke-button-bench.md) · 브랜치: `codex/P0-b-poke-button-bench`

**Codex가 할 일:** 아래 "열린 항목"을 위에서부터 반영한다. 항목마다 **문제**는 반드시 해결하고, **제안**은 권장이다(다르게 하면 보고 ③에 이유를 적는다). **확인할 시험**은 새로 넣거나 고칠 자동화 시험이다. 같은 브랜치에 새 커밋으로 하고, 반영한 항목의 상태를 `반영함 (<커밋 제목>)`으로 바꿔 같은 커밋에 넣는다. 커밋 본문에 보고 ①~⑤. 판단이 필요하면 맨 아래 "질문"에 적고 멈춘다. push·`main` 머지는 하지 않는다. 끝나면 `HANDOFF.md`의 "Codex 결과"를 채운다.

## 회차

| 회차 | 날짜 | 리뷰 대상 | 리뷰어 | 결과 |
|---|---|---|---|---|
| 1 | 2026-10-10 | `6562171` P0-b 시험대 | Claude | **수정 후 실측.** 빌드·시험은 통과. 부모 버튼이 메시 재질을 덮는 문제, 동시 활성 수 누락, 시험대 위치 기준 등 6건 |

### 1회차에 재현한 것

- 바뀐 C++ 5개 강제 재컴파일(샘플 플러그인 끔), 경고·오류 0. `vBoR.*` 18개 Success(P0-a 12 + P0-b 6), `TEST COMPLETE. EXIT CODE: 0`.
- **잘 된 것:**
  - 원본 손끝을 엔진 XR 손 추적(`UHeadMountedDisplayFunctionLibrary::GetHandTrackingState`, IndexTip, Tracked일 때만)으로 읽는다. limiter가 옮기는 ISDK 손 모델과 무관하다. 명세 §2 요구에 맞는 좋은 선택이다.
  - 레이는 ISDK 소스대로 컴포넌트를 비활성화해 끈다(활성 조건에 `IsActive`가 들어감). 컨트롤러 리그와 원거리 잡기 감지기는 없다. limiter는 `RigModifiersToSpawn`에 들어 있다. 시작 로그에 레이 상태, modifier 목록, 데이터 소스가 찍힌다.
  - 버튼 부모: Actor +Z 윗면, `IsdkPokeInteractable` 피치 -90°로 누르는 방향을 맞췄다. `OnConstruction`에서 메시 경계로 판정 평면과 쉬는 위치를 맞추고, 다시 실행해도 위치가 그대로다. `NormalOffset`을 눌림 깊이로 둔 것도 ISDK 의미(hover·취소 판정 거리에 더함, `IsdkPokeInteractable.cpp:234-239`)에 맞다.
  - 샘플 플러그인 참조 없음, 주석 말투 맞음.
- `vBoR.uproject`에 `XRBase` 명시: 모듈이 XRBase에 의존할 때 UBT 경고를 막는 용도이고, 이미 다른 플러그인 때문에 켜져 있어 동작은 바뀌지 않는다. 받아들인다.
- 확인 범위: UE 5.7 OpenXR의 기본 추적 공간은 Stage다(`Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.cpp:1650`). 바닥 기준이지만 원점이 가디언 중앙이다(R1-3의 근거).

## 열린 항목

### R1-1 상태 색이 버튼 메시의 원래 재질을 덮는다 — 열림

**문제**
- **위치:** `Source/vBoR/Interaction/VBoRPokeButton.cpp:84-94` — BeginPlay에서 버튼 메시의 모든 재질 슬롯을 `GrabbableLit` 동적 재질로 바꾼다.
- **발생 조건:** 자식 Blueprint에 실제 차량 버튼 메시(라벨·텍스처가 있는 재질)를 넣을 때.
- **영향:** 실행하면 라벨과 텍스처가 사라지고 단색 버튼이 된다. "메시만 바꾸면 적용"(D3)이 깨지고, 라벨 판독은 평가 대상이다(U-04). 명세 §2 문구가 재질 보존을 말하지 않은 Claude의 누락이었고, 명세를 고쳤다.

**제안**
- 원래 재질 슬롯은 건드리지 않는다. 동적 재질을 슬롯에 넣는 코드를 없앤다.
- **Normal 상태에서는 덧입힘 없음**(`SetOverlayMaterial(nullptr)`) — 쉬는 모습이 실제 메시 그대로여야 한다.
- hover·select에서만 `UMeshComponent::SetOverlayMaterial`(`Engine/Source/Runtime/Engine/Classes/Components/MeshComponent.h:56`)로 반투명 색을 덧입힌다. 덧입힘 재질은 BeginPlay에 동적 재질 하나를 만들어 두고 색만 바꿔 재사용한다.
- 덧입힘 재질 후보: ISDK `GrabbableLit`(`IsdkContentAssetPaths::Materials::GrabbableDefault`). 에셋 안에 `BLEND_Translucent`·`Opacity`·`ColorParam` 문자열이 보이지만 실제 혼합 방식은 확인되지 않았다. 시험에서 `UMaterialInterface::GetBlendMode()`(`Engine/Source/Runtime/Engine/Public/Materials/MaterialInterface.h:1010`)가 `BLEND_Translucent`인지, 불투명도를 무엇으로 조절하는지(`ColorParam` 알파인지 별도 파라미터인지) 확인한다. 반투명이 아니면 ISDK의 다른 반투명 재질(`RoundedBoxUnlit` 등, `Plugins/MetaXRInteraction/Content/Materials/`)을 같은 방법으로 확인한다. 쓸 것이 없으면 "질문"에 적고 멈춘다(에셋은 사용자가 만든다).
- 불투명도는 프로퍼티로 둔다. 임시값: hover 0.35(라벨이 비쳐 보이게), select 0.6. 합격 기준이 아니다.
- 주의: 덧입힘은 메시를 한 번 더 그린다. hover·select인 버튼만 켜지므로 버튼 수가 많아도 부담은 작다.

**확인할 시험**
- BeginPlay 뒤 버튼 메시의 원래 재질 슬롯이 BeginPlay 전과 같은 객체다.
- Normal → 덧입힘 없음, Hover·Select → 덧입힘 재질의 색·불투명도가 각 프로퍼티 값이다. Normal로 돌아오면 다시 없음.
- 덧입힘 재질의 혼합 방식이 반투명이다.

### R1-2 몰린 묶음의 동시 활성 수가 select 중인 버튼을 빼고 센다 — 열림

**문제**
- **위치:** `Source/vBoR/P0Probe/P0ButtonBench.cpp:166-171` — `GetCurrentState() == Hover`만 센다.
- **발생 조건:** 한 버튼을 누르는(select) 동안 손끝이 이웃 버튼에도 hover할 때.
- **영향:** U-04가 막으려는 "둘 이상 동시 활성"이 1로 세어진다. 이 시험대가 답해야 할 핵심 질문(ISDK 기본이 하나만 활성인가)을 놓친다. 명세 §2 표의 "hover 상태인 버튼 수"가 Claude의 잘못된 문구였고, "hover 또는 select"로 고쳤다.

**제안**
- 상태 배열을 받아 활성 수를 세는 순수 함수(예: `CountActive`)를 측정 헤더에 두고, Hover 또는 Select를 활성으로 센다. 시험대는 프레임 끝에 그 함수를 부른다.
- 패널 표시를 "Cluster active (양손 합)"로 바꾼다. 두 손이 같이 묶음에 있으면 합쳐서 센다는 것을 이름으로 드러낸다.

**확인할 시험**
- `SimultaneousHover` → `SimultaneousActive`로 바꾸고: {Select, Hover} = 2, {Select} = 1, {Normal, Disabled} = 0, 2 이상 진입 횟수는 전환 때만.

### R1-3 시험대가 머리가 아니라 Pawn 원점 기준으로 놓인다 — 열림

**문제**
- **위치:** `Source/vBoR/P0Probe/P0ButtonBenchGameMode.cpp:18-22` — Pawn 위치 + (45, 0, 120) cm에 생성.
- **발생 조건:** UE 5.7 OpenXR의 기본 추적 공간은 Stage다(`Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.cpp:1650`). 그래서 Pawn 원점은 가디언 중앙 바닥이다. 참가자가 가디언 중앙이 아닌 곳에 서 있거나 앉아 있으면 문제가 된다.
- **영향:** 시험대가 손 닿는 거리에 없을 수 있다. 위치가 GameMode 프로퍼티라 World Settings에서는 고칠 수도 없다.

**제안**
- 배치 책임을 GameMode에서 시험대로 옮긴다. GameMode는 시험대를 생성만 한다.
- 시험대는 BeginPlay에 바로 놓지 않는다. Tick에서 `GEngine->XRSystem->IsTracking(IXRTrackingSystem::HMDDeviceId)`(`Engine/Source/Runtime/HeadMountedDisplay/Public/IXRTrackingSystem.h:120`, `:70`)가 처음 true가 된 프레임에 놓는다. XR 시스템이 없으면(자동화·헤드셋 없는 PIE) 바로 Pawn 카메라 기준으로 놓는다.
- 머리 기준: Pawn `Camera`의 월드 위치와 **Yaw만**(Pitch·Roll 제외) 쓴다. 위치 = 카메라 + Yaw 정면 × 45 cm − 월드 Z × 25 cm(프로퍼티, 임시값). 시험대 방향은 그 Yaw.
- 버튼을 시험대에 붙인다(`AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform)`). 그러면 시험대 하나만 옮겨도 다 따라온다. ISDK 버튼 visual은 Actor가 움직이면 기준 자세를 다시 잡는다(`Plugins/MetaXRInteraction/Source/OculusInteraction/Private/Interaction/IsdkPokeButtonVisual.cpp:236-257`).
- `R` 키: 시험대가 `EnableInput(PlayerController)`(`Engine/Source/Runtime/Engine/Classes/GameFramework/Actor.h:1389`) 뒤 `InputComponent->BindKey(EKeys::R, IE_Pressed, ...)`로 받는다(P0-a와 같은 방식). 다시 놓기 전에 진행 중인 누름 측정을 모두 취소하고 "Bench re-placed" 이벤트를 남긴다. 이 취소는 R1-5의 추적 취소 수에 넣지 않는다.
- 머리 Transform → 시험대 Transform 계산은 순수 함수로 나눈다.
- 주의: VR Preview 중 키보드는 PIE 창 포커스가 있어야 들어온다(P0-a 명세 §5 키보드 항목).

**확인할 시험**
- 배치 계산: Yaw 30°·Pitch −40°·Roll 15°인 머리에서 시험대는 Yaw 30°만 따르고, 수평 정면 45 cm, 아래 25 cm.
- 다시 놓기: 진행 중인 누름이 취소되고, 추적 취소 수는 그대로다.
- 붙임: 시험대를 옮기면 버튼 Actor들이 같은 상대 위치로 따라온다.

### R1-4 select 순간의 원본 깊이를 기록한다 — 열림

**문제**
- **위치:** `Source/vBoR/P0Probe/P0ButtonBench.cpp:134-137` (Select 처리).
- **영향:** select는 ISDK 손 모델의 poke 지점이 판정 평면(앞면 아래 눌림 깊이 3 mm)에 닿을 때 일어나고, 관통 깊이는 엔진 원본 손끝으로 잰다. 두 손끝 위치가 어긋나 있으면 관통 깊이를 그만큼 보정해서 읽어야 하는데, 지금은 그걸 알 방법이 없다.

**제안**
- `FP0ButtonPressMeasurement`에 select 순간 깊이(`DepthAtSelectMm`)와 가용 여부를 더한다. Select 처리에서 그 순간 원본 손끝으로 한 번 잰 값을 저장한다. 최대값과 별개다.
- **이 값은 부호를 남긴다**(0으로 자르지 않는다). 원본 손끝이 앞면보다 앞에 있으면 음수다. 그 자체가 두 손끝이 어긋났다는 정보다.
- 원본 손끝을 못 읽으면 n/a.
- Last press 줄과 로그에 "select depth"로 표시한다.

**확인할 시험**
- select 순간 값이 최대값과 따로 저장된다. 음수가 그대로 남는다. 못 읽으면 n/a다. 새 누름(hover)에 초기화된다.

### R1-5 버려진 측정과 놓친 시도를 센다 — 열림

**문제**
- **위치:** `Source/vBoR/P0Probe/P0ButtonBench.cpp:92` — 원본 손끝이 Tracked가 아닌 프레임이 한 번만 있어도 진행 중인 누름을 취소한다. 처리 자체는 맞지만 몇 번 버려졌는지 보이지 않는다.
- **영향:** 실측에서 기록이 적을 때 원인(추적 끊김인지, 누르지 못한 것인지)을 구분할 수 없다. 명세 §5의 "놓친 횟수"도 사람이 세야 한다.

**제안**
- 측정 구조체가 결과를 돌려주게 한다. 활성(hover 또는 select) 중에 무효 샘플이 오면 "추적 취소"로 1 늘린다. 이미 취소된 뒤 무효가 반복되면 세지 않는다.
- Unhover를 받았을 때 hover 중이었고 select가 한 번도 없었으면 "놓친 시도"로 1 늘린다. select→unselect 뒤에 오는 Unhover는 세지 않는다(Unselect가 이미 hover를 끄므로 자연히 빠진다).
- ISDK의 Cancel 이벤트로 끝난 것은 지금처럼 로그로만 남긴다. R(다시 놓기) 취소는 세지 않는다.
- 패널·로그: "Canceled (tracking): L=… R=… | Missed: L=… R=…".

**확인할 시험**
- 활성 중 무효 → +1, 그 뒤 무효 반복 → +0.
- hover → unhover → 놓친 시도 +1.
- hover → select → unselect → unhover → 놓친 시도 +0.

### R1-6 시험이 ISDK 내부 네이티브 API에 의존한다 — 열림

**문제**
- **위치:** `Source/vBoR/vBoR.Build.cs`(`PrivateIncludePaths`에 `Plugins/MetaXRInteraction/Source/ThirdParty/OculusInteractionLibrary/Include` 직접 지정, `OculusInteractionLibrary` 의존), `Source/vBoR/P0Probe/P0ButtonBenchTests.cpp:96`(`isdk_api/isdk_api.hpp`), 같은 파일 `:110-122`(네이티브 raycast).
- **영향:** ISDK는 이 헤더를 자기 모듈 안에서만 쓴다(`Plugins/MetaXRInteraction/Source/OculusInteraction/OculusInteraction.Build.cs:37-40`, PrivateIncludePaths). 시험 하나 때문에 게임 모듈 전체가 ISDK 내부 경로에 묶이고, ISDK를 올리면 깨지기 쉽다.

**제안**
- 15 mm 경계 시험을 공개 API로 바꾼다. `UIsdkPointablePlane::GetSize()`는 **반 크기**이고, `Size.X`가 평면 로컬 Y(Right), `Size.Y`가 로컬 Z(Up)다(`Plugins/MetaXRInteraction/Source/OculusInteraction/Public/Interaction/Surfaces/IsdkPointablePlane.h:50-57`, `Private/Interaction/Surfaces/IsdkPointablePlane.cpp:141`). 확인할 것:
  - (a) `GetSize()`가 (버튼 Actor Y 방향 반폭, Actor X 방향 반폭)과 같다.
  - (b) 평면 컴포넌트의 월드 Right 벡터가 Actor Y축과, Up 벡터가 Actor X축과 평행하다.
  - (c) 법선이 Actor +Z다(기존 시험 유지).
  - 이 셋이 맞으면 경계가 보이는 크기와 같다.
- 그다음 Build.cs의 `PrivateIncludePaths` 줄과 `OculusInteractionLibrary` 의존을 빼고, 시험의 `isdk_api.hpp` include와 `GetApiClippedPlaneSurface()` 사용을 지운다. `OculusInteraction`·`OculusInteractionPrebuilts` 의존은 그대로 둔다.

**확인할 시험**
- 위 (a)~(c). 메시를 90° 돌린 자식 구성에서도 (a)·(b)가 맞는다.
- Build.cs에서 두 줄을 뺀 뒤 깨끗하게 빌드된다.

## 참고 (열린 항목 아님)

- 부모 Pawn은 잡기와 입력 액션 바인딩을 꺼 둔다(`VBoRHandRigPawn.cpp:24-25, 32, 39`). P0-b는 poke만이라 맞다(D1). P1(골프백)에서 잡기를 켤 때 원거리 잡기 없이 켜는 방법은 그 명세에서 정한다.

## 질문

(Codex가 판단이 필요할 때 여기에 적고 멈춘다. Claude가 답을 이 절에 적는다.)
