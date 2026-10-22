# P5x 판 손잡이 — 별도 손잡이 BP로만

| 항목 | 값 |
|---|---|
| 단계 | P5 자유모드·리허설 준비(사용자 요청 2026-10-22). 시험 T-03(문 손 조작)·T-08(자유모드 — 콘솔·글러브박스) |
| 결정 근거 | **사용자(2026-10-22):** "기존 방식대로 가야하는데, 왜 door class에 손잡이 위치를 보여주는것들이 있냐 이거지. 그건 필요 없잖아" / "별도 손잡이 bp로만 해도 충분하지 않나? 굳이 문 클래스의 잡는 상자가 필요한가?" → Claude 안(문 클래스의 잡는 상자 삭제, 콘솔은 직선 이동 손잡이 BP) → "그렇게 해" |
| 베이스 브랜치 | `main` @ 이 명세 머지 뒤 |
| 작업 브랜치 | `codex/P5x-panel-handles-only` |
| 리뷰 파일 | `Docs/Reviews/P5x-panel-handles-only.md` (첫 리뷰 때 생김) |
| 상태 | 명세 — 구현 전 |
| 에디터 작업 | **있음** — `BP_ConsoleHandle` 만들기, `L_Main`에 놓고 콘솔 뚜껑 밑에 붙이기, 판 BP 3개 다시 저장(MCP). 사용자 요청 |
| 앞 명세 | [P5w-door-push-side.md](P5w-door-push-side.md) |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **판 클래스에서 내장 잡는 자리를 지운다** | `AVBoRHingedPanel`(`Interaction/VBoRHingedPanel.h/.cpp`)과 `AVBoRHingedDoor`에서 아래를 모두 지운다.<br>- 컴포넌트: `OutsideHandle`·`InsideHandle`(상자), `OutsideHandleMesh`·`InsideHandleMesh`, `OutsideGrab`·`InsideGrab`, `OutsideTransformer`·`InsideTransformer`, `DoorTransformer`(서브오브젝트 `HingeOnlyTransformer`)<br>- 클래스: `UVBoRDoorGrabTransformer`, `UVBoRDoorTransformer`(헤더 `:26-49`, 구현 `:22-39`)<br>- 프로퍼티: `bUseOutsideGrabArea`, `bUseInsideGrabArea`, `HandleVisualPressOffsetCm`<br>- 내부: `HandleVisualRestCm`, `HoldingGrabber`, `Hovering`, `bCancelPending`과 Tick의 `ForceCancel`(`:541-545`), `HandleOutsidePointer`·`HandleInsidePointer`·`HandlePointer`, `IsGrabAreaEnabled`, `UpdateGrabAreas`, `UpdateHandleVisuals`, `PreInitializeComponents`의 잡기 연결 반복문(`:404-415`), BeginPlay·EndPlay의 pointer 이벤트 묶기·풀기<br>- `AVBoRHingedDoor` 생성자의 손잡이 위치 두 줄(`VBoRHingedDoor.cpp:7-8`)<br>- 그 밖에 이것들만 쓰던 include·전방 선언 |
| D2 | **손잡이는 `AVBoRAxisPart`로만 붙는다** | 연결 규칙은 그대로다: `OutsideAxisHandle`·`InsideAxisHandle` 칸, 문 밑에 붙인 손잡이의 `DoorHandleSide`로 찾기(`CollectHandleCandidates`), 공유·반대 면·Child Actor 검사(`EvaluateSetup`), `BeginGrabFromAxisPart`·`BeginAxisGrab`·`EndAxisHold`. 판 잡기의 손잡이 번호(0 바깥 / 1 안쪽)와 `FVBoRDoorMotion`은 그대로다.<br>- **손잡이가 없는 칸은 잡을 수 없다**(지금은 내장 상자가 대신 잡혔음). 밀기는 그대로 된다.<br>- 잠금이 켜진 판에 손잡이가 하나도 없으면 지금처럼 BeginPlay Warning을 한 번 남긴다. 조건은 `bUseLatch && !OutsideAxisHandle && !InsideAxisHandle`로 줄이고, 문장의 "or grab area"를 뺀다(`:433-437`) |
| D3 | **콘솔 손잡이 BP를 만든다** | `Content/Main/Car/Parts/BP_ConsoleHandle`. 부모는 정확히 `AVBoRAxisPart`. Event Graph는 비운다.<br>- **동작(U-11 "잡으면 버튼처럼 들어간다, 손잡이 자체는 당겨지지 않는다"):** `Movement = Translate`, `MaxTravelCm = 0.75`(지금 눌림 표시 거리, `P3VehicleMock.cpp:171`), `Release = Return`, `DoorHandleSide = Outside`, 손바닥 잡기만(`bAllowPalmGrab` 켬, `bAllowPinchGrab`·`bAllowPush` 끔), `OpenThreshold` 기본(0.8).<br>- 그래서 잡고 안쪽으로 0.6 cm 이상 누르면 판이 넘겨받아 잠금이 풀리고 같은 손으로 들어 올린다(`VBoRHingedPanel.cpp:383`, `VBoRAxisPart.cpp:579-581`). 지금(잡기만 하면 풀림)과 다른 점이다.<br>- **누르는 방향은 지금 눌림 표시 방향과 같다:** 콘솔 뚜껑 경첩 좌표의 −X(차 −X, `P3VehicleMock.cpp:167`). `Axis`·`bReverse`·`AxisPivot` 회전으로 맞춘다.<br>- **메시·자리:** 지금 콘솔 손잡이 표시와 같은 Cube, 같은 크기(scale 0.02×0.03×0.14), 같은 월드 자리(콘솔 경첩 좌표 (36,0,0)). 값은 근사 차(D4)와 같게 둔다(P4i E2가 대조) |
| D4 | **근사 차 콘솔도 손잡이 Actor를 쓴다** | `AP3VehicleMock`(`P3Probe/P3VehicleMock.h/.cpp`)이 글러브박스 손잡이(`:187-198`)처럼 콘솔 손잡이 `AVBoRAxisPart`를 만들고 `ConsoleLid->OutsideAxisHandle`로 연결한다. 멤버 이름은 `ConsoleHandle`. 자리값은 `Placeholder|Approximate console` 카테고리 프로퍼티로 둔다(기존 `ConsoleLidPivotCm` 옆).<br>- 지금 콘솔 줄 `:168-172`, `:176`과 글러브박스의 `bUseInsideGrabArea = false`(`:184`)는 지운다<br>- **근사 차 문 손잡이 순서를 바꾼다(개정 1, Q2).** 지금은 문을 `FinishSpawning`한 뒤 손잡이를 만들어 `SetAxisHandle`로 넣는다(`:141-162`). 이미 Play 중인 월드에서는 문 BeginPlay가 먼저 돌아 D2 경고가 한 번 찍힌다. 글러브박스(`:197`)처럼 바꾼다: ① 문을 `SpawnActorDeferred`할 때 `FinishSpawning`과 **같은 `DoorWorld`**를 쓴다(지금은 Yaw 180 고정과 차 Yaw+180으로 서로 다름. 차가 돌아 있으면 `FinishSpawning`에서 문이 움직여, 먼저 붙인 손잡이도 따라 움직인다) ② 두 손잡이를 만들어 `FinishSpawning`하고 `Door->HingePivot`에 `KeepWorldTransform`으로 붙인다 ③ `Door->OutsideAxisHandle`·`InsideAxisHandle` 칸에 넣는다 ④ 그다음 `Door->FinishSpawning(DoorWorld)`. 문 손잡이의 월드 자리·Actor 원점(차 원점)·경첩 부모·칸 연결이 지금과 같아야 하고, 문 손잡이의 부착 로그는 생기지 않는다(이미 경첩에 붙어 있음). 다르게 나오면 질문에 적고 멈춘다 |
| D5 | **P2 시험대 문도 손잡이 Actor를 쓴다** | `AP2DoorBench`(`P2Probe/P2DoorBench.cpp:81-82`)가 문 상자 자리를 옮기던 것을, 근사 차 문 손잡이(`P3VehicleMock.cpp:146-162`)와 같은 방식의 `AVBoRAxisPart` 두 개를 만들어 연결하는 것으로 바꾼다. 자리는 지금 값(`OutsideHandleInsetCm` 등 `P2DoorBench.h:28-31`)을 그대로 쓴다. `L_P2_Door` 레벨 에셋은 열거나 저장하지 않는다 |
| D6 | **판 BP와 `L_Main`을 다시 저장한다** | C++ 컴포넌트를 지운 뒤 `BP_Door_FL`, `BP_ConsoleLid`, `BP_Glovebox`를 열어 컴파일·저장한다(그 컴포넌트에 저장된 값이 남지 않게). 저장 전후에 Output Log의 이 BP 관련 Warning·Error를 보고 ③에 적는다.<br>- `L_Main`: `BP_ConsoleHandle`을 놓고 Outliner에서 `BP_ConsoleLid` 밑에 붙인다(문 손잡이와 같은 방법, Blueprint-Guide §6.1). 근사 차 콘솔 손잡이와 같은 월드 자리. 그 밖의 Actor는 바꾸지 않는다 |
| D7 | **P4i 목록에 콘솔 손잡이를 넣는다** | `P3Probe/P4iCarBlueprintTests.cpp`: `P4iBlueprints()`에 `BP_ConsoleHandle`(부모 `AVBoRAxisPart`) 추가, 생성 개수 15 → 16(`:117`, `:169`, `:220`), 붙이기 짝(`:119`, `:268`)에 `BP_ConsoleHandle → BP_ConsoleLid` 추가, E2 짝에 `Mock->ConsoleHandle`, E3에 "콘솔 연결·경첩에 붙음" 확인 추가. E4의 "레벨에 하나씩"은 목록을 따라 자동으로 늘어난다 |

## 1. 왜 하는가

문 BP에 손잡이 자리를 정하는 방법이 둘이라 헷갈린다(사용자). 내장 상자는 손잡이 BP를 붙이면 자동으로 꺼지는데(`VBoRHingedPanel.cpp:346`), BP와 뷰포트에는 계속 보인다. 실제로 내장 상자를 손잡이로 쓰는 것은 콘솔 뚜껑뿐이었다(`BP_ConsoleLid`에 저장된 `HandleVisualPressOffsetCm`, Claude 확인 2026-10-22). 콘솔도 `AxisPart`의 직선 이동으로 같은 동작을 낼 수 있으므로 방법을 하나로 합친다.

## 2. 범위

### 만들 것

- D1·D2 판 클래스 정리.
- D3 `BP_ConsoleHandle`, D6 BP 다시 저장·`L_Main` 배치(MCP).
- D4 근사 차, D5 P2 시험대.
- D7과 §3의 기존 시험 고치기, 새 시험 `vBoR.P5x.*`(§4).

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| `AVBoRAxisPart` 동작 바꾸기 | 직선 이동·복귀·넘겨주기는 이미 있다. 막히면 질문에 적고 멈춘다 |
| `FVBoRDoorMotion`, 측정 문구(`GetMeasurementText`) | 판 잡기 경로는 그대로다. `Attempts`(내장 상자 hover 집계)는 이제 늘지 않지만 칸은 그대로 둔다(문구 형식을 바꾸지 않음) |
| 미는 면(`PushFaceBox`, `bPushOnMeshSurface`), 잠금 | 그대로 |
| 판 BP를 클래스별로 나누기, 손잡이 수별 클래스 | 사용자 결정 — 손잡이 BP 수로 정한다 |
| `Development/` 레벨 에셋 저장 | D5 |
| Blueprint-Guide·Company-PC-Guide 고치기 | 머지 뒤 Claude |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- **문·글러브박스 동작:** 이미 손잡이 BP로 열고 닫는다. 연결·넘겨주기·잠금·밀기·소리·관전 이름이 그대로다.
- **콘솔:** 잠금 켬, 손바닥으로 열기 끔(닫는 방향 손바닥만, U-11), 최대 100°, 관전 이름 "Console Box". 바뀌는 것은 D3의 "눌러야 풀림" 하나다.
- **시나리오·태그:** 콘솔·글러브박스는 시나리오 단계에 없다. `Car.DriverDoor` 문은 손잡이 BP로 07(손잡이 풀림)·08(열기)을 지금처럼 판정한다.
- **기존 시험 감사(Claude grep, 2026-10-22).** 지우는 이름을 쓰는 시험은 아래뿐이다. 이 밖의 시험이 전체 실행에서 바뀌면 고치지 말고 질문에 적고 멈춘다.

| 시험 | 줄 | 처리 |
|---|---|---|
| `vBoR.P2Door.World.PalmHandleOverlap` | `P2DoorWorldTests.cpp:38-39` | **지운다** — 내장 상자의 손바닥 overlap(P2 E20)만 본다. 손잡이 BP의 잡기는 P2b가 본다 |
| `vBoR.P2Door.World.AssemblyAndRig` | `:60-64` | 문·경첩 확인은 남기고, 손잡이 상자·잡기·transformer 확인은 시험대가 만든 손잡이 Actor 두 개가 문 `HingePivot`에 붙어 연결됐는지로 바꾼다 |
| `vBoR.P2Door.World.PlacementResetAndTotals` | `:100` | 손잡이 높이(바닥+95·+85 cm)를 손잡이 Actor의 `AxisPivot`으로 본다 |
| `vBoR.P2b.Door.BenchSizeBounds` | `P2bAxisPartTests.cpp:185-186` | 시험대 문 크기를 바꿀 때 손잡이 Actor 자리가 따라가는지로 바꾼다(D5) |
| `vBoR.P2b.Door.TransferReleaseLatch` | `:364`, 도우미 `:53` | "연결된 문의 상자 충돌 꺼짐·직접 이벤트 무시" 확인을 지운다(상자가 없음). 나머지 그대로 |
| P2c 도우미·`vBoR.P2c.Door.E12CompatibilityAndOptionalLatch` | `P2cHingedPanelTests.cpp:54-55`, `:259-260` | 상자 자리 줄과 "문 기본 손잡이 자리 그대로" 확인을 지운다 |
| P2d 도우미 | `P2dLatchedPanelTests.cpp:37` | 플래그 줄 지움 |
| `vBoR.P2d.Panel.E2DisabledGrabIgnoresInjectedSelect` | `:100-108` | **지운다** — 꺼진 내장 상자만 본다 |
| `vBoR.P2d.Panel.E3FrontGrabUnlockAndSameHandLift` | `:118-132` | 콘솔 손잡이 Actor로: 끝까지 누르면 풀림·같은 손으로 들어 올림. 눌림 표시 확인은 손잡이 Actor가 눌린 거리(cm)로 본다 |
| `vBoR.P2d.Panel.E4PushClosesAndRelatches` | `:140` | 여는 단계만 손잡이 Actor로 |
| `vBoR.P2d.Panel.E5ButtonEventUnlockWithoutSDKCancel` | `:173-174` | transformer 취소 관찰을 손잡이 Actor 쪽 취소가 없는지로 바꾼다 |
| `vBoR.P2d.Panel.E7MissingOpeningWarning` | `:207-208` | 플래그 줄을 지우고 손잡이 없는 잠긴 판으로(D2 경고). 기다리는 경고 문장(`:207` "…no opening handle or grab area")은 D2의 새 문장에 맞춘다(개정 1에서 적음 — Codex는 이미 그렇게 함) |
| `vBoR.P2d.Door.E10ConnectedInsideBoxIgnoresSelect` | `:249-257` | **지운다** — 연결된 문의 내장 상자만 본다 |
| `vBoR.P2d.Panel.E11LatchDisabledPushAndGrab` | `:265-269` | 잡기 부분을 손잡이 Actor로 |
| `vBoR.P2d.Panel.E13PalmOpeningDefaultKeepsBothFaces` | `:278` | `HandleVisualPressOffsetCm` 기본값 확인만 지운다 |
| `vBoR.P2d.Panel.E14VisualReturnsOnReleaseLatchAndReset` | `:294-319` | 손잡이 Actor가 놓기·잠김·`ResetDoor`·SDK 놓기 뒤 쉬는 자리(이동 0)로 돌아오는지로 바꾼다 |
| `vBoR.P2d.Panel.E15OpenLidRegrabMovesBothWays` | `:327-341` | 잡기를 손잡이 Actor로 |
| `vBoR.P2d.P3.E16ActualConsoleConfiguration` | `:355-373` | 근사 차 콘솔: 내장 상자 확인을 지우고, `ConsoleHandle`이 직선 0.75 cm·복귀·바깥 칸으로 연결됐고 누르는 방향이 경첩 −X인지로 바꾼다 |
| `vBoR.P2e.Assembly.E3ChildOutsideHinge`·`E8AmbiguousSide`·`E10FirstConnectedKeepsHandle` | `P2ePartAssemblyTests.cpp:153`, `:200`, `:224` | "내장 상자 남음" 확인을 "그 칸에 손잡이 없음(`OutsideAxisHandle == nullptr`)"으로 바꾼다 |
| `vBoR.P2f.E9MovingMesh` | `P2fMeshGrabTests.cpp:312` | 플래그 줄만 지운다 |
| `vBoR.P2c.P3.E13ConsolePushAndGrab` (개정 1, Q1) | `P2cHingedPanelTests.cpp:272` | `TestNull(Lid->OutsideAxisHandle)`("console uses front grab instead of axis handle")를 "`Lid->OutsideAxisHandle == Car->ConsoleHandle`이고 손잡이가 `Lid->HingePivot`에 붙음"으로 바꾼다. 나머지(잠김, 밀기 거부, `Unlock`, 판 잡기 접근자로 100° 제한)는 그대로 |
| `vBoR.P2e.Assembly.E12MockUnchanged` (개정 1, Q1·Q2) | `P2ePartAssemblyTests.cpp:252-261` | 경고 0은 **그대로**(D4 순서 변경으로 지킨다). 부착 로그는 "글러브박스 1개" → "글러브박스와 콘솔 2개, 각각 그 손잡이 이름"으로 바꾼다(콘솔도 글러브박스처럼 판 BeginPlay에서 붙음). `:259` "Console keeps grab area"는 콘솔 손잡이 연결·경첩 부모·Actor 원점 확인(글러브박스 줄 `:258`과 같은 형태)으로 바꾼다. 문 손잡이 원점·부모·연결, 글러브박스 확인, `:260-263` 판 3개 설정 깨끗함은 그대로 |
| P5a 도우미·`vBoR.P5a.E9ActualCarConfigurationAndCleanup`·`E11MissingOpeningWarnings` | `P5aGloveboxTests.cpp:36`, `:221`, `:238-246`, `:278` | 내장 상자 확인·주입 select·플래그 줄을 지운다. 손잡이 Actor 확인은 그대로 |
| `vBoR.P5h.E8E12ParentInteractionStates` | `P5hSpectatorTests.cpp:319-321` | "판을 잡는 중"을 손잡이 Actor 잡기 또는 판 잡기 접근자로 만든다 |
| P4i E1–E4 | D7 | D7대로 |

- **감사가 빠뜨린 것(개정 1):** 처음 표는 지우는 이름(`OutsideHandle` 등)만 grep해서, 근사 차 콘솔을 "손잡이 없음"으로 확인하던 P2c E13·P2e E12와 근사 차 문의 생성 순서(Q2)를 놓쳤다. 다시 감사한 범위: `ConsoleLid`, `no opening handle`, `attached handle=`, `AxisHandle` null 확인을 쓰는 모든 시험 — 위 두 줄 외에 새로 걸리는 것은 없다(P2b `:403-405`, P2d `:349` E16(표에 있음), P5f `:205`, P5h `:358`은 콘솔 손잡이와 무관하거나 Codex 전체 실행에서 통과).
- 지운 시험은 보고 ③에 이름을 모두 적는다. 전체 개수 = 556 − 지운 수 + `vBoR.P5x.*` 수.
- `P3Probe/P5eRearviewMirrorTests.cpp:26`은 룸미러의 같은 이름(`HoldingGrabber`)이라 상관없다.

## 4. 시험 — `vBoR.P5x.*`

| ID | 상황 | 확인 |
|---|---|---|
| E1 | 판 클래스 기본 객체 | `AVBoRHingedPanel`·`AVBoRHingedDoor`·`BP_Door_FL`·`BP_ConsoleLid`·`BP_Glovebox` 기본 객체의 `UBoxComponent`는 `PushFaceBox` 하나, `UIsdkGrabbableComponent`는 0개 |
| E2 | 콘솔 손잡이 덜 누름 | 근사 차 콘솔: 손잡이를 잡고 0.5 cm만 누름 → 잠김 그대로, 뚜껑 0° |
| E3 | 콘솔 손잡이 끝까지 | 0.75 cm 누름 → 풀림 1, 같은 손을 들어 올리면 뚜껑이 따라 열림(각 > 0). 손을 펴면 손잡이가 쉬는 자리로 돌아가고 뚜껑은 그 각에 머문다 |
| E4 | 손잡이 없는 판 | 잠금 켠 판에 손잡이 없음 → BeginPlay Warning 1번, 잡을 곳 없음. `Unlock()` 뒤 바깥 면 밀기는 받는다 |
| E5 | `BP_ConsoleHandle` | 부모 `AVBoRAxisPart`, Event Graph 비움, `Translate`·0.75 cm·`Return`·`Outside`. `L_Main`에 하나, Outliner 부모가 `BP_ConsoleLid`. (D7의 P4i E1·E4로 해도 된다) |
| E6 | `L_Main` Play 연결 | `BP_ConsoleLid`의 `OutsideAxisHandle`이 그 손잡이이고 `HingePivot`에 붙는다(D7의 P4i E3로 해도 된다) |

## 5. 구현자가 정할 것

- 시험에서 손잡이 Actor를 잡고 누르는 방법(기존 `P5aPull` 같은 도우미 재사용 여부).
- 근사 차 콘솔 손잡이 자리값 프로퍼티 이름.
- E5·E6을 P4i 확장으로 할지 P5x 시험으로 따로 둘지.

## 6. 완료 조건

1. D1–D7 구현, §3 표대로 기존 시험 정리, `vBoR.P5x.*` 통과.
2. `vBoR.*` 전체 통과. 개수와 지운 시험 목록을 보고에 적는다.
3. non-unity(`-Module=vBoR`)·unity 빌드 성공, vBoR 경고 0.
4. `grep -rn "OutsideHandle\b\|InsideHandle\b\|HandleVisualPressOffsetCm\|bUseOutsideGrabArea\|bUseInsideGrabArea\|UVBoRDoorGrabTransformer" Source/vBoR` 결과가 비어 있다(룸미러 등 다른 클래스의 같은 이름은 제외하고 보고).
5. MCP 재조회: `BP_ConsoleHandle` 값, `L_Main`의 부모 관계·월드 자리. 저장한 에셋은 `BP_ConsoleHandle`·`BP_Door_FL`·`BP_ConsoleLid`·`BP_Glovebox`·`L_Main`뿐.
6. 주석은 영어 짧은 조각문. 커밋 본문 보고 ①–⑤.

## 7. 로컬에서 확인할 것

- **빌드·시험:** 끝에 한 번.
- **에디터:** D3·D6 MCP 작업 뒤 저장하고 닫는다. 사용자는 `BP_Door_FL` 뷰포트에 손잡이 상자가 더 보이지 않는지 본다.
- **Quest 3(T-03·T-08, `not-tested`):** 콘솔 손잡이를 잡고 살짝 눌러 여는 동작이 자연스러운지(0.6 cm 문턱), 문·글러브박스 손잡이 동작이 그대로인지.

## 개정 이력

| 날짜 | 내용 |
|---|---|
| 2026-10-22 | 처음 작성 |
| 2026-10-23 | 개정 1 — Codex Q1·Q2 답. §3 표에 P2c E13·P2e E12 추가, P2d E7 경고 문장 명시, D4에 근사 차 문 손잡이 생성 순서(같은 `DoorWorld`, FinishSpawning 전 칸 넣기) |
