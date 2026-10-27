# P5ze 문 안쪽 그립 — 리뷰

## 1회차 (2026-10-28, Claude) — 통과 (문제 없음, 참고 4개)

- 리뷰한 커밋: `1da8c45` feat: 문 안쪽 고정 그립 잡기 추가. 베이스 `main@c8fff56`. 명세 개정 1.

### 확인한 것

| 항목 | 결과 |
|---|---|
| D1 | `EVBoRDoorHandleSide::Grip`을 맨 끝에 추가(기존 값 순서 그대로) |
| D2 | `GripAxisHandle` 칸, 후보 분류 3칸(Grip이 안쪽 레버 후보에 섞이지 않음 `CollectHandleCandidates`), 공유·여러 후보·Child Actor 규칙을 레버와 같이 적용, `HingePivot`에 부착 |
| D3 | `BeginGrab(…, bUnlockLatch=false)`는 잠겨 있으면 실패하고 `Unlock`을 부르지 않음. 잠긴 동안 그립의 잡기 입력·충돌을 끄고 hover를 지움(`RefreshGripInput`, 매 Tick·`ApplyState`). 래치 때 `CancelSelections`가 그립도 끝냄 |
| D4·D5 | 그립은 열림 비율 검사 없이 잡는 즉시 문 잡기를 시도하고, 실패하면(다른 손이 문을 잡음) 매 Tick 다시 시도. 시작할 때 `RebaseGrab`로 지금 손 점을 기준으로 함 — 쌓인 이동 미적용 |
| D6 | 축 값 0 고정(`AcceptTransform` 무시, Tick에서 `RestPivot`), 밀기·걸림 끔, 열림·닫힘 이벤트 없음 |
| D7 | `GripGrabs[2]`·`Since()`. 레버 `Grabs`·`Attempts`·측정 문구 그대로 |
| D8 | 레버 경로는 `bUnlockLatch` 기본 true로 같음. 관성·밀기 경로 변경 없음 |
| 범위 | 근사 차·에셋·레벨 변경 없음. 시험 fixture는 Probe 폴더 |
| 시험 | 새 `vBoR.P5ze.*` 10개. 기존 시험 파일 변경 0 |
| 빌드·시험 재현 | `1da8c45`: unity 빌드 Succeeded. `vBoR.*` 615개 중 614 통과, `vBoR.P0Buck.E8Persistence` 1개 실패(`Saved/Logs/P5zeReviewAll.log`) — 참고 1. 그 시험만 다시 돌려 14/14 통과(`P5zeReviewP0Buck.log`). Codex 전체 실행은 615/615 |

### 문제

없음.

### 참고 (고치지 않아도 됨)

1. **`vBoR.P0Buck.E8Persistence`가 두 번째로 간헐 실패했다**(P5zb 리뷰 참고 1과 같은 `candidate replacement failed (OS error 5)`, 이번엔 반복 38회째). P5ze와 무관하다. 두 번 나왔으므로 mid review 뒤 명세 목록에 올린다(HANDOFF).
2. **슬롯이 종류를 이긴다.** `IsDoorGrip()`은 연결된 칸(`DoorHandle == 2`)을 먼저 본다. 그래서 Grip 부품을 `InsideAxisHandle` 칸에 지정하면 레버로 동작하고, 레버를 `GripAxisHandle` 칸에 지정하면 그립으로 동작한다. 기존 "slot wins" 규칙과 같다. 다만 Grip ↔ 안쪽 칸 불일치에는 Check setup 경고가 없다(바깥·안쪽 불일치만 경고). 회사에서는 칸을 비우고 자동 연결을 쓰면 된다.
3. **문 밑에 없는 그립은 조용히 안 잡힌다**(연결된 문이 없으면 입력을 끔). 미러 부착 경고와 같은 묶음으로 mid review 뒤 다룬다.
4. **Quest 3(T-03, `not-tested`):** 레버와 그립이 가까우면 엉뚱한 것이 잡힐 수 있다. 메시로 잡기를 쓰면 둘 다 피벗을 각자 가운데로 둔다(Blueprint-Guide §4.3).

## 질문

(없음)
