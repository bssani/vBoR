# P5z 안내판 — 정한 자리에 안내문만, 자유모드에서는 숨김

| 항목 | 값 |
|---|---|
| 단계 | P5 리허설 준비(사용자 요청 2026-10-24). 시험 T-06(순서·완료 판정)·T-08(자유모드)·T-09(실제 운영 — 안내 이해) |
| 결정 근거 | **사용자(2026-10-24):** "3d widget 같은 경우에는 scenario message만 간단히 보여주고, 지정된 위치에 나오게끔 할 수 있나? 지금은 사람을 쫒아 다니잖아. 내가 정한 위치에 그 scenario 위젯이 나왔으면 좋겠어. 그리고 Free Mode 일때는 widget이 안나왔으면 좋겠어."<br>**Claude 질문 → 사용자 답(모두 권장안, 마지막만 아님):**<br>① 위치 → **단계마다 지정**(레벨의 위치 표식 + 단계의 태그. 태그 없는 단계는 직전 위치 유지)<br>② 표시 → **안내문만**(단계 번호·상태 줄·VR 진행자 한 줄 뺌, "안내 완료"·"준비 중"도 뺌)<br>③ 확인·시점 버튼 → **안내판 밑에 고정**<br>④ 자유모드 → **안내판과 좌석 버튼 모두 숨김**(좌석 변경은 진행자 키 1·2·Home만) |
| 베이스 브랜치 | `main` @ 이 명세 머지 뒤 |
| 작업 브랜치 | `codex/P5z-fixed-guidance-panel` |
| 리뷰 파일 | `Docs/Reviews/P5z-fixed-guidance-panel.md` (첫 리뷰 때 생김) |
| 상태 | 명세 — 구현 전 |
| 에디터 작업 | **있음** — `L_Main`에 위치 표식 놓기, `DA_Main_Scenario` 단계마다 `PanelTag` 넣기(MCP) |
| 앞 명세 | [P5y-bp-pawn-repeat-play-grab.md](P5y-bp-pawn-repeat-play-grab.md) |

## 0. 지금 동작 (Claude 확인 2026-10-24)

- 안내판(`Panel`, `UWidgetComponent`)은 단계 시작(`EnterStep` → `PlacePanel`, `Interaction/VBoRScenarioRunner.cpp:368`·`:379`), Pawn 이동(`HandlePawnMoved`, `:591-594`), 경로 도착(`:828`)마다 **머리 앞** `PanelDistanceCm`에 다시 놓인다(`PlacePanelAtHead`, `:596-615`). 머리가 추적되기 전에는 숨긴다(`bPanelPlacementPending`, `:631`, `:717`).
- 확인 버튼·시점 버튼(`ConfirmButton`·`ActionButton`, poke 버튼)과 이름표도 같은 때 머리 기준으로 놓인다(`:605-612`). 다시 놓은 뒤 `ConfirmLockSeconds` 동안 확인을 막는다(`:613`).
- 글: 안내문 + 단계 번호(`02 · 2/3`) + 상태 줄(`Press confirm`·`Please wait`·`Return to the driver seat`·`Follow the instructions`), 준비 전 `Preparing`, 끝 `Guidance complete`, 자유모드 `FreeModeInstruction`(`GetPanelText`, `:646-662`).
- VR 진행자 한 줄(`OperatorPanel`)이 안내판 옆·아래에 붙는다(`RefreshPanel`, `:700-717`). PC 진행자 창(P5i)은 따로 있다.
- 자유모드에서는 두 버튼이 좌석 바꾸기 버튼이 된다(`:725-742`, P5o).

## 1. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **단계에 안내판 자리 태그를 둔다** | `FVBoRScenarioStep`(`Interaction/VBoRScenarioAsset.h`)에 `FName PanelTag`를 더한다(`MarkerTag` 옆, ToolTip "Panel anchor tag. Optional"). 찾는 법은 `MarkerTag`와 같다: `FindP3Tag`로 정확히 하나, `P3Scene`으로 장면 컴포넌트(`:250-252`). 못 찾거나 둘 이상·장면이 아니면 시작 검사 문제로 막는다(문구 "`<ID>`: panel tag `<tag>` …", `MarkerTag` 문구와 같은 형식). C++ 기본 시나리오(`VBoRScenarioAsset.cpp`)에는 넣지 않는다 |
| D2 | **위치 표식 Actor를 만든다** | `AVBoRPanelAnchor`(`Interaction/`, `Blueprintable` 아님 — 레벨에 그대로 놓는 도구). 루트 장면 컴포넌트 + `UArrowComponent`(+X, 참가자 쪽) + **에디터에서만 보이는** 안내판 크기 판(`AVBoRScenarioRunner` 기본 `PanelSizeCm`, 50×32 cm, 게임에서 숨김·충돌 없음) + 버튼 두 자리 표시(같은 규칙, D4). 태그는 Actor Tags에 단다. **태그만 맞으면 다른 Actor·컴포넌트도 된다**(D1 규칙) — 이 클래스는 자리 잡기를 돕는 것뿐이다 |
| D3 | **태그가 있는 시나리오는 정한 자리에만 놓는다** | 시나리오의 단계 중 **하나라도** `PanelTag`가 있으면 "고정 모드"다.<br>- 단계 시작 때 그 단계 표식이 있으면 안내판을 표식의 월드 위치·회전에 놓는다. 안내판 앞면이 표식 +X(화살표) 쪽을 본다.<br>- 그 단계에 태그가 없으면 **직전 자리 그대로**(사용자 답 ①). 첫 단계에 태그가 없으면 시작 검사 문제로 막는다("first step needs a panel tag in fixed mode").<br>- Pawn 이동·경로 도착·시점 전환·머리 추적 여부로 **다시 놓지 않는다**. 머리 추적 전에도 숨기지 않는다.<br>- **단계 시작 확인 잠금은 그대로다**(`EnterStep`, 빠른 두 번 누름·늦은 진행자 키가 다음 단계를 넘기지 않게, P3 §112·E2 — 개정 3). 그에 더해 자리가 실제로 바뀐 때만 지금처럼 다시 놓기 잠금과 기록("Panel repositioned — confirmation lock")을 남긴다. 같은 표식·태그 없음이면 다시 놓기 기록이 없다(단계 시작 잠금은 걸린다).<br>- **태그가 하나도 없는 시나리오는 지금 동작 그대로**(머리 앞, 다시 놓기 포함). 근사 차·시험 시나리오가 여기에 해당한다 — 이 경로의 기존 시험을 바꾸지 않기 위해서다 |
| D4 | **버튼은 안내판 밑에 고정한다** | 고정 모드에서 확인·시점 버튼과 이름표는 안내판(표식) 기준 고정 오프셋에 둔다: 안내판 아래 가장자리에서 `PanelButtonGapCm`(새 프로퍼티, 기본 8 cm) 아래, 좌우 ±12 cm(지금 간격), 누르는 면(+Z)이 표식 +X를 본다. 표식이 기울어 있으면 같이 기운다. 머리 앞 모드(D3 마지막)는 지금 그대로 |
| D5 | **안내판에는 안내문만** | `GetPanelText`는 진행 중인 단계의 `Instruction`만 돌려준다. 그 밖(준비 전·끝·자유모드)은 빈 글이고, **빈 글이면 안내판을 숨긴다**. 단계 번호·상태 줄·`Preparing`·`Guidance complete`를 뺀다. **두 모드 모두** 적용한다(사용자 답 ②).<br>- 참가자가 할 일은 버튼 이름표가 대신한다("Confirm", "Move to passenger seat", "Return to driver seat" — 지금 그대로) |
| D6 | **VR 진행자 한 줄을 없앤다** | `OperatorPanel`은 게임에서 늘 숨긴다(두 모드 모두). `GetOperatorText`·`F1` 상세는 그대로 둔다(PC 진행자 창·기록이 쓴다). 컴포넌트 자체를 지울지는 구현자가 정한다(지우면 관련 시험 접근자도 정리) |
| D7 | **자유모드에서는 안내판과 버튼을 모두 숨긴다** | 자유모드 좌석 버튼(`:725-742`)을 없앤다 — 두 버튼은 자유모드에서 늘 숨김. 좌석 변경은 진행자 키 1·2·Home(지금 그대로)뿐이다. `FreeModeInstruction` 프로퍼티는 쓰는 곳이 없어지므로 지운다(`VBoRScenarioRunner.h:51`, `.cpp:114`) |
| D8 | **`L_Main`에 표식을 놓고 단계에 태그를 넣는다(MCP)** | 처음 자리는 **자리값**이다 — 사용자가 에디터에서 옮긴다. Actor 라벨·태그와 맡는 단계:<br>- `PanelAnchor_Outside`(`Guide.Outside`): 01·02·03·07·08·19·20 — 운전석 문 바깥, 시작 위치에서 문을 볼 때 문 앞쪽 위(서 있는 눈높이 근처)<br>- `PanelAnchor_Trunk`(`Guide.Trunk`): 04·05·06·16 — 트렁크 뒤 바깥<br>- `PanelAnchor_Driver`(`Guide.Driver`): 09·10·11·12·13·18 — 운전석 눈(`BP_Car_Core`의 `DriverView`) 앞 약 60 cm, 약 15 cm 아래<br>- `PanelAnchor_Passenger`(`Guide.Passenger`): 14 — `PassengerView` 앞 같은 규칙<br>- `PanelAnchor_Rear`(`Guide.Rear`): 15 — `RearView` 앞 같은 규칙<br>- 모든 표식은 Outliner `Car` 폴더 밑 `Guidance` 하위 폴더에 둔다(`Approx` 아님 — 회사에서 지우지 않게). 화살표가 그 단계의 참가자 쪽을 본다.<br>- `DA_Main_Scenario`의 19단계에 위 태그를 넣는다. 그 밖의 칸(ID·안내문·규칙·태그·순서)은 그대로.<br>- 실제 자리 값(cm)은 보고에 표로 적는다 |
| D9 | **근사 차도 같은 표식 5개를 만든다(개정 2)** | `P3FlowTests.cpp`의 `vBoR.P4c.E10E11E12MockAssetDefaults`(`:1078-1082`)와 `vBoR.P4d.E12E13AssetRearFlow`(`:1284-1287`)는 `DA_Main_Scenario`를 **근사 차 월드에서** 시작 검사·실행한다. D8로 DA에 `PanelTag`가 들어가면 근사 차에 표식이 없어 D1 검사가 막힌다.<br>- `AP3VehicleMock`(`P3Probe/P3VehicleMock.h/.cpp`)이 경로·키 테이블처럼 `AVBoRPanelAnchor` 5개를 spawn한다. 태그는 D8과 같은 `Guide.Outside`·`Guide.Trunk`·`Guide.Driver`·`Guide.Passenger`·`Guide.Rear`. 자리는 `Placeholder\|Approximate guidance` 카테고리 프로퍼티(차 좌표 cm·Yaw)로 두고, 값은 D8과 같은 규칙(근사 차의 `DriverEye`·`PassengerView`·`RearView`·운전석 문·트렁크 기준)으로 정한다. 종료 때 같이 지운다.<br>- 근사 차의 C++ 기본 시나리오에는 태그가 없으므로 근사 차를 쓰는 다른 시험은 머리 앞 모드 그대로다 |

## 2. 범위

### 만들 것

- D1·D3–D7 런너 변경, D2 표식 클래스, D8 에셋(MCP), D9 근사 차 표식.
- §3 표대로 기존 시험 정리, 새 시험 `vBoR.P5z.*`(§4).

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| 바닥 경로·표식 고리·화살표(`MarkerTag`·`RouteTag`) 동작 | 그대로 |
| 안내판 글꼴·크기·색, 위젯 해상도 | 그대로 |
| 머리 앞 모드(태그 없는 시나리오)의 놓기 규칙 | D3. 근사 차·시험 경로 |
| PC 진행자 창(P5i) | 그대로 |
| 좌석 변경 키, 시점 전환 규칙 | 그대로 |
| `BP_Car_Core`에 표식 컴포넌트 넣기 | 레벨 Actor로 한다(P4i E2 근사 차 대조를 늘리지 않음). 태그만 맞으면 사용자가 나중에 컴포넌트로 옮겨도 된다(D2) |
| Blueprint-Guide 고치기 | 머지 뒤 Claude |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- **판정·흐름:** 단계 규칙·확인 주체·확인 잠금 시간·시점 전환·암전·기록 문구("Panel repositioned …" 포함)는 그대로다.
- **머리 앞 모드:** 태그가 없는 시나리오에서 안내판·버튼 놓기, 다시 놓기, 머리 추적 전 숨김은 지금과 같다.
- **버튼 이름표 글**은 그대로다(진행 중 단계에서).
- **기존 시험 감사(Claude 스크립트, 2026-10-24).** 안내판 글·자리·버튼·VR 진행자 줄·자유모드 좌석 버튼을 보는 시험을 모두 찾았다. **자리·버튼 자리 시험은 머리 앞 모드라 그대로다.** 바뀌는 것은 아래뿐이다(줄은 `P3Probe/`).

| 시험 | 줄 | 처리 |
|---|---|---|
| `vBoR.P3Flow.E12ReturnBeforeExit` | `P3FlowTests.cpp:361` | 안내판 "Return to the driver seat" 확인 → 안내판이 안내문만이고, 시점 버튼 이름표가 "Return to driver seat"인지로 |
| `vBoR.P3Flow.E16E18E19DataOrderCompletionAndSlate` | `:406`, `:410`, `:413` | Slate 글이 안내문과 **같음**(포함이 아니라 같음), 단계 번호 줄(`:410` "02 · 2/3")은 없음, 끝 문구(`:413`)는 "끝나면 안내판 숨김"으로 |
| `vBoR.P3Flow.R1ParticipantAndOperatorText` | `:485`, `:497`, `:499`, `:505` | 안내판 상태 문구(`Please wait` 등) 확인 → 안내문만. `:499` VR 진행자 줄 확인 → 게임에서 숨김. `:505` 준비 글 → 준비 전 안내판 숨김. 진행자 글(`GetOperatorText`) 확인은 그대로 |
| `vBoR.P3Flow.R1EyeLevelAndReadableStyle` | `:531-532` | 글 내용 확인만 안내문으로. 글꼴·자리 확인은 그대로 |
| `vBoR.P3Flow.R1EventPanelRepositionAndOcclusion` | `:616` | VR 진행자 줄 자리 확인 → 숨김. 안내판·버튼 다시 놓기 확인은 그대로 |
| `vBoR.P3b.E12OperatorPlateOcclusion` | `:858-874` | VR 진행자 줄이 판에 가리지 않는지 → 진행자 줄 숨김으로. 안내판 확인은 그대로 |
| `vBoR.P4h.E10SeatAndSourceGates` | `P4hPointAlignmentTests.cpp:268` | 안내판 글 확인을 안내문만으로(그 시점에 단계가 없으면 숨김) |
| `vBoR.P5g.E1E2CompletedFreeMode` | `P5gFreeModeTests.cpp:111-121` | `FreeModeInstruction` 줄 지움. 자유모드 안내판 숨김·두 버튼 숨김으로 |
| `vBoR.P5g.E3E4E5E6ParticipantSeats` | `:143-153` | 참가자 좌석 버튼 시험 → **자유모드에서 두 버튼이 숨고 눌러도 좌석이 안 바뀜**, 진행자 키 1·2·Home으로는 바뀜(지금 키 시험이 있으면 그대로) |
| `vBoR.P5g.E7E8E10E13OperatorAndRestart` | `:164`, `:180` | 안내판 글 확인 → 자유모드 숨김. 버튼 확인 → 숨김 |
| `vBoR.P5i.E18ReapplyRefusedAwayOrDuringFade` | `P5iOperatorPanelTests.cpp:347` | 안내판 글 확인을 안내문만으로 |
| `vBoR.P5g.E9E11E12GuardsAndTracking` (개정 1, Q1) | `P5gFreeModeTests.cpp:199`, `:202` | `:199` 추적 전 숨김 확인은 그대로(설명만 "자유모드는 늘 숨김"에 맞춰도 됨). `:202` "Recovery places panel"·"Recovery shows seat buttons" → 추적 복구 뒤에도 안내판·두 버튼 **숨김**. End 거부·참가자 권한·End 1회 집계 단언은 그대로 |
| `vBoR.P4c.E10E11E12MockAssetDefaults`, `vBoR.P4d.E12E13AssetRearFlow` (개정 2) | `P3FlowTests.cpp:1078-1082`, `:1284-1287` | **고치지 않는다.** D9로 근사 차에 표식이 생겨 DA가 그대로 검사를 통과해야 한다. 두 시험에는 안내판 자리 단언이 없다 |
| `vBoR.P3Flow` DA 필드 대조(`E9`, `:1035-1076`) (개정 2) | `:1060-1074` | `PanelTag`를 보지 않으므로 그대로 통과한다. D8 단계별 태그 확인 한 줄(`S.PanelTag`)을 **더해도 된다**(권장) |
| `vBoR.P5o.E1E2SeatViewsFoundAtStart` (개정 1) | `P5iOperatorPanelTests.cpp:706`, `:713`, 도우미 `:47` | `:706` "Free seat buttons shown" → 자유모드에서 좌석 버튼 숨김. `:713` "need both seats"는 늘 숨김이라 뜻이 없어진다 — 지우거나 "숨김 유지"로 바꾼다. 좌석 시점 찾기 횟수·`EvaluateOperatorButton` 단언은 그대로. 도우미 `SeatButtonsShown`은 남겨도 되고 지워도 된다 |

- **감사가 빠뜨린 것(개정 1):** 처음 스크립트는 안내판 글·자리·버튼 접근자·진행자 줄만 찾고 **숨김·표시 단언**(`IsHidden`, `bHiddenInGame`)을 찾지 않아 P5g E9(Codex Q1)와 P5o E1E2를 놓쳤다. 다시 감사한 범위: 모든 시험의 `IsHidden`·`bHiddenInGame`·`IsVisible` 단언 중 런너 안내판·버튼·이름표·진행자 줄·좌석을 보는 것 28곳. 위 두 줄 외에는 바뀌지 않는다 — 평가 중 버튼(`P3FlowTests.cpp:261`·`:460`·`:467`·`:524-529`·`:940`·`:946`), 머리 앞 모드 자리(`:605`·`:607`), 바닥 경로·정렬점(`:716`·`:743`·`:776`·`:778`, P4h `:257`·`:306`), 자유모드·암전·다시 불러오기 중 숨김(P5g `:137`·`:179`·`:221` — 늘 숨김이라 그대로 통과).
- 이 표 밖의 시험이 바뀌면 고치지 말고 질문에 적고 멈춘다. 특히 머리 앞 모드 자리 시험(`R1ConfirmTimeLock`, `R1EventPanelRepositionAndOcclusion`의 안내판·버튼 부분, `P3b.E5E6ArrivalAndLock`, `P3b.E11ViewpointFacing`, P4d·P4e·P5i의 다시 놓기)은 바뀌면 안 된다.
- 지운 단언은 보고 ③에 시험 이름과 함께 적는다.

## 4. 시험 — `vBoR.P5z.*`

근사 차 월드(D9 표식 5개가 있음)에서 시나리오에 `PanelTag`를 넣어 고정 모드로 만든다. 다른 자리가 필요하면 표식을 더 spawn한다.

| ID | 상황 | 확인 |
|---|---|---|
| E1 | 고정 모드 단계 시작 | 안내판 월드 위치·회전 = 표식(오차 0.01 cm·0.01°), 앞면이 표식 +X 쪽. 머리가 추적되기 전에도 보임 |
| E2 | 머리·Pawn 이동, 경로 도착, 시점 전환 | 안내판·버튼 자리가 바뀌지 않음, 확인 잠금 안 걸림 |
| E3 | 다음 단계가 다른 표식 / 같은 표식 / 태그 없음 | 다른 표식: 옮기고 다시 놓기 기록 1회. 같은 표식: 안 옮김·기록 없음. 태그 없음: 직전 자리 유지·기록 없음. **세 경우 모두 단계 시작 잠금은 걸림**(개정 3) |
| E4 | 버튼 자리 | 두 버튼이 안내판 아래 가장자리에서 `PanelButtonGapCm` 아래, ±12 cm, 누르는 면이 표식 +X. 기운 표식(피치 20°)에서도 같은 상대 자리 |
| E5 | 시작 검사 | 태그 못 찾음·둘 이상·장면 아님 → 각각 시작 막힘과 문제 문구. 고정 모드 첫 단계에 태그 없음 → 막힘 |
| E6 | 글 | 진행 중: 안내판 글 = 안내문(같음). 준비 전·끝·자유모드: 숨김. VR 진행자 줄은 두 모드 모두 숨김 |
| E7 | 자유모드 | 안내판·두 버튼 숨김. 버튼을 눌러도(이벤트 주입) 좌석 안 바뀜. 진행자 1·2·Home은 바뀜 |
| E8 | 태그 없는 시나리오 | 머리 앞 모드 그대로(기존 P3 시험이 보장 — 여기서는 한 줄 확인만) |
| E9 | 표식 Actor | 에디터 판·버튼 자리 표시는 게임에서 숨김, 충돌 없음 |

## 5. 구현자가 정할 것

- 고정 모드 판정 시점(시작 검사 때 한 번이 자연스럽다), 모드를 보관하는 방법.
- `OperatorPanel`을 숨길지 지울지(D6).
- 표식 Actor의 에디터 표시 방법(판은 `UStaticMeshComponent` Plane에 반투명 재질 등).
- D8 자리값의 정확한 값.

## 6. 완료 조건

1. D1–D8 구현, §3 표대로 기존 시험 정리, `vBoR.P5z.*` 통과.
2. `vBoR.*` 전체 통과(개수와 지운 단언 보고).
3. non-unity·unity 빌드 성공, vBoR 경고 0.
4. MCP 재조회: `L_Main` 표식 5개(라벨·태그·폴더·자리), `DA_Main_Scenario` 19단계 `PanelTag`. 저장한 에셋은 이 둘뿐.
5. 주석 영어 짧은 조각문, 화면·에디터 글 영어. 커밋 본문 보고 ①–⑤(D8 자리값 표 포함).

## 7. 로컬에서 확인할 것

- **빌드·시험:** 끝에 한 번.
- **에디터:** D8 MCP 작업 뒤 저장하고 닫는다. 사용자는 표식 5개를 원하는 자리로 옮긴다(에디터의 판·버튼 표시로 크기를 본다). 버튼이 손 닿는 자리인지 같이 본다.
- **Quest 3(`not-tested`):** 단계마다 안내판이 정한 자리에 뜨고 따라오지 않는지, 14·15 시점에서 보이는지, 버튼이 닿는지, 자유모드에서 아무것도 안 뜨는지(T-06·T-08·T-09).

## 개정 이력

| 날짜 | 내용 |
|---|---|
| 2026-10-24 | 처음 작성 |
| 2026-10-24 | 개정 3 — 리뷰 R1-1. D3의 "같은 표식이면 잠그지 않는다"가 단계 시작 확인 잠금까지 빼게 했다. 단계 시작 잠금은 그대로라고 고침, E3 확인 수정 |
| 2026-10-24 | 개정 2 — Claude 다시 감사. DA를 근사 차에서 실행하는 P4c E10·P4d E12E13이 D8 태그로 막히는 것을 찾음 → D9(근사 차 표식 5개) 추가, §3 표에 두 시험·DA 필드 대조 추가 |
| 2026-10-24 | 개정 1 — Codex Q1 답. §3 표에 P5g E9E11E12·P5o E1E2 추가(자유모드 버튼 표시 단언), 숨김·표시 단언 다시 감사 |
