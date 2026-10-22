# P5x 판 손잡이 — 질문

첫 리뷰 전. 명세 §3의 중단 규칙에 따라 질문을 기록했고, 개정 1 답을 반영함. Claude 리뷰 대기.

## 1회차 (2026-10-23, Claude) — 통과 (문제 없음, 참고 3개)

- 리뷰한 커밋: `2efff1a`(중단 wip) + `a59956c` fix: 판 손잡이 연결과 생성 순서 정리. 베이스 `main@f668723`. 명세 개정 1(`5617297`).
- Codex 토큰: 12.49M(중단 전후 한 세션 합계). 중단(Q1·Q2)은 Claude 감사 누락 탓이다.

### 확인한 것

| 항목 | 결과 |
|---|---|
| D1 | 헤더·구현에서 명세 목록만 지웠다. 남은 추가 줄은 D2 경고 조건·문장, 손 펴기 때 `HoldingGrabber` 없이 `EndAxisHold`, `bUseLatch` 툴팁(내장 상자 문구 제거)뿐. `CancelSelections`는 손잡이 Actor 정리만 남았고 `PreInitializeComponents`는 잡기 연결 없이 그대로 |
| D2 | 경고 조건 `bUseLatch && !OutsideAxisHandle && !InsideAxisHandle`, 문장 "Latched panel has no opening handle." |
| D3·D6 | `BP_ConsoleHandle` 새로(Translate·`MaxTravelCm`). `BP_Door_FL`·`BP_ConsoleLid`·`BP_Glovebox`·`L_Main`에 지운 컴포넌트·프로퍼티 이름이 남지 않았다(바이너리 검사). `L_Main`에 `BP_ConsoleHandle` 있음. 바뀐 에셋은 명세의 5개뿐. 에셋 한글 0 |
| D4 | 근사 차 콘솔 손잡이: 직선·0.75 cm·복귀·바깥·`bReverse`(경첩 −X)·"Console Box", 메시 회전·크기는 옛 표시와 같음(경첩 X 90°, 0.02×0.03×0.14). 문은 deferred와 `FinishSpawning`에 같은 `DoorWorld`, 손잡이를 먼저 붙이고 칸에 넣은 뒤 `FinishSpawning`(Q2 답대로) |
| D5 | P2 시험대가 손잡이 Actor 두 개를 기존 자리 값으로 만들고 칸에 넣는다. 종료 때 같이 지운다 |
| D7 | P4i 목록·16개·짝·연결 확인에 콘솔 손잡이 추가, E1–E4 통과 |
| 시험 | 지운 시험 3개는 §3 표대로(P2Door PalmHandleOverlap, P2d E2, P2d E10). P2d 단언 106 → 76은 그 두 시험과 내장 상자 확인 줄 때문이다. E14는 손잡이 Actor가 손 펴기·잠김·Reset·SDK 놓기 뒤 0 cm로 돌아오는지로, E16은 직선·0.75 cm·복귀·바깥·손바닥만·경첩 부모·앞 가운데 36 cm·경첩 −X로 바뀌었다. 새 `vBoR.P5x.*` 4개는 기본 객체 컴포넌트, 덜 누름(0.5 cm 잠김 유지), 끝까지 누름(풀림·같은 손으로 들어 올림·놓으면 손잡이 0, 판 그 각), 손잡이 없는 판(경고 1·`Unlock` 뒤 밀기)을 본다 |
| 빌드·시험 재현 | non-unity·unity Succeeded(링크만 함. Codex 로그 `P5x-Rev1-NonUnity.log` 4개·`P5x-Rev1-Unity.log` 2개 컴파일, 경고 0). `vBoR.*` 557/557 통과(556 − 3 + 4, `Saved/Logs/P5xReviewAll.log`) |

### 문제

없음.

### 참고 (고치지 않아도 됨)

1. **콘솔 동작이 바뀌었다:** 잡기만 하면 풀리던 것이 0.6 cm 이상 눌러야 풀린다(명세 D3). Quest 3에서 문턱이 자연스러운지 본다(T-08, `not-tested`). 문턱은 `BP_ConsoleHandle`의 `OpenThreshold`·`MaxTravelCm`으로 바꾼다.
2. **`Attempts`(내장 상자 hover 집계)는 이제 늘지 않는다.** 측정 문구 형식을 지키려고 칸은 그대로다(명세 범위 밖). 손잡이 시도는 손잡이 Actor의 집계로 본다.
3. **사용자 확인:** `BP_Door_FL` 뷰포트에 손잡이 상자가 더 보이지 않는지. Blueprint-Guide의 내장 잡는 자리·콘솔 설명은 머지 뒤 Claude가 고친다.

## 질문

### Q1 — 기존 시험 감사 표 밖의 콘솔 단언 두 곳

- **상태: 반영함 (fix: 판 손잡이 연결과 생성 순서 정리)** — 질문한 대로다. §3 표에 P2c E13·P2e E12를 넣었다. E13은 콘솔 손잡이 연결·경첩 부모로, E12는 부착 로그를 글러브박스+콘솔 2개(각 손잡이 이름)로, `:259`는 콘솔 손잡이 연결·경첩 부모·Actor 원점으로 바꾼다. 문·글러브박스 단언과 경고 0은 그대로다. 내 처음 감사가 지우는 이름만 grep해서 놓친 것이다. `ConsoleLid`·경고 문장·부착 로그·`AxisHandle` null 확인으로 다시 감사했고, 새로 걸리는 시험은 더 없다.
- (질문 원문, Codex)
- **문제:** P5x D3·D4는 콘솔에 별도 `AVBoRAxisPart` 손잡이를 요구하지만, §3 감사 표에 없는 다음 두 시험은 내장 잡기 영역을 전제로 한다. 허용 표 밖이므로 해당 단언은 변경하지 않았다.
  - `Source/vBoR/P2Probe/P2cHingedPanelTests.cpp:272`, `vBoR.P2c.P3.E13ConsolePushAndGrab`: `Lid->OutsideAxisHandle == nullptr` 요구. 새 콘솔 손잡이와 직접 충돌한다.
  - `Source/vBoR/P2Probe/P2ePartAssemblyTests.cpp:252-253`, `vBoR.P2e.Assembly.E12MockUnchanged`: 부착 로그 1개·글러브박스만 요구. 콘솔 손잡이도 판 BeginPlay에서 연결되어 2개가 된다. 같은 파일 `:259`도 `ConsoleLid->OutsideAxisHandle == nullptr` 요구.
- **질문:** 위 두 시험을 §3 허용 표에 추가하고, 콘솔 손잡이 연결·경첩 부모를 확인하며 부착 로그를 글러브박스+콘솔 2개로 확인하도록 개정할 것인가? 기존 도어·글러브박스 단언은 유지해야 한다.
- **증거:** `Saved/Logs/P5x-Full.log` 전체 1회: 발견/완료 557/557, 성공 555, 실패 2, 로그 exit -1·프로세스 exit 255. 실패는 위 두 시험뿐. 신규 P5x E1–E4 4/4와 P4i E1–E4는 성공. 전체 수는 556 − 삭제 3 + 신규 4 = 557.

### Q2 — 근사 차 문의 BeginPlay에 손잡이가 아직 없는 순간

- **상태: 반영함 (fix: 판 손잡이 연결과 생성 순서 정리)** — 바꿔도 된다. 순서: 문을 `SpawnActorDeferred`할 때도 `FinishSpawning`과 같은 `DoorWorld`를 쓴다 → 두 손잡이를 만들어 `FinishSpawning` → `Door->HingePivot`에 `KeepWorldTransform`으로 붙임 → `OutsideAxisHandle`·`InsideAxisHandle` 칸에 넣음 → `Door->FinishSpawning(DoorWorld)`. **같은 `DoorWorld`가 필요한 이유:** 지금 deferred 자세는 Yaw 180 고정이고 `FinishSpawning`은 차 Yaw+180이다. 차가 돌아 있으면 `FinishSpawning`에서 문이 움직여 먼저 붙인 손잡이도 따라 움직인다. D2 경고 조건과 E12 경고 0은 그대로다. 문 손잡이 월드 자리·원점·부모·연결이 지금과 다르게 나오거나 부착 로그가 생기면 멈추고 다시 적는다.
- (질문 원문, Codex)
- **문제:** D2의 새 조건 `bUseLatch && !OutsideAxisHandle && !InsideAxisHandle` 때문에 근사 차가 경고 1개를 낸다. `Source/vBoR/P3Probe/P3VehicleMock.cpp:144`에서 문의 `FinishSpawning()`으로 BeginPlay가 먼저 실행되고, 손잡이는 `:153`, `:162`에서 연결된다. 따라서 최종 연결은 정상이어도 `Latched panel has no opening handle. Ignore button Unlock.`가 찍힌다. 위 E12의 `No new mock warning`도 0 대신 1로 실패한다. 이 경고는 환경 잡음이 아니다.
- **질문:** 근사 차 문의 손잡이 두 개를 문의 FinishSpawning 전에 만들고 Outside/Inside 칸에 넣는 순서로 바꿔도 되는가? D2의 경고 조건은 유지하고, 이미 경첩에 붙인 상태로 BeginPlay에 들어가 기존 문 손잡이의 월드 자리와 부착 로그 수를 보존하는 방식을 명세에 명시해 주기 바란다. E12의 경고 0 기대값을 1로 약화하지 않는다.
- **확인할 시험:** Q1 개정 뒤 E12의 경고 0·기존 문 손잡이 자리/부모 유지와 콘솔·글러브박스 연결 로그를 확인. 현재 추가 제품 수정·기존 단언 수정·재실행은 하지 않았다.

명세 개정 1의 Q1·Q2 답 반영(2026-10-23, Codex). 열린 질문 없음. 자세한 보고 ①–⑤는 커밋 본문과 HANDOFF.md에 남긴다.
