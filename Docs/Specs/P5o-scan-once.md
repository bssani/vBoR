# P5o 매 프레임 월드 순회 없애기 — 좌석 시점·TV 띠 부품은 처음 한 번 찾기

| 항목 | 값 |
|---|---|
| 단계 | P5 정리. 실제 차 CAD(Actor 수만 개)를 L_Main에 넣기 전 성능 위험 제거. 새 T-ID 없음 |
| 결정 근거 | **사용자 질문(2026-10-20):** "Tag로 찾는데 부품이 엄청 많으면 비용이나 시간이 많이 들지 않나?"<br>- 태그를 단 부품 수는 상관없다. 문제는 검색 하나가 레벨의 **모든 Actor**를 훑는다는 점이다.<br>- 시작 검사(`InitializeFlow`)·버튼 누를 때 한 번은 괜찮다. **매 frame 전체를 훑는 곳 3군데**가 문제다(§0). 근사 차는 Actor가 수십 개라 드러나지 않았다.<br>- 사용자: "작으니까 너가 고쳐" — Claude가 구현함 |
| 베이스 브랜치 | `main` @ 49e8718 |
| 작업 브랜치 | `claude/scan-once` |
| 상태 | 구현함(Claude) — 로컬 빌드·전체 시험으로 확인 |

## 0. 고친 곳

| # | 어디 | 전 | 후 |
|---|---|---|---|
| D1 | 진행자 창 — 동승석·2열 버튼 판정·꼬리표(`Interaction/VBoRScenarioOperator.cpp` `CommandViewpoint`) | frame마다 2번 전체 순회. Actor마다 컴포넌트 배열 2개 복사 | 시작 검사(`InitializeFlow`)에서 한 번 찾아 `SeatViews[2]`(weak)에 둔다. 다음 시작 검사 때 다시 찾는다 |
| D2 | 자유모드 좌석 버튼(`Interaction/VBoRScenarioRunner.cpp` `RefreshPanel`) | 자유모드 frame마다 `FindP3Tag` 2번 | D1의 값을 쓴다 |
| D3 | TV 띠 — 지금 만지는 부품(`Spectator/VBoRSpectatorSubsystem.cpp` `SampleParts`) | frame마다 전체 순회 + interface 형 변환 | 첫 `SampleParts`에서 한 번 훑어 부품 목록(weak)을 만든다. 뒤에 생긴 Actor는 spawn 알림으로, 서브레벨·스트리밍으로 들어온 레벨은 레벨 추가 알림으로 더한다. 지운 Actor는 목록에서 뺀다 |

## 1. 지켜야 할 조건 (바꾸면 안 되는 것)

- **찾는 규칙은 그대로다.** Actor 태그면 그 Actor의 시점 컴포넌트가 하나일 때만, 컴포넌트 태그면 그 컴포넌트다. 맞는 것이 정확히 하나가 아니면 없음(null)이다(`ScanSeatView` = 옛 `CommandViewpoint` 본문).
- 버튼 판정·꼬리표·거절 이유, 자유모드 좌석 버튼(둘 다 있어야 보임), TV 띠 이름 보류 1 s·최근 둘 규칙(P5h E9–E11)은 그대로다.
- 시작 검사 전에는 좌석 명령이 "Scenario has not started"로 먼저 거절되고 꼬리표도 정렬 컴포넌트가 없어 비어 있다(`VBoRScenarioOperator.cpp:118-119`·`:264`). 그래서 시작 검사 전에 값이 비어 있어도 결과가 같다.
- 시작 때 한 번 하는 검사(태그 검사·연결, R 때 문 찾기)는 바꾸지 않는다.

## 2. 달라지는 것 — 경계 상황

| 상황 | 전 | 후 | 이유 |
|---|---|---|---|
| Play 중 같은 태그의 시점을 **더함** | 바로 "여럿" → 없음 | 다음 시작 검사까지 처음 찾은 것 | 실제 차 좌석 시점은 Play 중에 생기거나 바뀌지 않는다 |
| Play 중 시점을 **지움** | 바로 없음 | 바로 없음(weak) | 같음 |
| 같은 frame에 두 부품을 잡음 | 이름 순서 = Actor 순회 순서(정해지지 않음) | 목록에 들어온 순서 | 둘 다 정해진 순서가 아니다. 시험이 순서에 기대지 않는다 |

## 3. 엔진 근거

- UE 5.6부터 `UWorld::OnActorSpawned`는 Actor가 다 만들어진 뒤(FinishSpawning 뒤)에 불린다. Deferred spawn도 같다(`Engine/Source/Runtime/Engine/Private/LevelActor.cpp:51-55`, `Private/World.cpp:6581-6584`).
- 서브레벨·World Partition 셀이 붙을 때 `FWorldDelegates::LevelAddedToWorld`가 불린다(`Private/World.cpp:3958`). 전역 알림이라 내 월드인지 확인한다(`Classes/Engine/World.h:4391`·`:4469`).
- L_Main은 지금 World Partition이 아니다(`Content/__ExternalActors__` 없음). 회사 CAD를 서브레벨로 넣을 수 있어 레벨 추가도 받는다.

## 4. 시험

| ID | 시험 | 확인 |
|---|---|---|
| 개정 | `vBoR.P5i.R1OneSnapshotPerFrame` | frame마다 월드 순회 2번 → **0번** |
| E1·E2 | `vBoR.P5o.E1E2SeatViewsFoundAtStart` | 시작 검사에서 좌석마다 1번. 자유모드 5 frame·스냅숏 동안 0번. 두 버튼 가능, 좌석 버튼 보임 / Play 중 더한 2열 중복은 다음 시작 검사까지 무시. 지운 동승석은 바로 "Seat not found", 좌석 버튼 숨김. 다시 시작 검사하면 2번 찾고 2열 중복을 잡음 |
| E3–E5 | `vBoR.P5o.E3E5SpectatorPartsCollectedOnce` | 5 frame에 월드 순회 1번, 부품 아닌 Actor는 목록에 없음 / 첫 수집 뒤 spawn한 부품 이름이 바로 나옴(다시 훑지 않음), 지운 부품은 빠지고 이름은 보류 뒤 사라짐 / 추가된 레벨의 부품을 받고 다른 월드 알림은 무시 |

기존 시험 감사:
- P5i R1의 다른 단언(스냅숏 = 실제 판정·꼬리표·스타일)은 그대로 통과해야 한다.
- P5h E10E11은 `SampleParts`를 바로 부른다. 첫 호출에서 수집하므로 그대로다.
- P3Flow·P5g·P4h 시험은 시점을 시작 검사 전에 만들고, 시작 뒤에는 위치·회전만 바꾼다(`P3FlowTests.cpp:841`, `P4hPointAlignmentTests.cpp:214`). 시점을 더하거나 지우는 기존 시험은 없다.

## 5. 로컬에서 확인할 것

- 빌드(non-unity·unity), `vBoR.*` 전체 시험.
- 회사 PC: 실제 CAD를 넣은 L_Main에서 `stat unit`의 Game ms를 진행자 창을 연 채로, 순차 흐름과 자유모드에서 잰다(Company-Handover §8). 합격 기준은 정하지 않는다.
