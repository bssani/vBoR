# P5zb 목표 표시 고리 — 지면 높이 따라가기

| 항목 | 값 |
|---|---|
| 단계 | P5 리허설 준비 — 회사 CAD 넣기 전 정리(사용자 결정 2026-10-25). 시험 T-01(데이터·Buck 정렬)·T-06(순서 안내) 보강 |
| 결정 근거 | **사용자(2026-10-25):** 회사 CAD는 타이어 바닥이 Z 0에 있지 않다. Blueprint-Guide §2-2의 **B안**(CAD를 온 자리 그대로 두고 `FloorWorldZCm`에 지면 높이를 넣음)으로 간다.<br>**Claude 확인:** 바닥에 놓는 것 중 목표 표시 고리만 월드 Z 0.5 cm에 박혀 있다(`Interaction/VBoRScenarioRunner.cpp:758`). 정렬(`VBoRBuckAlignment.cpp:33`), 박스 착지(`VBoRBoxStand.cpp:61`), 키 복귀(`VBoRKeyFob.cpp:202`), 플레이 영역 검사(XY만, `VBoRScenarioRunner.cpp:770-785`)는 이미 Actor 위치나 설정을 읽는다 |
| 베이스 브랜치 | `main` @ 이 명세 머지 뒤 |
| 작업 브랜치 | `codex/P5zb-floor-marker-height` |
| 리뷰 파일 | `Docs/Reviews/P5zb-floor-marker-height.md` (첫 리뷰 때 생김) |
| 상태 | 구현 리뷰 통과(1회차) |
| 에디터 작업 | **없음** |
| 앞 명세 | [P5za-door-coast.md](P5za-door-coast.md) |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **고리 높이 = 정렬 컴포넌트의 지면 높이 + 0.5 cm** | `AVBoRScenarioRunner`의 목표 표시 갱신(`VBoRScenarioRunner.cpp:749-760`)에서 고리 네 조각의 월드 Z를 `FloorWorldZCm + 0.5`로 둔다. `FloorWorldZCm`은 러너가 이미 찾아 둔 `Alignment`(`VBoRScenarioRunner.h:97`, 찾는 곳 `.cpp:221-226`)의 값이다(`VBoRVehicleAlignmentComponent.h:68`).<br>- `Alignment`이 없으면 0을 쓴다(지금과 같음). 흐름은 정렬 컴포넌트 없이 시작하지 않으므로 실제로는 늘 있다.<br>- `bHeightFromFloor`와 상관없이 `FloorWorldZCm`을 쓴다. 4점 정렬도 이 값을 그대로 쓴다(`VBoRPointAlignment.cpp:52`).<br>- 고리의 XY(목표 아래 ±30 cm), 회전, 크기, 숨김 규칙은 그대로 |
| D2 | **매 갱신마다 읽는다** | 값을 BeginPlay에 한 번 담아 두지 않는다. PIE 중 `FloorWorldZCm`을 바꾸면 다음 갱신에 고리가 따라간다(지면 높이를 맞추는 중에 보기 쉽게) |
| D3 | **0.5는 이름 있는 상수로** | `0.5`가 "바닥과 깊이 싸움을 피하는 띄움(cm)"임을 이름과 짧은 영어 주석으로 둔다. 값은 바꾸지 않는다 |

## 1. 왜 하는가

지면이 0이 아닌 회사 CAD에서 고리가 바닥 위에 떠 있거나 바닥 밑에 묻혀 보이지 않는다. 고리는 화살표와 함께 참가자에게 "여기로"를 알리는 표시다. 화살표(목표 +160 cm, `:745`·`:754`)는 목표 위치 기준이라 이미 맞다.

## 2. 범위

### 만들 것

- D1–D3.
- 시험 `vBoR.P5zb.*`(§4).

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| 화살표 높이, 바닥 경로(`AVBoRFloorRoute`) 그리기 | 이미 Actor·목표 기준이다. 경로 Actor는 사용자가 지면 높이에 놓는다(Blueprint-Guide §2-2) |
| 키 테이블·박스 받침·시작 표시의 자동 높이 맞춤 | 사용자가 레벨에서 놓는다(Blueprint-Guide §2-2). 코드로 옮기지 않는다 |
| 바닥을 trace로 찾기 | 지면 높이의 기준은 `FloorWorldZCm` 하나다(U-17) |
| `FloorWorldZCm`의 `EditCondition` 등 정렬 컴포넌트 설정 변경 | 범위 밖 |
| `L_Main`·`BP_Car_Core` 값 변경 | 근사 차는 지면 0이다. 회사에서 사용자가 넣는다 |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- **지면 0(지금 모든 레벨·시험)에서 결과가 같다:** 고리 Z 0.5, XY·회전·크기·숨김 그대로.
- **기존 시험 감사:** 고리를 보는 기존 시험은 숨김 상태만 본다 — `P3Probe/P3FlowTests.cpp:642`, `:650`, `:712`, `:739`, `:771`, `:1459`, `:1464`, `P3Probe/P5gFreeModeTests.cpp:32`. 고리의 위치를 보는 시험은 없다(`MarkerRing` 전체 검색, Claude 2026-10-26). **기존 시험 파일은 고치지 않는다.** 하나라도 고쳐야 통과하면 멈추고 리뷰 파일 "질문"에 적는다.

## 4. 시험 — `vBoR.P5zb.*`

`P3FlowTests.cpp`의 흐름 시험 world와 `FP3bTestAccess::Ring` 같은 방식으로 고리를 읽는다(새 파일이면 같은 접근 구조체를 둔다 — `friend` 선언이 필요하면 헤더에 한 줄 더하는 것은 허용). 경로가 없는 단계(목표 표시가 보이는 단계)에서 본다.

| ID | 상황 | 확인 |
|---|---|---|
| E1 | **지면 0** — 기본 `FloorWorldZCm` 0, 목표 표시가 보이는 단계 | 고리 네 조각 월드 Z = 0.5(±0.01). XY는 목표 XY ±30 cm(지금과 같음) |
| E2 | **지면 37 cm** — 흐름 시작 전 정렬 컴포넌트의 `FloorWorldZCm` = 37 | 고리 Z = 37.5(±0.01). XY는 E1과 같은 규칙 |
| E3 | **음수 지면** — `FloorWorldZCm` = −12.5 | 고리 Z = −12.0(±0.01) |
| E4 | **실행 중 바꿈** — E1 상태에서 `FloorWorldZCm` = 20으로 바꾼 뒤 한 Tick | 고리 Z = 20.5 |
| E5 | **화살표 그대로** — E2에서 화살표(`MarkerArrow`) 위치 | 목표 위치 + (0,0,160) — 지면 값과 상관없음 |

- 수치는 위를 기준으로 하되, 구현자가 같은 상황을 만드는 다른 값을 쓰면 보고 ③에 적는다.

## 5. 구현자가 정할 것

- 상수 이름, 시험을 새 파일로 둘지 `P3FlowTests.cpp` 옆 새 파일로 둘지(기존 파일 수정은 안 됨).

## 6. 완료 조건

1. D1–D3 구현, `vBoR.P5zb.*` E1–E5 통과.
2. `vBoR.*` 전체 통과, 기존 시험은 고치지 않고 결과 그대로(개수 = 기존 + P5zb 시험 수).
3. non-unity(`-Module=vBoR`)·unity 빌드 성공, vBoR 경고 0.
4. 주석은 영어 짧은 조각문(AGENTS.md).
5. 커밋 본문 보고 ①–⑤.

## 7. 로컬에서 확인할 것

- **빌드·시험:** 끝에 한 번(AGENTS.md 토큰 규칙).
- **에디터:** 없음.
- **회사 PC(T-01·T-06, `not-tested`):** `FloorWorldZCm`을 지면 높이로 넣은 뒤 PIE·Quest 3에서 목표 고리가 바닥에 붙어 보이는지 본다.

## 개정 이력

| 날짜 | 내용 |
|---|---|
| 2026-10-26 | 처음 작성 |
