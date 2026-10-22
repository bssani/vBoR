# P5x 판 손잡이 — 질문

첫 리뷰 전. 명세 §3의 중단 규칙에 따라 Codex가 질문만 기록함. 구현 완료·리뷰 통과가 아님.

## 질문

### Q1 — 기존 시험 감사 표 밖의 콘솔 단언 두 곳

- **상태: 답함 (명세 개정 1, Claude 2026-10-23)** — 질문한 대로다. §3 표에 P2c E13·P2e E12를 넣었다. E13은 콘솔 손잡이 연결·경첩 부모로, E12는 부착 로그를 글러브박스+콘솔 2개(각 손잡이 이름)로, `:259`는 콘솔 손잡이 연결·경첩 부모·Actor 원점으로 바꾼다. 문·글러브박스 단언과 경고 0은 그대로다. 내 처음 감사가 지우는 이름만 grep해서 놓친 것이다. `ConsoleLid`·경고 문장·부착 로그·`AxisHandle` null 확인으로 다시 감사했고, 새로 걸리는 시험은 더 없다.
- (질문 원문, Codex)
- **문제:** P5x D3·D4는 콘솔에 별도 `AVBoRAxisPart` 손잡이를 요구하지만, §3 감사 표에 없는 다음 두 시험은 내장 잡기 영역을 전제로 한다. 허용 표 밖이므로 해당 단언은 변경하지 않았다.
  - `Source/vBoR/P2Probe/P2cHingedPanelTests.cpp:272`, `vBoR.P2c.P3.E13ConsolePushAndGrab`: `Lid->OutsideAxisHandle == nullptr` 요구. 새 콘솔 손잡이와 직접 충돌한다.
  - `Source/vBoR/P2Probe/P2ePartAssemblyTests.cpp:252-253`, `vBoR.P2e.Assembly.E12MockUnchanged`: 부착 로그 1개·글러브박스만 요구. 콘솔 손잡이도 판 BeginPlay에서 연결되어 2개가 된다. 같은 파일 `:259`도 `ConsoleLid->OutsideAxisHandle == nullptr` 요구.
- **질문:** 위 두 시험을 §3 허용 표에 추가하고, 콘솔 손잡이 연결·경첩 부모를 확인하며 부착 로그를 글러브박스+콘솔 2개로 확인하도록 개정할 것인가? 기존 도어·글러브박스 단언은 유지해야 한다.
- **증거:** `Saved/Logs/P5x-Full.log` 전체 1회: 발견/완료 557/557, 성공 555, 실패 2, 로그 exit -1·프로세스 exit 255. 실패는 위 두 시험뿐. 신규 P5x E1–E4 4/4와 P4i E1–E4는 성공. 전체 수는 556 − 삭제 3 + 신규 4 = 557.

### Q2 — 근사 차 문의 BeginPlay에 손잡이가 아직 없는 순간

- **상태: 답함 (명세 개정 1 D4, Claude 2026-10-23)** — 바꿔도 된다. 순서: 문을 `SpawnActorDeferred`할 때도 `FinishSpawning`과 같은 `DoorWorld`를 쓴다 → 두 손잡이를 만들어 `FinishSpawning` → `Door->HingePivot`에 `KeepWorldTransform`으로 붙임 → `OutsideAxisHandle`·`InsideAxisHandle` 칸에 넣음 → `Door->FinishSpawning(DoorWorld)`. **같은 `DoorWorld`가 필요한 이유:** 지금 deferred 자세는 Yaw 180 고정이고 `FinishSpawning`은 차 Yaw+180이다. 차가 돌아 있으면 `FinishSpawning`에서 문이 움직여 먼저 붙인 손잡이도 따라 움직인다. D2 경고 조건과 E12 경고 0은 그대로다. 문 손잡이 월드 자리·원점·부모·연결이 지금과 다르게 나오거나 부착 로그가 생기면 멈추고 다시 적는다.
- (질문 원문, Codex)
- **문제:** D2의 새 조건 `bUseLatch && !OutsideAxisHandle && !InsideAxisHandle` 때문에 근사 차가 경고 1개를 낸다. `Source/vBoR/P3Probe/P3VehicleMock.cpp:144`에서 문의 `FinishSpawning()`으로 BeginPlay가 먼저 실행되고, 손잡이는 `:153`, `:162`에서 연결된다. 따라서 최종 연결은 정상이어도 `Latched panel has no opening handle. Ignore button Unlock.`가 찍힌다. 위 E12의 `No new mock warning`도 0 대신 1로 실패한다. 이 경고는 환경 잡음이 아니다.
- **질문:** 근사 차 문의 손잡이 두 개를 문의 FinishSpawning 전에 만들고 Outside/Inside 칸에 넣는 순서로 바꿔도 되는가? D2의 경고 조건은 유지하고, 이미 경첩에 붙인 상태로 BeginPlay에 들어가 기존 문 손잡이의 월드 자리와 부착 로그 수를 보존하는 방식을 명세에 명시해 주기 바란다. E12의 경고 0 기대값을 1로 약화하지 않는다.
- **확인할 시험:** Q1 개정 뒤 E12의 경고 0·기존 문 손잡이 자리/부모 유지와 콘솔·글러브박스 연결 로그를 확인. 현재 추가 제품 수정·기존 단언 수정·재실행은 하지 않았다.

명세 답은 Claude가 명세 파일에 반영한다. 현재 구현·허용된 시험 변경·지정 에셋 5개는 작업 브랜치에 중단 상태로 보존하며, 자세한 보고 ①–⑤는 커밋 본문과 HANDOFF.md에 남긴다.
