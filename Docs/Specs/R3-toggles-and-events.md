# R3 연결 끄고 켜기 + BP 이벤트·명령

| 항목 | 값 |
|---|---|
| 단계 | 구조 보수 R3(`Docs/Refactor-Plan.md` §4, 요구 2·4). 시험 계획 T-ID 없음(동작 기본값 그대로) |
| 결정 근거 | **사용자(2026-10-28):** "start 버튼 누르면 sidemirror가 열리는데, 이걸 가능하게 할지 아닐지 c++에서 수정하지 않는 이상 불가능해." "블루프린트 조립하고 기능 추가하는 걸 쉽게."<br>**사용자(2026-11-01, Q4):** 끄고 켤 연결은 시동 → 미러 **하나뿐**(키 → 문 잠금 해제, 웰컴 라이트 등 없음).<br>**지금(계획 §2.3):**<br>- 미러는 BeginPlay에서 차량 전원 컴포넌트를 찾아 **무조건** 구독한다(`VBoRFoldingMirror.cpp:56-60`). 미러를 BP에서 접고 펴는 함수도, 상태 이벤트도 없다(`VBoRFoldingMirror.h` — `GetState`·`GetAngleDeg`·`ResetMirror`만).<br>- 2열 등받이는 `Fold`·`Unfold` 명령은 있지만 상태 이벤트가 없다(`VBoRFoldingSeat.h:28-32`).<br>- 진행자 다시 시작(R)은 Runner가 클래스별로 `ResetStand`·`ResetPanel`·`ResetSeat`·`ResetMirror`·`ResetDoor`를 직접 부른다(`VBoRScenarioRunner.cpp:526-538`). BP에서 붙인 추가 동작(조명·소리 등)은 이때 같이 초기화될 방법이 없다 |
| 베이스 브랜치 | `main` (**R8a 머지 뒤**) |
| 작업 브랜치 | `codex/R3-toggles-and-events` |
| 리뷰 파일 | `Docs/Reviews/R3-toggles-and-events.md` (첫 리뷰 때 생김) |
| 상태 | 명세 작성 |
| 개정 | **1 (2026-11-01, Claude, 머지 전 자체 감사):** 코드 대조로 빈틈 5개를 메움 — ① 전원 핸들러가 이미 끝 상태여도 다시 움직이던 것(D2로만 닿는 새 경우) ② BeginPlay 안 상태 변화의 D3 이벤트 ③ 리프트게이트 기존 `OnStateChanged`는 같은 상태도 낸다(바꾸지 않음) ④ 다시 시작 때 전원 끔이 미러 리셋보다 먼저 와서 생기는 이벤트 순서 ⑤ 끔일 때 전원 없음 경고. D1·D2·D3·D4·§3·E5·E8·E9·E10 |
| 에디터 작업 | **없음** |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **미러 `Follow vehicle power`** | `AVBoRFoldingMirror`에 체크박스(기본 **켬** — 지금 동작). 끄면 BeginPlay에서 전원 컴포넌트를 찾지도 구독하지도 않고(`FindUnique`·`Side mirror: No vehicle power` 경고 없음 — 전원이 필요 없으므로, `VBoRFoldingMirror.cpp:56-62`), 시동을 켜고 꺼도 미러는 움직이지 않는다. 켬(기본)에서는 경고 포함 지금 그대로(P5q E5의 경고 수 3 유지). Category `Mirror folding` |
| D2 | **미러 BP 명령** | `Fold()`·`Unfold()`(BlueprintCallable, `bool` — 움직이기 시작했으면 true). 지금 전원이 부르는 접기·펴기와 **같은 경로**(모터 소리·속도·상태 전이)를 쓴다. 이미 그 방향 끝 상태(`Folded`에 `Fold`, `Unfolded`에 `Unfold`)면 아무것도 하지 않고 false. **전원 핸들러도 같다:** 지금 `HandlePowerChanged`(`VBoRFoldingMirror.cpp:85-91`)는 끝 상태를 보지 않고 `Unfolded`에서 시동 켬이 오면 다시 `Unfolding`으로 들어가 모터 소리를 낸다. 이 경우는 D2(`Unfold()` 뒤 시동 켬)로만 생기므로, 끝 상태면 아무것도 하지 않게(소리·이벤트 없음) 바꾼다. 기존 흐름(시동은 접힘에서 켜고 펼침에서 끔)에는 영향이 없다. 움직이는 중 반대 명령은 지금 전원이 바뀔 때처럼 방향을 바꾼다. `Follow vehicle power`와 상관없이 언제나 동작한다(켜져 있으면 다음 전원 변화가 다시 미러를 움직인다) |
| D3 | **상태 이벤트** | 상태가 바뀔 때마다 한 번: 미러 `OnStateChanged(Mirror, State)`(`Unfolding`·`Unfolded`·`Folding`·`Folded`), 등받이 `OnStateChanged(Seat, State)`(`Standing`·`Folding`·`Folded`·`Unfolding`). 리프트게이트는 이미 있다(`VBoRPoweredPanel.h:118`). 같은 상태로 다시 들어가면 내지 않는다(미러·등받이). **리프트게이트 기존 이벤트는 바꾸지 않는다:** `ResetPanel`이 언제나 `PublishState`로 같은 상태도 낸다(`VBoRPoweredPanel.cpp:135-147, 297-301`). **처음 시작 중에는 내지 않는다:** 미러는 `OnConstruction`에서 `Unfolded`(`VBoRFoldingMirror.cpp:40`), BeginPlay의 `ResetMirror`에서 `Folded`(`:55`)가 되고, 시작 때 전원이 켜져 있으면 BeginPlay 안에서 `Unfolding`이 된다(`:60`). `HasActorBegunPlay()`가 false인 동안의 상태 변화는 이벤트를 내지 않는다(D4와 같은 기준). 등받이는 `OnConstruction`·BeginPlay에서 `ResetSeat`(`VBoRFoldingSeat.cpp:34`) |
| D4 | **`OnReset` 이벤트** | 미러·등받이·리프트게이트·문/콘솔(`AVBoRHingedPanel`)에 `OnReset(Part)`. 그 부품의 Reset 함수(`ResetMirror`·`ResetSeat`·`ResetPanel`·`ResetDoor`)가 **Play 중에** 불릴 때 한 번(진행자 다시 시작, BP 호출). 처음 시작(BeginPlay·`OnConstruction`에서 부르는 Reset)에서는 내지 않는다 — 예: `HasActorBegunPlay()`가 true일 때만. 리셋으로 상태가 바뀌면 D3 이벤트를 **낸다**(순서: 상태 → OnReset). 바뀌지 않으면 D3는 없다(미러·등받이) |
| D5 | **나머지 그대로** | Runner 다시 시작 순서·대상(`VBoRScenarioRunner.cpp:526-538` — 받침대 → 리프트게이트 → 등받이 → **전원 끔** → 미러 → 문), 전원 컴포넌트(태그 `Car.StartButton`), 미러 시작 상태(접힘), 기본값에서의 모든 동작·소리·시나리오 판정 |

## 1. 왜 하는가

- 시동 → 미러 펼침을 C++ 수정 없이 끄고, 필요하면 BP에서 다른 조건(키 버튼, 시나리오 단계 등)으로 미러를 움직인다.
- BP Event Graph에서 부품 상태에 반응(조명·소리·다른 부품)하고, 다시 시작 때 그 반응도 초기화할 수 있게 한다. 계획 §1 원칙 "C++는 상태와 판정, BP는 표현과 선택적 연결"의 최소 구현이다.

## 2. 범위

### 만들 것

- D1–D4. 이벤트는 `BlueprintAssignable`, 명령은 `BlueprintCallable`, Category는 부품의 기존 Category.
- 시험 `vBoR.R3.*`.

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| 키 → 문 잠금 해제, 웰컴 라이트, 키 들고 접근 이벤트 | 사용자 Q4 — 없음 |
| 문·콘솔 `Open`·`Close` 자동 열기 명령 | 문에는 모터가 없다. 새 동작이 된다 |
| AxisPart 이벤트 추가 | 이미 `OnPartOpened`·`Closed`·`Released`·`OnDetentChanged`·`OnDetentNudge`가 있다(`VBoRAxisPart.h:146-150`) |
| 범용 "반응 규칙" 데이터·DataAsset | 계획 §5 |
| Runner 다시 시작을 이벤트 기반으로 바꾸기 | 평가 흐름. 그대로 두고 부품이 `OnReset`만 낸다 |
| AGENTS.md·Blueprint-Guide 개정 | 문서는 Claude가 따로(R6) |
| `L_Main`·차 BP 값 변경 | 에디터 작업 없음 |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- 기본값(`Follow vehicle power` 켬)에서 미러·등받이·리프트게이트·문 동작, 소리, 로그, 시나리오 판정이 지금과 같다.
- 이벤트를 아무도 받지 않으면 동작에 영향이 없다.
- **기존 시험 감사(Claude 2026-11-01, 미러·등받이 클래스 검색):** `P3Probe/P5qPowerMirrorTests.cpp`(시동 → 미러, 시작 접힘 `:138-139`), `P3Probe/P5sPartSoundTests.cpp`(미러 모터 소리), `P3Probe/P3FlowTests.cpp`(다시 시작), `P3Probe/P4iCarBlueprintTests.cpp`, `P3Probe/R2aMotionGuideTests.cpp`, 리프트게이트 `P1Probe/P4bPoweredPanelTests.cpp`, 문 리셋 시험들 — 기본값에서 모두 그대로 통과해야 한다.
- **개정 1 대조(코드 기준):** 미러 끝 상태 전원 무시(D2)는 기존 시험에 닿지 않는다 — 기존 시험의 시동은 모두 접힘(또는 접는 중)에서 켜고 펼침(또는 펴는 중)에서 끈다(P5q·P5s는 `ResetMirror`/BeginPlay 뒤 켬). `Unfolded`에서 시동 켬을 단언하는 시험이 있으면 멈추고 질문. 리프트게이트 `OnStateChanged` 횟수 단언(P4b 관찰자)은 이 명세가 리프트게이트 이벤트를 바꾸지 않으므로 그대로. **고쳐도 되는 단언: 없음.** 고쳐야 통과하면 멈추고 리뷰 파일 "질문"에 적는다.

## 4. 시험 — `vBoR.R3.*`

이벤트는 시험용 관찰 객체(`UP2cEventObserver`·`R2aTestObserver` 방식)로 받는다.

| ID | 상황 | 확인 |
|---|---|---|
| E1 | 미러 `Follow vehicle power` 켬(기본), 시동 켬 → 끔 | 지금처럼 펼침 → 접힘(P5q와 같은 결과). 상태 이벤트 `Unfolding`→`Unfolded`→`Folding`→`Folded` 각 1번 |
| E2 | 끔, 시동 켬 | 미러 그대로 `Folded`, 이벤트 없음, 모터 소리 없음. 전원 컴포넌트가 없는 세계에서 끔으로 Play → 미러 경고 0개 |
| E3 | 끔, `Unfold()` | true, 펼침 완료, 이벤트 `Unfolding`·`Unfolded`. 다시 `Unfold()` → false, 이벤트 없음 |
| E4 | 펴는 중 `Fold()` | 방향 바뀜, `Folding` 이벤트, 결국 `Folded` |
| E5 | 켬 상태에서 `Unfold()` 완료 뒤 시동 켬 → 끔 | 시동 켬: 각도·상태 그대로 `Unfolded`, 이벤트 0, 모터 소리 시작 없음(D2). 시동 끔: `Folding`→`Folded` 각 1번(전원 따라가기 유지) |
| E6 | 등받이 `Fold()`·`Unfold()` | 상태 이벤트 각 전이마다 1번 |
| E7 | Play 중 `ResetMirror`·`ResetSeat`·`ResetPanel`·`ResetDoor` | 각 부품 `OnReset` 1번 |
| E8 | BeginPlay(처음 시작), 에디터 `OnConstruction` | `OnReset` 0번, D3 상태 이벤트 0번 — 미러는 시작 때 전원 꺼짐·켜짐 두 경우(켜짐이면 BeginPlay 뒤 Tick에서 `Unfolded` 1번만). 관찰자는 스폰 전에 붙일 수 없으므로 `SpawnActorDeferred` 뒤 바인딩하고 `FinishSpawning` |
| E9 | 진행자 다시 시작(Runner Restart) | Runner가 Reset을 부르는 부품마다 `OnReset` 1번 — 리프트게이트(대상 + `Car.Liftgate`), 등받이·미러 전부, 문(대상 + `Car.DriverDoor`). 그 밖의 `AVBoRHingedPanel`(예: 콘솔)은 0번. 같은 부품이 두 경로로 잡혀도 1번(TSet). 다시 시작 결과(문 닫힘·잠김, 리프트게이트 리셋, 받침대 박스 하나, 등받이 세움)는 지금과 같음 |
| E10 | 펼친 미러에 다시 시작 | 미러 즉시 `Folded`, 모터 소리 없음, `OnReset` 1번이 마지막. **켬:** 시동 켜서 펼친 상태 → 전원 끔이 먼저 와서 `Folding` → `ResetMirror`가 `Folded` → `OnReset`(순서 그대로 단언). **끔:** `Unfold()`로 펼친 상태 → `Folded` → `OnReset` |

## 5. 구현자가 정할 것

- 미러 접기·펴기 공통 경로를 함수로 뽑는 방법(전원 핸들러와 `Fold`/`Unfold`가 같이 부르게).
- 이벤트 델리게이트 선언 이름(기존 `FVBoRPoweredStateEvent` 방식 권장).
- D4 "처음 시작 제외"를 판단하는 방법.

## 6. 완료 조건

1. D1–D5 구현, §3 조건 지킴.
2. `vBoR.R3.E1`–`E10` 통과, 기존 시험 변경 없이 통과.
3. non-unity(`-Module=vBoR`)·unity 빌드 성공, `vBoR.*` 전체 통과(작업 끝에 한 번).
4. 새 주석·표시 이름 영어(AGENTS.md).

## 7. 로컬에서 확인할 것

- 빌드: 위 3.
- 에디터(사용자): ① 미러 BP에서 `Follow vehicle power` 끄고 Play → 시동 버튼을 눌러도 미러가 안 움직임 ② 미러 BP Event Graph에서 다른 조건(예: 키 버튼 이벤트)으로 `Unfold` 호출 ③ 부품 `OnStateChanged`·`OnReset`에 BP 동작(예: 조명)을 붙이고 진행자 R로 초기화되는지.
- Quest 3: 없음(기본 동작 그대로).
