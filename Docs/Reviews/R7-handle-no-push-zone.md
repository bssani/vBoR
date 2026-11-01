# R7 손잡이 근처 밀기 금지 — 리뷰

| 회차 | 날짜 | 대상 | 리뷰어 | 결과 |
|---|---|---|---|---|
| 1 | 2026-11-01 | `6267fce` feat: 손잡이 근처 패널 밀기 차단 | Claude | **열린 항목 없음 — 통과.** 빌드: unity 최신(Codex 최종 트리). `vBoR.*` **675/675**, Fail 0, `EXIT CODE: 0`(`Saved/Logs/ReviewR7All.log`). R7 12/12 |

## 1회차 (2026-11-01, `main...codex/R7-handle-no-push-zone`)

### 확인한 것

- **AGENTS.md 제약:** 범위 안. 영어 주석. 에디터 작업 없음.
- **D1·D2:** 패널 `Tick`에서 손 판정점 5개를 경첩 좌표 → 월드로 바꿔, 연결된 손잡이(`LinkedDoor == this`) GrabBox 월드 상자(스케일 반영 크기 + 여유, 회전 포함)에 하나라도 들어가면 `FVBoRDoorHand::bNearHandle`. 모션의 `FindPush`가 그 손을 `NearHandle`로 거절 — 미는 중인 손도 같은 `FindPush`를 거치므로 기존 "손을 뗀" 경로로 끝난다(D4).
- **D3:** `Locked` 다음, `OpeningDisabled` 앞에서 거절. 잠긴 문은 지금처럼 `Locked`로 센다.
- **기존 번호:** `NearHandle`은 `OpeningDisabled` 뒤·`Count` 앞. 측정 문구에 `near handle=%d` 추가.
- **D6:** 잡기 경로는 손 `Points[0]`만 쓰고 `bNearHandle`을 보지 않는다. 잡기 > 밀기 순서 그대로.
- **D8 표시:** `HandlePushZones(true)`가 로그 없는 `EvaluateSetup` 후보로 그린다(보라, 새 종류 `NearHandle`). R2a·R2c 모양 수 단언 영향 없음.
- **§3 허용 수정만:** `P2dLatchedPanelTests` E4·E11, `P5aGloveboxTests` E4·E5의 시험 준비에 `NoPushNearHandleCm = 0` 한 줄씩. 단언·손 위치 변경 없음.

### 참고 (항목 아님)

1. **작은 뚜껑의 밀 수 있는 면적.** 글러브박스·콘솔 뚜껑은 손잡이가 미는 면 가운데 가까이 있어서, 기본 8 cm면 손바닥으로 닫을 수 있는 면이 많이 줄 수 있다(P5a E4·E5가 기본값에서 실패한 이유). 회사 차에서 보라 상자를 보고 뚜껑은 값을 줄이거나(예: 3 cm) 0으로 둘지 정한다. 문은 8 cm로 시작.
2. `FindPush`의 "Keep locked count first" 주석이 새 `NearHandle` 줄 아래에 남아 있다. 뜻은 그대로라 고치지 않는다.
3. Quest 3 T-03(운전석에서 손잡이로 뻗을 때 문이 먼저 밀리지 않음 등)은 `not-tested`. 확인할 때 임시로 끈 `Allow palm opening`을 다시 켠다.

### 질문

없음.
