# P0-b 리뷰 기록

명세: [Docs/Specs/P0-b-poke-button-bench.md](../Specs/P0-b-poke-button-bench.md) · 브랜치: `codex/P0-b-poke-button-bench`

**Codex가 할 일:** 상태가 `열림`인 항목을 위에서부터 반영한다. 항목마다 **문제**는 반드시 해결하고, **제안**은 권장이다(다르게 하면 보고 ③에 이유를 적는다). **확인할 시험**은 새로 넣거나 고칠 자동화 시험이다. 같은 브랜치에 새 커밋으로 하고, 반영한 항목의 상태를 `반영함 (<커밋 제목>)`으로 바꿔 같은 커밋에 넣는다. 커밋 본문에 보고 ①~⑤. 판단이 필요하면 맨 아래 "질문"에 적고 멈춘다. push·`main` 머지는 하지 않는다. 끝나면 `HANDOFF.md`의 "Codex 결과"를 채운다.

## 회차

| 회차 | 날짜 | 리뷰 대상 | 리뷰어 | 결과 |
|---|---|---|---|---|
| 1 | 2026-10-10 | `6562171` P0-b 시험대 | Claude | **수정 후 실측.** 빌드·시험은 통과. 부모 버튼이 메시 재질을 덮는 문제, 동시 활성 수 누락, 시험대 위치 기준 등 6건 |
| 2 | 2026-10-10 | `fe53a8d` 1회차 반영 + VR 첫 실행 로그 | Claude | **반영 확인. 새 항목 3건(작음).** R1-1~R1-6 제안대로 구현됨. VR 실행에서 원거리 잡기 감지기가 만들어지는 것을 발견(R2-1). **측정 자체는 약 15 fps라 참고용** — 렌더링 설정 결정 필요(사용자) |
| 3 | 2026-10-10 | `f2594fd` 2회차 반영 | Claude | **통과. 열린 항목 없음.** R2-1·R2-2 제안대로. R2-3은 Claude의 잘못된 지적(두 호출은 BeginPlay와 EndPlay에 하나씩) |

### 3회차에 확인한 것

- R2-1: 양손 잡기 컴포넌트에 `SetGrabDetectionTypeAllowed(DistanceGrab/RayGrab, false)`. 생성자 시점에 잡기 컴포넌트가 이미 있다는 근거(`IsdkRigComponent.cpp:61-62, 233-235`, `IsdkGrabInteractionRigComponent.cpp:41-42`)와 잡기 리그를 다시 켜도 차단이 유지되는 시험(`GrabDetectionRestrictions`)이 있다. 시작 로그는 객체 수 대신 손별 `grabber active / distance allowed / ray grab allowed`를 찍는다.
- R2-2: 단독 버튼 놓친 시도와 몰린 묶음 지나가기를 손별로 나눴다(`AttemptGroups`).
- R2-3: Claude가 diff에서 `SetOverlayMaterial(nullptr)` 두 줄을 중복으로 잘못 읽었다. 실제로는 `VBoRPokeButton.cpp:87`(BeginPlay)과 `:96`(EndPlay)이다. Codex가 고치지 않은 것이 맞다.
- `vBoR.*` 24개 Success(샘플 플러그인 끔)를 재현. **에디터가 열려 있어(11:11 시작, 핫 리로드 DLL `UnrealEditor-vBoR-6845.dll` 생성) 재컴파일 재현은 못 했다.** Codex 빌드 로그 `P0ButtonR2Build.log`는 Succeeded. 머지 전에 에디터를 닫고 재컴파일로 확인한다.

### 2회차에 확인한 것

- `vBoR.*` 22개 Success(샘플 플러그인 끔)를 현재 바이너리로 재현. **에디터가 열려 있어 C++ 재컴파일 재현은 못 했다** — Codex 빌드 로그(`Saved/Logs/P0ButtonR1Build.log`, 경고·오류 0)만 확인. 에디터를 닫은 뒤 다음 회차에 재컴파일로 재현한다.
- R1-1: 원래 재질 슬롯을 건드리지 않고 hover·select에서만 `SetOverlayMaterial`, `GrabbableLit` 반투명·`ColorParam` 알파 → 불투명도는 에셋 로드 시험으로 확인함. R1-2: hover 또는 select. R1-3: 첫 HMD 추적 프레임에 카메라 Yaw 기준 배치, `R` 다시 놓기, 버튼 붙임. R1-4: 부호 있는 select 깊이. R1-5: 추적 취소·놓친 시도. R1-6: 내부 헤더·라이브러리 의존 제거, 공개 API 경계 시험. 모두 제안대로다.
- **VR 첫 실행(2026-10-10, 사용자, Link, RTX 3060) 로그에서:** 레이 `active=0, enabled=0`(양손), limiter 활성, 데이터 소스 `IsdkFromOpenXRHandDataSource`, 먼 버튼 hover·select 0, 추적 취소 0, 몰린 묶음 동시 활성 최대 1. 측정 수치는 조사 기록 "2026-10-10 P0 첫 VR 실행"에 정리했다.
- **프레임:** 로그 프레임 번호로 약 15 fps(P0-a·P0-b 둘 다). 데스크톱 렌더링(Lumen·레이트레이싱·VSM) + 에디터 VR Preview의 HMD 전체 해상도 때문으로 보인다. 이 상태의 관통 깊이·시간은 지연이 섞여 있어 판단 근거로 쓰지 않는다.

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

없음 — 3회차(2026-10-10) 통과.

## 2회차 항목 (모두 반영함)

### R2-1 원거리·레이 잡기 감지 유형을 부모 Pawn에서 막는다 — 반영함 (`f2594fd`)

**Codex 반영:** 양손 리그 생성자에서 `DistanceGrab`·`RayGrab`을 허용 목록에서 제거함. `GetGrabber()`는 리그 생성자에서 만든 GrabInteraction의 기본 하위 객체를 반환하므로 이 시점에 존재함(`IsdkRigComponent.cpp:61-62, 233-235`, `IsdkGrabInteractionRigComponent.cpp:41-42`). 객체 개수 대신 양손 grabber 활성·감지 유형 허용 여부를 표시함. `GrabDetectionRestrictions`에서 BeginPlay 및 잡기 리그 재활성화 뒤 양손의 두 유형 차단을 확인함. 같은 시험은 수정 전에 양손 8개 차단 assertion에서 실패했고 수정 후 통과함.

**문제**
- **위치:** `Source/vBoR/Interaction/VBoRHandRigPawn.cpp:17-26`(`ConfigurePokeRig`) — 잡기 리그를 비활성화만 한다. 같은 파일 `DescribeRig`는 원거리 잡기 감지기 **객체 수**를 센다.
- **발생 조건:** 2026-10-10 VR 실행 로그에 `Distance grab: 2`가 찍혔다. ISDK 잡기 컴포넌트는 원거리 잡기 감지기를 항상 기본 하위 객체로 만든다(`Plugins/MetaXRInteraction/Source/OculusInteraction/Private/Interaction/IsdkGrabberComponent.cpp:51-52`). 지금은 잡기 리그가 꺼지면 잡기 컴포넌트도 꺼져서(`OculusInteractionPrebuilts/Private/Rig/IsdkGrabInteractionRigComponent.cpp:112-133`) 동작하지 않는다.
- **영향:** 이 Pawn은 부모다(D3). P1(골프백)에서 잡기를 켜는 순간 원거리 잡기와 레이 잡기도 같이 살아난다. 원거리 잡기는 U-03·U-04 금지다. 로그의 "2"도 실제로 켜진 것인지 오해를 부른다.

**제안**
- `ConfigurePokeRig`에서 잡기 컴포넌트(`GetGrabber()`, `IsdkRigComponent.h:193`)에 `SetGrabDetectionTypeAllowed(EIsdkGrabDetectorType::DistanceGrab, false)`와 `(RayGrab, false)`를 건다(`OculusInteraction/Public/Interaction/IsdkGrabberComponent.h:200`). 감지 여부는 `IsGrabDetectionTypeAllowed`로 판정된다(`IsdkGrabberComponent.cpp:206-213, 236-243, 848-851`).
- 생성자에서 잡기 컴포넌트가 아직 없으면 BeginPlay(또는 리그 초기화 뒤)에 건다. 어느 시점에 거는지 근거를 보고한다.
- `DescribeRig`는 객체 수 대신 손별로 "grabber active=?, distance allowed=?, ray grab allowed=?"를 찍는다.

**확인할 시험**
- 메모리 월드에서 Pawn을 BeginPlay까지 돌린 뒤, 양손 잡기 컴포넌트에서 DistanceGrab·RayGrab이 허용되지 않음. 잡기 리그를 다시 활성화해도 허용되지 않음.

### R2-2 "놓친 시도"를 단독 버튼과 몰린 묶음으로 나눈다 — 반영함 (`f2594fd`)

**Codex 반영:** 손별 `Missed single`·`cluster pass`로 패널·주기 로그를 분리함. Unhover 이벤트에도 단독·묶음 구분을 표시함. 먼 버튼은 기존 hover·select 집계를 유지하며 두 합계에서 제외함. 추적 취소는 별도 합계를 유지함. `AttemptGroups`에서 단독/묶음 hover→unhover의 독립 집계, 먼 버튼 제외, 추적 취소·다른 손 분리를 확인함. `ButtonComponents`에서 실제 패널의 새 항목을 확인함.

**문제**
- **위치:** `Source/vBoR/P0Probe/P0ButtonBench.cpp` — hover 뒤 select 없이 끝나면 모두 "Missed"로 합산한다.
- **발생 조건:** 몰린 묶음에서는 손가락이 목표로 가는 동안 이웃 버튼을 차례로 hover한다. 2026-10-10 실행에서 Missed 22건 중 대부분이 몰린 묶음의 지나가기였다.
- **영향:** 단독 버튼에서 실제로 못 누른 시도와 몰린 묶음의 지나가기가 한 숫자에 섞여 "못 누른 횟수"로 읽을 수 없다.

**제안**
- 손별로 단독 버튼과 몰린 묶음의 놓친 시도를 따로 센다(먼 버튼은 hover 자체가 0이어야 하므로 그대로). 패널: `Missed single: L/R | cluster pass: L/R`.

**확인할 시험**
- 단독 버튼 hover→unhover는 single에만, 몰린 묶음은 cluster에만 더해진다.

### R2-3 덧입힘 해제 중복 호출 — 닫음 (Claude의 잘못된 지적)

**Codex 확인:** 현재 작업 시작점 `902fca5`와 리뷰 대상 `fe53a8d` 모두 `BeginPlay`의 `ButtonMesh->SetOverlayMaterial(nullptr)`가 한 번뿐임(`VBoRPokeButton.cpp:87`). `EndPlay`와 비활성 상태 처리의 호출은 다른 생명주기이므로 유지함. 제거할 중복이 없어 버튼 소스는 변경하지 않음. 기존 `ButtonComponents`의 Normal 덧입힘 없음/복귀 시험은 통과함.

- **위치:** `Source/vBoR/Interaction/VBoRPokeButton.cpp` BeginPlay — `ButtonMesh->SetOverlayMaterial(nullptr);`가 두 번 연속 있다. 동작 영향은 없다. 하나를 지운다.

## 1회차 항목 (모두 반영함, `fe53a8d`)


### R1-1 상태 색이 버튼 메시의 원래 재질을 덮는다 — 반영함 (`fe53a8d`)

**반영 확인:** `GrabbableLit`의 `BLEND_Translucent`와 `ColorParam` 알파 → Opacity 연결을 실제 로드한 에셋에서 확인함. 원래 슬롯은 보존하고 hover·select에만 MID 하나를 덧입힘. Normal에는 없음. `OverlayMaterial`·`ButtonComponents` 시험에서 원래 슬롯 객체, 상태별 색·설정 불투명도, Normal 복귀를 확인함.

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

### R1-2 몰린 묶음의 동시 활성 수가 select 중인 버튼을 빼고 센다 — 반영함 (`fe53a8d`)

**반영 확인:** `CountActive`가 hover 또는 select를 셈. 패널은 `Cluster active (both hands)`로 표시함. `SimultaneousActive`에서 select+hover=2, select=1, normal/disabled=0, 2 이상 진입 전환 횟수를 확인함.

**문제**
- **위치:** `Source/vBoR/P0Probe/P0ButtonBench.cpp:166-171` — `GetCurrentState() == Hover`만 센다.
- **발생 조건:** 한 버튼을 누르는(select) 동안 손끝이 이웃 버튼에도 hover할 때.
- **영향:** U-04가 막으려는 "둘 이상 동시 활성"이 1로 세어진다. 이 시험대가 답해야 할 핵심 질문(ISDK 기본이 하나만 활성인가)을 놓친다. 명세 §2 표의 "hover 상태인 버튼 수"가 Claude의 잘못된 문구였고, "hover 또는 select"로 고쳤다.

**제안**
- 상태 배열을 받아 활성 수를 세는 순수 함수(예: `CountActive`)를 측정 헤더에 두고, Hover 또는 Select를 활성으로 센다. 시험대는 프레임 끝에 그 함수를 부른다.
- 패널 표시를 "Cluster active (양손 합)"로 바꾼다. 두 손이 같이 묶음에 있으면 합쳐서 센다는 것을 이름으로 드러낸다.

**확인할 시험**
- `SimultaneousHover` → `SimultaneousActive`로 바꾸고: {Select, Hover} = 2, {Select} = 1, {Normal, Disabled} = 0, 2 이상 진입 횟수는 전환 때만.

### R1-3 시험대가 머리가 아니라 Pawn 원점 기준으로 놓인다 — 반영함 (`fe53a8d`)

**반영 확인:** GameMode는 생성만 함. 시험대가 첫 유효 HMD 추적을 기다려 Camera의 위치·Yaw로 배치함(XR가 없으면 Camera 기준). `R` 바인딩·측정 취소·`Bench re-placed` 기록, 버튼 Actor 붙임을 추가함. `HeadPlacement`의 기울어진 머리, `AttemptOutcomes`의 공유 다시 놓기 취소 함수, `ButtonComponents`의 버튼 상대 위치 유지 시험으로 확인함. 실제 HMD 대기·PIE 키 포커스는 기기 후속 확인임.

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

### R1-4 select 순간의 원본 깊이를 기록한다 — 반영함 (`fe53a8d`)

**반영 확인:** `DepthAtSelectMm`·가용 여부를 추가하고 select 순간에만 저장함. 최대값과 따로 부호를 보존하며 새 hover에서 초기화됨. Last press 및 select 이벤트 로그에 깊이/n/a를 표시함. `SelectDepth` 시험으로 음수·최대값 분리·무효/n/a·초기화를 확인함.

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

### R1-5 버려진 측정과 놓친 시도를 센다 — 반영함 (`fe53a8d`)

**반영 확인:** 활성 측정의 무효 샘플은 추적 취소를 한 번 셈. select 없는 Unhover는 놓친 시도로 셈. 누름별 누적 계수를 손별로 합산하여 패널·로그에 표시함. `AttemptOutcomes`에서 무효 반복, hover만 끝난 경우, 정상 누름 뒤 Unhover, 수동 다시 놓기 취소를 확인함. ISDK Cancel은 합계에 넣지 않음.

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

### R1-6 시험이 ISDK 내부 네이티브 API에 의존한다 — 반영함 (`fe53a8d`)

**반영 확인:** 내부 헤더·native raycast·native surface 접근과 Build.cs의 내부 모듈/경로 의존을 제거함. `ButtonComponents`가 공개 GetSize·Right/Up 벡터·법선으로 경계를 확인함. 메시 90 deg 회전 구성도 확인했고 의존 제거 후 vBoR 전체 소스를 재컴파일함.

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

## Codex 2회차 반영 보고 (2026-10-10)

① **바뀐 파일**

- `Source/vBoR/Interaction/VBoRHandRigPawn.cpp`: 원거리·레이 잡기 감지 유형 차단, 손별 실제 상태 표시.
- `Source/vBoR/P0Probe/P0ButtonBenchMeasurement.h`: 단독 버튼 놓친 시도와 몰린 묶음 지나가기 합계 분리.
- `Source/vBoR/P0Probe/P0ButtonBench.cpp`: 새 합계를 패널·로그에 표시, Unhover 로그 분류.
- `Source/vBoR/P0Probe/P0ButtonBenchTests.cpp`: `GrabDetectionRestrictions`·`AttemptGroups` 추가, 실제 패널 표시 시험 갱신.
- 이 리뷰 파일: R2-1~R2-3 상태·근거·보고 기록. git에서 제외된 `HANDOFF.md`에는 커밋 후 결과 기록.

② **명세 §4 완료 조건**

| 조건 | 확인 상태 |
|---|---|
| 1. 측정·표시 | 충족. 기존 측정 유지, 단독 놓침/묶음 지나가기 분리. 실제 패널 문자열 자동화 확인. HMD 가독성은 후속 확인 |
| 2. 금지 입력·limiter | 코드·메모리 월드 확인. 양손 DistanceGrab/RayGrab 차단, grab 리그 재활성화 뒤에도 차단 유지. 기존 ray 비활성·컨트롤러 없음·limiter 구성 유지. 실제 먼 버튼 가리키기는 미확인 |
| 3. 원본 손끝 | 기존 XR IndexTip 경로 유지. limiter 모델 위치를 읽지 않음. 기기 정확도·깊이 실측은 미확인 |
| 4. 합성 자동화 | 충족. P0-b 12개(새 시험 2개 포함), 기존 P0-a 12개 전부 통과 |
| 5. 샘플 독립 | 충족. 샘플 플러그인을 끄고 빌드·시험. vBoR 소스의 샘플 경로·모듈 참조 없음 |
| 6. API 대조 | 충족. 이번 변경은 아래 헤더·구현으로 확인. 기존 버튼·원본 손끝 API 근거는 `fe53a8d` 보고 ④ 유지. 추정 API 없음 |
| 7. 로컬 빌드·시험 | 충족. UE 5.7 Win64 Development Succeeded, 컴파일·링크 경고/오류 0. `vBoR.*` 24/24 Success, 프로세스 종료 코드 0 |
| 8. 로컬 후속 확인 | 아래 ⑤에 기록 |

검증 로그: `Saved/Logs/P0ButtonR2Build.log`, `P0ButtonR2Automation.log` (`TEST COMPLETE. EXIT CODE: 0`), 수정 전 실패 재현 `P0ButtonR2Red.log` (예상된 차단 assertion 실패). 자동화 시작 시 알려진 미사용 InfoCard의 샘플 에셋 CDO 오류 등은 Agent-Workflow §2.5의 잡음이며, 자동화 실패는 0건임.

③ **명세·제안과 다르게 한 것 및 이유**

- R2-1·R2-2는 제안대로 반영함. Grabber는 생성자에서 존재하므로 BeginPlay로 늦추지 않음. ISDK 기본 감지기 객체를 지우지 않고 허용 유형으로 차단함(R2-1).
- R2-3은 현재 및 리뷰 대상 소스에 중복이 없어서 제거하지 않음. 확인 근거를 해당 항목에 기록함.
- 그 밖의 범위 변경 없음. XR·렌더링 설정 및 사용자 시험 레벨 변경 없음.

④ **확인한 API 근거**

- `Plugins/MetaXRInteraction/Source/OculusInteractionPrebuilts/Public/Rig/IsdkRigComponent.h:193` — `GetGrabber()`; `:214` — `GetGrabInteraction()`.
- `Plugins/MetaXRInteraction/Source/OculusInteraction/Public/Interaction/IsdkGrabberComponent.h:192,200` — `IsGrabDetectionTypeAllowed`·`SetGrabDetectionTypeAllowed`. 구현 `Private/Interaction/IsdkGrabberComponent.cpp:848-865`에서 허용 목록 조회·제거만 수행함을 확인함. 감지 Tick 및 hovered 대상 수집도 허용 여부로 제한함(`:206-213,236-243`).
- `Plugins/MetaXRInteraction/Source/OculusInteractionPrebuilts/Private/Rig/IsdkRigComponent.cpp:61-62,233-235` 및 `Private/Rig/IsdkGrabInteractionRigComponent.cpp:41-42` — GrabInteraction 및 Grabber 생성·반환 시점. `IsdkGrabInteractionRigComponent.cpp:112-133` — 리그 활성 상태가 Grabber 활성으로 연결됨.
- `C:/Program Files/Epic Games/UE_5.7/Engine/Source/Runtime/Engine/Classes/Components/ActorComponent.h:563,590` — 시험의 `Activate(bool)` 및 상태 표시의 `IsActive()`.
- 기존 원본 손끝·plane·overlay·limiter API는 이전 반영 보고(`fe53a8d` 커밋 본문 ④)와 동일함.

⑤ **로컬에서 확인할 것**

- 빌드·합성 시험: 이번 세션에서 완료. 샘플 플러그인을 끈 UE 5.7 Editor 빌드 및 전체 `vBoR.*` 24개 통과.
- 에디터: 기존 P0-b 레벨에서 양손 상태 로그(감지 유형 0), 단독/묶음 카운터 구분과 패널 가독성, 자식 Blueprint 메시 교체·90 deg 회전·라벨 보존·덧입힘 색 확인. C++ 시험은 실제 렌더링 증거가 아님.
- Quest 3 Link: 회사 PC에서 명세 §5의 양손·크기별 반복, 몰린 목표 3곳, 먼 버튼 가리키기, limiter·눌림·색·소리 체감 및 `R` 다시 놓기 확인. 정식 측정은 계속 `not-tested`.
- 이 작업은 P0 실측이며 명세 §5에 따라 T-ID 시험이 아님. 후속 T-07(도달 평가)·T-09(운영)에는 입력 차단 및 기기 측정 근거로 연결하되, 해당 시험을 통과 처리하지 않음.

## 질문

(2026-10-10 Codex R2 반영: 질문 없음.)

(Codex가 판단이 필요할 때 여기에 적고 멈춘다. Claude가 답을 이 절에 적는다.)
