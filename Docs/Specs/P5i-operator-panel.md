# P5i 진행자 창 — 따로 뜨는 영어 버튼 창 + 상태 정보 + 정렬 다시 적용(키보드 단축키는 그대로)

| 항목 | 값 |
|---|---|
| 단계 | P5 자유모드·리허설(진행자 지원). 계획서 §6 P5 "진행자 지원" |
| 결정 근거 | **사용자 요청(2026-10-16):** 진행자 편의를 위해 control panel 창을 따로 만들어 버튼으로 누르게 한다(키보드 버튼은 기억하기 어려움). **사용자 문답(2026-10-16):** 글자는 **영어**, 창에는 **버튼 + 상태 정보**(지금 VR 안 F1 정보와 같은 내용), 누를 수 없는 버튼은 흐리게. **사용자 요청(2026-10-16, 2차):** 처음 잡은 정렬이 틀어지면 **처음 설정한 값으로** 되돌리는 기능과 그 버튼. 창 디자인을 만들어 구현한다 — 리더들이 보므로 진중하고 밝지 않은 블루 계열. **2차 문답:** 정렬 버튼은 **저장값 다시 적용만**(지그에서 다시 저장은 넣지 않음), 평가는 **에디터 VR Preview**로 진행, 창은 **Play 때 자동으로 뜨고 툴바 아이콘으로 다시 앞으로**(툴바는 P5j), 디자인은 사용자 디자인 시스템 **CX Toolkit 그대로**, 글꼴은 **엔진 기본 Roboto**. TV 띠 색·배치는 따로(P5k — TV를 꽉 채우고 UI가 시야를 가리지 않게). **3차 결정(2026-10-16):** "F1 F5 쓰지 말고 알파벳 키를 사용"(§0-1). AGENTS.md 구조 원칙 "손 또는 키보드 입력 → 같은 기능 명령", "진행자 대리 조작과 참가자의 독립 수행을 구분한다", 확정 제약 U-07(참가자마다 다시 맞추지 않음) |
| 베이스 브랜치 | `main` @ P5h 머지(PR #22, `db977a1`) 뒤 이 명세 갱신 커밋(P5g의 `End`·`F6`과 자유모드, P5h의 영어 미션 문구·관전 띠를 씀) |
| 작업 브랜치 | `codex/P5i-operator-panel` |
| 리뷰 파일 | `Docs/Reviews/P5i-operator-panel.md` (첫 리뷰 때 생김) |
| 디자인 | `Docs/Design/P5i-operator-panel/` — `Main`(미션 진행 중)·`FreeMode`·`NeedsCheck`(시작 전, 정렬 확인 필요). 브라우저로 열면 보인다. 색·크기는 각 요소의 inline style이 기준이다(D9). `Toolbar`는 P5j용, `TvBand`는 P5k에서 다시 그린다 |
| 상태 | 명세 — 구현 전(디자인 사용자 확인 대기) |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **따로 뜨는 OS 창** | Slate 창(`SWindow`)을 `FSlateApplication::AddWindow`로 띄운다(엔진 `Runtime/Slate/Public/Framework/Application/SlateApplication.h:1576`, 제목 `Runtime/SlateCore/Public/Widgets/SWindow.h:287`, 닫기 버튼 `:339`). 제목은 `vBoR Operator`다. 게임 창(TV)과 다른 모니터에 둔다(§7). 러너가 Play를 시작할 때 만들고 `EndPlay`에서 닫는다. **만들지 않는 조건:** `FApp::CanEverRender()`가 거짓(자동화 `-nullrhi`·commandlet, 엔진 `Runtime/Core/Public/Misc/App.h:411-418`)이거나 `GIsAutomationTesting`이 참(엔진 `Runtime/Core/Public/CoreGlobals.h:623` — 렌더링되는 에디터에서 자동화를 돌리면 시험 월드마다 창이 뜨므로). **`FSlateApplication::IsInitialized()`만으로는 막지 못한다** — `-nullrhi` 자동화에서도 Slate는 초기화돼 있다(P5h E17이 그 상태에서 글꼴을 쟀다). 게임·PIE 월드에서만 만든다. **닫기 버튼은 없다**(실수로 닫으면 진행자 버튼이 사라짐). 최소화는 된다. 레벨을 다시 열면(P5g F6) 창이 닫혔다 다시 뜨는데, **같은 세션 안에서는 자리·크기를 기억해 같은 자리에 띄운다**(만들 때 `ScreenPosition`·`ClientSize`, 엔진 `SWindow.h:297`·`:300`. 닫기 전에 `GetPositionInScreen`·`GetSizeInScreen`, `:498`·`:501`). **창을 보이게 하는 공개 함수 하나를 둔다**(없으면 만들고, 최소화돼 있으면 풀고, 앞으로 가져옴 — `SWindow.h:758` `IsWindowMinimized`, `:928` `Restore`, `:575` `BringToFront`). P5j의 에디터 툴바 아이콘이 이 함수를 부른다 |
| D2 | **버튼 = 키와 같은 명령** | 각 버튼은 해당 키와 **같은 명령**을 진행자 출처로 넣는다(`QueueLive(…, Operator)`, `Interaction/VBoRScenarioRunner.cpp:296-299`. 키 처리 함수 `:614-623`). `In-VR operator info`는 흐름 명령이 아니다 — 정보 처리 함수(`KeyOperatorInfo`, `:624-629`, 키는 I로 바꿈)를 그대로 부른다. 버튼 이름 옆에 키를 같이 적는다(키를 익히는 데 도움). 아래 §0-1 표. **새 명령은 D8의 정렬 다시 적용(A) 하나뿐이다** — 키도 같이 연결한다(버튼으로만 되는 동작은 없음, §3) |
| D3 | **누를 수 없는 버튼은 흐리게** | 지금 누르면 흐름이 버릴 명령의 버튼은 흐리게 하고 눌리지 않게 한다. **판정은 `Dispatch`와 같은 함수 하나로 한다**(`:338-428`의 받기·버리기 조건을 버리는 이유와 함께 돌려주는 함수로 빼고, `Dispatch`와 창이 같이 씀). 규칙이 두 군데로 갈리면 창은 눌리는데 흐름이 버리거나, 그 반대가 생긴다. `Dispatch`의 동작·센 값·기록은 그대로다. 명령별 조건이 `switch` 안에도 있다는 것에 주의한다 — 확인(`:394-402`), 건너뛰기의 기준 복귀(`:404-406`), 처음부터의 태그 검사·저장 정렬(`:409-412`). **좌석 버튼은 정렬 컴포넌트가 받을지까지 본다:** 흐름이 받은 좌석 명령도 정렬 컴포넌트가 다시 거부한다 — 전환 중, 저장 정렬 없음·확인 필요, **정렬 무시로 시작함**(`Interaction/VBoRVehicleAlignmentComponent.cpp:97-104` `RequestSwitch`), 대상 좌석을 못 찾음(`:91`). 이 거부 조건도 정렬 컴포넌트의 읽기 함수 하나로 빼서 `RequestSwitch`와 판정 함수가 같이 쓴다. `Dispatch`는 지금처럼 전환 버림으로 센다. `In-VR operator info` 버튼은 레벨 다시 열기 암전 중에만 버린다(`:626`). 정렬 다시 적용의 조건(D8)도 같은 함수에 넣는다 |
| D4 | **상태 정보(영어)** | 창 위쪽에 영어로 보인다(배치는 D9). ① 흐름 상태: `Getting ready` / `Mission k of N — <ID> <영어 미션>`(P5h의 `VBoRSpectator::MissionText`, `Spectator/VBoRSpectatorModel.h:29` — TV와 같은 문구) / `Free mode`(완료로 들어왔는지 `End`인지) ② 지금 기다리는 것: `Alignment check`(시작 전 정렬 대기) / `Waiting for the rule` / `Operator confirmation needed` / `Waiting for participant confirmation` / `Confirm locked`(단계 시작 직후 잠금) / 자유모드 `Nothing — participant explores` ③ 정렬: `Applied` / `Needs check — <이유>` / `No saved alignment`(`VBoRVehicleAlignmentComponent.h:14`). 정렬 무시로 시작했으면 뒤에 `· started without alignment`(러너 `VBoRScenarioRunner.h:64`) ④ 좌석: `Driver` / `Passenger` / `Rear` / `Switching…` ⑤ 센 값: 이전·재시도·건너뛰기·`End`·정렬 다시 적용·버림(지난 단계·권한·규칙·전환·잠금) ⑥ 마지막 명령 결과: `<명령> — accepted` 또는 `— discarded (<이유>)`. 이유는 D3 판정 함수가 돌려준 것을 영어로 쓴다. 지금 VR 안 진행자 줄·정보(`GetOperatorText`, `:510-532`)는 한국어 그대로 두되, 안의 키 이름만 새 키로 바꾼다(`F5 처음부터` → `R 처음부터` `:513`, 기록 `(F1)` → `(I)` `:628`) |
| D5 | **창에 초점이 있어도 키가 된다** | 진행자가 창을 누르면 키보드 초점이 창으로 간다. 그때도 같은 키(§0-1)가 같은 명령을 내야 한다(창이 키를 받아 같은 명령으로 넘김). 게임 창에 초점이 있을 때는 지금 키 연결(`:125-137`)을 쓴다 — §0-1대로 F5 → R(`:133`), F1 → I(`:137`), A 추가. 한 번 누른 키가 두 번 들어가지 않게 한다(창과 게임이 같은 키를 동시에 받지 않음) |
| D6 | **새로 고침** | 버튼 흐림·상태 문구는 흐름이 바뀌면 바로, 적어도 초당 10회 다시 그린다(진행자가 누를 때 낡은 상태를 보지 않게) |
| D7 | **(P5j로 나눔)** | 에디터 툴바 아이콘은 새 에디터 모듈이 필요해 다음 명세 P5j에서 한다. 이 명세는 D1의 "창을 보이게 하는 공개 함수"만 둔다 |
| D8 | **정렬 다시 적용(A) — 처음 설정한 값으로** | **무엇:** 설치 때 저장한 Buck 정렬(`Saved/Calibration/BuckEye.json`)을 다시 읽고, 환경 검사(트래킹 원점·플레이 영역)를 다시 하고, Pawn을 저장값으로 다시 놓는다. 레벨을 열 때 하는 것(`Interaction/VBoRVehicleAlignmentComponent.cpp:22-33` `BeginPlay`, `:47-69` `InitializeAlignment`)과 같은 결과이고, 레벨은 다시 열지 않는다. **참가자 머리 자세를 읽지 않는다**(U-07, P3 D2 — `:63` 주석). 저장값이 하나이므로 누구에게나 같은 자리다. **왜:** 진행 중에 가디언이 잠깐 흔들리거나 OS 재중심이 오면 정렬이 `NeedsCheck`가 되고(`:71-75`, `:80-85`) 레벨을 다시 열 때까지 남는다. 그동안 좌석 전환·처음부터가 막힌다(D3). **받는 때:** 진행자만(키 A·버튼). 기준 시점(운전석)에 있고, 시점 전환 중이 아니고, 레벨 다시 열기 암전 중이 아닐 때. 흐름 상태(시작 전·진행 중·자유모드)와 정렬 상태(`Applied`·`NeedsCheck`·`NoSavedValue`)는 따지지 않는다. 다른 좌석이면 버리고 이유는 `Return to the driver seat first`. **암전:** 좌석 전환처럼 `FadeSeconds`로 검게 → 완전히 검을 때 한 번 다시 읽기·검사·놓기 → 밝게(`:112-134`와 같은 모양). 결과가 `NeedsCheck`여도 암전은 푼다. 다시 놓은 뒤 `OnPawnMoved`를 한 번 알린다(러너가 패널을 다시 놓음, `:130`). 암전 동안 다른 좌석·흐름 명령은 지금 전환 중 규칙으로 버린다. 지금 `RequestSwitch`는 `Applied`일 때만 받고(`:100`) `AdvanceSwitch`는 `Applied`가 아니면 중단하므로(`:115-121`), 다시 적용은 그 조건과 따로 연다. **흐름:** 흐름이 정렬을 기다리며 아직 시작하지 않았으면(`VBoRScenarioRunner.cpp:207-208`) `Applied`가 된 순간 레벨을 열 때처럼 1단계를 시작한다. 진행 중·자유모드에서는 단계·generation을 바꾸지 않는다. 정렬 무시로 시작한 기록(`bAlignmentIgnored`)은 그대로 둔다. **결과 문구(D4 ⑥):** `Re-apply saved alignment — accepted (Applied)` / `— accepted (still needs check: <이유>)` / `— accepted (no saved alignment)` / `— discarded (<이유>)`. 한국어 기록 줄 `진행자 정렬 다시 적용 — <상태>`. **한계(창에도 씀):** Quest 트래킹 공간 자체가 움직였으면 같은 저장값으로는 의자와 맞지 않는다. 그때는 Buck 보정 레벨(P0-c)에서 다시 저장한다 |
| D9 | **창 디자인 — CX Toolkit 어두운 테마** | 기준은 `Docs/Design/P5i-operator-panel/`의 `Main`·`FreeMode`·`NeedsCheck`다(색·크기·간격은 요소의 inline style). **색(sRGB hex):** 바탕 `#0b0f17`, 패널 `#131a26`, 패널 안 강조·지금 좌석 `#1c2534`, 구분선 `#2e3a4e`, 보조 버튼 테두리 `#66748a`, 글자 `#eef2f7`, 보조 글자 `#9aa6b8`, 주 버튼·강조 `#5b8cff`(그 위 글자 `#0b0f17`), 머리 띠 `#0d2a66`(그 위 글자 `#ffffff`), 정상 `#3cc285`, 확인 필요 `#f2b544`, 위험 `#ff7a72`. **Slate에 넣을 때 `FLinearColor::FromSRGBColor(FColor::FromHex(...))`로 바꾼다**(엔진 `Runtime/Core/Public/Math/Color.h:123`, `:573`). `FLinearColor`에 hex/255를 그대로 넣으면 선형 값으로 읽혀 색이 달라진다. **글꼴:** 엔진 Roboto의 `Regular`·`Bold`만(`FCoreStyle::GetDefaultFontStyle`, 엔진 `Runtime/SlateCore/Public/Styling/CoreStyle.h:51`). **Slate 글꼴 크기는 pt(96 DPI)라 디자인 px × 0.75로 넣는다**(예: 버튼 14 px → 10.5, 제목 24 px → 18). **창:** 기본 client 크기 720 × 1000. 내용은 세로 스크롤 상자에 넣는다(창을 줄여도 잘리지 않게). **배치(위 → 아래):** ① 머리 띠(D4 ①과 완료 수) ② 상태 패널(D4 ②–⑥) ③ 정렬 확인 필요일 때만 경고 상자(`NeedsCheck` 디자인, 테두리·글자 `#f2b544`) ④ `Flow` 패널(`Confirm`은 두 칸 너비) ⑤ `Seats` | `Alignment and view` 두 패널 ⑥ `Free mode` 패널 ⑦ 맨 아래 한 줄 `Shortcut keys also work while this window is focused.` **버튼 모양:** 높이 40 px, 모서리 6 px, 오른쪽에 키 표시. 다섯 가지 — 주(채움 `#5b8cff`), 보조(테두리 `#66748a`), 흐림(테두리 `#2e3a4e`, 바탕 `#0b0f17`, 글자 `#66748a`), 지금 좌석(테두리 `#5b8cff`, 바탕 `#1c2534`, `Current` 꼬리표), 위험(`Reload level` — 테두리·글자 `#ff7a72`). **주 버튼은 하나뿐이다:** 받을 수 있을 때의 `Confirm`, 그리고 정렬이 `NeedsCheck`이고 받을 수 있을 때의 `Re-apply saved alignment`. `Re-apply` 아래 한 줄 설명은 받을 수 있으면 `Uses the Buck alignment saved at setup. Never reads the participant's head.`, 다른 좌석이면 `Return to the driver seat first.` `In-VR operator info`에는 켜짐 상태(`On`/`Off`)를 붙인다 |
| D10 | **(P5k로 옮김)** | TV 띠 색은 TV 화면 배치 개정과 함께 P5k에서 한다(사용자 요청 2026-10-16: TV를 꽉 채우고 UI가 시야를 가리지 않게). **이 명세는 관전 서브시스템(`Spectator/`)을 고치지 않는다** |

### 0-1. 버튼

| 묶음 | 버튼(영어) | 키 | 명령 |
|---|---|---|---|
| Flow | `Confirm` | Space | 확인 |
| Flow | `Next step` | → | 건너뛰기 |
| Flow | `Previous step` | ← | 이전 |
| Flow | `Restart from step 1` | **R**(F5에서 바꿈) | 처음부터 |
| Flow | `Start without alignment` | O | 정렬 무시하고 시작 |
| Seats | `Driver seat` | Home | 운전석 |
| Seats | `Passenger seat` | 1 | 동승석 |
| Seats | `Rear seat` | 2 | 2열 |
| Alignment and view | `Re-apply saved alignment` | **A**(새 키) | 저장 정렬 다시 적용(D8) |
| Alignment and view | `In-VR operator info` | **I**(F1에서 바꿈) | VR 안 진행자 정보 켜고 끄기 |
| Free mode | `Enter free mode` | End | 자유모드로 바로(P5g D2) |
| Free mode | `Reload level` | F6 | 레벨 다시 열기(P5g D8) |

**키 바꿈(사용자 결정 2026-10-16 "F1 F5 쓰지 말고 알파벳 키"):** F1·F5는 엔진 개발 빌드의 디버그 키와 겹친다(엔진 `Engine/Config/BaseInput.ini:39` F1 와이어프레임, `:43` F5 셰이더 복잡도). 엔진은 키 입력을 기록한 뒤 디버그 명령을 실행하므로(엔진 `Runtime/Engine/Private/UserInterface/PlayerInput.cpp:428-462`) PIE에서 둘 다 일어난다. 그래서 처음부터는 **R**, VR 안 진행자 정보는 **I**, 새 정렬 다시 적용은 **A**다(이름 첫 글자 — Restart·Info·Alignment). 세 글자 모두 엔진 디버그 키(`BaseInput.ini:37-54`, 글자 키 없음)와 이 레벨의 다른 키(`O`)와 겹치지 않는다. `R`은 P2 문 시험대 레벨에서만 쓴다(`P2Probe/P2DoorBench.cpp:49`). **F1·F5 연결은 없앤다**(옛 키가 같은 명령을 계속 내지 않게). 나머지 키(Space·←·→·O·End·F6·Home·1·2)는 그대로다. `Reload level`은 위험 모양이고 `Enter free mode`와 같은 패널 안 오른쪽에 둔다(D9). 누르면 한 번 더 묻는 확인 창은 만들지 않는다 — P5g가 이미 자유모드에서만 받는다.

## 1. 왜 만드는가

- 진행자는 키 12개(Space·←·→·R·O·End·F6·Home·1·2·I·A)를 외워야 한다. 버튼 창이면 이름을 보고 누른다.
- 지금 진행자 정보는 VR 안 패널(I, 전에는 F1)에만 있어, 헤드셋을 쓰지 않은 진행자는 PC 화면에서 상태를 볼 수 없다.
- 진행 중 정렬이 `NeedsCheck`가 되면 지금은 레벨을 다시 열어야 풀린다. 진행자가 그 자리에서 저장값으로 다시 맞춘다(D8).
- 리더들이 보는 화면이라 진중한 한 가지 톤으로 맞춘다(D9).

## 2. 범위

### 만들 것

**1. 진행자 창** — D1–D6, D9, §0-1.
**2. 공통 판정 함수** — D3(`Dispatch`와 창이 같이 씀).
**3. 정렬 다시 적용** — D8(명령·A 키·버튼).
**4. 키 바꿈** — §0-1(F5 → R, F1 → I, 진행자 줄 안의 키 이름).
**5. 자동화 시험** — §4 전부.

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| 새 흐름 규칙·D8 밖의 새 명령 | 버튼은 기존 명령만 낸다(D2) |
| §0-1 밖의 키 바꾸기 | F5 → R, F1 → I(사용자 결정)와 A 추가만 한다 |
| 참가자 머리 기준 recenter | U-07 확정 제약 — 체격 차이가 지워짐 |
| 지그에서 다시 저장하는 버튼 | 사용자 결정 — 다시 적용만. 다시 저장은 P0-c 레벨 |
| 에디터 툴바 아이콘·에디터 모듈 | 다음 명세 P5j |
| VR 안 진행자 줄·정보를 영어로 바꾸기 | D4 — 한국어 그대로(키 이름만 바꿈) |
| 관전 화면(TV 띠) 바꾸기 | P5k(D10) |
| Pretendard 등 글꼴 파일 | 사용자 결정 — 엔진 Roboto |
| 원격(다른 PC·태블릿) 조작, 웹 화면 | 요청 없음 |
| 확인 대화상자 | §0-1 표 아래 |
| 레벨·에셋 저장, UMG 에셋 | C++ Slate로 만든다 |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- 기존 `vBoR.*`가 모두 그대로 통과한다. 특히 P3 흐름(권한·확인 잠금·지난 단계 버림·전환 중 버림), P0-c·P3 정렬, P5g, P5h E17. 기존 단언은 바꾸지 않는다. **예외는 키 이름을 보는 두 곳뿐이다(사용자 결정):** `P3Probe/P3FlowTests.cpp:406-408`(F1 연결 → I 연결), `P3Probe/P5gFreeModeTests.cpp:168`(진행자 줄의 `F5` → `R`). 시험 이름 문자열의 "F1"은 그대로 둬도 된다.
- 버튼과 키는 같은 명령이다. 버튼으로만 되는 동작은 없다.
- 진행자 출처로 기록한다(참가자 수행과 구분).
- 정렬 다시 적용은 참가자 머리를 읽지 않는다. 저장값과 차의 eye point만 쓴다.
- 자동화·헤드셋 없는 실행에서도 경고·오류 없이 지나간다(창을 만들지 않음).

## 4. 경계 상황과 그 시험

| # | 상황 | 기대 동작 | 자동화 시험 |
|---|---|---|---|
| E1 | 자동화(`-nullrhi` — Slate는 켜져 있음)·`GIsAutomationTesting` | 창을 만들지 않음, 경고·오류 없음. 창을 보이게 하는 함수를 불러도 만들지 않음 | 상태·로그 |
| E2 | 버튼마다 누름 | 같은 키를 눌렀을 때와 같은 명령(종류·진행자 출처·토큰)이 들어감(R·I·A 포함) | 명령 비교(창을 띄우지 않고 버튼 처리 함수를 부름) |
| E3 | 시작 전(정렬 대기 — 저장 정렬 없음·확인 필요) | `Start without alignment`·`Re-apply saved alignment`·`In-VR operator info`만 누를 수 있음. 나머지 흐림 | 흐림 |
| E4 | 진행 중, 규칙 대기 / 규칙 만족·진행자 확인 필요 / 확인 잠금 중 / 참가자 확인 단계 | `Confirm` 흐림 / 켬 / 흐림 / 흐림 | 흐림 |
| E5 | 시점 전환 중(암전) | 좌석·흐름·정렬 다시 적용 버튼 흐림(지금 전환 중 버림 규칙) | 흐림 |
| E6 | 자유모드(운전석) | `Confirm`·`Next`·`Previous`·`Enter free mode`·`Start without alignment` 흐림, 좌석·`Restart`·`Reload level`·`Re-apply` 켬 | 흐림 |
| E7 | 판정 일치 | 여러 상태에서 모든 버튼에 대해 "켜짐 ⇔ 지금 `Dispatch`가 받음"(A 포함) | 판정 함수와 실제 처리 비교 |
| E8 | 상태 문구 | 각 상태에서 D4 ①–⑥이 영어로 맞게 나옴. 버린 명령 뒤 `discarded (<reason>)` | 문구 |
| E9 | 같은 frame에 버튼과 키(같은 명령) | 지금 토큰 규칙대로 하나만 처리되고 나머지는 지난 단계로 버림(P3 E1과 같음) | 버림 수 |
| E10 | 창에 초점이 있을 때 키 | 같은 명령 한 번(두 번 들어가지 않음) | 명령 수 |
| E11 | 레벨 다시 열기 | 창이 닫혔다 같은 자리·크기로 다시 뜸 | 자리 기억(창 없이 저장값만) |
| E12 | 기존 시험 | 모두 통과. 기존 단언은 바꾸지 않는다 | 전체 시험 |
| E13 | 정렬 무시로 시작함(`O`) | 좌석 버튼 3개 흐림(정렬 컴포넌트가 거부 — 이유도 그렇게 나옴). 누르면 지금처럼 전환 버림으로 셈 | 흐림·판정 일치 |
| E14 | 레벨 다시 열기 암전 중 | 모든 버튼 흐림(`In-VR operator info`·`Re-apply` 포함) | 흐림 |
| E15 | 진행 중 가디언이 잠깐 바뀌어 `NeedsCheck` → 원래대로 → 운전석에서 A | 암전 → 다시 놓기 → 밝아짐. `Applied`. 단계·generation 그대로. `OnPawnMoved` 한 번 | 상태·흐름 |
| E16 | 시작 전 `NeedsCheck`(흐름이 정렬을 기다림) → 환경 원래대로 → A | `Applied`가 되고 1단계 시작 | 흐름 |
| E17 | 환경이 여전히 다름 / 저장 파일 없음 → A | 암전은 풀림. `NeedsCheck`(이유) / `NoSavedValue` 그대로. 결과 문구 `still needs check: …` / `no saved alignment` | 상태·문구 |
| E18 | 동승석·2열에 있음 / 전환 중 / 레벨 다시 열기 암전 중 → A | 흐림. 누르면 버리고 이유(`Return to the driver seat first` 등) | 흐림·판정 일치 |
| E19 | 참가자 머리가 운전석 eye에서 30 cm 떨어져 있음 → A | 다시 놓은 Pawn 변환이 저장값과 eye point로 계산한 것과 같음(머리 위치와 무관) | Pawn 변환 |
| E20 | 다시 적용 암전 중 좌석·확인·건너뛰기 명령 | 전환 중으로 버림 | 버림 수 |
| E21 | 버튼 모양 | 각 상태에서 고른 모양(주·보조·흐림·지금 좌석·위험)이 D9과 같음. 주 버튼은 한 번에 하나 | 모양 선택(창 없이) |
| E22 | 창 기본 색 | D9의 hex를 sRGB로 바꾼 값과 같음 | 색 값 |
| E23 | 창 문구 폭 | 버튼 이름·키·상태 문구가 디자인 너비 안에 한 줄로 들어감(가장 긴 `Needs check — <이유>`·결과 문구는 말줄임) | Slate 글꼴 측정(P5h E17 방식) |
| E24 | 키 바꿈 | 키 연결 목록에 R·I·A가 있고 F1·F5는 없음. VR 안 진행자 줄(자유모드)에 `R 처음부터`가 보이고 `F5`는 없음 | 키 연결·문구 |

## 5. 구현자가 정할 것

- D9 디자인에 없는 세부(1–2 px 간격, 스크롤 막대 모양), 상태 문구의 정확한 영어 표현(D4·D8에 적은 것은 그대로).
- 판정 함수의 모양(예: `CanAccept(Command) → (bool, Reason)`). `Dispatch`가 먼저 이것을 부르고, 받을 때만 지금처럼 처리한다.
- 정렬 다시 적용을 정렬 컴포넌트의 시점 전환 경로 위에 얹을지 따로 둘지(D8 조건을 지키면 됨).
- 창의 자리·크기를 기억하는 방법(같은 프로세스 안 정적 값 또는 사용자 설정 ini). 다른 세션까지 기억하는 것은 선택.
- 창을 러너가 가질지, 따로 둘지(러너의 명령·상태를 읽으면 됨).

## 6. 완료 조건

1. §2의 1–5가 들어 있다.
2. §4 시험이 모두 자동화로 통과한다. 기존 `vBoR.*`도 통과한다.
3. non-unity(`-Module=vBoR`)·unity 빌드에서 vBoR 경고·오류가 0이다. 전체 시험은 최종 트리로 한 번 돌린다.
4. AGENTS.md 보고 ①–⑤를 커밋 본문과 `HANDOFF.md`에 남긴다.
5. 시험 상태는 `not-tested`다.

## 7. 로컬에서 확인할 것 — 사용자

### PC + TV + Quest 3 (`L_P3_Flow`, 에디터 VR Preview)
1. Windows 디스플레이를 **확장**으로 두고, 게임 창을 TV에, `vBoR Operator` 창을 PC 모니터에 둔다(복제면 TV에도 진행자 창이 보임).
2. 창이 디자인(`Docs/Design/P5i-operator-panel/`)과 같은 느낌인지 본다. Windows 디스플레이 배율(125%·150%)에서 글자가 잘리지 않는지 본다.
3. 버튼으로 한 차례 진행해 본다: 시작 → 확인·건너뛰기 → 좌석 → 자유모드 → 레벨 다시 열기. 흐린 버튼이 상태와 맞는지 본다.
4. 창을 누른 뒤에도 키보드 단축키가 되는지 본다. R·I를 눌러도 화면이 와이어프레임·셰이더 복잡도로 바뀌지 않는지 본다.
5. **정렬 다시 적용:** 운전석에서 A(또는 버튼) → 짧은 암전 뒤 차가 같은 자리에 있는지 본다. 헤드셋을 쓴 채 의자 옆에 서서 눌러도 차가 움직이지 않는지 본다(머리를 읽지 않음). 동승석에서는 흐린지 본다.
6. 레벨을 다시 연 뒤 창이 같은 자리에 다시 뜨는지 본다. Play를 멈추면 닫히는지 본다.

## 8. 확인한 API 근거

경로: `Source/vBoR/` 아래(`main` @ P5h 머지 `db977a1`), 엔진은 `C:/Program Files/Epic Games/UE_5.7/Engine/Source/` 아래.

| 쓸 것 | 위치 | 쓰임 |
|---|---|---|
| 창 띄우기 | 엔진 `Runtime/Slate/Public/Framework/Application/SlateApplication.h:1576`(`AddWindow`), `Runtime/SlateCore/Public/Widgets/SWindow.h:287`(`Title`), `:297`·`:300`(`ScreenPosition`·`ClientSize`), `:339`(`HasCloseButton`), `:498`·`:501`(지금 자리·크기) | D1 |
| 창 보이게 하기 | 엔진 `SWindow.h:575`(`BringToFront`), `:758`(`IsWindowMinimized`), `:928`(`Restore`) | D1 |
| 창을 만들지 않는 조건 | 엔진 `Runtime/Core/Public/Misc/App.h:411-418`(`CanEverRender`), `Runtime/Core/Public/CoreGlobals.h:623`(`GIsAutomationTesting`) | D1·E1 |
| 키 연결·키 → 명령 | `Interaction/VBoRScenarioRunner.cpp:125-137`(키), `:296-299`(`QueueLive`), `:614-629`(키 처리, 정보 `:624-629`) | D2·D5 |
| 엔진 디버그 키 | 엔진 `Engine/Config/BaseInput.ini:37-54`(`DebugExecBindings`, F1 `:39`·F5 `:43`), `Runtime/Engine/Private/UserInterface/PlayerInput.cpp:428-462`(입력 기록 뒤 디버그 명령 실행) | §0-1(키 바꿈) |
| 키 이름을 보는 기존 단언 | `P3Probe/P3FlowTests.cpp:406-408`, `P3Probe/P5gFreeModeTests.cpp:168` | §3 |
| 명령 받기·버리기 조건 | `:338-428`(`Dispatch`), `:215`·`:235`(`RuleSatisfied`·`NeedsConfirmation`, const) | D3 |
| 좌석 전환 거부 조건 | `Interaction/VBoRVehicleAlignmentComponent.cpp:89-104`(`SwitchViewpoint`·`RequestSwitch`) | D3·E13 |
| 정렬 적용·검사·재중심 | `VBoRVehicleAlignmentComponent.cpp:22-33`(`BeginPlay` — 파일 읽기), `:47-69`(`InitializeAlignment`), `:71-75`(`HandleRecenter`), `:77-87`(매 frame 검사) | D8 |
| 전환 암전·다시 놓기 | `VBoRVehicleAlignmentComponent.cpp:112-134`(`AdvanceSwitch`), `:130`(`OnPawnMoved`) | D8 |
| 흐름의 정렬 대기 | `Interaction/VBoRScenarioRunner.cpp:156-209`(`InitializeFlow`, `:207-208` 대기) | D8·E16 |
| 지금 진행자 문구 | `:510-532`(`GetOperatorText`) | D4 |
| 영어 미션 문구 | `Spectator/VBoRSpectatorModel.h:29`(`VBoRSpectator::MissionText`) | D4 |
| 정렬 상태·좌석 | `Interaction/VBoRVehicleAlignmentComponent.h:14`(상태), `:23-27`, `:45`, 러너 `VBoRScenarioRunner.h:51-57`(읽기 함수), `:64`(정렬 무시) | D4 |
| 색 변환 | 엔진 `Runtime/Core/Public/Math/Color.h:123`(`FLinearColor::FromSRGBColor`), `:573`(`FColor::FromHex`) | D9 |
| 글꼴 | 엔진 `Runtime/SlateCore/Public/Styling/CoreStyle.h:51`(`GetDefaultFontStyle`, 크기 float pt), `Runtime/SlateCore/Public/Fonts/SlateFontInfo.h:15`(96 DPI) | D9 |
| 모듈 | `vBoR.Build.cs:13`(Slate·SlateCore 있음) | D1 |

## 개정 이력

| 날짜 | 개정 | 내용 |
|---|---|---|
| 2026-10-16 | 처음 | 작성(사용자 요청·문답: 따로 뜨는 창, 영어, 버튼 + 상태 정보) |
| 2026-10-16 | 구현 전 개정 | P5h 머지(`db977a1`) 뒤 베이스·줄 번호. D1 창을 만들지 않는 조건(`-nullrhi`에서도 Slate가 켜져 있어 `IsInitialized`로는 못 막음), 자리 기억 API. D2 F1 버튼은 F1 처리 함수. D3 `switch` 안 조건과 좌석의 정렬 컴포넌트 거부 조건도 같은 판정에(정렬 무시로 시작하면 좌석 전환이 안 됨). D4 영어 미션은 P5h 함수. E13·E14 추가 |
| 2026-10-16 | 구현 전 개정 2 | 사용자 2차 요청·문답: D8 정렬 다시 적용(F7, 저장값만, 머리 읽지 않음), D9 창 디자인(CX Toolkit 어두운 테마, Roboto, `Docs/Design/P5i-operator-panel/`), D10 TV 띠 같은 색, D1 창을 보이게 하는 공개 함수. 툴바 아이콘은 P5j로 나눔(D7). E3·E5·E6·E14 갱신, E15–E23 추가 |
| 2026-10-16 | 구현 전 개정 3 | 사용자 결정 "F1 F5 쓰지 말고 알파벳 키": 처음부터 R, VR 안 정보 I, 정렬 다시 적용 A(F7 대신). F1·F5 연결 없앰, 진행자 줄 안의 키 이름도 바꿈. 키 이름을 보는 기존 단언 두 곳만 고침(§3). E24 추가. 디자인의 키 표시도 바꿈 |
| 2026-10-16 | 구현 전 개정 4 | D10(TV 띠 색)을 P5k로 옮김 — 사용자 요청(TV 꽉 채우기·UI가 시야를 가리지 않게)으로 TV 배치를 다시 정하므로 색과 함께 한다. 이 명세는 관전 서브시스템을 고치지 않는다 |
