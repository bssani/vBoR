# P5zc 생성 버튼 클래스 — 리프트게이트·키 버튼을 BP로 고르기

| 항목 | 값 |
|---|---|
| 단계 | P5 리허설 준비 — 회사 CAD 넣기(사용자 요청 2026-10-27). 시험 T-02(박스 적재 — 리프트게이트)·T-10(키) 보강 |
| 결정 근거 | **사용자(2026-10-27):** "liftgate나 lever 같은 경우 버튼을 동적으로 생성하는데, 내가 원하는 버튼을 생성하게끔 할 수 없나?" → "리프트게이트는 여는 버튼, 닫는 버튼 두 개 다 해야 해. 그리고 키 버튼도 두 개 해야 해." "버튼만 liftgate 같이 넣을 수 있게 하자."<br>**Claude 확인:** 기어 끝 버튼은 이미 Child Actor(`BP_GearButton`)라 바꿀 수 있다(Blueprint-Guide §6.2). 리프트게이트(`VBoRPoweredPanel.cpp:67`)와 키(`VBoRKeyFob.cpp:83`)는 `AVBoRPokeButton::StaticClass()`를 고정으로 만든다 |
| 베이스 브랜치 | `main` @ 이 명세 머지 뒤 |
| 작업 브랜치 | `codex/P5zc-spawned-button-class` |
| 리뷰 파일 | `Docs/Reviews/P5zc-spawned-button-class.md` (첫 리뷰 때 생김) |
| 상태 | 명세 — 구현 전 |
| 에디터 작업 | **없음** (버튼 BP는 사용자가 만든다) |
| 앞 명세 | [P5zb-floor-marker-height.md](P5zb-floor-marker-height.md) |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **버튼마다 클래스 칸 하나, 모두 4개** | 이 문서에서 부르는 이름이다. 실제 이름은 구현자가 정한다.<br>- `AVBoRPoweredPanel`: `OpenButtonClass`, `CloseButtonClass`. Category `Powered Panel\|Buttons`(자리 칸 `VBoRPoweredPanel.h:94-96` 옆)<br>- `AVBoRKeyFob`: `UnlockButtonClass`, `LockButtonClass`. Category는 `Key`<br>- 타입은 `TSubclassOf<AVBoRPokeButton>`, **기본값은 비어 있음**. 표시 이름은 영어(예 `Open button class`), 툴팁 예: `Empty = default cube button. Set = this BP's mesh/size/material/press depth kept.` |
| D2 | **비어 있으면 지금과 완전히 같다** | 칸이 비었거나 고를 수 없는 클래스(추상 클래스 등)면 `AVBoRPokeButton::StaticClass()`로 지금 코드 그대로 만든다. 크기(`ButtonSizeCm`), 키의 누름 깊이 1 mm·색 재질까지 지금과 같다. 고를 수 없는 클래스면 Warning 한 줄을 남긴다(어느 Actor의 어느 칸인지) |
| D3 | **고른 BP는 BP 값을 그대로 쓴다** | 클래스를 고르면 다음을 **덮어쓰지 않는다**: 버튼 메시 크기(지금 `ButtonSizeCm`으로 덮어씀 `VBoRPoweredPanel.cpp:69`·`VBoRKeyFob.cpp:84`), 키의 누름 깊이(`:85`), 키의 색 재질(`:91-96`), `HitAreaScale`, 메시 에셋. BP에서 메시를 넣고 크기를 정했으므로 덮으면 찌그러진다 |
| D4 | **고른 BP에도 늘 하는 것** | 동작에 필요한 것은 클래스와 상관없이 지금처럼 한다.<br>- **자리:** 리프트게이트는 `…ButtonFromHinge` × 경첩(`:66`), 키는 `UnlockButtonCm`·`LockButtonCm`(`VBoRKeyFob.cpp:81-82`). 생성 Transform에 들어가므로 BP 루트의 크기는 Actor 크기 1로 생성한다(지금과 같음)<br>- **붙이기:** 리프트게이트 `HingePivot`(`:72`), 키 `KeyMesh`(`:90`), 월드 자세 유지<br>- **소리:** 판·키의 `ButtonPressedCue`·`ButtonReleasedCue`를 버튼에 복사(`:70`, `:87`). 소리를 정하는 곳은 지금처럼 판·키 하나다(Blueprint-Guide §4.4)<br>- **키만:** `PressFilter` 연결(`:86`), 생성 직후 `Poke` 끔(`:89`) — 들었을 때만 눌림(P4a)<br>- **이벤트 연결:** 리프트게이트 `OnButtonPressed` → 열기·닫기(`:82-83`), 키는 지금 연결 그대로 |
| D5 | **관전 화면 이름은 비운다** | 생성한 버튼 4개의 `SpectatorName`은 클래스와 상관없이 빈 값으로 둔다. 키 버튼이 TV 띠에 따로 뜨지 않게 하는 지금 동작(`vBoR.P5h` "Key child buttons unnamed")을 BP가 바꾸지 않게 한다 |
| D6 | **버튼 BP의 자리 규칙은 지금 버튼과 같다** | Actor 원점 = 누르는 면 가운데, Actor +Z = 누르는 면이 바라보는 쪽(바깥). 근사 버튼과 같다(`VBoRPoweredPanel.h:93` "Button top +Z faces user"). Blueprint-Guide에는 머지 뒤 Claude가 적는다 |

## 1. 왜 하는가

회사 CAD에서 리프트게이트 열기·닫기 버튼과 키 버튼을 실제 형상으로 보여 주려면 메시를 바꿔야 한다. 지금은 엔진 Cube를 크기만 바꿔 만들기 때문에 바꿀 방법이 없다. 박스 받침의 `BoxClass`(P5t)와 같은 방식으로 BP를 고르게 한다.

## 2. 범위

### 만들 것

- `AVBoRPoweredPanel`·`AVBoRKeyFob`: D1–D5.
- 시험 `vBoR.P5zc.*`(§4).

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| 기어 끝 버튼 | 이미 Child Actor로 바꿀 수 있다 |
| 버튼 BP 에셋 만들기, `L_Main`·`BP_Liftgate`·`BP_KeyFob_*` 값 바꾸기 | 사용자가 회사에서 한다. 기본은 빈 칸(지금과 같음) |
| Blueprint 이벤트 확장(웰컴 라이트, 키 들고 접근 시 미러 펼침 등), BP Event Graph 허용 규칙 변경 | **mid review(2026-10-28) 뒤로 미룸**(사용자 2026-10-27). 따로 결정·명세 |
| 버튼 동작(누름 판정, 깊이 계산, 소리 재생 규칙) 변경 | 그대로 |
| 버튼 클래스 칸을 다른 부품(시나리오 확인 버튼 `VBoRScenarioRunner.cpp:155` 등)에 추가 | 사용자 요청 범위가 리프트게이트·키 |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- **칸이 비어 있으면 결과가 지금과 같다(D2).** 생성 클래스, 크기, 누름 깊이, 색, 자리, 붙이기, 소리, 이벤트.
- **기존 시험 감사 — 고치지 않는다.** 생성 버튼을 보는 시험은 모두 빈 칸(기본) 상태로 돈다: `P1Probe/P4bPoweredPanelTests.cpp`(`:88`·`:112`·`:138-173`·`:271-292`·`:352`), `P3Probe/P4aKeyTableTests.cpp`(`:70-71`·`:116-117`·`:150-194`·`:245-290`·`:327-368`), `P3Probe/P4iCarBlueprintTests.cpp:179-181`(근사 차와 BP 버튼 컴포넌트 비교·자리), `P1Probe/P5rSoundCueTests.cpp:54`, `P3Probe/P5sPartSoundTests.cpp:183-187`, `P3Probe/P5hSpectatorTests.cpp:366`. `BP_Liftgate`·`BP_KeyFob_*`의 새 칸도 비어 있으므로 P4i 비교가 같다(Claude 감사 2026-10-27, `OpenButton|CloseButton|UnlockButton|LockButton` 전체 검색). **이 파일들은 고치지 않는다.** 하나라도 고쳐야 통과하면 멈추고 리뷰 파일 "질문"에 적는다.

## 4. 시험 — `vBoR.P5zc.*`

고를 클래스는 시험 전용 C++ 하위 클래스(`AVBoRPokeButton` 자식, 시험 파일·Probe 폴더 안)로 만든다. 생성자에서 메시 크기 `(0.07, 0.05, 0.02)`, `PressDepthMm` 4, `HitAreaScale` 1.5, `SpectatorName` "Test"처럼 기본과 다른 값을 둔다. 리프트게이트는 `P4bPoweredPanelTests.cpp`, 키는 `P4aKeyTableTests.cpp`의 world·poke 방식을 따른다(새 파일에 같은 도우미).

| ID | 상황 | 확인 |
|---|---|---|
| E1 | **리프트게이트 열기만 고름** — `OpenButtonClass` = 시험 클래스, 닫기는 빈 칸 | 열기 버튼은 시험 클래스, 메시 크기·`PressDepthMm`·`HitAreaScale`은 시험 클래스 값. 닫기 버튼은 기본 클래스, 크기 = `ButtonSizeCm`/100. 두 버튼의 경첩 기준 자리 = `…FromHinge`(지금과 같음) |
| E2 | **리프트게이트 둘 다 고름, 동작** — 두 칸 모두 시험 클래스. 열기 버튼 poke → 열림, 닫기 버튼 poke → 닫힘 | 상태 변화가 기본 버튼과 같다. 판의 `ButtonPressedCue`가 두 버튼에 복사됨. 판이 열린 뒤에도 버튼이 경첩에 붙어 같은 상대 자리 |
| E3 | **키 둘 다 고름** — `UnlockButtonClass`·`LockButtonClass` = 시험 클래스 | 두 버튼이 시험 클래스, 크기·누름 깊이 = 시험 클래스 값, 색 재질을 덮지 않음(메시 재질 0번이 동적 재질이 아님). 자리 = 키 루트 + `UnlockButtonCm`·`LockButtonCm`(회전 포함) |
| E4 | **키 동작 그대로** — E3 상태. 들기 전 poke → 무시, 들고 poke → `OnUnlockPressed`·`OnLockPressed` 1번씩, 놓으면 다시 무시 | `PressFilter`·`Poke` 활성 규칙이 기본 버튼과 같다(P4a) |
| E5 | **이름 비움** — E1·E3 | 생성 버튼 4개 `SpectatorName` 빈 값 |
| E6 | **고를 수 없는 클래스** — 칸에 추상 클래스(시험용 `Abstract` 자식) | 기본 버튼이 생기고 Warning 한 줄(Actor 이름·칸 이름 포함) |
| E7 | **빈 칸은 그대로** — 네 칸 모두 비움 | 생성 클래스 = `AVBoRPokeButton`, 크기 = `ButtonSizeCm`/100, 키 누름 깊이 1, 키 색 재질 있음 |

- 수치는 위를 기준으로 하되, 구현자가 같은 상황을 만드는 다른 값을 쓰면 보고 ③에 적는다.

## 5. 구현자가 정할 것

- 칸 이름·표시 이름·툴팁 문구(영어).
- 클래스 고르는 도우미를 두 Actor가 같이 쓸지 따로 둘지.
- 시험 전용 하위 클래스를 둘 위치(Probe 폴더 안).

## 6. 완료 조건

1. D1–D6 구현(D6은 주석 한 줄), `vBoR.P5zc.*` E1–E7 통과.
2. `vBoR.*` 전체 통과, 기존 시험은 고치지 않고 결과 그대로(개수 = 기존 + P5zc 시험 수).
3. non-unity(`-Module=vBoR`)·unity 빌드 성공, vBoR 경고 0.
4. 주석·표시 이름·툴팁·로그는 영어(AGENTS.md).
5. 커밋 본문 보고 ①–⑤.

## 7. 로컬에서 확인할 것

- **빌드·시험:** 끝에 한 번(AGENTS.md 토큰 규칙).
- **에디터(사용자, 머지 뒤):** `AVBoRPokeButton` 부모로 버튼 BP를 만들고(D6 자리 규칙), `BP_Liftgate`의 열기·닫기 칸, `BP_KeyFob_Front`·`_Rear`의 열림·잠금 칸에 고른다.
- **Quest 3(T-02·T-10, `not-tested`):** 바꾼 버튼이 보이는 자리·크기, 누를 때 들어가는 깊이, 키를 들었을 때만 눌리는지, 리프트게이트가 열리고 닫힐 때 버튼이 따라가는지.

## 개정 이력

| 날짜 | 내용 |
|---|---|
| 2026-10-27 | 처음 작성 |
