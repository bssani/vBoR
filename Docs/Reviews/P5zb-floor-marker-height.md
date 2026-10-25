# P5zb 목표 고리 지면 높이 — 리뷰

## 1회차 (2026-10-26, Claude) — 통과 (문제 없음, 참고 3개)

- 리뷰한 커밋: `5e4f011` fix: 목표 고리를 설정한 지면 높이에 맞춤. 베이스 `main@844d96f`. 명세 처음 판.

### 확인한 것

| 항목 | 결과 |
|---|---|
| D1 | 고리 Z = `Alignment->FloorWorldZCm`(없으면 0) + 0.5. `bHeightFromFloor`를 보지 않음. XY·회전·크기·숨김 그대로 |
| D2 | `Tick` 끝에서 `UpdateMarker()`를 매 frame 부름. 고리·화살표의 숨김과 위치는 `UpdateMarker` 안에서만 정하고(`VBoRScenarioRunner.cpp` 전체 검색), 그 입력(`bRunning`·`bRouteAllowed`·`bRouteArrived`·`StepIndex`·목표)은 바뀌는 곳마다 이미 `UpdateMarker`를 불렀다. 그래서 매 frame 불러도 보이는 결과는 같고, 지면 값만 바로 따라간다 |
| D3 | `MarkerFloorLiftCm` 상수와 이유 주석 |
| 범위 | 화살표·경로·키 테이블·받침·설정 변경 없음. 시험 접근용 `friend` 한 줄(§4 허용) |
| 시험 | 새 `vBoR.P5zb.*` 5개(E3은 `bHeightFromFloor` 끔까지 같이 봄). 기존 시험 파일 변경 0 |
| 빌드·시험 재현 | `5e4f011`: unity 빌드 Succeeded. `vBoR.*` 596개 중 595 통과, `vBoR.P0Buck.E8Persistence` 1개 실패(`Saved/Logs/P5zbReviewAll.log`). 그 시험만 두 번 다시 돌려 둘 다 14/14 통과(`P5zbReviewP0Buck1.log`·`2.log`) — 아래 참고 1 |

### 문제

없음.

### 참고 (고치지 않아도 됨)

1. **`vBoR.P0Buck.E8Persistence`가 한 번 실패했다.** 실패한 단언은 `P0Probe/P0BuckAlignmentTests.cpp:213`의 반복 저장 교체이고, 메시지는 `candidate replacement failed (OS error 5)`다. Windows가 파일 교체를 거부한 것으로, 백신이나 색인기가 방금 쓴 파일을 잡고 있을 때 난다. P5zb는 이 경로를 건드리지 않고, 다시 돌리면 통과한다. Codex 전체 실행(596/596)에서도 통과했다. 또 나오면 그 시험에 따로 명세를 둔다.
2. **매 frame `SetWorldLocation` 5번:** 값이 같으면 엔진이 이동을 건너뛰므로 비용은 무시할 수준이다. 그대로 둔다.
3. **회사 PC(T-01·T-06, `not-tested`):** `FloorWorldZCm`을 지면 높이로 넣고 고리가 바닥에 붙어 보이는지 본다(Blueprint-Guide §2-2).

## 질문

(없음)
