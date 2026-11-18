# R15 좌석 기준점 태그 이름 바꾸기, 2열 3자리

| 항목 | 값 |
|---|---|
| 결정 근거 | 사용자 2026-11-18. 좌석 기준점은 SgRP다. 태그를 `Car.DriverSgRP`·`Car.PassengerSgRP`·`Car.RearLeftSgRP`·`Car.RearCenterSgRP`·`Car.RearRightSgRP`로 바꾸고, 진행자 창에 2열 세 자리 버튼을 둔다 |
| 베이스 / 작업 브랜치 | `main` / `codex/R15-seat-sgrp-tags` |
| 에디터 작업 | 없음. `BP_Car_Core`·`DA_Main_Scenario`의 태그는 사용자가 바꾼다 |

## 지금

- 태그 문자열이 박힌 곳:
  - `VBoRPointAlignment.cpp:42-43` (운전석 기준점)
  - `VBoRScenarioRunner.cpp:487` (진행자 좌석 키), `:620`, `:1036-1037` (버튼 글자)
  - `VBoRVehicleAlignmentComponent.cpp:227`, `:295` (좌석 이름)
  - `VBoRScenarioAsset.cpp:42-43` (기본 시나리오 14·15)
- 진행자 좌석 키: `Home` 운전석, `1` 동승석, `2` 2열(`VBoROperatorPanel.cpp:24-26`). 명령은 `EVBoRFlowCommand::Passenger`·`Rear`, 대상은 `SeatViews[2]`(`VBoRScenarioOperator.cpp:72`).
- 옮기는 양은 기준점끼리 위치 차이뿐이다(`VBoRVehicleAlignmentComponent.cpp:54`). 이것은 바꾸지 않는다.

## 할 일

- **태그 다섯 개**를 새 이름으로 쓴다.

  | 자리 | 새 태그 | 옛 태그(계속 읽음) |
  |---|---|---|
  | 운전석 | `Car.DriverSgRP` | `Car.DriverView` |
  | 동승석 | `Car.PassengerSgRP` | `Car.PassengerView` |
  | 2열 왼쪽 | `Car.RearLeftSgRP` | `Car.RearView` |
  | 2열 가운데 | `Car.RearCenterSgRP` | 없음 |
  | 2열 오른쪽 | `Car.RearRightSgRP` | 없음 |

  - 옛 태그도 같은 자리로 읽는다. 저장된 BP·시나리오가 깨지지 않게 하기 위해서다. 태그 표는 한 곳에 둔다.
  - Check setup은 옛 태그를 찾으면 **알림** 한 줄을 띄운다. 시작은 막지 않는다. 예: "Car.DriverView: rename to Car.DriverSgRP".
  - 같은 자리에 새 태그와 옛 태그가 둘 다 있으면 지금의 중복 규칙(하나만)을 그대로 쓴다.
- **기본 시나리오(`UVBoRScenarioAsset` 기본값) 14·15**를 새 태그로 바꾼다. 15는 `Car.RearLeftSgRP`.
- **진행자 좌석 키:** `Home` 운전석, `1` 동승석, `2` 2열 왼쪽, `3` 2열 가운데, `4` 2열 오른쪽. 진행자 창의 좌석 버튼도 다섯 개로 늘린다. 배치는 지금 좌석 버튼 줄을 따른다.
- **글자:**
  - 좌석 이름: Driver / Passenger / Rear left / Rear center / Rear right.
  - 참가자 버튼: "Move to passenger seat" / "Move to rear left seat" / "Move to rear center seat" / "Move to rear right seat".
  - 태그가 없는 자리의 키를 누르면 지금처럼 거부한다.
- `Docs/Blueprint-Guide.md`의 좌석점 표(130·138·369줄 근처)를 새 태그로 고친다.

## 하지 말 것

- 옮기는 계산, 정렬, 좌석 단계 규칙 바꾸기.
- BP·시나리오 에셋 수정.
- 기존 시험 단언 수정. 단 진행자 버튼 개수나 키 목록을 세는 단언이 있으면 새 개수로만 고친다(보고 ③에 목록).

## 시험 `vBoR.R15.*`

- E1: 새 태그 다섯 개 → `Home`·`1`–`4`가 각 자리로 옮긴다. 이동량 = 그 기준점 − 운전석 기준점.
- E2: 옛 태그(`Car.DriverView`·`Car.PassengerView`·`Car.RearView`)만 있음 → 지금처럼 동작하고, Check setup 알림 세 줄이 나온다.
- E3: 좌석 이름과 참가자 버튼 글자가 위 표대로다(2열 가운데 단계).

## 완료 조건

E1–E3 통과, 두 빌드 성공, `vBoR.*` 전체 통과. 보고 ①–⑤.

## 사용자 확인

- `BP_Car_Core`의 좌석점 태그를 새 이름으로 바꾸고, 2열 가운데·오른쪽 점을 더한다(SgRP 값). `DA_Main_Scenario`의 좌석 단계 Rule target도 새 태그로 고른다.
- Check setup에서 "rename" 알림이 없어졌는지 본다. `1`–`4`로 자리를 옮겨 본다.
- Quest 3 T-05 `not-tested`.

## 개정 1 (Claude 2026-11-18) — Codex Q1·Q2

기존 시험을 다시 훑었다(`Move to … seat` 문구, 좌석 이름, 키 `1`·`2`, `Rear` 명령, 좌석 검색 횟수). 충돌은 아래 두 곳뿐이다. 키 `2` → 2열 왼쪽(`P5gFreeModeTests.cpp:139,155`, `P5zFixedGuidanceTests.cpp:246`), `Move to passenger seat`, 좌석 이름 `Driver`는 그대로 맞는다.

- **Q1 허용:** `P5zFixedGuidanceTests.cpp:326,374,377`의 기대 문구를 `Move to rear left seat`로 바꾼다. 문구만 바꾸고 다른 단언은 그대로 둔다. 옛 태그에만 옛 문구를 남기는 분기는 만들지 않는다.
- **Q2 허용:** `P5iOperatorPanelTests.cpp:700,715`의 시작·재시작 좌석 검색 횟수를 2 → 4로 바꾼다. 같은 `ScanSeatView` 경로로 네 자리를 검색한다. Play 중 0회 단언은 그대로 둔다.
- 이 다섯 줄 말고 기존 단언을 바꿔야 하면 다시 질문하고 멈춘다.
