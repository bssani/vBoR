# P5i 진행자 창 — 따로 뜨는 영어 버튼 창 + 상태 정보(키보드 단축키는 그대로)

| 항목 | 값 |
|---|---|
| 단계 | P5 자유모드·리허설(진행자 지원). 계획서 §6 P5 "진행자 지원" |
| 결정 근거 | **사용자 요청(2026-10-16):** 진행자 편의를 위해 control panel 창을 따로 만들어 버튼으로 누르게 한다(키보드 버튼은 기억하기 어려움). **사용자 문답(2026-10-16):** 글자는 **영어**, 창에는 **버튼 + 상태 정보**(지금 VR 안 F1 정보와 같은 내용), 누를 수 없는 버튼은 흐리게. AGENTS.md 구조 원칙 "손 또는 키보드 입력 → 같은 기능 명령", "진행자 대리 조작과 참가자의 독립 수행을 구분한다" |
| 베이스 브랜치 | `main` @ P5h 머지(PR #22, `db977a1`) 뒤 이 명세 갱신 커밋(P5g의 `End`·`F6`과 자유모드, P5h의 영어 미션 문구를 씀) |
| 작업 브랜치 | `codex/P5i-operator-panel` |
| 리뷰 파일 | `Docs/Reviews/P5i-operator-panel.md` (첫 리뷰 때 생김) |
| 상태 | 명세 — 구현 전 |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **따로 뜨는 OS 창** | Slate 창(`SWindow`)을 `FSlateApplication::AddWindow`로 띄운다(엔진 `Runtime/Slate/Public/Framework/Application/SlateApplication.h:1576`, 제목 `Runtime/SlateCore/Public/Widgets/SWindow.h:287`, 닫기 버튼 `:339`). 제목은 `vBoR Operator`다. 게임 창(TV)과 다른 모니터에 둔다(§7). 러너가 Play를 시작할 때 만들고 `EndPlay`에서 닫는다. **만들지 않는 조건:** `FApp::CanEverRender()`가 거짓(자동화 `-nullrhi`·commandlet, 엔진 `Runtime/Core/Public/Misc/App.h:411-418`)이거나 `GIsAutomationTesting`이 참(엔진 `Runtime/Core/Public/CoreGlobals.h:623` — 렌더링되는 에디터에서 자동화를 돌리면 시험 월드마다 창이 뜨므로). **`FSlateApplication::IsInitialized()`만으로는 막지 못한다** — `-nullrhi` 자동화에서도 Slate는 초기화돼 있다(P5h E17이 그 상태에서 글꼴을 쟀다). 게임·PIE 월드에서만 만든다. **닫기 버튼은 없다**(실수로 닫으면 진행자 버튼이 사라짐). 최소화는 된다. 레벨을 다시 열면(P5g F6) 창이 닫혔다 다시 뜨는데, **같은 세션 안에서는 자리·크기를 기억해 같은 자리에 띄운다**(만들 때 `ScreenPosition`·`ClientSize`, 엔진 `SWindow.h:297`·`:300`. 닫기 전에 `GetPositionInScreen`·`GetSizeInScreen`, `:498`·`:501`) |
| D2 | **버튼 = 키와 같은 명령** | 각 버튼은 해당 키와 **같은 명령**을 진행자 출처로 넣는다(`QueueLive(…, Operator)`, `Interaction/VBoRScenarioRunner.cpp:296-299`. 키 처리 함수 `:614-623`). 새 규칙을 만들지 않는다. `In-VR operator info`는 흐름 명령이 아니다 — F1 처리 함수(`KeyOperatorInfo`, `:624-629`)를 그대로 부른다. 버튼 이름 옆에 키를 같이 적는다(키를 익히는 데 도움). 아래 §0-1 표 |
| D3 | **누를 수 없는 버튼은 흐리게** | 지금 누르면 흐름이 버릴 명령의 버튼은 흐리게 하고 눌리지 않게 한다. **판정은 `Dispatch`와 같은 함수 하나로 한다**(`:338-428`의 받기·버리기 조건을 버리는 이유와 함께 돌려주는 함수로 빼고, `Dispatch`와 창이 같이 씀). 규칙이 두 군데로 갈리면 창은 눌리는데 흐름이 버리거나, 그 반대가 생긴다. `Dispatch`의 동작·센 값·기록은 그대로다. 명령별 조건이 `switch` 안에도 있다는 것에 주의한다 — 확인(`:394-402`), 건너뛰기의 기준 복귀(`:404-406`), 처음부터의 태그 검사·저장 정렬(`:409-412`). **좌석 버튼은 정렬 컴포넌트가 받을지까지 본다:** 흐름이 받은 좌석 명령도 정렬 컴포넌트가 다시 거부한다 — 전환 중, 저장 정렬 없음·확인 필요, **정렬 무시로 시작함**(`Interaction/VBoRVehicleAlignmentComponent.cpp:97-104` `RequestSwitch`), 대상 좌석을 못 찾음(`:91`). 이 거부 조건도 정렬 컴포넌트의 읽기 함수 하나로 빼서 `RequestSwitch`와 판정 함수가 같이 쓴다. `Dispatch`는 지금처럼 전환 버림으로 센다. F1 버튼은 레벨 다시 열기 암전 중에만 버린다(`:626`) |
| D4 | **상태 정보(영어)** | 창 위쪽에 영어로 보인다. ① 흐름 상태: `Getting ready` / `Mission k of N — <ID> <영어 미션>`(P5h의 `VBoRSpectator::MissionText`, `Spectator/VBoRSpectatorModel.h:29` — TV와 같은 문구) / `Free mode`(완료로 들어왔는지 `End`인지) ② 지금 단계가 기다리는 것: `Waiting for the rule` / `Operator confirmation needed` / `Waiting for participant confirmation` / `Confirm locked`(단계 시작 직후 잠금) ③ 정렬: `Applied` / `Needs check` / `No saved alignment` / `Alignment ignored`(`VBoRVehicleAlignmentComponent.h:14`, 러너 `VBoRScenarioRunner.h:64`) ④ 좌석: `Driver` / `Passenger` / `Rear` / `Switching…` ⑤ 센 값: 이전·재시도·건너뛰기·`End`·버림(지난 단계·권한·규칙·전환·잠금) ⑥ 마지막 명령 결과: `<명령> — accepted` 또는 `— discarded (<이유>)`. 이유는 D3 판정 함수가 돌려준 것을 영어로 쓴다. 지금 VR 안 진행자 줄·F1 정보(`GetOperatorText`, `:510-532`)는 한국어 그대로 둔다 |
| D5 | **창에 초점이 있어도 키가 된다** | 진행자가 창을 누르면 키보드 초점이 창으로 간다. 그때도 같은 키(§0-1)가 같은 명령을 내야 한다(창이 키를 받아 같은 명령으로 넘김). 게임 창에 초점이 있을 때는 지금 키 연결(`:125-137`) 그대로다. 한 번 누른 키가 두 번 들어가지 않게 한다(창과 게임이 같은 키를 동시에 받지 않음) |
| D6 | **새로 고침** | 버튼 흐림·상태 문구는 흐름이 바뀌면 바로, 적어도 초당 10회 다시 그린다(진행자가 누를 때 낡은 상태를 보지 않게) |

### 0-1. 버튼

| 묶음 | 버튼(영어) | 키 | 명령 |
|---|---|---|---|
| Flow | `Confirm` | Space | 확인 |
| Flow | `Next step` | → | 건너뛰기 |
| Flow | `Previous step` | ← | 이전 |
| Flow | `Restart from step 1` | F5 | 처음부터 |
| Flow | `Start without alignment` | O | 정렬 무시하고 시작 |
| Free mode | `Enter free mode` | End | 자유모드로 바로(P5g D2) |
| Free mode | `Reload level` | F6 | 레벨 다시 열기(P5g D8) |
| Seats | `Driver seat` | Home | 운전석 |
| Seats | `Passenger seat` | 1 | 동승석 |
| Seats | `Rear seat` | 2 | 2열 |
| View | `In-VR operator info` | F1 | VR 안 진행자 정보 켜고 끄기 |

`Reload level`처럼 되돌릴 수 없는 버튼은 다른 버튼과 떨어뜨리거나 색을 다르게 한다(실수 방지). 누르면 한 번 더 묻는 확인 창은 만들지 않는다 — P5g가 이미 자유모드에서만 받는다.

## 1. 왜 만드는가

- 진행자는 키 11개(Space·←·→·F5·O·End·F6·Home·1·2·F1)를 외워야 한다. 버튼 창이면 이름을 보고 누른다.
- 지금 진행자 정보는 VR 안 패널(F1)에만 있어, 헤드셋을 쓰지 않은 진행자는 PC 화면에서 상태를 볼 수 없다.

## 2. 범위

### 만들 것

**1. 진행자 창** — D1–D6, §0-1.
**2. 공통 판정 함수** — D3(`Dispatch`와 창이 같이 씀).
**3. 자동화 시험** — §4 전부.

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| 새 흐름 규칙·새 명령 | 버튼은 기존 명령만 낸다(D2) |
| 키보드 단축키 없애기·바꾸기 | 그대로 둔다 |
| VR 안 진행자 줄·F1 정보를 영어로 바꾸기 | D4 — 그대로 |
| 관전 화면 바꾸기 | P5h |
| 원격(다른 PC·태블릿) 조작, 웹 화면 | 요청 없음 |
| 확인 대화상자 | D2 표 아래 |
| 레벨·에셋 저장, UMG 에셋 | C++ Slate로 만든다 |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- 기존 `vBoR.*`가 모두 그대로 통과한다. 특히 P3 흐름(권한·확인 잠금·지난 단계 버림·전환 중 버림)과 P5g. 기존 단언은 바꾸지 않는다.
- 버튼과 키는 같은 명령이다. 버튼으로만 되는 동작은 없다.
- 진행자 출처로 기록한다(참가자 수행과 구분).
- 자동화·헤드셋 없는 실행에서도 경고·오류 없이 지나간다(창을 만들지 않음).

## 4. 경계 상황과 그 시험

| # | 상황 | 기대 동작 | 자동화 시험 |
|---|---|---|---|
| E1 | 자동화(`-nullrhi` — Slate는 켜져 있음)·`GIsAutomationTesting` | 창을 만들지 않음, 경고·오류 없음 | 상태·로그 |
| E2 | 버튼마다 누름 | 같은 키를 눌렀을 때와 같은 명령(종류·진행자 출처·토큰)이 들어감 | 명령 비교(창을 띄우지 않고 버튼 처리 함수를 부름) |
| E3 | 시작 전(정렬 대기 — 저장 정렬 없음·확인 필요) | `Start without alignment`·`In-VR operator info`만 누를 수 있음. 나머지 흐림 | 흐림 |
| E4 | 진행 중, 규칙 대기 / 규칙 만족·진행자 확인 필요 / 확인 잠금 중 / 참가자 확인 단계 | `Confirm` 흐림 / 켬 / 흐림 / 흐림 | 흐림 |
| E5 | 시점 전환 중(암전) | 좌석·흐름 버튼 흐림(지금 전환 중 버림 규칙) | 흐림 |
| E6 | 자유모드 | `Confirm`·`Next`·`Previous`·`Enter free mode` 흐림, 좌석·`Restart`·`Reload level` 켬 | 흐림 |
| E7 | 판정 일치 | 여러 상태에서 모든 버튼에 대해 "켜짐 ⇔ 지금 `Dispatch`가 받음" | 판정 함수와 실제 처리 비교 |
| E8 | 상태 문구 | 각 상태에서 D4 ①–⑥이 영어로 맞게 나옴. 버린 명령 뒤 `discarded (<reason>)` | 문구 |
| E9 | 같은 frame에 버튼과 키(같은 명령) | 지금 토큰 규칙대로 하나만 처리되고 나머지는 지난 단계로 버림(P3 E1과 같음) | 버림 수 |
| E10 | 창에 초점이 있을 때 키 | 같은 명령 한 번(두 번 들어가지 않음) | 명령 수 |
| E11 | 레벨 다시 열기 | 창이 닫혔다 같은 자리·크기로 다시 뜸 | 자리 기억(창 없이 저장값만) |
| E12 | 기존 시험 | 모두 통과. 기존 단언은 바꾸지 않는다 | 전체 시험 |
| E13 | 정렬 무시로 시작함(`O`) | 좌석 버튼 3개 흐림(정렬 컴포넌트가 거부 — 이유도 그렇게 나옴). 누르면 지금처럼 전환 버림으로 셈 | 흐림·판정 일치 |
| E14 | 레벨 다시 열기 암전 중 | 모든 버튼 흐림(F1 포함) | 흐림 |

## 5. 구현자가 정할 것

- 창의 배치(묶음 순서·버튼 크기·색), 상태 문구의 정확한 영어 표현.
- 판정 함수의 모양(예: `CanAccept(Command) → (bool, Reason)`). `Dispatch`가 먼저 이것을 부르고, 받을 때만 지금처럼 처리한다.
- 창의 자리·크기를 기억하는 방법(같은 프로세스 안 정적 값 또는 사용자 설정 ini). 다른 세션까지 기억하는 것은 선택.
- 창을 러너가 가질지, 따로 둘지(러너의 명령·상태를 읽으면 됨).

## 6. 완료 조건

1. §2의 1–3이 들어 있다.
2. §4 시험이 모두 자동화로 통과한다. 기존 `vBoR.*`도 통과한다.
3. non-unity(`-Module=vBoR`)·unity 빌드에서 vBoR 경고·오류가 0이다. 전체 시험은 최종 트리로 한 번 돌린다.
4. AGENTS.md 보고 ①–⑤를 커밋 본문과 `HANDOFF.md`에 남긴다.
5. 시험 상태는 `not-tested`다.

## 7. 로컬에서 확인할 것 — 사용자

### PC + TV + Quest 3 (`L_P3_Flow`)
1. Windows 디스플레이를 **확장**으로 두고, 게임 창을 TV에, `vBoR Operator` 창을 PC 모니터에 둔다(복제면 TV에도 진행자 창이 보임).
2. 버튼으로 한 차례 진행해 본다: 시작 → 확인·건너뛰기 → 좌석 → 자유모드 → 레벨 다시 열기. 흐린 버튼이 상태와 맞는지 본다.
3. 창을 누른 뒤에도 키보드 단축키가 되는지 본다.
4. 레벨을 다시 연 뒤 창이 같은 자리에 다시 뜨는지 본다.
5. VR Preview(PIE)에서도 창이 뜨는지, Play를 멈추면 닫히는지 본다.

## 8. 확인한 API 근거

경로: `Source/vBoR/` 아래(`main` @ P5h 머지 `db977a1`), 엔진은 `C:/Program Files/Epic Games/UE_5.7/Engine/Source/` 아래.

| 쓸 것 | 위치 | 쓰임 |
|---|---|---|
| 창 띄우기 | 엔진 `Runtime/Slate/Public/Framework/Application/SlateApplication.h:1576`(`AddWindow`), `Runtime/SlateCore/Public/Widgets/SWindow.h:287`(`Title`), `:297`·`:300`(`ScreenPosition`·`ClientSize`), `:339`(`HasCloseButton`), `:498`·`:501`(지금 자리·크기) | D1 |
| 창을 만들지 않는 조건 | 엔진 `Runtime/Core/Public/Misc/App.h:411-418`(`CanEverRender`), `Runtime/Core/Public/CoreGlobals.h:623`(`GIsAutomationTesting`) | D1·E1 |
| 키 연결·키 → 명령 | `Interaction/VBoRScenarioRunner.cpp:125-137`(키), `:296-299`(`QueueLive`), `:614-629`(키 처리, F1) | D2·D5 |
| 명령 받기·버리기 조건 | `:338-428`(`Dispatch`), `:215`·`:235`(`RuleSatisfied`·`NeedsConfirmation`, const) | D3 |
| 좌석 전환 거부 조건 | `Interaction/VBoRVehicleAlignmentComponent.cpp:89-104`(`SwitchViewpoint`·`RequestSwitch`) | D3·E13 |
| 지금 진행자 문구 | `:510-532`(`GetOperatorText`) | D4 |
| 영어 미션 문구 | `Spectator/VBoRSpectatorModel.h:29`(`VBoRSpectator::MissionText`) | D4 |
| 정렬 상태·좌석 | `Interaction/VBoRVehicleAlignmentComponent.h:14`(상태), `:23-27`, `:45`, 러너 `VBoRScenarioRunner.h:51-57`(읽기 함수), `:64`(정렬 무시) | D4 |
| 모듈 | `vBoR.Build.cs:13`(Slate·SlateCore 있음) | D1 |

## 개정 이력

| 날짜 | 개정 | 내용 |
|---|---|---|
| 2026-10-16 | 처음 | 작성(사용자 요청·문답: 따로 뜨는 창, 영어, 버튼 + 상태 정보) |
| 2026-10-16 | 구현 전 개정 | P5h 머지(`db977a1`) 뒤 베이스·줄 번호. D1 창을 만들지 않는 조건(`-nullrhi`에서도 Slate가 켜져 있어 `IsInitialized`로는 못 막음), 자리 기억 API. D2 F1 버튼은 F1 처리 함수. D3 `switch` 안 조건과 좌석의 정렬 컴포넌트 거부 조건도 같은 판정에(정렬 무시로 시작하면 좌석 전환이 안 됨). D4 영어 미션은 P5h 함수. E13·E14 추가 |
