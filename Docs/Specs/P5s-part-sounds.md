# P5s 부품 소리 칸 — 문·버튼·레버·노브·핸들·박스·시동·사이드미러

| 항목 | 값 |
|---|---|
| 단계 | P5 자유모드·리허설 준비(사용자 요청 2026-10-21). 시험 T-04·T-08 |
| 결정 근거 | **사용자(2026-10-21):** "오디오를 수정할 수 있어야 해. 단순히 오디오 파일만 수정하는 게 아니라, 타이밍 이런 것도." 범위 답: **리프트게이트, 문·콘솔·글러브박스, 버튼·레버·노브·핸들·박스, 시동·사이드미러 전부.** 타이밍 방식 답: **이벤트별 소리 칸.**<br>리프트게이트와 공통 구조는 P5r(PR #57)에서 끝났다. 이 명세는 같은 구조를 나머지 부품에 넣는다 |
| 베이스 브랜치 | `main` @ 이 명세 머지 뒤 |
| 작업 브랜치 | `codex/P5s-part-sounds` |
| 리뷰 파일 | `Docs/Reviews/P5s-part-sounds.md` (첫 리뷰 때 생김) |
| 상태 | 명세 — 구현 전 |
| 에디터 작업 | **없음.** BP·레벨은 바꾸지 않는다. BP에는 새 칸이 기본값으로 보인다 |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **P5r 구조를 그대로 쓴다** | `FVBoRSoundCue`(순간)·`FVBoRHeldSoundCue`(지속)·`FVBoRSoundPlayback`(`Interaction/VBoRSoundCue.h`). P5r D2의 재생 규칙(frame 시간, 무음 칸은 요청 0, 상태가 끝나면 지속 소리 멈춤, 초기화는 바로 멈춤)을 그대로 지킨다.<br>- 칸마다 오디오 컴포넌트 하나다(다른 칸끼리 안 끊음). 컴포넌트는 그 부품의 움직이는 쪽에 붙인다(지금 소리 컴포넌트가 붙은 자리, §8).<br>- 새 구조·관리자·서브시스템은 만들지 않는다 |
| D2 | **P5r 규칙 하나를 고친다 — 지연 0인 순간 소리는 이벤트마다 실제로 낸다** | **지금:** `FVBoRSoundPlayback::Play`가 같은 칸의 실제 `Play()`를 frame당 한 번으로 합친다(`VBoRSoundCue.cpp:12-14`).<br>**문제:**<br>- 시험이 `Tick` 없이 콜백만 부르는 부품(버튼·전원)은 frame 값이 바뀌지 않는다. 그러면 두 번째 누름부터 실제 소리가 안 난다.<br>- 지금 1축 부품은 한 `Tick`에 걸림 알림이 둘이면 `Play()`를 두 번 부른다. 그것을 세는 기존 시험이 있다(§3).<br>**고칠 것:**<br>- 지연 0인 순간 소리(`Queue`에서 바로 재생)는 같은 frame이라도 부를 때마다 실제로 `Play()`한다.<br>- frame당 한 번 규칙은 **긴 frame에서 반복·지연이 여러 번 만료될 때**(`Advance`)에만 남긴다.<br>- P5r 시험은 그대로 통과해야 한다(P5r E11은 지속 칸만 본다) |
| D3 | **지금 소리가 나는 이벤트는 기본값이 지금 소리다 — 들리는 것은 그대로** | 지금 소리는 모두 ISDK poke 누름 소리(`IsdkContentAssetPaths::Audio::Interaction_BasicPoke_ButtonPress`)다. 그 소리가 기본값이고 지연은 0이다. 같은 순간에 같은 횟수로 난다.<br>**새 이벤트는 무음이 기본이다.**<br>**예외 하나 — 박스 보류음(D9):** 지금은 손 추적을 잃은 동안 frame마다 다시 낸다(`VBoRCarryObject.cpp:576`, 매 `Tick`의 `UpdateVisual`). frame 수에 따라 바뀌므로(AGENTS.md frame 시간 원칙) 지속 칸 반복 0.25 s로 바꾼다 |
| D4 | **버튼(`AVBoRPokeButton`)** | 칸: `PressedCue`(기본 지금 소리), `ReleasedCue`(기본 무음).<br>- **누름 소리는 Select마다 난다. `PressFilter`와 상관없다.** 지금 ISDK 재생기와 같다(`VBoRPokeButton.cpp:80`). 키를 쥔 손의 거부된 누름도 지금처럼 소리가 난다(P4a 리뷰 R1-1 뒤 결정). 안 쥔 키는 `Poke`가 꺼져 Select가 없다.<br>- 놓음 소리는 Unselect마다 난다.<br>- **ISDK 재생기(`PointerAudio`)에 Select 소리를 더 넣지 않는다**(두 번 나지 않게). `PointerAudio` 컴포넌트와 `SetPointable`은 그대로 둔다. P4i D7의 미리보기 world 보호가 그 경로를 본다(`VBoRPokeButton.cpp:73-80`).<br>- 버튼은 지금 `Tick`이 없다. 지연이 있는 소리가 예약됐을 때만 `Tick`을 켜도 된다(버튼이 많아서). 방법은 구현자가 정한다.<br>- `PressAudio` 컴포넌트는 `PressedCue`의 컴포넌트로 다시 쓰는 것을 권장한다(BP 3개 `BP_GearButton`·`BP_ReachButton`·`BP_StartButton`의 컴포넌트 목록이 덜 바뀜) |
| D5 | **버튼을 스스로 만드는 부품** | 리프트게이트(`VBoRPoweredPanel.cpp:56`)와 키(`VBoRKeyFob.cpp:75-85`)는 Play 때 기본 `AVBoRPokeButton`을 만든다. 그래서 BP에서 그 버튼 소리를 고칠 수 없다.<br>- 두 부품에 `ButtonPressedCue`·`ButtonReleasedCue`를 둔다(기본: 지금 소리·무음).<br>- 버튼을 만들 때(`FinishSpawning` 전) 그 버튼의 `PressedCue`·`ReleasedCue`로 넣는다. 두 버튼(열기·닫기, 열림·잠금)이 같은 칸을 쓴다 |
| D6 | **1축 부품(`AVBoRAxisPart`)** — 손잡이·레버·선바이저 | 칸: `OpenedCue`·`ClosedCue`(열림·닫힘 기준 도달, `VBoRAxisPart.cpp:589-590`), `DetentCue`(걸림 바뀜), `NudgeCue`(차선 변경 걸림, `:504-508`). 모두 기본 지금 소리.<br>- 걸림·차선 변경은 지금처럼 `Tick`에서 알림과 같은 순간에 낸다(P5b R1-1 — 이동 콜백 안에서 내지 않음).<br>- 열림·닫힘은 지금 자리(`EvaluateState`)에서 낸다 |
| D7 | **경첩 판(`AVBoRHingedPanel`)** — 문·콘솔·글러브박스 | 칸: `UnlockedCue`(풀림), `LatchedCue`(잠김). 기본 지금 소리. 자리는 지금과 같다(`VBoRHingedPanel.cpp:575-576`). 문(`AVBoRHingedDoor`)은 상속으로 같이 받는다 |
| D8 | **노브·스티어링 휠** | 노브 `StepCue`(칸 넘김, `VBoRKnob.cpp:147`·`:168`). 휠 `LimitCue`(한계 도달, `VBoRSteeringWheel.cpp:221-224`). 둘 다 기본 지금 소리, 지금처럼 `Tick`에서 낸다 |
| D9 | **박스(`AVBoRCarryObject`)** | 칸: `BlockedCue`(순간, 막힘에 들어간 순간 `VBoRCarryObject.cpp:377`, 기본 지금 소리), `PausedCue`(**지속**, 손에 든 채 추적 보류인 동안, 기본 지금 소리·`RepeatSeconds` 0.25).<br>- 보류가 풀리거나(복구·놓음) 박스를 초기화하면 멈춘다.<br>- 0.25 s는 근사 값이다. 지금처럼 이어진 신호로 들리되 frame 수와 상관없게 한 값이다. 팀이 바꾼다 |
| D10 | **사이드미러(`AVBoRFoldingMirror`)** — 지금 무음 | 칸: `UnfoldMotorCue`·`FoldMotorCue`(지속, 펼치는 중·접는 중), `UnfoldedCue`·`FoldedCue`(순간, 펼침·접힘 도착). 모두 무음이 기본이다.<br>- **움직이는 중에 방향이 바뀌면**(P5q D5) 앞 방향 모터를 그 칸의 페이드대로 끝내고, 새 방향 모터를 같은 frame에 시작한다.<br>- **`ResetMirror`(BeginPlay·R)는 소리를 내지 않는다.** 예약을 지우고 바로 멈춘다. 그래서 Play를 시작할 때나 R에서 접힘 소리가 나지 않는다.<br>- R은 전원을 끈 직후 미러를 초기화한다(`VBoRScenarioRunner.cpp:521-522`). 전원 꺼짐 이벤트로 시작된 접는 모터도 `ResetMirror`가 바로 멈춘다.<br>- 소리 자리는 미러 하우징이다(움직이는 쪽) |
| D11 | **전원(`UVBoRVehiclePowerComponent`)** — 지금 무음 | 칸: `PowerOnCue`·`PowerOffCue`(순간, 기본 무음). 시동음·꺼짐음이다.<br>- 상태가 실제로 바뀔 때만 난다(`OnPowerChanged`와 같은 순간). 같은 frame 요청은 하나로 합쳐지므로(P5q D4) 소리도 한 번이다.<br>- **R(`EVBoRPowerSource::Restart`)은 소리를 내지 않는다.** 예약을 지우고 바로 멈춘다. 전원이 없거나 둘이면(사용 불가) 소리도 없다.<br>- 컴포넌트에는 `Tick`이 없다. 지연이 있을 때만 켜도 된다.<br>- 소리 자리는 소유 Actor(`BP_Car_Core`)의 루트, 곧 차 원점이다. 부품 쪽에서 들려야 하면 `Attenuation`으로 조정한다 |
| D12 | **시험용 접근** | 부품마다 칸의 요청 수·frame과 오디오 컴포넌트를 시험이 읽고 바꿀 수 있어야 한다. P5r처럼 `WITH_DEV_AUTOMATION_TESTS` 안에 둔다(`VBoRPoweredPanel.h` `GetSoundPlaybackForTest`, `FP5rTestAccess`) |

## 1. 왜 만드는가

- P5r로 리프트게이트 소리는 BP에서 고칠 수 있게 됐다. 나머지 부품은 아직 ISDK 딸깍 하나가 코드에 박혀 있거나(손잡이·레버·문·휠·박스), 파일만 바꿀 수 있다(버튼·노브).
- 시동과 사이드미러는 소리가 없다.
- 팀이 실제 차 소리와 타이밍을 BP에서 맞출 수 있게 한다.

## 2. 범위

### 만들 것

1. `FVBoRSoundPlayback` 규칙 하나 고침(D2).
2. 소리 칸: 버튼(D4), 게이트·키의 버튼 칸(D5), 1축 부품(D6), 경첩 판(D7), 노브·휠(D8), 박스(D9), 사이드미러(D10), 전원(D11).
3. 기존 시험의 관찰 도우미만 고침(§3).
4. 자동화 시험 §4(`vBoR.P5s.*`).

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| 룸미러·2열 시트·키(쥐기·떨어짐)·문 여닫는 움직임의 새 소리 | 지금 소리가 없고, 사용자가 고른 범위에 없다 |
| 키 버튼에 맞춘 차량 소리(열림·잠김 삑) | 01·02 단계 에셋 규칙은 사용자 몫이다(U-10 세부). 키 이벤트는 이미 있다 |
| 각도 지점 소리 | 사용자 답(P5r ②) |
| hover 소리 | 요청 없음 |
| 움직임·판정·이벤트·색 바꾸기 | §3 |
| BP·`L_Main` 바꾸기 | 에디터 작업 없음 |
| 소리 파일 만들기 | 팀이 한다 |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- **지금 들리는 소리는 그대로다(D3).** 같은 소리가 같은 순간에 같은 횟수로 난다. 예외는 박스 보류음뿐이다(D9).
- 움직임·잠금·걸림·판정·이벤트 순서와 횟수, 색, 입력 규칙이 그대로다.
- 걸림·한계·칸 넘김 소리는 지금처럼 `Tick`에서 낸다. 이동·SDK 콜백 안에서 새로 내지 않는다(P5b R1-1·P5d·P5f).
- P5r 시험(`vBoR.P5r.*`)은 그대로 통과한다(D2).
- **기존 시험 감사 — 바꿀 수 있는 것은 관찰을 연결하는 도우미뿐이다. 단언의 기대 수는 바꾸지 않는다.**
  - 도우미(본문만 바꿈): `P2Probe/P2dTestFixture.h:58-61` `FP2dTestAccess::ObserveAudio`, `P3Probe/P5bGearLeverTests.cpp:45-47`, `P3Probe/P5cTurnSignalTests.cpp:23`, `P3Probe/P5dSteeringWheelTests.cpp:29`, `P3Probe/P5mLeverTests.cpp:26`.
    - 지금은 부품의 `StateAudio` 하나를 관찰자로 바꾼다. 칸이 여럿이 되면 **관찰자 하나를 그 부품의 모든 칸에 끼운다.** 그러면 `Plays`가 지금처럼 그 부품의 소리 요청을 모두 센다(D2 덕분에 같은 frame 두 번도 2다).
  - 그대로 통과해야 하는 단언(줄 그대로):
    - `P2dLatchedPanelTests.cpp:127`·`:180`(소리 에셋이 있음 포함)·`:201`
    - `P5bGearLeverTests.cpp:143`·`:147`·`:169`·`:172`·`:177`·`:188`·`:192`·`:195`
    - `P5cTurnSignalTests.cpp:76-77`·`:96`·`:119`·`:149-150`
    - `P5dSteeringWheelTests.cpp:105`·`:107`·`:109`·`:242-243`
    - `P5mLeverTests.cpp:128`·`:130`
  - 바꿀 수 있는 단언 둘:
    - `P3Probe/P5fKnobTests.cpp:187` `K->StateAudio->Sound` → `StepCue.Sound`(뜻 같음: 칸 넘김 소리가 있음).
    - `P3Probe/P4iCarBlueprintTests.cpp:143` `GetSelectAudio() == PressAudio` → `GetSelectAudio() == nullptr`(D4 — ISDK 재생기는 이제 소리를 내지 않음). 같은 시험의 `:137`(미리보기는 연결 안 함)·`:142`(Game은 pointable 연결)는 그대로다.
- **P4i·P5p 시험:** `vBoR.P4i.E2`는 BP와 근사 차 부품의 편집 가능한 설정을 비교하고, `vBoR.P5p.E11`은 `AVBoRAxisPart`에 선언된 편집 가능한 프로퍼티를 옮겨 비교한다. 새 칸은 양쪽 다 기본값이라 같다. 두 시험의 단언은 바꾸지 않는다.
- frame마다 월드 전체를 훑지 않는다(P5o).
- `AVBoRPoweredPanel`의 칸 7개(P5r)는 그대로다. 게이트에는 버튼 칸 둘(D5)만 더한다.

## 4. 경계 상황과 그 시험 (`vBoR.P5s.*`)

시험 world에 부품 C++ 클래스를 직접 만든다. 칸에는 시험이 아무 소리나 넣는다. **실제 재생이 아니라 요청·frame·관찰자의 `Play`/`Stop`/`FadeOut`**을 본다(P5r D3).

| ID | 상황 | 확인 |
|---|---|---|
| E1 | 기본값 | D3 표대로다. 지금 소리 칸은 ISDK 소리·지연 0, 새 칸(`ReleasedCue`, 미러 4칸, 전원 2칸)은 무음, `PausedCue`는 지속·0.25 s다. 게이트·키의 `ButtonPressedCue`는 지금 소리다 |
| E2 | 들리는 것 그대로 | 부품마다 지금 소리 나는 이벤트를 한 번씩 일으킨다. 그 칸에 요청 1, 실제 `Play` 1이 지금과 같은 순간(같은 호출 또는 같은 `Tick`)에 난다. 다른 칸은 0이다 |
| E3 | 지연 0 연속(D2) | 버튼을 `Tick` 없이 두 번 누름 → 실제 `Play` 2. 1축 부품이 한 `Tick`에 걸림을 둘 넘음 → `DetentCue` `Play` 2. P5r 시험은 그대로 통과 |
| E4 | 지연 | 부품 종류마다 칸 하나에 지연 0.3 s(버튼 `PressedCue`, 1축 `OpenedCue`, 판 `LatchedCue`, 노브 `StepCue`, 휠 `LimitCue`, 박스 `BlockedCue`, 미러 `UnfoldedCue`, 전원 `PowerOnCue`). 이벤트 뒤 0.2 s까지 0, 0.3 s에 1. 버튼·전원은 그동안만 `Tick`이 켜져도 된다(끝나면 꺼져도 된다) |
| E5 | 버튼 | Select → `PressedCue` 1, Unselect → `ReleasedCue` 1. `PressFilter`가 거부해도 누름 소리는 난다(지금과 같음). ISDK `PointerAudio`의 Select 소리는 비어 있다(두 번 안 남). `Poke`가 꺼진 버튼은 소리가 없다 |
| E6 | 게이트·키 버튼 | 게이트의 열기·닫기 버튼과 키의 열림·잠금 버튼에 `ButtonPressedCue`·`ButtonReleasedCue`가 들어간다. 게이트 버튼 소리와 게이트 받음 확인음(P5r `OpenAcceptedCue`)은 따로 난다 |
| E7 | 1축 칸끼리 | 방향지시 레버에서 차선 변경 뒤 걸림이 한 `Tick`에 겹침(P5c 경우) → `NudgeCue` 1, `DetentCue` 1, 서로 끊지 않음. 열림·닫힘은 각자 칸 |
| E8 | 미러 | 전원 켬 → `UnfoldMotorCue` 시작, 도착 frame에 멈춤(페이드 칸대로), `UnfoldedCue` 1. 끔 → 접는 칸들. 펼치는 중 끔 → 펼침 모터 멈춤 + 접힘 모터 시작이 같은 frame. BeginPlay와 R에는 요청 0(전원 꺼짐 이벤트로 시작된 모터도 R에서 바로 멈춤). 전원이 없으면 소리 0 |
| E9 | 전원 | 버튼·진행자·직접 호출로 켜고 끄면 `PowerOnCue`·`PowerOffCue`가 바뀔 때마다 1. 같은 값 `SetPowerOn`, 같은 frame 두 요청 → 바뀐 만큼만. R → 요청 0, 지연 중인 시동음도 지움. 전원이 0개·2개면 0 |
| E10 | 박스 보류 | 손에 든 채 추적 보류 → `PausedCue` 시작, 보류 동안 0.25 s마다 요청. 복구·놓음·`ResetToPose`에서 멈춤. `Tick` 1.0 s 한 번과 0.1 s 열 번의 요청 수가 같다. 막힘에 들어갈 때 `BlockedCue` 1(막힌 동안 다시 안 남) |
| E11 | 초기화 | 판 `ResetDoor`, 미러 `ResetMirror`, 전원 R, 박스 `ResetToPose`, 그리고 모든 부품의 `EndPlay` → 지연 예약이 지워지고 바로 멈춘다(페이드 없음). 그 뒤 `Tick` 10 s 동안 요청 0. 초기화가 지금 내는 소리가 있으면 그대로다 |
| E12 | 기존 시험 | §3 감사대로 통과한다(도우미 본문과 허용한 두 줄만 바뀜) |

## 5. 구현자가 정할 것

- 칸·필드 이름(권장 이름과 다르면 보고 ③에 쓴다). Category는 부품의 기존 Category 아래 `|Sound` 권장.
- 오디오 컴포넌트를 생성자에서 만들지, BeginPlay에서 만들지. 지금 있는 소리 컴포넌트(`StateAudio`·`PressAudio`·`StepClick`·`LimitClick`·`LockAudio`)를 어느 칸에 다시 쓸지.
- 버튼·전원의 지연용 `Tick`을 켜고 끄는 방법.
- 시험에서 요청·frame을 읽는 방법(D12).

## 6. 완료 조건

1. D1–D12를 지킨다.
2. E1–E12가 통과한다.
3. 기존 `vBoR.*` 시험이 모두 통과한다. 바뀐 기존 시험은 §3에 적은 도우미 본문과 두 줄뿐이다.
4. non-unity·unity 빌드에서 vBoR 경고가 없다.
5. 빌드·전체 시험은 끝에 한 번(AGENTS.md 토큰 규칙). 에디터 작업은 없다.
6. 보고는 AGENTS.md "작업 방식" 형식으로, 커밋 본문과 HANDOFF "Codex 결과"에 쓴다.

## 7. 로컬에서 확인할 것 — 사용자(VR, 끝에 한꺼번에)

1. 팀 소리 파일은 `Content/Main/Audio/`(권장)에 둔다.
2. 부품 BP의 Details `…|Sound`에서 칸마다 소리·지연·반복·페이드·감쇠를 넣는다.
   - 게이트·키 버튼 소리는 `BP_Liftgate`·`BP_KeyFob_*`의 `ButtonPressedCue`다.
   - 시동음은 `BP_Car_Core`의 전원 컴포넌트 칸이다.
3. L_Main Play에서 듣는다.
   - 손잡이·문 잠김·풀림, 레버 걸림, 노브·휠, 박스 막힘·보류.
   - 시동 → 미러 펼침·접힘, 펼치는 중에 끄기.
   - R 직후 소리가 나지 않는지.
4. 박스 보류음 간격(0.25 s)이 너무 잦거나 드문지 본다.
5. 소리가 부품 쪽에서 들리는지 본다. 아니면 `Attenuation`을 넣는다.
6. 시험 상태는 VR 전까지 `not-tested`다.

## 8. 확인한 근거

| 무엇 | 근거 | 쓰는 곳 |
|---|---|---|
| P5r 구조·재생 | `Interaction/VBoRSoundCue.h:10-51`, 같은 frame 합침 `VBoRSoundCue.cpp:12-14`, 지연 0 바로 재생 `:22-27`, 게이트 사용 `VBoRPoweredPanel.cpp`(PR #57) | D1·D2 |
| 버튼 | 소리 컴포넌트 `VBoRPokeButton.cpp:41-45`, ISDK 재생기 연결 `:73-80`, 이벤트·필터 `:119-124`, 컴포넌트 선언 `VBoRPokeButton.h:48-53` | D4·E5 |
| 버튼을 만드는 부품 | 게이트 `VBoRPoweredPanel.cpp:56`·`:68-71`, 키 `VBoRKeyFob.cpp:75-85`(`Poke` 끔 `:85`·`:194`) | D5 |
| 1축 부품 | 컴포넌트 `VBoRAxisPart.cpp:156-158`, 걸림 알림 `:500-508`, 열림·닫힘 `:589-590` | D6 |
| 경첩 판 | 컴포넌트 `VBoRHingedPanel.cpp:91-95`, 풀림·잠김 `:575-576`, `ResetDoor` `VBoRHingedPanel.h:64` | D7·E11 |
| 노브·휠 | 노브 `VBoRKnob.cpp:46-47`·`:147`·`:168`, 휠 `VBoRSteeringWheel.cpp:60-61`·`:221-224` | D8 |
| 박스 | 컴포넌트 `VBoRCarryObject.cpp:82-86`, 막힘 `:377`, 보류(매 `UpdateVisual`) `:566`·`:576`, 매 `Tick` 호출 `:361-363`, `ResetToPose` `VBoRCarryObject.h:78` | D9·E10 |
| 미러·전원 | 미러 `VBoRFoldingMirror.cpp`(BeginPlay `ResetMirror` `:37`), 전원 `VBoRVehiclePowerComponent.cpp`(`SetPowerOn` 출처 `:71-77`), R 순서 `VBoRScenarioRunner.cpp:521-522` | D10·D11 |
| 시험 관찰 도우미 | `P2dTestFixture.h:11-17`·`:58-61`, `P5bTestFixture.h:10-16` | §3 |
| 시험 친구 선언 | 1축 `VBoRAxisPart.h`(FP2b–FP5m), 판 `VBoRHingedPanel.h:120-124`, 휠 `VBoRSteeringWheel.h:89`, 노브 `VBoRKnob.h:90`, 박스 `VBoRCarryObject.h:160` | §3·D12 |

## 개정 이력

| 날짜 | 내용 |
|---|---|
| 2026-10-21 | 처음 작성(Claude) |
