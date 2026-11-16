# R12 시나리오 흐름·편집 정리 — 확인 버튼 없앰, 좌석 이동 버튼 하나, 안내판 방향, 시나리오 칸 쉽게

| 항목 | 값 |
|---|---|
| 단계 | 구조 보수. 회사 차 시나리오 운영 |
| 결정 근거 | **사용자 2026-11-17.** (1) "confirm 버튼은 다 없애 버려. operator가 하면 되니까." (2) 좌석 확인은 운전석에서 "Move to passenger" → 동승석에서 "Move to rear" → 2열에서 "Return to driver seat" → 운전석에서 시트 접힘. 지금은 도착하면 Confirm과 이동 버튼이 같이 뜨고, 다음 단계 안내판이 다음 좌석 자리로 먼저 옮겨 가 버튼이 손에 닿지 않는다(2열로 안 간 사례). (3) "panel anchor가 나를 바라보고 있으면 좋겠어." (4) "Marker Tag, Panel Tag, Route Tag 이런게 너무 많아서 헷갈려. 쉽게 scenario 수정할 수 있게." 사용자가 R12·R13을 한 번에 작업하라고 함 → 이 명세 하나(A·B·C·D) |
| 베이스 / 작업 브랜치 | `main` / `codex/R12-scenario-flow-and-editing` |
| 리뷰 파일 | `Docs/Reviews/R12-scenario-flow-and-editing.md` |
| 에디터 작업 | 없음. `DA_Main_Scenario`·`L_Main` 값은 사용자가 고친다(§7) |

## 0. 지금 코드 (확인한 것)

- 참가자 확인 버튼: `ConfirmButton`, 보임 조건 `CanParticipantConfirm()`(`VBoRScenarioRunner.cpp:610-614`). 참가자·진행자 확인 권한은 `ConfirmBy`(`VBoRScenarioAsset.h:10,23`), 판정 `VBoRScenarioOperator.cpp:165-172`.
- 좌석 단계(`Rule` = Viewpoint)는 늘 확인이 필요하다(`NeedsConfirmation`, `VBoRScenarioRunner.cpp:604-608`). 도착 전 이동 버튼 = "Move to …", 도착 뒤 = Confirm + "Return to driver seat"(`:983-1007`).
- 고정 안내판은 단계 시작 때 그 단계 `PanelTag` 표식 자세 그대로 놓는다(`PlacePanel`, `:863-890`). 버튼은 `Pose * FRotator(-90,0,0)`, 좌우 `PanelButtonSideCm`. 다시 놓는 조건이 "안내판 자세 ≠ 표식 자세"다(`:868`).
- 좌석 이동: `SwitchViewpoint`·`ReturnToBase`(`VBoRVehicleAlignmentComponent.cpp:201-211,278-291`), 거부 사유 `SwitchRejection`(`:237-244`, 정렬 `Applied` 아니면 거부).
- 경로 끝 도착은 경로를 끄고 안내판만 옮긴다. 단계를 끝내지 않는다(`:1091-1102`) — 바꾸지 않는다.
- TV 문구: `SpectatorText`가 비면 **Step Id 번호표**(`Spectator/VBoRSpectatorModel.cpp:8-21`) → 번호를 바꾸면 밀린다.
- UE 5.7 에디터: 배열 제목 `TitleProperty`는 서식 문자열을 받는다(`Engine/Source/Editor/PropertyEditor/Private/ItemPropertyNode.cpp:672-699`, `FTitleMetadataFormatter`). `GetOptions` 메타도 지원(`PropertyEditorUtils.cpp`).

## 1. 결정

### A. 참가자 확인 버튼 없앰

- **A1.** 참가자 확인 버튼은 **어떤 단계에서도 보이지 않고 눌리지 않는다**(숨김 + Poke 비활성). 확인은 진행자(`Enter` 등 기존 진행자 확인)만 한다.
  - 액터는 지금처럼 만들어 둬도 된다(시험·스타일 색 유지). 자리 계산도 그대로 둬도 된다. 보이지만 않으면 된다.
  - 이동 버튼(`ActionButton`)의 자리는 **바꾸지 않는다**(가운데로 옮기지 않음).
- **A2.** `ConfirmBy`의 의미: `None` = 확인 없음, 그 밖(`Participant`·`Operator`·`Either`) = **진행자 확인**. 저장된 값은 그대로 읽힌다(이전 에셋 수정 불필요).
  - 참가자 출처 `Confirm` 명령 처리 코드는 그대로 둬도 된다(버튼이 없으니 들어오지 않는다). 단 진행자 창 문구는 "Operator confirm pending" 쪽으로(참가자 확인 대기 문구가 나오지 않게, `:966`).
- **A3.** `Rule` = Confirm은 지금처럼 늘 확인이 필요하다 → 진행자 확인.

### B. 좌석 단계 — 버튼 하나로 다음 자리로

- **B1. 도착 전:** 버튼 하나, 글자 "Move to <그 좌석>"(지금과 같음). 누르면 그 좌석으로 이동(지금과 같음).
- **B2. 도착 뒤(규칙 충족):** 버튼 하나. 글자와 동작은 **다음 단계**로 정한다.
  - 다음 단계가 좌석 단계면 "Move to <다음 좌석>". 누르면 **지금 단계 완료 + 다음 좌석으로 이동**을 한 명령으로 한다.
  - 아니면(다음이 다른 단계, 또는 마지막) "Return to driver seat". 누르면 **지금 단계 완료 + 운전석 복귀**.
  - 완료 기록 문구는 참가자 완료로 남긴다(예: "Participant seat advance"). 관찰 완료를 도착만으로 하지 않는다(AGENTS.md) — 참가자가 버튼을 누른 것이 완료다.
- **B3. 함께 성공·함께 실패:** 이동이 거부되면(`SwitchRejection` — 정렬 `Applied` 아님, 전환 중, 좌석 없음) **단계도 완료하지 않는다**. 이유는 지금처럼 기록·진행자 창에 남긴다.
- **B4. 진행자:** `Enter`(확인)로 도착한 좌석 단계를 완료하는 것, `1`·`2`·`Home`, `Next`·`Previous`는 지금과 같다. 진행자 확인으로 넘긴 뒤 다음 단계가 좌석 단계면 그 단계의 B1 버튼이 뜬다(이동은 하지 않음).
- **B5. 좌석 단계의 안내판 자리:**
  - 도착 전에는 **직전 단계의 안내판 자리**에 그대로 둔다(참가자가 지금 있는 자리). 첫 단계가 좌석 단계면 그 단계 표식.
  - 도착하면(전환 끝) 그 단계 `PanelTag` 표식으로 옮긴다(`PanelTag`가 비면 그대로).
  - B2로 이동해 다음 단계에 들어간 경우도 같다: 전환이 끝나 도착하면 다음 단계 표식, 운전석 복귀로 다음 단계(좌석 단계 아님)에 들어갔으면 그 단계 표식.
- **B6. 버튼 숨김:** 전환(암전) 중에는 버튼을 숨긴다(지금과 같음). 두 번 눌러도 한 번만 처리.
- **B7. 좌석 단계가 아닌데 운전석이 아닐 때**(예: 진행자가 `2`로 옮김) "Return to driver seat" 버튼 — 지금과 같다. 누르면 이동만, 단계 완료는 아님.

### C. 안내판이 참가자를 바라봄

- **C1.** Runner에 `bPanelFacesParticipant`(기본 **켬**). 켜면 고정 안내판을 놓을 때 **표식 위치는 그대로**, 방향의 **Yaw만** 참가자 머리 쪽(수평)으로 바꾼다. 표식의 Pitch·Roll(기울기)은 그대로 둔다. 버튼·라벨은 지금처럼 안내판 자세를 따른다.
- **C2. 언제:** 안내판을 **놓는 순간마다 한 번**(단계 시작, B5의 도착 재배치). 그 사이에는 따라 돌지 않는다(누르려는 버튼이 움직이지 않게).
  - 다시 놓는 조건(`:868`)은 표식 자세가 아니라 **계산한 목표 자세**와 비교한다 — 매 frame 다시 놓이면 안 된다(확인 잠금 `ConfirmLockSeconds`가 계속 걸리는 것도 막는다).
- **C3. 경계:** 머리 추적이 없거나 머리와 표식의 수평 거리가 10 cm 미만이면 표식 Yaw를 그대로 쓴다. 꺼져 있으면 지금과 완전히 같다. 머리 기준 배치(고정 모드가 아닐 때)는 바꾸지 않는다.

### D. 시나리오 칸 쉽게 (`FVBoRScenarioStep`, `UVBoRScenarioAsset`)

- **D1. 목록 제목:** `Steps`에 `TitleProperty`로 "`{StepId}` · `{Instruction}`"이 보이게(서식은 구현자, `ItemPropertyNode.cpp:672-699`).
- **D2. 안 쓰는 칸 숨김**(`EditCondition` + `EditConditionHides`):
  - `TargetTag`: `Rule`이 Confirm이면 숨김.
  - `DoorOpenDeg`: `Rule`이 DoorOpened일 때만. `SeatedSeconds`: Seated일 때만.
  - 숨긴 칸의 저장 값은 지우지 않는다.
- **D3. 고르기(`GetOptions`):** `PanelTag`·`RouteTag`·`MarkerTag`·`TargetTag`를 목록에서 고른다.
  - 목록 = **지금 에디터에 열린 레벨**에서 찾은 태그(Actor·Component Tags) — `PanelTag`는 `AVBoRPanelAnchor`, `RouteTag`는 `AVBoRFloorRoute`, `MarkerTag`·`TargetTag`는 `Car.` 로 시작하는 태그. 맨 앞에 `None`.
  - 레벨을 못 읽으면(에셋만 열림 등) 지금 값과 `None`만이라도 보여 준다. **지금 값이 목록에 없어도 지우거나 바꾸지 않는다.**
  - 직접 입력이 막히면 그대로 둔다(목록으로만 고름) — 단 위 조건 때문에 기존 값은 유지된다.
- **D4. 칸 순서·묶음:** 기본(`StepId`·`Instruction`·`SpectatorText`·`Rule`·`TargetTag`·`ConfirmBy`) → 표시(`PanelTag`·`RouteTag`·`MarkerTag`) → 세부(`DoorOpenDeg`·`SeatedSeconds`·`bRequireBaseViewpoint`, `AdvancedDisplay`로 접기). 구조체에서 분류가 안 나뉘면 순서와 `AdvancedDisplay`만.
- **D5. 이름·설명(영어, `DisplayName`·`ToolTip`):** `ConfirmBy` → "Operator confirm"(값 표시 `None` = "Not needed", `Operator` = "Operator"; `Participant`·`Either`는 목록에서 숨김 `UMETA(Hidden)` — 저장 값은 그대로 읽고 A2대로 진행자 확인). `PanelTag` → "Panel at", `RouteTag` → "Floor route", `MarkerTag` → "Target marker", `TargetTag` → "Rule target". 짧은 ToolTip에 무엇을 하는지(`AGENTS.md` 주석 규칙).
- **D6. TV 문구:** `SpectatorText`가 비면 **`Instruction`** → 그것도 비면 지금의 번호표 → 그것도 없으면 Unknown. 번호를 바꿔도 밀리지 않는다.

## 2. 범위

### 만들 것
- 위 A–D. 자동 시험 `vBoR.R12.*`(§4).

### 범위 밖 — 하지 말 것
- `DA_Main_Scenario`·`L_Main`·BP·표식 위치 수정(사용자).
- 경로 도착으로 단계 완료, 안내판이 계속 따라 돌기, 버튼 가운데 정렬.
- 좌석 이동 방식(암전·위치 차이) 변경, 정렬 거부 조건 변경.
- 자유모드 동작 변경(자유모드는 지금처럼 안내판·버튼 없음).
- 시나리오 데이터 구조 변경(필드 추가·삭제·이름 바꾸기). D는 메타·표시·옵션 함수만 — 저장 형식 그대로.

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- 저장된 `DA_Main_Scenario`·시험 에셋이 다시 저장하지 않아도 그대로 읽히고 같은 흐름(확인 주체만 A2대로)으로 진행된다.
- 진행자 확인·`Next`·`Previous`·`Restart`·좌석 키·자유모드 전환, 확인 잠금(`ConfirmLockSeconds`), 시작 검사(`ValidationProblems`)·Check setup.
- 같은 명령이 같은 frame에 손·진행자로 들어와도 한 번(AGENTS.md).
- 버튼 색·크기(`Panel Style`·`PanelButtonSizeCm`), 라벨 글꼴.
- `bPanelFacesParticipant` 꺼짐이면 안내판·버튼 자세가 지금과 같다.

### 바뀌는 기존 시험 단언 — 이 범주만

다음 파일에서 **참가자 확인 버튼이 보이거나 눌린다고 기대하는 단언**, **좌석 단계 도착 뒤 Confirm + Return 두 버튼을 기대하는 단언**, **좌석 단계 안내판이 단계 시작 때 그 단계 표식에 있다고 기대하는 단언**만 새 동작으로 바꾼다:
`P3Probe/P3FlowTests.cpp`(`FP3FlowTestAccess::Confirm` 사용처 — 230·448·466-473·524·622·634·719행 근처), `P5gFreeModeTests.cpp`(`Button(R,0)`), `P5iOperatorPanelTests.cpp`(`SeatButtonsShown` 47행, 172행), `P5zFixedGuidanceTests.cpp`(`Button(R,0)`), `R9GuidanceStyleTests.cpp`(68행 근처 — 색 단언은 유지되어야 한다).
- 안내판 방향(C)으로 깨지는 자세 단언은 **단언을 바꾸지 말고** 그 시험 world에서 `bPanelFacesParticipant = false`로 둔다.
- `P5hSpectatorTests.cpp` 159·169행(빈 `SpectatorText` → 번호표)은 그 시험의 `Instruction`이 비어 있으면 D6 순서로도 그대로 통과해야 한다. 바뀌어야 하면 이유와 함께 보고.
- **그 밖의 단언이 깨지면 멈추고 리뷰 파일 "질문"에 적는다.** 바꾼 단언은 전부 파일:줄과 이유를 보고 ③에 적는다.

## 4. 시험 — `vBoR.R12.*`

| ID | 내용 | 경계 |
|---|---|---|
| E1 | 참가자 확인 버튼 없음 | 모든 `Rule`·`ConfirmBy`(None·Participant·Operator·Either)에서 숨김·Poke 비활성, 진행자 확인으로만 완료. 확인 잠금 중 진행자 확인 거부 그대로 |
| E2 | 좌석 연속 | 12(Confirm) → 진행자 확인 → 13(동승석, 안내판 = 12 자리, 버튼 "Move to passenger seat") → 누름 → 도착 → 안내판 13 표식, 버튼 "Move to rear seat" → 누름 → 13 완료 + 2열 이동 → 도착 → 안내판 14 표식, "Return to driver seat" → 누름 → 14 완료 + 운전석 → 15 표식 |
| E3 | 함께 실패 | 정렬 `NeedsCheck`에서 B2 버튼 → 단계 그대로, 이동 없음, 거부 기록. 전환 중 두 번 누름 → 한 번 |
| E4 | 진행자 | 도착한 좌석 단계를 진행자 확인 → 다음 좌석 단계는 이동 없이 B1 버튼. `Previous`로 좌석 단계로 돌아옴 → 지금 자리에 B1/B2 맞게 |
| E5 | 마지막·단독 좌석 단계 | 좌석 단계가 마지막이면 "Return to driver seat" = 완료 + 복귀. 좌석 단계 하나만 있고 다음이 Confirm |
| E6 | 안내판 방향 | 켬: 위치 = 표식, Yaw = 머리 쪽, Pitch·Roll = 표식. 머리 이동해도 같은 단계에선 안 돔, 다음 배치 때 다시 계산. 추적 없음·10 cm 미만 → 표식 Yaw. 꺼짐 → 지금과 같음. 매 frame 재배치 없음(확인 잠금 기록 1회) |
| E7 | 편집 메타 | `TitleProperty`·`EditCondition`·`GetOptions`·`DisplayName` 메타가 붙어 있음(리플렉션으로 확인). `GetOptions` 함수: 표식·경로·`Car.` 태그를 찾음, 레벨 없으면 지금 값 포함 |
| E8 | TV 문구 순서 | `SpectatorText` → `Instruction` → 번호표 → Unknown |
| E9 | 기존 에셋 | `Participant`·`Either` 값 단계가 진행자 확인으로 완료, 저장 값 그대로 |

## 5. 구현자가 정할 것

- B2 명령의 형태(새 명령 또는 기존 명령 조합). 단 B3 원자성.
- B5의 "도착" 감지 위치(전환 완료 이벤트 등).
- D3 레벨 접근 방법(에디터 월드 찾기), D1 서식.
- 진행자 창·로그 문구(영어 조각문).

## 6. 완료 조건

- §4 통과, `vBoR.*` 전체 통과(§3 범주 밖 단언 그대로).
- non-unity(`-Module=vBoR`)·unity 빌드 경고 없음.
- 커밋 본문·`HANDOFF.md` "Codex 결과"에 보고 ①–⑤, ③에 바꾼 단언 목록.
- 머지 뒤 Claude가 AGENTS.md(TV 문구 규칙)·Blueprint-Guide §9.3을 고친다.

## 7. 로컬에서 확인할 것 — 사용자

1. `DA_Main_Scenario`를 열어 목록 제목·숨김·고르기가 보이는지. 값이 그대로인지.
2. 15번(시트 접힘) **Panel at = `Guide.Driver`**. 13·14 `Instruction` 문구(예: "Move and check from the passenger seat.").
3. `PanelAnchor_Passenger`·`PanelAnchor_Rear`를 그 좌석에 앉았을 때 손이 닿는 앞쪽으로.
4. Quest 3(T-05 좌석 시점 전환, T-06 순서·완료): 12 → 운전석에서 동승석 → 동승석에서 2열 → 2열에서 운전석 → 15, 안내판이 나를 향하는지, Confirm 버튼이 없는지. `not-tested`.
