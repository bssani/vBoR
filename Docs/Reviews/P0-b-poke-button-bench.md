# P0-b 리뷰 기록

명세: [Docs/Specs/P0-b-poke-button-bench.md](../Specs/P0-b-poke-button-bench.md) · 브랜치: `codex/P0-b-poke-button-bench`

**Codex가 할 일:** 아래 "열린 항목"을 위에서부터 반영한다. 같은 브랜치에 새 커밋으로 하고, 반영한 항목의 상태를 `반영함 (<커밋 제목>)`으로 바꿔 같은 커밋에 넣는다. 커밋 본문에 보고 ①~⑤. 판단이 필요하면 맨 아래 "질문"에 적고 멈춘다. push·`main` 머지는 하지 않는다. 끝나면 `HANDOFF.md`의 "Codex 결과"를 채운다.

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

- **중요도:** 중간. 명세 §2 "hover·select 색이 버튼 메시에 보여야 한다"는 문구가 재질 보존을 말하지 않은 Claude의 누락이다. 명세를 고쳤다.
- **위치:** `Source/vBoR/Interaction/VBoRPokeButton.cpp:84-94` — BeginPlay에서 버튼 메시의 모든 재질 슬롯을 `GrabbableLit` 동적 재질로 바꾼다.
- **발생 조건:** 자식 Blueprint에 실제 차량 버튼 메시(라벨·텍스처가 있는 재질)를 넣을 때.
- **영향:** 실행하면 라벨과 텍스처가 사라지고 단색 버튼이 된다. "메시만 바꾸면 적용"(D3)이 깨지고, 라벨 판독은 평가 대상이다(U-04).
- **할 일:** 명세 §2-2 "상태 표시"대로. 원래 재질을 바꾸지 않고, 메시 위에 덧입히는 방식(`UMeshComponent::SetOverlayMaterial` 등 엔진 기능)으로 hover·select를 표시한다. 쓸 수 있는 반투명 재질 에셋이 엔진·ISDK에 없으면, 새 에셋이 필요하다는 것을 "질문"에 적는다(에셋은 사용자가 에디터에서 만든다). 그동안 시험대 Cube는 지금처럼 색이 보여야 한다.

### R1-2 몰린 묶음의 동시 활성 수가 select 중인 버튼을 빼고 센다 — 열림

- **중요도:** 중간. 명세 §2 표가 "hover 상태인 버튼 수"라고 잘못 썼다(Claude). 명세를 "hover 또는 select"로 고쳤다.
- **위치:** `Source/vBoR/P0Probe/P0ButtonBench.cpp:166-171` — `GetCurrentState() == Hover`만 센다.
- **발생 조건:** 한 버튼을 누르는(select) 동안 손끝이 이웃 버튼에도 hover할 때.
- **영향:** U-04가 막으려는 "둘 이상 동시 활성"이 1로 세어진다. 이 시험대가 답해야 할 핵심 질문(ISDK 기본이 하나만 활성인가)을 놓친다.
- **할 일:** hover 또는 select 상태인 버튼 수를 센다. 표시 이름을 "Cluster active"로. 시험 `SimultaneousHover`도 그 의미로.

### R1-3 시험대가 머리가 아니라 Pawn 원점 기준으로 놓인다 — 열림

- **중요도:** 중간. 실측할 수 있는지에 걸린다.
- **위치:** `Source/vBoR/P0Probe/P0ButtonBenchGameMode.cpp:18-22` — Pawn 위치 + (45, 0, 120) cm.
- **발생 조건:** UE 5.7 OpenXR의 기본 추적 공간은 Stage라서, Pawn 원점은 가디언 중앙 바닥이다. 참가자가 가디언 중앙이 아닌 곳에 서 있거나 앉아 있을 때.
- **영향:** 시험대가 손 닿는 거리에 없을 수 있다. 사용자가 위치를 바꾸려 해도 GameMode 프로퍼티라 World Settings에서는 고칠 수 없다.
- **할 일:** 명세 §2-4대로. 첫 유효 HMD 자세가 잡힌 뒤 머리 기준(정면 수평 45 cm, 눈높이 아래 25 cm — 임시값)으로 시험대를 놓고, 진행자 키(기본 `R`)로 현재 머리 기준으로 다시 놓는다. 다시 놓을 때 진행 중인 누름 측정은 취소한다.

### R1-4 select 순간의 원본 깊이를 기록한다 — 열림

- **중요도:** 낮음~중간. 측정 해석용이다.
- **위치:** `Source/vBoR/P0Probe/P0ButtonBench.cpp:134-137` (Select 처리).
- **이유:** select는 ISDK 손 모델의 poke 지점이 판정 평면(앞면 아래 눌림 깊이 3 mm)에 닿을 때 일어난다. 관통 깊이는 엔진 원본 손끝으로 잰다. 두 손끝 위치가 다르면 관통 깊이를 그만큼 보정해 읽어야 한다. select 순간의 원본 깊이가 3 mm 근처면 둘이 맞는 것이다.
- **할 일:** 누름 기록에 "select 순간 원본 깊이(mm)"를 더한다(패널 Last press와 로그). 명세 §2 표에 추가했다.

### R1-5 버려진 측정과 놓친 시도를 센다 — 열림

- **중요도:** 낮음.
- **위치:** `Source/vBoR/P0Probe/P0ButtonBench.cpp:92` — 원본 손끝이 Tracked가 아닌 프레임이 한 번만 있어도 진행 중인 누름을 취소한다. 맞는 처리지만 몇 번 버려졌는지 보이지 않는다.
- **영향:** 실측에서 기록이 적을 때 원인(추적 끊김인지, 누르지 못한 것인지)을 구분할 수 없다. 명세 §5의 "놓친 횟수"도 사람이 세야 한다.
- **할 일:** 패널·로그에 손별 합계로 (a) 추적 때문에 취소된 누름 수 (b) hover했지만 select 없이 끝난 시도 수. 시험 추가.

### R1-6 시험이 ISDK 내부 네이티브 API에 의존한다 — 열림

- **중요도:** 낮음.
- **위치:** `Source/vBoR/vBoR.Build.cs`(`PrivateIncludePaths`에 `Plugins/MetaXRInteraction/Source/ThirdParty/OculusInteractionLibrary/Include` 직접 지정, `OculusInteractionLibrary` 의존), `Source/vBoR/P0Probe/P0ButtonBenchTests.cpp:96`(`isdk_api/isdk_api.hpp`).
- **이유:** ISDK는 이 헤더를 자기 모듈 안에서만 쓴다(`Plugins/MetaXRInteraction/Source/OculusInteraction/OculusInteraction.Build.cs:37-40`, PrivateIncludePaths). ISDK를 올리면 깨지기 쉽고, 게임 모듈 전체가 시험 하나 때문에 내부 헤더 경로를 갖는다.
- **할 일:** 15 mm 경계 시험을 공개 API로 바꾼다(`UIsdkPointablePlane::GetSize()`의 반 크기와 평면 컴포넌트의 월드 축이 버튼 가로·세로와 맞는지). 그다음 Build.cs의 경로 추가와 `OculusInteractionLibrary` 의존을 뺀다.

## 참고 (열린 항목 아님)

- 부모 Pawn은 잡기와 입력 액션 바인딩을 꺼 둔다(`VBoRHandRigPawn.cpp:24-25, 32, 39`). P0-b는 poke만이라 맞다(D1). P1(골프백)에서 잡기를 켤 때 원거리 잡기 없이 켜는 방법은 그 명세에서 정한다.

## 질문

(Codex가 판단이 필요할 때 여기에 적고 멈춘다. Claude가 답을 이 절에 적는다.)
