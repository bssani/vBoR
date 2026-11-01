# R5 설정 검사 — Runner 에디터 버튼 하나로 레벨 전체 점검

| 항목 | 값 |
|---|---|
| 단계 | 구조 보수 R5(`Docs/Refactor-Plan.md` §4, 요구 3). 시험 계획 T-ID 없음(에디터 도구, 평가 동작 그대로) |
| 결정 근거 | **실제 사례(2026-10-28 회사 PC):** 버튼 BP의 Class Defaults에 `Car.ReachButton`이 있어, 그 BP를 리프트게이트 버튼 칸(P5zc)에 넣자 Play 때 생성된 버튼까지 태그를 가져 `Tag Car.ReachButton: found 3 (need 1)`로 Start blocked. 원인을 Play해야만 보고, 로그만으로는 어디서 3개가 왔는지 모름.<br>**조용히 안 되는 것(P5ze 리뷰 참고 2·3, HANDOFF):** 사이드미러가 문 밑에 없으면 문과 같이 돌지 않고 경고도 없다(`VBoRFoldingMirror.cpp:56`). 그립이 어느 문에도 연결되지 않으면 잡히지 않고 경고도 없다(`VBoRAxisPart.cpp:541`). Grip 부품을 안쪽 칸에 넣는 등 Grip이 끼인 칸 불일치는 경고가 없다(`VBoRHingedPanel.cpp:247` — 바깥↔안쪽만 본다).<br>**시동 버튼:** 전원 컴포넌트는 `AVBoRPokeButton` 중 태그 `Car.StartButton`인 것만 묶는다(`VBoRVehiclePowerComponent.cpp:46-51`). 다른 종류 Actor에 붙인 태그는 조용히 무시 |
| 베이스 브랜치 | `main` |
| 작업 브랜치 | `codex/R5-check-setup` |
| 리뷰 파일 | `Docs/Reviews/R5-check-setup.md` (첫 리뷰 때 생김) |
| 상태 | 명세 작성 |
| 에디터 작업 | **없음** |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **Runner `Check setup` 버튼** | `AVBoRScenarioRunner`에 `CallInEditor` 함수 `CheckSetup`(DisplayName `Check setup`, Category `Guidance\|Setup`)와 결과 칸 `SetupReport`(Transient, VisibleAnywhere, MultiLine — `AVBoRHingedPanel::SetupReport` `VBoRHingedPanel.h:59`와 같은 방식). 누르면 D2–D6을 **레벨 전체**에 대해 검사한다. **부작용 없음:** 키 바인딩·델리게이트 구독·`Record`(진행 기록)·`Targets` 등 Runner 상태·Actor 생성을 건드리지 않는다. |
| D2 | **시나리오 태그 검사 = Play 때와 같은 문구** | `InitializeFlow`(`VBoRScenarioRunner.cpp:209-311`)가 `ValidationProblems`에 쓰는 검사를 같은 문구로 낸다: 정렬 컴포넌트 수, 시나리오 없음, 고정 패널 첫 단계, 단계별 대상 태그 수(`Tag X: found N (need 1)`)·종류 불일치, 원래 StepId, 표식·패널·바닥 경로·`Car.Footprint` 교차. **정렬 상태(Applied 여부)는 보지 않는다**(Play 때 정함). `InitializeFlow`의 동작·문구는 바꾸지 않는다(공통 함수로 뽑는 것은 구현자 재량, §3). |
| D3 | **Play 때 생기는 Actor도 센다** | 에디터 월드(아직 Play 시작 안 함 — `!World->HasBegunPlay()`)에서는 Play 때 생성될 Actor의 **클래스 기본 Actor Tags**를 태그 수에 더한다. 대상은 지금 있는 생성 칸 전부: ① `AVBoRKeyTable::KeyClass`(테이블마다 키 1개, `VBoRKeyTable.cpp:39-44`) ② 그 키 클래스 기본값의 `UnlockButtonClass`·`LockButtonClass` ③ 레벨에 놓인 `AVBoRKeyFob`의 두 버튼 칸(`VBoRKeyFob.cpp:113-114`) ④ `AVBoRPoweredPanel`의 `OpenButtonClass`·`CloseButtonClass`(`VBoRPoweredPanel.cpp:102-103`). 칸이 비었거나 Abstract·Deprecated 클래스면 Play 때 태그 없는 기본 버튼이 생기므로(`VBoRKeyFob.cpp:81-85`) 더하지 않는다. 종류 판정도 Play와 같다(① = `AVBoRKeyFob`, ②–④ = `AVBoRPokeButton`). 이미 Play 중인 월드(`HasBegunPlay()`)에서는 더하지 않는다 — 생성된 Actor가 실제로 있다. 클래스 BP의 **컴포넌트** 태그는 세지 않는다(범위 밖). `BoxStand::BoxClass`도 세지 않는다(박스를 대상으로 하는 단계 규칙이 없음). |
| D4 | **생성 칸 태그 설명** | D3의 ②–④ 버튼 칸 클래스가 **검사에 쓰이는 태그**(시나리오 단계의 Target·Marker·Panel·Route 태그, `Car.StartButton`)를 가지면 칸마다 한 줄: 누가(Actor 이름), 어느 칸, 어느 클래스, 어느 태그 — "Play 때 생성되는 버튼마다 이 태그가 붙는다". 그래서 `found 3`이 어디서 왔는지 보인다. ①(키 클래스의 태그)은 정상 사용법이라 설명하지 않는다(Blueprint-Guide — `BP_KeyFob_Front`의 `Car.FrontKey`). |
| D5 | **시동 버튼·전원** | `Car.StartButton`(D3과 같은 방식으로 셈)이 0개면 알림 "operator power only". 1개 이상이면 줄 없음(전원이 모두 묶음). `AVBoRPokeButton`이 아닌 Actor·컴포넌트에 붙어 있으면 문제(무시된다). 전원 컴포넌트(`UVBoRVehiclePowerComponent`) 0개면 알림(시동 버튼·미러 연동 없음), 2개 이상이면 문제(전원 꺼짐, `VBoRVehiclePowerComponent.cpp:37-44`와 같은 뜻). |
| D6 | **부착 검사** | ① `AVBoRFoldingMirror`의 부착 부모 Actor가 `AVBoRHingedPanel`이 아니면 문제 한 줄: "문 밑에 없음 — 문이 열려도 차에 남는다(차체에 붙는 미러면 무시)". 조건은 미러 BeginPlay와 같다(`VBoRFoldingMirror.cpp:56`). ② `DoorHandleSide`가 `None`이 아닌 `AVBoRAxisPart`를 어느 `AVBoRHingedPanel`도 손잡이로 주장하지 않으면(`GetHandleClaims` `VBoRHingedPanel.h:137` — 부착 후보 + 칸 지정) 문제 한 줄: Grip이면 "잡기 꺼짐", Outside·Inside면 "문 잠금을 풀지 못함". ③ 레벨의 모든 `AVBoRHingedPanel`에 대해 `EvaluateSetup()`(로그 없음, R7 D8)의 Problems·Notices를 패널 이름을 붙여 그대로 포함 — 문마다 버튼을 누르지 않아도 되게. |
| D7 | **Grip이 끼인 칸 불일치** | `AVBoRHingedPanel::EvaluateSetup`(`VBoRHingedPanel.cpp:247-250`)에서 칸에 직접 넣은 부품의 `DoorHandleSide`가 `None`이 아니고 칸과 다르면 문제. **기존 두 경우(바깥 칸에 Inside, 안쪽 칸에 Outside)는 문구 그대로**(`"%s slot %s has the opposite DoorHandleSide; slot wins."`). 새 경우(바깥·안쪽 칸에 Grip, 그립 칸에 Outside·Inside)는 새 문구 — 칸 이름, 부품 이름, 부품의 side, "slot wins". 한 칸에 문제는 한 줄. 이 함수는 문 `Check setup`과 BeginPlay 경고(`:387` `P2e setup:`)가 같이 쓰므로 둘 다 따라온다(의도). 동작(slot wins)은 그대로. |
| D8 | **출력** | 검사 끝에 헤더 한 줄(`<Runner 이름> setup check: <문제 수> problems, <알림 수> notices`, Log) 뒤에 **문제 하나·알림 하나마다 로그 한 줄**(문제 = Warning, 알림 = Log) — Output Log 검색 필터가 여러 줄 로그의 아랫줄을 숨기는 문제(HANDOFF) 때문. `SetupReport`에는 같은 줄을 줄바꿈으로 모은다. 문제·알림이 없으면 `No issues`. 로그 범주는 Runner가 쓰는 범주(구현자). |

## 1. 왜 하는가

- 회사 PC에서 Start blocked가 나면 Play → 로그 → 추측을 반복했다. 에디터에서 버튼 하나로 원인(어느 BP의 어느 칸)을 본다.
- 조용히 동작하지 않는 조립(미러·그립 부착, 칸 불일치)을 Play 전에 잡는다.
- 다른 차량을 조립할 팀원(요구 3)의 첫 점검 도구가 된다.

## 2. 범위

### 만들 것

- D1–D8. 시험 `vBoR.R5.*`.

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| Play 중 새 경고(미러·그립 부착 등) | 기존 시험의 경고 수 단언(`P5qPowerMirrorTests.cpp:243,249,259` — `Side mirror:` 접두 경고를 센다 등)이 깨진다. 이번에는 에디터 검사만. D7은 기존 `EvaluateSetup` 경로라 예외 |
| `InitializeFlow`의 문구·순서·`Start blocked` 기록 형식 변경 | P3 흐름 시험들이 문구를 단언한다 |
| 클래스 BP 컴포넌트 태그, `BoxClass` 태그 | D3 |
| `Car.Liftgate`·`Car.DriverDoor` 등 다시 시작용 태그 검사 | 계획 R5 범위 밖. 필요하면 따로 |
| 자동 수정(태그 지우기, 부착 바꾸기) | 보고만 한다 |
| 다른 부품(AxisPart·PoweredPanel 등)에 `Check setup` 버튼 추가 | Runner 하나로 모은다 |
| `L_Main`·BP 값 변경 | 에디터 작업 없음 |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- `InitializeFlow`의 동작·`ValidationProblems` 문구·순서, `Start blocked` 기록, Play 때 태그 수(`FindP3Tag`)는 그대로.
- `CheckSetup`은 Runner 상태·월드를 바꾸지 않는다(D1). Play 중 눌러도 진행이 바뀌지 않는다.
- D7의 기존 두 경우 문구와 "한 칸에 한 줄", slot wins 동작.
- 미러·AxisPart·PoweredPanel·KeyFob·KeyTable·전원 컴포넌트의 Play 동작과 로그는 그대로.
- **기존 시험 감사(Claude 2026-11-02, 코드 대조):**
  - **D7(`EvaluateSetup`)이 닿는 시험:** 칸에 직접 부품을 넣는 시험을 모두 대조했다 — `P2ePartAssemblyTests.cpp` E7E9(`:176-188`, 바깥 칸에 Outside / 바깥 칸에 Inside → 문제 1개, 문구 그대로), `:211`(Outside를 바깥 칸), `:286`(바깥 칸), `R7HandleNoPushZoneTests.cpp:48-57`(side = 칸), `P5zeDoorGripTests.cpp:232`(Grip을 그립 칸), `P5aGloveboxTests.cpp:44`·`P2c`·`P2d`·`P4i`·`P5q`(바깥 칸에 Outside 또는 BP). **새 불일치 경우(Grip이 끼인 경우)를 만드는 기존 시험은 없다** → 문제 수·`P2e setup:` 경고 수(`P2e`·`R7 E11 :264`) 그대로.
  - **D1–D6:** 새 함수만 더한다. 기존 Runner·미러·AxisPart 경로를 바꾸지 않으므로 P3·P4·P5 흐름 시험, P5q 경고 수, P4i 설정 비교(`P4iCarBlueprintTests.cpp:93-104` — 새 `SetupReport`는 Transient·빈 값이라 BP와 부모가 같다) 그대로.
  - **고쳐도 되는 단언: 없음.** 고쳐야 통과하면 멈추고 리뷰 파일 "질문"에 적는다.

## 4. 시험 — `vBoR.R5.*`

에디터 월드(R2a·R2c 시험의 `EWorldType::EditorPreview` 방식, BeginPlay 없음)에서 Runner·부품을 놓고 `CheckSetup`을 부른다. 태그를 클래스 기본값에 가진 버튼·키는 시험 파일 안 시험용 하위 클래스(생성자에서 `Tags.Add`)로 만든다. 결과는 `SetupReport`의 줄과 로그(`AddExpectedMessage` 또는 로그 포착)로 본다. **공통 구성:** 정렬 컴포넌트 1개(없으면 `Need 1 alignment component`가 모든 시험에 더해진다), 전원 컴포넌트 1개 + `Car.StartButton` 버튼 1개(D5 알림이 섞이지 않게) — E7·E13은 시험이 정한 대로 바꾼다. 아래 "문제 없음"은 **그 시험이 보는 대상에 대한 줄이 없다**는 뜻이다(문 기본 형상 등 다른 줄은 세지 않는다). 문구는 핵심 조각(태그·개수·Actor 이름·칸 이름)을 `Contains`로 본다 — D2의 기존 문구는 정확히.

| ID | 상황 | 확인 |
|---|---|---|
| E1 | 실제 사례 재현: `Car.ReachButton` 태그 버튼 1개 배치 + 리프트게이트 `OpenButtonClass`·`CloseButtonClass` = 클래스 기본 태그 `Car.ReachButton`인 버튼 클래스, 단계 `01` ButtonPressed `Car.ReachButton` | `Tag Car.ReachButton: found 3 (need 1)` 한 줄(배치 1 + 칸 2). D4 설명 2줄(리프트게이트 이름 + `OpenButtonClass`/`CloseButtonClass` + 클래스 이름 + 태그). 공통 구성에서 문제 수 = 4(태그 수 1 + 설명 2 + 종류 불일치 1 — `FindP3Tag`가 개수 ≠ 1이면 대상을 null로 돌려주므로(`VBoRScenarioRunner.cpp:73`) Play 때처럼 `01: tag Car.ReachButton type mismatch or missing`도 나온다) |
| E2 | E1에서 두 칸을 비움 | 태그·설명 문제 없음(found 1, 종류 맞음) |
| E3 | 키 테이블 `KeyClass` = 클래스 기본 태그 `Car.FrontKey`인 키 클래스, 단계 `01` KeyHeld `Car.FrontKey` | 태그·종류 문제 없음(D3 ①), D4 설명 없음. 같은 키 클래스의 `UnlockButtonClass` = 태그 `Car.FrontKey`인 버튼 클래스로 바꾸면 → `Tag Car.FrontKey: found 2 (need 1)` + `01: tag Car.FrontKey type mismatch or missing` + 설명 1줄(키 클래스 이름 + `UnlockButtonClass`) |
| E4 | 태그 0개(단계 `01` ButtonPressed `Car.Missing`) | `Tag Car.Missing: found 0 (need 1)`과 `01: tag Car.Missing type mismatch or missing` — `InitializeFlow`와 같은 문구 |
| E5 | 생성 칸 없는 Game 월드에서 BeginPlay 전 `CheckSetup` → 같은 월드 Play 뒤 `InitializeFlow`의 `ValidationProblems`(정렬 컴포넌트 없음·태그 2개·종류 불일치·잘못된 StepId·표식 종류 불일치를 섞은 구성) | 정렬 컴포넌트 수·태그 수·종류·StepId·표식·패널·경로 문제 줄이 같다(같은 순서). 정렬 상태 메시지는 `CheckSetup`에 없음 |
| E6 | Play 중 월드(`HasBegunPlay`)에서 E1과 같은 구성(버튼이 실제 생성됨) | `found 3`(실제 Actor) — 클래스 기본값을 **한 번 더 더하지 않는다**(5가 아님). D4 설명은 그대로 나온다 |
| E7 | `Car.StartButton`: 버튼 0개 / 버튼 2개 / 버튼 1개 + `AVBoRPokeButton` 아닌 Actor 1개. 전원 컴포넌트 0개 / 2개 | 0개: 알림 "operator power only". 2개: 줄 없음. 아닌 Actor: 문제 1줄(Actor 이름). 전원 0개: 알림, 2개: 문제 |
| E8 | 미러: 문(`AVBoRHingedPanel`) 밑 / 차 Actor 밑 / 부착 없음 | 문 밑: 그 미러 줄 없음. 나머지 둘: 미러마다 문제 한 줄(미러 이름) |
| E9 | `DoorHandleSide` Grip 부품: 문 밑(자동 연결) / 문 밖 + 칸 지정 / 문 밖 + 지정 없음. Inside 부품 문 밖 지정 없음. `None` 부품 문 밖 | 처음 둘: 그 부품 줄 없음. Grip 연결 없음: "잡기 꺼짐" 문제. Inside 연결 없음: "잠금을 풀지 못함" 문제. `None`: 줄 없음 |
| E10 | D7: 바깥 칸에 Grip, 안쪽 칸에 Grip, 그립 칸에 Inside, 그립 칸에 Outside(각각 따로) | 문 `EvaluateSetup` Problems에 새 문구 1줄(칸·부품·side), 동작은 slot wins(그 칸에 연결). Play 때 그 문구의 `P2e setup:` 경고 1줄(문의 다른 경고는 세지 않음). 기존 두 경우(바깥 칸 Inside, 안쪽 칸 Outside)는 기존 문구 1줄 그대로 |
| E11 | Runner `CheckSetup`에 D6 ③ | 문의 E10 문제가 문 이름과 함께 Runner 결과에도 나온다 |
| E12 | 부작용 없음 | `CheckSetup` 전후로 월드 Actor 수, Runner 진행 기록·`IsRunning`·`Targets` 수, 리프트게이트 버튼 델리게이트·키 바인딩 수가 같다. Play 중 월드(E6)에서 눌러도 현재 단계가 바뀌지 않는다 |
| E13 | D8 출력 | 헤더 1줄 + 문제마다 Warning 1줄 + 알림마다 Log 1줄. `SetupReport` 줄 수 = 문제 + 알림. 공통 구성 + Confirm 단계 하나(`01`, 태그 없음)뿐이면 `SetupReport == "No issues"`, Warning 0줄 |

## 5. 구현자가 정할 것

- `InitializeFlow`와 검사 코드를 공유하는 방법(공통 함수 + 부작용 분리 등). 단 §3의 Play 문구·순서는 그대로.
- D3 생성 칸 개수를 태그 수에 더하는 방법(예: `FindP3Tag`에 추가 출처 목록을 넘기는 별도 함수).
- D4–D7의 새 문구(영어, 핵심 조각 포함), 로그 범주.
- `GetHandleClaims`를 Runner에서 부를 방법(접근 지정 변경 또는 공개 래퍼).
- 새 시험 파일 위치(`P3Probe/` 권장).

## 6. 완료 조건

1. D1–D8 구현, §3 조건 지킴.
2. `vBoR.R5.E1`–`E13` 통과, 기존 시험 변경 없이 통과.
3. non-unity(`-Module=vBoR`)·unity 빌드 성공, `vBoR.*` 전체 통과(작업 끝에 한 번).
4. 새 주석·로그·표시 이름 영어(AGENTS.md).

## 7. 로컬에서 확인할 것

- 빌드: 위 3.
- 에디터(사용자): `L_Main`의 Runner Details → `Check setup`. ① 지금 레벨에서 결과가 `No issues` 또는 알 수 있는 항목만인지(미러가 문 밑에 있는지, 그립이 연결됐는지) ② 시험 삼아 버튼 BP Class Defaults에 시나리오 태그를 넣고 리프트게이트 버튼 칸에 넣은 뒤 `found 3` + 설명 2줄이 보이는지(확인 뒤 되돌림) ③ Output Log에서 `setup check`로 검색해 줄마다 보이는지.
- Quest 3: 없음.
