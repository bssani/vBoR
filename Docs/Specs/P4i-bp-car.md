# P4i L_Main을 BP 차로 — 근사 차를 부품 BP와 레벨 배치로 옮기기

| 항목 | 값 |
|---|---|
| 단계 | P4 통합 — 실제 차를 넣을 틀(U-14). 시험 T-01–T-10 흐름 전체 |
| 결정 근거 | **사용자 요청·문답(2026-10-21):**<br>- 사용자: "내가 작업하기 쉽게 L_Main이랑 블루프린트들 만들 수 있나?" 피벗·메시 충돌 작업은 팀 아트 디자이너가 한다.<br>- Claude 질문 "BP 차를 어느 레벨에?" → 사용자: **L_Main을 BP 차로**(권장안). L_Main에서 근사 차를 빼고, BP 틀을 근사 차 자리에 상자 메시로 놓는다. 근사 차는 `Development/P3/L_P3_Flow`에 그대로 둔다 |
| 베이스 브랜치 | `main` @ 이 명세 머지 뒤 |
| 작업 브랜치 | `codex/P4i-bp-car` |
| 리뷰 파일 | `Docs/Reviews/P4i-bp-car.md` (첫 리뷰 때 생김) |
| 상태 | 명세 — 구현 전 |
| 에디터 작업 | **있음.** 이 명세는 Codex에게 MCP로 Blueprint 만들기와 `L_Main` 수정을 맡긴다(AGENTS.md "에디터 MCP", Agent-Workflow §2.10). `L_Main`은 사용자 레벨이지만 사용자가 이 작업을 요청했다(2026-10-21) |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **부품 BP 17개 + 차 기준점 BP 1개를 만든다** | 이름은 아래를 권장한다. 부모는 [Blueprint-Guide](../Blueprint-Guide.md) §3·§4.2 표대로다.<br>- `Content/Main/Car/Core/BP_Car_Core` (부모 `Actor`)<br>- `Content/Main/Car/Parts/` 아래:<br>  - `BP_Door_FL` (`AVBoRHingedDoor`)<br>  - `BP_DoorHandle_FL_Ext`·`BP_DoorHandle_FL_Int` (`AVBoRAxisPart`)<br>  - `BP_ConsoleLid`·`BP_Glovebox` (`AVBoRHingedPanel`)<br>  - `BP_GloveboxHandle`·`BP_GearLever`·`BP_TurnSignalLever` (`AVBoRAxisPart`)<br>  - `BP_GearButton`·`BP_ReachButton` (`AVBoRPokeButton`)<br>  - `BP_SteeringWheel` (`AVBoRSteeringWheel`)<br>  - `BP_RearviewMirror` (`AVBoRTwoAxisPart`)<br>  - `BP_ConsoleKnob` (`AVBoRKnob`)<br>  - `BP_Liftgate` (`AVBoRPoweredPanel`)<br>  - `BP_RearSeatBack` (`AVBoRFoldingSeat`)<br>  - `BP_KeyFob_Front`·`BP_KeyFob_Rear` (`AVBoRKeyFob`)<br>**Event Graph는 비운다.** 메시와 값만 넣는다(Blueprint-Guide §4.1-7) |
| D2 | **값은 근사 차와 같게, BP 기본값에 넣는다** | 각 부품의 world 자리·회전·크기·축·한계·걸림·입력 허용·`SpectatorName`·태그를 근사 차가 Play 때 정하는 값과 같게 한다(`P3Probe/P3VehicleMock.cpp:113-369`, 기본값 `P3VehicleMock.h:37-128`).<br>- 값은 **BP 클래스 기본값**에 넣는다. 레벨 인스턴스는 덮어쓰지 않는다. 회사에서 BP 하나만 열어 고치면 되게 하기 위해서다.<br>- 메시는 근사 차와 같은 엔진 Cube와 같은 `MeshRestFromRoot`다. 회사에서 CAD로 바꾼다.<br>- 메시로 잡기·밀기(`bGrabWithMesh`·`bPushOnMeshSurface`)는 끔 그대로다 |
| D3 | **Actor는 차 원점에, 자리는 축 컴포넌트로** | 근사 차에서 Actor를 부품 자리에 둔 것이 있다. 이것들도 Actor를 차 원점에 두고, 그 차이를 컴포넌트 값으로 옮긴다(Blueprint-Guide §4.1-2, U-09). 회사 CAD와 같은 방식을 집에서 먼저 확인하기 위해서다.<br>**규칙(개정 1):** 근사 차 Actor의 차 기준 transform을 `T`라 한다.<br>- **Actor 기준 값에만 `T`를 곱한다.** Actor 루트 좌표로 읽히는 값이다.<br>- **경첩·축 기준 값은 그대로 둔다.** 축 컴포넌트 밑 컴포넌트의 상대 변환, 그리고 `…FromHinge`·`…FromPivot` 값이다. 축을 옮기면 같이 옮겨지기 때문이다. 두 번 더하면 어긋난다(Q1).<br>- 맞는지는 E2(world transform 같음)로 판정한다.<br>**해당하는 둘:**<br>- **문**(`:141-144`, `T` = 경첩 자리 + Yaw 180°):<br>  - `T`를 곱함: `HingePivot` 상대 변환, `MeshRestFromRoot`(기본값 `VBoRHingedPanel.h:83`도 Actor 기준이다).<br>  - 그대로: `HingePivot` 밑의 `PushFaceBox`·`OutsideHandle`·`InsideHandle`과 그 자식(`VBoRHingedPanel.cpp:47-85`의 `SetupAttachment`).<br>- **리프트게이트**(`:347`·`:350`, `T` = 차 뒤 끝, 이동만):<br>  - `T`를 곱함: `HingeLocationCm`, `MeshRestFromRoot` 위치.<br>  - 그대로: `OpenButtonFromHinge`·`CloseButtonFromHinge`(경첩 기준 `VBoRPoweredPanel.h:78-80`, 경첩 world와 곱함 `VBoRPoweredPanel.cpp:55`).<br>  - 닫힌 상태의 열기·닫기 버튼 자리는 차 기준 `(-241, ±20, 85) cm`다(Q1 계산).<br>- 다른 부품은 근사 차에서도 Actor가 차 원점이라 `T`가 단위 변환이다.<br>**예외 — 버튼 둘**(`BP_ReachButton`, `BP_GearButton`): 근사 차와 같은 자리에 Actor를 둔다. 버튼의 CAD 피벗 처리는 확인한 적이 없어서 이번 범위 밖이다. `BP_GearButton`은 D5의 Child Actor다 |
| D4 | **`BP_Car_Core`는 근사 차의 차 기준 컴포넌트 그대로** | 정렬 1, 정렬 점 4(`Order`·`Label`·자리), Eye Point 1(`DriverEye` 자리), 시점 3, 앉음 구역 1, 접근 구역 1, `Car.Footprint` 상자 1, `Car.Start` 표시 1(`P3VehicleMock.cpp:39-64`·`:74-111`).<br>- **시점은 셋 다 둔다:** `Car.DriverView`(근사 차의 `DriverEye` 자리), `Car.PassengerView`, `Car.RearView`.<br>  - 근사 차에는 `Car.DriverView`가 없다. 그래서 기준 좌석점으로 눈 점을 쓴다(P4h D6).<br>  - `Car.DriverView`를 눈 점과 같은 자리에 두면 기준 좌석점이 같은 자리라 결과가 같다. 회사에서는 세 시점을 SgRP로 옮기기만 하면 된다(Blueprint-Guide §3 주의 1).<br>- 근사 판(차체·좌석·대시보드)은 이 BP에 넣지 않는다(D6) |
| D5 | **연결은 Blueprint-Guide §6 방식으로** | **§6.1 손잡이:** 바깥·안쪽 손잡이는 `DoorHandleSide`를 바깥·안쪽으로 둔다. 레벨 Outliner에서 문 Actor 밑에 붙인다. 글러브박스 손잡이도 `DoorHandleSide` = 바깥으로 두고 글러브박스 밑에 붙인다. 근사 차처럼 `OutsideAxisHandle`을 직접 넣지 않는다.<br>**§6.2 기어 버튼:** `BP_GearLever` 안에 Child Actor 컴포넌트(클래스 `BP_GearButton`)를 `AxisPivot` 밑에 둔다. 자리는 근사 차의 `ButtonFromPivot`이다(`:211`).<br>**§6.3 키:** 키 테이블 두 개(`AVBoRKeyTable`)를 레벨에 놓는다. 각 테이블의 `KeyClass`를 `BP_KeyFob_Front`·`BP_KeyFob_Rear`로 바꾼다. 키 BP의 Actor Tags는 `Car.FrontKey`·`Car.RearKey`, `SpectatorName`은 "Key"다.<br>  - 키 버튼 태그(`.Unlock`·`.Lock`)는 넣지 않는다. 단계가 쓰지 않는다(Blueprint-Guide §6.3).<br>**§6.4 박스 받침:** 레벨의 `AVBoRBoxStand` 인스턴스에서 `LoadTarget`으로 레벨의 `AVBoRLoadTarget`을 고른다 |
| D6 | **`L_Main` 바꾸기** | **뺀다:** `AP3VehicleMock` Actor.<br>**놓는다:**<br>- `BP_Car_Core`와 부품 BP를 근사 차가 있던 transform에 놓는다(차 원점, 원점이 아니면 보고 ⑤에 적음).<br>- 레벨 Actor: 바닥 경로 둘(`AVBoRFloorRoute`, 태그·점은 `:133-140`), 키 테이블 둘(`:117-118`), 적재 영역(`:358-364`), 박스 받침(`:365-368`).<br>- **근사 형상**은 Outliner 폴더 `Approx`(회사에서 통째로 지움)에 둔다:<br>  - 근사 판 — 엔진 Cube `StaticMeshActor`, 충돌 없음. 근사 차의 `Plates` 0–11, 미러 받침, 노브 받침, 2열 방석, 근사 차와 같은 자리·크기(`:90-106`·`:80-85`·`:121-126`). 스티어링 판(12)은 근사 차도 Play 때 숨기므로 넣지 않는다.<br>  - 근사 트렁크 `AP1TrunkMock` — 자리는 `:347-348`. 짐이 착지할 막는 충돌이 이 Actor에 있다(`P1Probe/P1TrunkMock.cpp:25-26`).<br>  - 근사 표시 글자 하나 — "근사 형상·자리값 — 회사에서 Approx 폴더를 지우고 CAD로" 뜻, 시작 표시 근처. 근사 차의 `MockLabel`을 대신한다(AGENTS.md "근사 형상임을 드러낸다").<br>**그대로 둔다:** `VBoRScenarioRunner`, World Settings(GameMode Override), PlayerStart, 조명·바닥 등 그 밖의 모든 Actor.<br>- **레벨 Blueprint가 근사 차를 참조하면**(예: 키 이벤트에 웰컴 라이트 연결) 멈추고 리뷰 파일 "질문"에 적는다 |
| D7 | **C++ 제품 코드는 바꾸지 않는다 — 예외 하나(개정 2)** | 바꿀 것은 BP·`L_Main`과 새 시험 파일이다.<br>- BP·레벨만으로 근사 차와 같게 못 하는 것이 나오면 멈추고 "질문"에 적는다. 예: Child Actor 버튼이 레버를 안 따라옴, `Template Mismatch during attachment`, 연결이 안 됨. BP 로직으로 우회하지 않는다(Blueprint-Guide §0)<br>**예외 — 버튼 초기화 보호(Q2):** `AVBoRPokeButton::PreInitializeComponents`(`Interaction/VBoRPokeButton.cpp:72-78`)를 고친다. 그 world에 ISDK subsystem이 없으면(`UIsdkWorldSubsystem::TryGet`이 null, `Plugins/MetaXRInteraction/Source/OculusInteraction/Public/Subsystem/IsdkWorldSubsystem.h:107-111`) SDK 연결 세 줄(`SetPokeInteractable`·`SetPointable`·`SetSelectAudio`)을 건너뛴다.<br>- **이유:** 썸네일·BP 편집기 미리보기 world에는 subsystem이 없다. world subsystem은 기본으로 Game·Editor·PIE에만 생긴다(`Engine/Private/Subsystems/WorldSubsystem.cpp:39-41`). `SetPokeInteractable`은 Disabled에서도 `UIsdkWorldSubsystem::Get`을 부르고(`IsdkPokeButtonVisual.cpp:159-177`·`:214-219`), `Get`은 없으면 assert한다(`IsdkWorldSubsystem.h:119-128`). 그래서 지금은 `AVBoRPokeButton`의 BP 자식을 저장하면 에디터가 죽는다. 회사에서 버튼 BP를 저장해도 같다.<br>- Game·PIE는 subsystem이 있으므로 지금과 같다. `BeginPlay`와 다른 부모는 고치지 않는다.<br>- 이번 BP 저장 중에 다른 vBoR 부모에서 **같은 assert**("Failed to find a UIsdkWorldSubsystem")가 나면 같은 방식으로만 고치고 보고 ③에 적는다. 다른 종류의 crash는 멈추고 질문한다 |

## 1. 왜 만드는가

- 회사에서는 CAD 메시를 넣고, 축 컴포넌트를 CAD 경첩·회전 자리로 옮기기만 하면 되게 한다. 사용자와 아트 디자이너가 BP를 처음부터 만들지 않아도 된다.
- 지금 Blueprint-Guide §0은 "부품 부모 클래스는 근사 차에서만 돌았고, BP 자식으로 만들어 레벨에 놓은 적은 아직 없다"이다. 이 작업이 그 길을 집에서 처음 확인한다. §6.2–6.4의 "확인 안 됨" 연결 셋도 여기서 확인된다.

## 2. 범위

### 만들 것

1. D1의 BP 18개(MCP).
2. D6의 `L_Main` 변경(MCP), 저장.
3. 자동화 시험 §4(`vBoR.P4i.*`).
4. D7 예외 — `AVBoRPokeButton` 초기화 보호와 E7(개정 2).

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| CAD 메시·피벗·충돌, 버튼 CAD 피벗 처리 | 회사에서 한다(아트 디자이너). 버튼은 D3 예외 |
| 근사 색(미러·도달 버튼 파란색 등 `:249-255`·`:274-280`) | 근사 차가 Play 때 칠하는 근사 색이다. 재질 에셋을 새로 만들지 않는다. 기본 재질 그대로 둔다 |
| 근사 차 C++(`AP3VehicleMock`)·`L_P3_Flow`·다른 Development 레벨 바꾸기 | 근사 차는 시험대로 그대로 쓴다 |
| C++ 제품 코드, BP Event Graph 로직 | D7 |
| 생성 스크립트·에디터 도구를 리포에 남기기 | 일회성 작업이다. 쓴 스크립트는 `Saved/` 아래에만 둔다 |
| Blueprint-Guide·Company-PC-Guide 고치기 | Claude가 리뷰 뒤 고친다 |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- **근사 차와 같은 경험이다.** 부품의 world 자리·회전·크기·축·값이 같다(§4 E2). 흐름 19단계가 그대로 시작·진행된다.
- 기존 시험은 모두 그대로 통과한다. **감사:** `L_Main` 맵을 읽는 시험은 없다. `/Game/Main/`을 읽는 시험은 `DA_Main_Scenario`(`P3Probe/P3FlowTests.cpp:1035`·`:1078`·`:1284`)와 Pawn·GameMode BP(`P3Probe/P4fPawnTests.cpp:156-157`)뿐이고, 이 작업은 둘 다 바꾸지 않는다. 근사 차 시험(`vBoR.P3.*` 등)은 시험 world에 근사 차를 직접 만든다.
- `DA_Main_Scenario`·`BP_vBoRPawn`·`BP_vBoRGameMode`는 바꾸지 않는다.
- `Content/Main/` 에셋은 `Content/Development/` 에셋을 참조하지 않는다(AGENTS.md 구조 원칙). `AP1TrunkMock`은 C++ 클래스라 이 규칙에 걸리지 않는다. `Approx` 폴더에 두어 회사에서 지운다.
- BP·레벨에 CAD가 없으므로 커밋한다(Company-PC-Guide §5는 CAD를 쓰는 BP에 관한 것이다).

## 4. 경계 상황과 그 시험

| ID | 상황 | 확인 |
|---|---|---|
| E1 | BP가 있음 | D1의 18개를 `LoadClass`로 읽는다. 부모 클래스가 맞는다. Event Graph에 노드가 없다(가능하면) |
| E2 | **자리가 근사 차와 같음** | 시험 world 하나에 근사 차를, 다른 world에 BP들을 차 원점에 놓고 둘 다 BeginPlay한다. BP 쪽은 손잡이를 문·글러브박스 밑에, 레벨과 같이 붙인다. 부품마다 주요 컴포넌트의 world transform이 같은지 본다(위치 0.01 cm, 회전 0.01°, 크기 1e-4).<br>- 축·피벗: `HingePivot`·`AxisPivot`·`Pivot`<br>- 메시: `DoorMesh`·`PartMesh`·`WheelMesh`·`KnobMesh`·`ButtonMesh`·리프트게이트 판·시트 판<br>- 잡는 자리 상자(`OutsideHandle`·`InsideHandle`·`GrabBox`)<br>- 리프트게이트 버튼 둘(닫힌 상태 차 기준 `(-241, ±20, 85) cm`도 직접 확인, D3)<br>- `BP_Car_Core`의 정렬 점·눈 점·시점·구역·`Car.Footprint`·`Car.Start`<br>- 문을 40° 연 뒤, 기어 레버를 20° 돌린 뒤에도 한 번 더 본다(손잡이·버튼이 따라감) |
| E3 | 연결(BeginPlay 뒤) | 문의 바깥·안쪽 손잡이와 글러브박스 손잡이가 연결되고 `HingePivot` 밑에 붙는다(P2e). 기어 버튼이 레버 `AxisPivot` 밑의 Child Actor다. 키 테이블이 만든 키가 BP 키이고 태그가 맞다. `Car.*` 태그마다 Actor·컴포넌트가 정확히 하나다 |
| E4 | `L_Main` 정적 검사 | 맵 패키지를 읽어(Play 없이) 다음을 본다.<br>- `AP3VehicleMock`이 없다.<br>- D1의 BP가 하나씩 있다. `BP_KeyFob_*`는 테이블이 만들므로 레벨에 없다.<br>- 정렬 컴포넌트가 정확히 하나다.<br>- 손잡이 셋의 부모 Actor가 맞다.<br>- 키 테이블의 `KeyClass`가 맞다. 받침의 `LoadTarget`이 레벨의 적재 영역이다.<br>- 경로 둘의 태그·점이 근사 차와 같다.<br>- `Approx` 폴더 Actor에는 `Car.`·`Route.` 태그가 없다 |
| E5 | 참조 규칙 | `/Game/Main/` 아래 에셋의 의존성(Asset Registry)에 `/Game/Development/`가 없다 |
| E7 | 버튼 초기화 보호(D7 예외) | subsystem이 없는 world(예: `EWorldType::EditorPreview`, Actor 초기화까지)에 `AVBoRPokeButton`을 만들어도 assert 없이 끝난다. 그 world에서 `TryGet`이 null인 것도 단언한다. Game world 버튼은 지금처럼 poke·소리가 연결된다. 기존 버튼 시험(`vBoR.P0ButtonBench.*` 등)은 그대로 통과한다 |
| E6 | **Play 한 번 — MCP PIE**(자동화 아님) | `L_Main`을 PIE로 한 번 돌리고 Output Log를 본다.<br>- `시작 차단:`이 없다.<br>- `P2e setup:` 경고가 없다.<br>- `Template Mismatch during attachment`가 없다.<br>- `Non-cube DoorMesh`·`grab mesh`·`DoorMesh has no traceable` 경고가 없다.<br>- 그 전에 `BP_GearButton`·`BP_ReachButton`을 MCP로 저장해도(썸네일 생성 포함) 에디터가 죽지 않는다(D7 예외).<br>- 진행자 정보에 단계 01이 보인다.<br>로그 경로를 보고 ⑤에 적는다. headless `-game` 맵 로드는 쓰지 않는다(Agent-Workflow §2.5) |

## 5. 구현자가 정할 것

- **MCP를 부르는 방식.** Codex 앱은 MCP 호출마다 승인을 묻는다(Agent-Workflow §2.10). 그래서 **승인 수를 줄이는 쪽**을 권장한다. MCP 서버에는 Python 실행 기능이 있다(Agent-Workflow 도구 표). 그것으로 한 번에 만들고, 확인만 따로 부르는 방식을 먼저 검토한다. 스크립트는 `Saved/` 아래에 두고 커밋하지 않는다.
- E2의 비교 방법과 시험 world 구성. E4에서 맵 패키지를 읽는 방법.
- 근사 판 Actor 이름, 근사 표시 글자의 정확한 문장(뜻 유지).
- BP 이름은 D1 권장 이름을 쓴다. 바꾸면 보고 ③에 적는다.

## 6. 완료 조건

1. D1–D7을 지킨다.
2. E1–E5·E7이 통과하고, E6 결과를 보고 ⑤에 적는다.
3. 기존 `vBoR.*` 시험이 모두 통과한다.
4. non-unity·unity 빌드에서 vBoR 경고가 없다.
5. 순서: C++ 빌드 → 에디터를 열어 MCP 작업 → 저장 → 에디터 닫기 → 전체 시험 한 번(Agent-Workflow §2.10).
6. 보고는 AGENTS.md "작업 방식" 형식으로, 커밋 본문과 HANDOFF "Codex 결과"에 쓴다. MCP로 만들고 바꾼 에셋 목록을 ①에 넣는다.

## 7. 로컬에서 확인할 것 — 사용자(에디터·VR, 끝에 한꺼번에)

1. `L_Main`을 열어 차가 근사 차와 같아 보이는지 본다. Outliner에서 손잡이가 문 밑에, 글러브박스 손잡이가 글러브박스 밑에 있는지 본다.
2. VR Preview로 흐름 01–20을 한 번 돈다. 특히 BP로 처음 옮긴 것들을 본다:
   - 기어 버튼이 레버를 따라오는지(§6.2)
   - 키 집기·버튼(§6.3)
   - 박스 받침 → 트렁크(§6.4)
3. 회사에서는 `Approx` 폴더를 지우고 CAD 정적 메시를 넣는다. 그다음 BP마다 메시와 축 자리를 바꾼다(Blueprint-Guide — 리뷰 뒤 Claude가 이 틀에 맞게 고침).
   - **트렁크 안 CAD 메시에는 막는 충돌이 있어야 짐이 착지한다.** 근사 트렁크가 그 일을 한다(`P1TrunkMock.cpp:25-26`).
4. 시험 상태는 VR 전까지 `not-tested`.

## 8. 확인한 근거

| 무엇 | 근거 | 쓰는 곳 |
|---|---|---|
| 근사 차가 만드는 것·값 | 차 기준 컴포넌트 `P3Probe/P3VehicleMock.cpp:35-72`, 자리 `:74-111`, 부품 `:113-281`, 키 테이블 `:283-298`, 도달 버튼 자리 `:303-313`, 차 뒤 `:345-369`, 기본값 `P3VehicleMock.h:37-128` | D2–D6·E2 |
| 눈 점 찾기 | `BaseEye`가 비면 owner의 Eye Point 컴포넌트를 찾음 `Interaction/VBoRVehicleAlignmentComponent.cpp:25` | D4 |
| 손잡이 연결 | owner 밑 Actor 중 `DoorHandleSide`가 있는 1축 부품 `Interaction/VBoRHingedPanel.cpp:200-210`, `HingePivot`에 붙임 `:328`, Child Actor는 `HingePivot` 밑이어야 함 `:236-238` | D5·E3 |
| 키 클래스 | `AVBoRKeyTable::KeyClass` `Interaction/VBoRKeyTable.h:21`, Play 때 만듦 `VBoRKeyTable.cpp:42` | D5 |
| 흐름 시작 검사 | 정렬 컴포넌트 1개 `Interaction/VBoRScenarioRunner.cpp:221`, 태그 종류 `:240`·`:246`, 경로·차체 영역 `:256`·`:265`, "시작 차단:" 기록 `:295` | E3·E6 |
| 근사 트렁크 충돌 | 판마다 `QueryOnly`·모든 채널 Block `P1Probe/P1TrunkMock.cpp:25-26` | D6·§7 |
| MCP 승인·Python | Agent-Workflow §2.10, 도구 표(McpAutomationBridge 기능 목록) | §5 |

## 개정 이력

| 날짜 | 내용 |
|---|---|
| 2026-10-21 | 처음 작성(Claude) |
| 2026-10-21 | 개정 1(Claude, Codex Q1): D3를 규칙으로 고침 — Actor 기준 값에만 근사 차 Actor transform을 곱하고, 경첩·축 기준 값은 그대로 둔다. 처음 쓴 D3는 리프트게이트 버튼(경첩 기준)에도 오프셋을 더하라고 했고(235 cm 이중 적용), 문은 `MeshRestFromRoot`(Actor 기준)를 빠뜨렸다. 내 명세 실수. E2에 닫힌 버튼 자리 확인 추가 |
| 2026-10-21 | 개정 2(Claude, Codex Q2): D7에 예외 하나 — `AVBoRPokeButton::PreInitializeComponents`에서 ISDK subsystem이 없는 world면 SDK 연결을 건너뜀(썸네일 world assert로 버튼 BP 저장 시 에디터 crash). E7 추가, E6에 버튼 BP 저장 확인. 명세 작성 때 버튼 BP가 미리보기 world에서 초기화된다는 것을 보지 못했다 |
