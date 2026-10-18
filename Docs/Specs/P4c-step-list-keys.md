# P4c 단계 목록 19개 + 01·02 키 규칙

| 항목 | 값 |
|---|---|
| 단계 | P4 50% 항목 통합의 첫째(U-15: P4 골격을 셋으로 나눔). 원본 ID 19개를 시나리오 에셋에 모두 넣고, 01·02를 키로 판정한다. 시험 T-06·T-10 |
| 결정 근거 | U-10(01·02는 접근 거리가 아니라 키), P4a D8(생성자 기본 단계는 바꾸지 않음, 01·02는 에셋에서 고침), **U-15(사용자 2026-10-18):** P4 골격을 셋으로 나눔 — 이 명세 → P4d 차 뒤 구간(03–06) → P4e 시트 폴딩(16). 05는 진행자 확인만, 16은 진행자 창의 접기·펴기 버튼, 단계 목록은 Codex가 MCP로 에셋을 고침. P3b 리뷰 참고 1(시작 기록) |
| 베이스 브랜치 | `main` @ P2e 머지 뒤(`36bee02` + 이 명세 커밋) |
| 작업 브랜치 | `codex/P4c-step-list-keys` |
| 리뷰 파일 | `Docs/Reviews/P4c-step-list-keys.md` (첫 리뷰 때 생김) |
| 상태 | 명세 — 구현 전 |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **단계 규칙 둘을 더한다** | `EVBoRStepRule`(`Interaction/VBoRScenarioAsset.h:8`) **끝에** `KeyHeld`, `KeyUnlockAndLock`을 붙인다. 기존 값의 이름·순서는 그대로다. 대상은 `TargetTag`가 하나만 가리키는 `AVBoRKeyFob` 액터다(태그 찾기 `Interaction/VBoRScenarioRunner.cpp:52-64`, 액터로 바꾸기 `:204-216` — 다른 규칙과 같은 꼴). 대상이 키가 아니면 지금처럼 "종류 불일치"로 시작을 막는다(`:216`).<br>**`KeyHeld`:** 대상 키를 지금 쥐고 있다(`AVBoRKeyFob::GetHoldingHand() != INDEX_NONE`, `VBoRKeyFob.h:51`).<br>**`KeyUnlockAndLock`:** 단계에 들어온 뒤 대상 키의 `OnUnlockPressed`와 `OnLockPressed`(`VBoRKeyFob.h:64-65`)가 각각 한 번 이상 불렸다. 순서는 상관없다. 두 이벤트는 쥐지 않은 손의 누름만 낸다(`VBoRKeyFob.cpp:151-170`). 버튼의 `PressFilter`가 쥔 손의 누름을 먼저 막기 때문이다(`VBoRPokeButton.cpp:119`). 그래서 흐름은 거르지 않고 세기만 한다.<br>**세는 법:** `ButtonPressed`(`:246-249`, 기준값 `:317`)와 같은 꼴이다. `InitializeFlow`에서 묶고, 다시 초기화할 때(`:190-192`)와 `EndPlay`(`:172-173`)에서 푼다. 기준값은 `EnterStep`에서 정한다 |
| D2 | **근사 차 키에 액터 태그를 단다** | 앞 키 `Car.FrontKey`, 뒤 키 `Car.RearKey`를 `AVBoRKeyFob` 액터에 단다(`P3Probe/P3VehicleMock.cpp:101-102`, `:246-260`). 버튼 태그(`Car.FrontKey.Unlock` 등)는 그대로다. 이름이 달라서 `FindP3Tag`는 둘을 따로 찾는다 |
| D3 | **시작 기록 정리(P3b 리뷰 참고 1 권장안)** | 지금은 `EnterStep(0)`에 들어올 때마다 시작 기록을 다시 기다린다(`:310`). 또 01을 떠나거나 01에서 다시 01로 들어갈 때 "시작 표시를 거치지 않음"을 남긴다(`:307`). R도 그 줄을 남긴다(`:441-452`의 `RecordStartFallback`).<br>**바꾼다:**<br>- 다시 기다리는 때는 **흐름 시작**(`InitializeFlow`, 정렬 무시 O `:409`, 기다리던 흐름을 정렬 다시 적용 A로 시작 `VBoRScenarioOperator.cpp:189` — 1회차 리뷰 R1-1)과 **R 뒤 01 시작**뿐이다. R을 기준 시점에서 누른 경우와 운전석으로 돌아온 뒤(`:394-395`) 둘 다다.<br>- **←로 01에 들어올 때**(02 → 01, 01에서 ←)는 기록 상태를 바꾸지 않는다. 다시 기다리지 않고, "거치지 않음"도 남기지 않는다.<br>- **R 순간**에는 "거치지 않음"을 남기지 않는다. 그 시도는 끝났기 때문이다.<br>- 01을 떠날 때(→·규칙·확인·End) 아직 기다리는 중이면 "거치지 않음"을 한 번 남긴다. 지금과 같다(`:307`, `:325`, `RecordStartFallback` `:691-698`).<br>- 시작 표시 위에서 기록하는 조건(`:514`)은 그대로다 |
| D4 | **에셋 단계 목록 — Codex가 MCP로 고친다** | `/Game/P3/DA_P3_Scenario`(`Content/P3/DA_P3_Scenario.uasset`)의 `Steps`를 §0-1 표로 바꾼다. 생성자 기본 단계(`Interaction/VBoRScenarioAsset.cpp:10-25`)는 바꾸지 않는다(P4a D8 — 시험이 생성자 기본값을 쓰고, 손대지 않은 에셋이 같이 바뀜).<br>- **먼저 읽는다.** 지금 `Steps`가 생성자 기본값(14개: 01·02·07–15·18–20)과 같은지 본다. 다르면 사용자가 고친 것이다. 쓰지 말고 리뷰 파일 "질문"에 지금 내용을 적고 멈춘다.<br>- **고치는 방법:** MCP의 에셋 속성 기능을 먼저 찾는다(`search` → `describe`). 구조체 배열을 고치는 기능이 없으면 MCP Python으로 **이 에셋 하나의 `Steps`만** 고쳐도 된다. 다른 에셋·레벨·설정은 건드리지 않는다. 둘 다 안 되면 질문으로 멈춘다.<br>- 순서는 Agent-Workflow §2.10이다. C++ 빌드를 먼저 통과시키고, 에디터를 열어 고친 뒤 저장하고 닫는다. 전체 시험은 그 뒤 한 번 돌린다(§4 E9·E10이 에셋을 읽음) |
| D5 | **01·02의 TV 문구는 에셋에 넣는다** | ID 기본표(`Spectator/VBoRSpectatorModel.cpp:8-16`)의 01·02는 옛 접근 문구다. 그런데 P5h 시험이 그 문구를 단언한다(`P3Probe/P5hSpectatorTests.cpp:176`, `:181`). 그래서 기본표는 그대로 두고, 에셋의 `SpectatorText`(`VBoRScenarioAsset.h:18`)에 새 문구를 넣는다 |
| D6 | **03·04·06·16은 자리 단계다** | 이번에는 `Confirm` + 진행자 확인으로 둔다. 실제 규칙은 P4d(03–06)·P4e(16)가 바꾼다. **05는 끝까지 진행자 확인이다**(U-15) |

### 0-1. 단계 표 (에셋에 넣을 값)

빈칸은 구조체 기본값이다(`VBoRScenarioAsset.h:16-26`: `DoorOpenDeg` 45, `SeatedSeconds` 1.5, `bRequireBaseViewpoint` 끔, 태그 없음). 07–15·18–20의 규칙·태그·확인은 생성자 기본값(`VBoRScenarioAsset.cpp:13-25`)과 같다. 07의 `RouteTag`만 새로 넣는다(P3b 명세 §7-1, 지금은 사용자 할 일 — 이 작업으로 옮김).

| ID | 안내 `Instruction` | TV `SpectatorText` | `Rule` | `TargetTag` | `ConfirmBy` | 그 밖 |
|---|---|---|---|---|---|---|
| 01 | 테이블의 키를 집으세요. | Pick up the key from the table. | `KeyHeld` | `Car.FrontKey` | None | `MarkerTag` `Car.FrontKey` |
| 02 | 키 버튼으로 외장 조명을 켜고 꺼 보세요. | Turn the exterior lights on and off with the key. | `KeyUnlockAndLock` | `Car.FrontKey` | Either | |
| 03 | 차량 주변을 둘러보세요. | | `Confirm` | | Either | 경로는 P4d |
| 04 | 리프트게이트를 열어보세요. | | `Confirm` | | Operator | 자리(P4d) |
| 05 | 박스를 적재해 보세요. | | `Confirm` | | Operator | U-15 |
| 06 | 리프트게이트를 닫아보세요. | | `Confirm` | | Operator | 자리(P4d) |
| 07 | 도어 핸들을 조작해 보세요. | | `DoorUnlocked` | `Car.DriverDoor` | None | `RouteTag` `Route.ToDriverDoor` |
| 08 | 문을 열어보세요. | | `DoorOpened` | `Car.DriverDoor` | None | |
| 09 | 차량에 탑승해 보세요. | | `Confirm` | | Operator | |
| 10 | 좌석에 앉아보세요. | | `Seated` | `Car.DriverSeat` | Operator | |
| 11 | 문을 닫아보세요. | | `DoorLatched` | `Car.DriverDoor` | None | |
| 12 | 실내를 둘러보세요. | | `Confirm` | | Either | |
| 13 | 가장 먼 버튼까지 손가락을 뻗어보세요. | | `ButtonPressed` | `Car.ReachButton` | None | |
| 14 | 동승석 위치에서 확인하세요. | | `Viewpoint` | `Car.PassengerView` | Either | |
| 15 | 2열 위치에서 확인하세요. | | `Viewpoint` | `Car.RearView` | Either | |
| 16 | 시트 폴딩 각도를 확인하세요. | | `Confirm` | | Operator | 자리(P4e) |
| 18 | 하차를 위해 문을 열어보세요. | | `DoorOpened` | `Car.DriverDoor` | None | `bRequireBaseViewpoint` 켬 |
| 19 | 차량에서 내려보세요. | | `Confirm` | | Operator | |
| 20 | 문을 닫아보세요. | | `DoorLatched` | `Car.DriverDoor` | None | |

안내 문구는 계획서 §4 표의 한국어 초안이다. 사용자가 나중에 에셋에서 고친다.

## 1. 왜 만드는가

- 원본 ID 19개 중 5개(03–06, 16)가 단계 목록에 없다. 진행자 창·TV의 진행 막대도 14개로 나온다.
- 01·02는 U-10 전의 접근 규칙(`Approach`·확인)이다. 키와 테이블(P4a)은 이미 있지만 흐름이 키를 보지 않는다.
- 01이 키 집기로 바뀌면서 시작 기록의 빈틈(P3b 리뷰 참고 1)을 같이 정리한다.

## 2. 범위

### 만들 것

1. **흐름**(`AVBoRScenarioRunner`): D1 규칙 둘, D3 시작 기록.
2. **근사 차**(`AP3VehicleMock`): D2 키 태그.
3. **에셋**: D4·D5 — §0-1 표(MCP).
4. **자동화 시험**: §4 전부.

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| 생성자 기본 단계 바꾸기 | P4a D8. 시험이 생성자 기본값(14개)을 쓴다 |
| TV ID 기본표 바꾸기 | D5. P5h 단언 |
| 03–06·16의 실제 규칙, 트렁크·리프트게이트·박스를 근사 차에 넣기, 07 경로 시작점 옮기기, "영역 모름"일 때 다시 읽기 | P4d·P4e(U-15) |
| R·F6에서 키를 테이블로 되돌리기 | R은 지금처럼 문만 되돌린다. F6은 레벨을 다시 연다(P5g) |
| 웰컴 라이트 | 사용자 몫(U-10) |
| `L_P3_Flow` 레벨, 다른 에셋 고치기 | D4는 `DA_P3_Scenario` 하나만 |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- 기존 규칙 8개의 판정은 그대로다. enum의 기존 값 이름·순서도 그대로다(끝에 붙임 — 저장된 에셋과 시험이 이름으로 읽음).
- 생성자 기본 단계와 TV 기본표는 그대로다. 그래서 기존 P3·P3b·P5g·P5h·P5i 시험의 단언도 그대로다.
- 키 동작(P4a: 쥐기·버튼·던지기·테이블로 돌아가기)과 버튼 태그는 그대로다.
- 시작 기록은 처음 지나는 경우가 지금과 같다: 흐름 시작 → 01 동안 시작 표시 위에서 기록 → 못 지났으면 01을 떠날 때 "거치지 않음" 한 번(`vBoR.P3b.E13StartMarkerRecords`, `P3Probe/P3FlowTests.cpp:806-825`).
- 한 frame에 규칙으로 여러 단계를 넘기지 않는다(`:397` — 들어간 frame에는 완료하지 않음).
- 기존 단언은 바꾸지 않는다.

## 4. 경계 상황과 그 시험

| # | 상황 | 기대 동작 | 자동화 시험 |
|---|---|---|---|
| E1 | 01(`KeyHeld`): 키를 안 쥠 → 쥠 | 안 쥔 동안 완료 안 됨. 쥔 다음 처리 frame에 "참가자 규칙"으로 끝나고 02 시작 | 규칙 |
| E2 | 흐름 시작 전부터 키를 쥐고 있음 | 01은 들어간 frame에는 안 끝나고 다음 frame에 끝남 | 규칙 |
| E3 | 02에서 ←(키를 쥔 채) | 01에 들어가고 다음 frame에 다시 02로. 02의 기준값이 새로 잡혀 01 전의 누름은 세지 않음 | 규칙·흐름 |
| E4 | 02(`KeyUnlockAndLock`): 열림만 / 잠금만 / 열림 → 잠금 / 잠금 → 열림 | 둘 다 불리기 전에는 규칙 대기. 둘 다 불리면 확인 대기(참가자 확인 버튼 보임, Space 완료) | 규칙·확인 |
| E5 | 02에 들어오기 전(01)에 누름, 뒤 키(`Car.RearKey`)의 누름 | 세지 않음 | 규칙 |
| E6 | `InitializeFlow`를 두 번 부름, Runner `EndPlay` | 이벤트가 두 번 묶이지 않음(한 번 눌러 한 번 셈), `EndPlay` 뒤 키 이벤트가 Runner에 안 감 | 묶기 |
| E7 | `KeyHeld`·`KeyUnlockAndLock`의 태그가 키가 아닌 대상(`Car.DriverDoor`) / 없는 태그 | 시작 차단, "종류 불일치" / "0개 발견" | 검사 |
| E8 | 시작 기록: ① 02 → ← → 01을 →로 떠남 ② 01에서 ← ③ 01 중(표시 안 지남) 기준 시점에서 R ④ 01 중 동승석에서 R → 운전석 복귀 | ① 첫 01을 떠날 때의 "거치지 않음" 한 번만, 다시 떠날 때 없음 ② 기록 상태 그대로 ③ R 순간 "거치지 않음" 없음, R 뒤 시작 표시에 서면 "시작 위치" 기록 ④ R 순간 없음, 복귀 뒤 01에서 다시 기다림 | 기록 |
| E9 | 에셋 `/Game/P3/DA_P3_Scenario` 읽기 | 19개, ID 순서 01–16·18–20, 각 줄이 §0-1 표와 같음(규칙·태그·확인·`MarkerTag`·`RouteTag`·`bRequireBaseViewpoint`·01·02 `SpectatorText`) | 에셋 |
| E10 | 근사 차 + 정렬 + Runner에 이 에셋 | `ValidationProblems` 비어 있음(19개 대상·표시·경로 모두 찾음), 시작하면 01. TV 진행 막대 전체 19 | 에셋·흐름 |
| E11 | 근사 차 키 태그 | `Car.FrontKey`·`Car.RearKey` 각각 액터 하나, 버튼 태그 4개 그대로 | 근사 차 |
| E12 | 생성자 기본값 | `UVBoRScenarioAsset` 새 객체의 단계는 지금처럼 14개(01·02·07–15·18–20), 01은 `Approach` | 기본값 |
| E13 | 기존 시험 | 모두 그대로 통과(§3) | 전체 시험 |

## 5. 구현자가 정할 것

- 키 이벤트를 세는 자료 구조(`ButtonCounts`와 같은 map 또는 키별 구조체).
- D3을 코드로 나누는 방법(예: `EnterStep`에 "다시 기다림" 인자, R 경로에서만 켬).
- MCP로 에셋을 고치는 capability(D4 범위 안).
- 시험에서 키를 쥔 상태·이벤트를 만드는 방법(시험용 friend, `Broadcast` 직접 호출 등).

## 6. 완료 조건

1. §2의 1–4가 들어 있다.
2. §4 시험이 모두 자동화로 통과한다. 기존 `vBoR.*`도 통과한다.
3. non-unity(`-Module=vBoR`)·unity 빌드에서 vBoR 경고·오류가 0이다. 전체 시험은 에셋을 고친 뒤 최종 트리로 한 번 돌린다.
4. AGENTS.md 보고 ①–⑤를 커밋 본문과 `HANDOFF.md`에 남긴다. ①에 `DA_P3_Scenario.uasset`, ③에 쓴 MCP capability, ⑤에 고치기 전 에셋 내용(생성자 기본값과 같았는지)을 적는다.
5. 시험 상태는 `not-tested`다.

## 7. 로컬에서 확인할 것 — 사용자(에디터·VR, 끝에 한꺼번에)

1. 에디터에서 `DA_P3_Scenario`를 열어 19개 단계와 문구를 본다. 문구는 여기서 고치면 된다.
2. VR: 01에서 앞 테이블 키를 집으면 02로 넘어가는지 본다. 02에서 열림·잠금을 한 번씩 누르면 확인 버튼이 뜨는지 본다.
3. 03–06·16은 진행자 Space로 넘긴다(다음 두 명세 전까지).
4. TV 띠의 01·02 영어 문구와 진행 막대(19)를 본다.

## 8. 확인한 API 근거

경로: `Source/vBoR/` 아래(`main` @ `36bee02` 줄 번호).

| 쓸 것 | 위치 | 쓰임 |
|---|---|---|
| 단계 규칙 enum·구조체 | `Interaction/VBoRScenarioAsset.h:8`, `:12-27` | D1·D4 |
| 생성자 기본 단계 | `Interaction/VBoRScenarioAsset.cpp:3-26` | D4·E12 |
| 태그 찾기(하나만) | `Interaction/VBoRScenarioRunner.cpp:52-64` | D1·D2 |
| 대상 바꾸기·검사 | `Interaction/VBoRScenarioRunner.cpp:204-216` | D1·E7 |
| 버튼 세기·묶기·풀기 | `Interaction/VBoRScenarioRunner.cpp:190-192`, `:246-249`, `:172-173`, `:703-709`, 기준값 `:317` | D1 |
| 규칙 판정·확인 | `Interaction/VBoRScenarioRunner.cpp:273-302` | D1·E4 |
| 단계 들어가기·시작 기록 | `Interaction/VBoRScenarioRunner.cpp:305-321`, `:323-325`, `:514`, `:691-698` | D3 |
| ←·R·R 뒤 복귀 | `Interaction/VBoRScenarioRunner.cpp:440-452`, `:394-395` | D3 |
| 한 frame에 한 단계 | `Interaction/VBoRScenarioRunner.cpp:397` | §3·E2 |
| 키 쥔 손·이벤트 | `Interaction/VBoRKeyFob.h:51`, `:64-65`, `Interaction/VBoRKeyFob.cpp:151-170` | D1 |
| 쥔 손 누름 막기 | `Interaction/VBoRPokeButton.cpp:119` | D1 |
| 근사 차 키 테이블·태그 | `P3Probe/P3VehicleMock.cpp:101-102`, `:246-260` | D2 |
| TV 기본표·P5h 단언 | `Spectator/VBoRSpectatorModel.cpp:4-23`, `P3Probe/P5hSpectatorTests.cpp:176`, `:181` | D5 |
| 시작 기록 시험 | `P3Probe/P3FlowTests.cpp:806-825` | §3 |
| 07 경로 태그 | `P3Probe/P3VehicleMock.cpp:103-106`(`Route.ToDriverDoor`) | §0-1 |
| 에디터 MCP 순서·금지 | `Docs/Agent-Workflow.md` §2.10 | D4 |

## 개정 이력

| 날짜 | 개정 | 내용 |
|---|---|---|
| 2026-10-18 | 처음 | 작성(U-15: P4 골격을 셋으로 나눔, 05 진행자 확인만, 16 진행자 창 버튼, 단계 목록은 Codex가 MCP로 에셋 수정) |
| 2026-10-18 | 구현 중 Q1 답 | 생성자 기본 단계 수 13 → **14**로 정정(D4·§1·§2·E12). Claude가 잘못 셌음 — 명세 문제. 에셋은 기존 14개를 §0-1 표의 19개로 바꾼다(03–06·16을 더하고 01·02·07을 고침) |
| 2026-10-18 | 1회차 리뷰 | D3 흐름 시작 경로에 A(정렬 다시 적용으로 시작)를 더함. 처음에 빠뜨림 — Claude 명세 문제(R1-1) |
