# P5q 시동 버튼 — 전원 켜짐·꺼짐과 전동 사이드미러, 다른 동작을 붙일 자리

| 항목 | 값 |
|---|---|
| 단계 | P5 자유모드 차량 기능 추가(사용자 요청 2026-10-21). 시험 T-08 |
| 결정 근거 | **사용자(2026-10-21):** "전원 버튼 누르면 양 옆 백미러가 열렸으면 좋겠거든 … 접혀있는 상태인데, 열리는거야. 그리고 시동 버튼 누르는 함수에 다른거 추가할 수 있으니 그것까지 고려해보고."<br>**Claude 질문 → 사용자 답(모두 권장안):**<br>① 다시 누르면 → **꺼지고 미러 접힘**<br>② 진행자 창에도 전원 → **넣음**(같은 명령) |
| 베이스 브랜치 | `main` @ P5p 머지 뒤 |
| 작업 브랜치 | `codex/P5q-power-mirrors` |
| 리뷰 파일 | `Docs/Reviews/P5q-power-mirrors.md` (첫 리뷰 때 생김) |
| 상태 | 명세 — 구현 전 |
| 에디터 작업 | **있음** — BP 3개 만들기, `BP_Car_Core`에 컴포넌트 추가, `L_Main` 배치(MCP). 사용자 요청 |
| 앞 명세 | [P5p-sun-visor.md](P5p-sun-visor.md) |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **전원 상태 = `BP_Car_Core`의 컴포넌트 하나** | 새 C++ `UVBoRVehiclePowerComponent`(권장 이름)를 `BP_Car_Core`에 하나 넣는다.<br>- 상태는 켜짐/꺼짐이고, 시작은 꺼짐이다.<br>- 함수: `TogglePower()`·`SetPowerOn(bool)`·`IsPowerOn()`(BlueprintCallable/Pure).<br>- **이벤트 `OnPowerChanged(bool bOn)`**(BlueprintAssignable). 상태가 실제로 바뀔 때만, 바뀔 때마다 한 번 보낸다.<br>- 레벨에 **정확히 하나**여야 한다. 0개·2개 이상이면 경고 한 번을 남기고 전원 기능을 끈다(버튼·진행자 명령 모두 무시) |
| D2 | **시동 버튼 = 태그 `Car.StartButton`의 `AVBoRPokeButton`** | 전원 컴포넌트가 BeginPlay에 이 태그의 버튼을 찾아 `OnButtonPressed`(`Interaction/VBoRPokeButton.h:66-67`)에 묶는다. 누를 때마다 `TogglePower()`다.<br>- 찾기는 **BeginPlay에 한 번, 클래스로 거른 순회**로 한다(P5o — frame마다 월드를 훑지 않음).<br>- 버튼이 없으면 경고 한 번을 남긴다. 진행자 명령은 그대로 쓸 수 있다.<br>- 버튼이 둘 이상이면 모두 묶는다. 같은 frame 규칙(D4)이 중복을 막는다 |
| D3 | **진행자 명령 하나 — 같은 함수** | `EVBoRFlowCommand::Power`·`EVBoROperatorButton::Power`, 진행자 창 버튼 "Power on/off"(권장 문구), 키 **`E`**(지금 안 씀, `VBoRScenarioRunner.cpp:162-178`).<br>- 실행하면 전원 컴포넌트의 `TogglePower()`를 부른다. 참가자 버튼과 같은 함수다(AGENTS.md "입력 경로가 둘이어도 명령은 하나").<br>- **켜짐 조건은 2열 시트 F·U와 같다**(`Interaction/VBoRScenarioOperator.cpp:139-149`). 전원 컴포넌트가 없으면 거절("No vehicle power")하고 버튼이 꺼진다. 시작 전·전환 중에는 기존 규칙대로 버린다.<br>- 진행자 창 배치는 시트 접기 칸 옆에 칸 하나(`VBoROperatorPanel.cpp:210` 근처) |
| D4 | **같은 frame에 두 번 들어와도 한 번만 바뀐다** | 같은 frame에 들어온 토글 요청은 출처(참가자 버튼·진행자)와 상관없이 하나로 친다. 예를 들어 손으로 누르는 순간 진행자가 `E`를 눌러도 한 번만 바뀐다(AGENTS.md 구조 원칙 "동시에 들어와도 상태가 뒤집히지 않게").<br>- 바뀔 때마다 Log에 상태와 출처를 남긴다. 참가자 버튼인지 진행자인지 구분한다(AGENTS.md "진행자 대리 조작과 참가자 독립 수행 구분") |
| D5 | **전동 접이 미러 = 새 C++ 부모 `AVBoRFoldingMirror`(권장 이름)** | - **움직이는 것은 사이드미러 하우징 전체다(개정 1, 사용자 2026-10-21).** 거울면을 포함한 미러 하나가 통째로 접혔다 펼쳐진다. 안쪽 거울면은 따로 움직이거나 조정하지 않는다. 메시는 하나다. CAD에서는 하우징과 거울면을 merge해서 `MirrorMesh` 하나로 넣는다.<br>- **축:** `HingePivot`의 +Z 둘레로 돈다. 위치·회전은 편집할 수 있다. CAD 미러 경첩이 기울어 있을 수 있어서다.<br>- **메시 자리:** `MeshRestFromRoot`(차 원점 피벗을 보존, U-09·P4e D1과 같은 방식)는 **펼친 자리**다. CAD 설계 자세가 펼친 상태이기 때문이다.<br>- **값:** `FoldAngleDeg`(접힌 각, 부호로 방향, 근사 약 65°), `SpeedDegPerSecond`(근사 약 30°/s).<br>- **상태:** 접힘·펼치는 중·펼침·접는 중.<br>  - Play 시작은 **접힘**이다. 에디터에서는 펼친 자리로 보인다.<br>  - 전원 켜짐 → 펼침 쪽, 꺼짐 → 접힘 쪽으로 움직인다. 움직이는 중에 바뀌면 **지금 각에서 바로 방향만 바꾼다**(튀지 않음).<br>  - 각속도 상한을 지키고, frame 시간에만 따른다.<br>- **이벤트를 받는 방법:** BeginPlay에 레벨의 전원 컴포넌트를 찾아 `OnPowerChanged`에 묶는다. 찾기는 한 번만이다(P5o). 없으면 경고 한 번을 남기고 접힌 채로 둔다.<br>- **손으로는 만지지 않는다.** 잡기·밀기·충돌이 없다. 요청에 없다.<br>- 2열 시트(`AVBoRFoldingSeat`)는 바꾸지 않는다. 시트는 진행자 F·U가 모든 시트를 찾아 움직이므로(`VBoRScenarioRunner.cpp:475-477`), 미러를 시트 클래스로 만들면 F가 미러도 접는다 |
| D6 | **문에 달린 미러는 문과 같이 돈다** | 운전석 미러는 레벨 Outliner에서 **문 Actor 밑에 붙인다**(손잡이와 같은 방법, Blueprint-Guide §6.1).<br>- 미러가 BeginPlay에 부모 Actor가 `AVBoRHingedPanel`이면 그 판의 `HingePivot`에 KeepWorldTransform으로 붙는다(손잡이 연결 `VBoRHingedPanel.cpp:328`과 같은 방식).<br>- **`AVBoRHingedPanel` 코드는 바꾸지 않는다.** 미러 쪽에서 처리한다. 판은 손잡이 후보로 1축 부품만 본다(`:200-210`). 그래서 미러는 판의 판정에 끼지 않는다.<br>- 동승석 쪽은 문 BP가 없으므로 미러를 차 원점에 그냥 둔다 |
| D7 | **R(초기화)은 처음 상태로** | 진행자 R(Restart, `VBoRScenarioRunner.cpp:500-511`)에서 전원을 꺼짐으로 두고, 모든 미러를 **즉시** 접힘으로 둔다(움직임 없음). 시트 초기화와 같은 자리다 |
| D8 | **다른 동작을 붙이는 자리 = `OnPowerChanged`** | "시동 버튼 누르는 함수에 다른 거 추가" — 버튼 함수에 줄을 늘리지 않는다.<br>- 새 부품은 전원 컴포넌트의 `OnPowerChanged`를 받아 스스로 움직인다. 미러가 첫 예다.<br>- 이벤트가 BlueprintAssignable이므로, 나중에 사용자가 정하면 BP에서도 받을 수 있다.<br>- 이번에는 미러 말고 다른 것을 붙이지 않는다 |
| D9 | **BP·`L_Main`** | - `BP_Car_Core`에 전원 컴포넌트를 추가한다.<br>- `Content/Main/Car/Parts/BP_StartButton`(`AVBoRPokeButton`): Actor Tags `Car.StartButton`, `SpectatorName` "Start Button", `HitAreaScale` 기본값.<br>- `BP_SideMirror_L`·`BP_SideMirror_R`(`AVBoRFoldingMirror`): `SpectatorName` "Side Mirror".<br>- **`L_Main`:**<br>  - 시동 버튼은 대시보드 위 운전석 오른쪽 근처다. 근사 차 대시보드 윗면 Z 91(`P3VehicleMock.cpp:38`·`:105`)에 둔다. 도달 버튼처럼 Actor를 버튼 자리에 둔다(P4i D3 예외와 같다 — 버튼 CAD 피벗 처리는 확인 안 함).<br>  - 미러 L은 차 원점에 놓고 `BP_Door_FL` 밑에 Outliner로 붙인다(D6).<br>  - 미러 R은 차 원점에 둔다.<br>  - 미러 근사 자리는 문 앞쪽 바깥, 근사 차 기준 약 X 40, Y ∓100, Z 100이다. 근사 판 크기다.<br>  - 정확한 값은 구현자가 정한다 |

## 1. 왜 만드는가

- 자유모드에서 시동 버튼을 누르면 접혀 있던 사이드미러가 펼쳐진다. 다시 누르면 꺼지고 접힌다.
- 나중에 시동에 다른 동작(예: 계기판 표시)을 붙일 때 버튼 코드를 고치지 않게 이벤트 자리를 둔다.

## 2. 범위

### 만들 것

1. C++: `UVBoRVehiclePowerComponent`(D1·D2·D4·D7), `AVBoRFoldingMirror`(D5·D6·D7), 진행자 명령 `Power`(D3).
2. BP 3개, `BP_Car_Core` 변경, `L_Main` 배치(MCP).
3. 자동화 시험 §4(`vBoR.P5q.*`).

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| 미러를 손으로 접기·밀기 | 요청 없음 |
| 안쪽 거울면 각도 조정(따로 움직이는 거울) | 사용자(2026-10-21): "사이드 미러 안에 있는 미러는 따로 조정 안 할거야" |
| 키 잠금으로 미러 접기, 소리, 계기판 | 요청 없음. 붙일 자리만 둔다(D8) |
| 시나리오 단계·태그 규칙 | 자유모드 기능이다 |
| `AVBoRFoldingSeat`·`AVBoRHingedPanel`·근사 차·`L_P3_Flow` 바꾸기 | D5·D6. 근사 차는 시험대로 그대로 둔다 |
| 범용 "차량 상태 시스템" | 이벤트 하나(D8)로 충분하다(AGENTS.md "나중에 쓸 추상화 금지") |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- **기존 시험은 그대로 통과한다.** 근사 차에는 전원 컴포넌트가 없다. 그래서 근사 차 world에서는 `Power` 버튼이 꺼져 있고, 켜진 버튼 단언이 바뀌지 않는다. 감사한 단언:
  - `P3Probe/P5iOperatorPanelTests.cpp:156`(시작 전), `:174`(전환 중), `:177`(자유모드), `:256`(다시 읽기)
  - P4e·P4h의 진행자 명령 시험(시트 F·U, 점 기록)
- 새 enum 값 `EVBoROperatorButton::Power`는 `Count` 앞에 넣는다. 버튼 목록·키 표(`VBoROperatorPanel.cpp:13-30` 근처)에 키가 겹치지 않아야 한다.
- 시동 버튼 한 번 누름 = 토글 한 번이다. 다른 경로로는 전원이 바뀌지 않는다. 예외는 R과 `SetPowerOn` 직접 호출뿐이다.
- frame마다 월드 전체를 훑지 않는다(P5o).
- `P4i` 시험: 새 BP는 `P4iBlueprints()` 목록에 없으므로 E1·E4 단언은 그대로다. E5(참조 규칙)는 새 BP에도 적용된다.
- 보이는 메시·재질은 그대로다. 미러는 근사 Cube다.

## 4. 경계 상황과 그 시험 (`vBoR.P5q.*`)

**시험 대상:** E1–E12는 네이티브 클래스(전원 컴포넌트를 단 Actor, `AVBoRPokeButton`, `AVBoRFoldingMirror`)를 시험 world에 만들어 한다. 실제 BP와 `L_Main`은 E13 정적 검사로 본다.
- 버튼 누름은 버튼 `Poke`에 Select 포인터 이벤트를 보내 넣는다(`P3Probe/P5bGearLeverTests.cpp:409-410`). 손 추적 검사를 타지 않는다.
- 이 정리는 P5p Q1(실제 BP에 합성 손 입력을 못 넣음)에서 나왔다.

| ID | 상황 | 확인 |
|---|---|---|
| E1 | 시작 상태 | 전원 꺼짐. BeginPlay 뒤 미러 둘이 접힌 각이다. 에디터 자리(OnConstruction)는 펼친 자리다 |
| E2 | 버튼으로 켬 | `Car.StartButton` 버튼이 눌림 → 전원 켜짐, `OnPowerChanged(true)` 한 번. 미러가 속도 상한을 지키며 펼쳐지고, 끝나면 펼친 자리(메시 world transform이 `MeshRestFromRoot` 자리, 0.01 cm·0.01°) |
| E3 | 다시 눌러 끔 | 전원 꺼짐, 이벤트 `false` 한 번, 미러가 접힌 각으로 돌아간다 |
| E4 | 움직이는 중 바꿈 | 펼치는 중간에 끄면 그 각에서 바로 접는 쪽으로 간다. 각이 frame 사이에 튀지 않는다(속도 × dt 이하) |
| E5 | 진행자 `Power` | 버튼과 같은 결과다. 진행자 창 버튼과 `E` 키가 같은 명령이다 |
| E6 | 같은 frame 두 요청 | 같은 frame에 버튼 누름 + 진행자 `Power`(또는 버튼 둘) → 한 번만 바뀐다. 다음 frame 요청은 다시 바뀐다 |
| E7 | R | 켜짐·펼침 상태에서 R → 꺼짐, 미러가 즉시 접힘. 다음 frame들에 움직이지 않는다 |
| E8 | 전원 컴포넌트 없음·둘 | 경고 한 번(각각). 버튼을 눌러도 아무것도 안 바뀐다. 진행자 `Power` 버튼이 꺼져 있다(거절 이유 "No vehicle power"). 미러는 접힌 채다 |
| E9 | 시동 버튼 없음 | 경고 한 번. 진행자 `Power`는 동작한다 |
| E10 | 문에 붙인 미러 | Outliner식으로 문 Actor에 붙인 미러가 BeginPlay 뒤 문 `HingePivot` 밑에 있다. 문을 40° 열면 미러도 같이 돈다. 같은 문의 손잡이 연결(P2e)은 그대로다 |
| E11 | 이벤트로 붙이기(D8) | 시험용 받는 쪽을 `OnPowerChanged`에 묶으면 바뀔 때마다 값을 받는다. 바뀌지 않는 `SetPowerOn`(같은 값)에는 받지 않는다 |
| E12 | 진행자 창 켜짐 규칙 | 전원 컴포넌트가 있는 world에서 자유모드 → `Power` 켜짐. 시작 전·전환 중 → 꺼짐(기존 규칙). 근사 차 world의 기존 단언(§3)은 그대로 |
| E13 | `L_Main` 정적 검사 | 맵 패키지를 Play 없이 읽는다.<br>- `BP_Car_Core`에 전원 컴포넌트가 하나 있다.<br>- `Car.StartButton` Actor가 하나 있다.<br>- 미러 BP가 둘 있다. L의 Outliner 부모는 `BP_Door_FL`이다.<br>- 미러 둘의 루트 상대 transform이 단위 변환이다(차 원점) |

## 5. 구현자가 정할 것

- 클래스·함수 이름, 진행자 버튼 문구(권장 "Power on/off"), 경고 문장.
- 미러 근사 자리·크기·`FoldAngleDeg` 부호·속도(D5·D9 범위 안). 접히는 방향은 차 몸 쪽(뒤)이다.
- 전원 컴포넌트를 찾는 방법(BeginPlay 한 번, D2·D5).
- 같은 frame을 판정하는 방법(D4. 예: `GFrameCounter`).
- MCP 호출 방식(MCP Python 일괄 권장). 스크립트는 `Saved/`에만 둔다.

## 6. 완료 조건

1. D1–D9를 지킨다.
2. E1–E13이 통과한다.
3. 기존 `vBoR.*` 시험이 모두 통과한다.
4. non-unity·unity 빌드에서 vBoR 경고가 없다.
5. 순서: C++ 빌드 → 에디터를 열어 MCP 작업 → 저장 → 에디터 닫기 → 전체 시험 한 번.
6. 보고는 AGENTS.md "작업 방식" 형식으로, 커밋 본문과 HANDOFF "Codex 결과"에 쓴다. 만든·바꾼 에셋 목록을 ①에 넣는다.

## 7. 로컬에서 확인할 것 — 사용자(VR, 끝에 한꺼번에)

1. `L_Main` Play: 사이드미러가 접혀 있다. 시동 버튼을 누르면 펼쳐지고, 다시 누르면 접힌다. 진행자 `E`도 같다.
2. 운전석 문을 연 상태에서 시동을 켜도 미러가 문과 같이 있다.
3. 회사에서는 CAD 미러 메시를 각 미러 BP에 넣고 경첩 자리·축을 맞춘다.
   - 지금 Blueprint-Guide는 "문에 붙어 같이 도는 미러는 문 메시에 merge"라고 한다. 접히는 미러는 문 메시에서 **빼서** 미러 BP에 넣는다. 머지 뒤 Claude가 가이드를 고친다.
4. 시험 상태는 VR 전까지 `not-tested`(T-08).

## 8. 확인한 근거

| 무엇 | 근거 | 쓰는 곳 |
|---|---|---|
| 버튼 이벤트 | `OnButtonPressed`·`OnButtonReleased` `Interaction/VBoRPokeButton.h:66-69`, Select에서 발행 `VBoRPokeButton.cpp:122` | D2 |
| 진행자 명령 본보기(시트 F·U) | 명령 enum `Interaction/VBoRScenarioRunner.h:23`, 버튼 enum `Interaction/VBoROperatorPanel.h:13`, 버튼·키 표 `VBoROperatorPanel.cpp:27-28`, 칸 `:210`, 켜짐 규칙 `VBoRScenarioOperator.cpp:139-149`, 실행 `VBoRScenarioRunner.cpp:475-477`, 키 `:177-178`, R `:500-511` | D3·D7 |
| 시간에 따라 접는 본보기 | `AVBoRFoldingSeat` `Interaction/VBoRFoldingSeat.h`·`.cpp`(상태 4개, `SpeedDegPerSecond`, `MeshRestFromRoot`) | D5 |
| 문에 붙이기 | 손잡이 후보 `VBoRHingedPanel.cpp:200-210`, `HingePivot`에 KeepWorld 붙임 `:328` | D6 |
| frame마다 순회 금지 | P5o(`Docs/Specs/P5o-scan-once.md`) | D2·D5 |
| 근사 차 자리 | 대시보드 `P3VehicleMock.cpp:38`·`:105`, 문 경첩 `P3VehicleMock.h:48` | D9 |
| 기존 단언 | `P3Probe/P5iOperatorPanelTests.cpp:156`·`:174`·`:177`·`:256` | §3·E12 |

## 개정 이력

| 날짜 | 내용 |
|---|---|
| 2026-10-21 | 처음 작성(Claude) |
| 2026-10-21 | 개정 1(Claude, 사용자 확인): D5에 "움직이는 것은 하우징 전체, 안쪽 거울면은 따로 조정 안 함, 메시 하나"를 적음. 범위 밖 표를 나눔. 동작 변경 없음 |
| 2026-10-21 | 개정 2(Claude, P5p Q1에서): §4 앞에 시험 대상(네이티브 클래스 vs 실제 BP)과 버튼 누름 넣는 법을 적음. 요구 변경 없음 |
