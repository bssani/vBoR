# R16 리프트게이트 닫힘 경고음 — 열림·닫힘 경고음을 따로

| 항목 | 값 |
|---|---|
| 단계 | 구조 보수. 자유모드(P5) 표현 — R11(녹음한 차 소리 칸)의 후속 |
| 결정 근거 | **사용자 2026-11-19:** "Liftgate에 warning cue가 있는데 닫을 때랑 열 때랑 달라. 이 부분 업데이트해야 해." 실제 차의 경고음이 방향마다 다르다. 지금은 `WarningCue` 하나를 두 방향이 같이 쓴다(`Interaction/VBoRPoweredPanel.cpp:381`) |
| 베이스 / 작업 브랜치 | 이 명세가 들어간 `main` / `codex/R16-liftgate-close-warning` |
| 리뷰 파일 | `Docs/Reviews/R16-liftgate-close-warning.md` (첫 리뷰 때 Claude가 만듦) |
| 에디터 작업 | 없음. 소리 에셋을 칸에 넣는 것은 사용자가 회사 PC에서 한다(§7) |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **새 칸 `CloseWarningCue` 하나만 더한다** | `AVBoRPoweredPanel`에 `FVBoRHeldSoundCue CloseWarningCue`(`Powered Panel\|Sound`, 기본 **빈 칸**)를 더한다.<br>- **기존 `WarningCue`는 이름·기본값(ISDK 소리, 0.5 s 반복)을 그대로 두고 열림 경고음으로 쓴다.** 이름을 바꾸면 이미 저장한 BP의 값이 사라지고, 기존 시험 30여 곳(`GetWarningRequests`·`WarningCue`·`WarningAudio`)이 깨진다.<br>- ToolTip만 고친다. `WarningCue`: `Open warning, accept to arrival. Also used for close while Close warning cue is empty`. `CloseWarningCue`: `Close warning, accept to arrival. Empty = use Warning cue` |
| D2 | **비면 기존 칸 (R11 D2와 같은 규칙)** | `CloseWarningCue.Sound`가 비어 있으면 닫힘도 지금처럼 `WarningCue`를 울린다 → 지금 BP·근사 차·기존 시험은 결과가 그대로다.<br>판정은 **요청을 받아 경고음을 시작하는 frame**(`Tick`의 `bPendingCommand` 처리, `:378-382`)에 한 번 한다 |
| D3 | **닫힘 경고음이 있을 때만 새 슬롯** | 닫힘을 받았고 `CloseWarningCue.Sound`가 있으면 새 슬롯 `EVBoRPoweredSound::CloseWarning`으로 시작한다. 그 밖에는 기존 `Warning` 슬롯이다.<br>- 슬롯은 enum의 `Closing` 뒤, `SlotCount` 앞에 둔다(R11의 `Opening`·`Closing`과 같은 방식, `VBoRPoweredPanel.h:20`). 생성자에서 컴포넌트를 만들지 않고, 처음 쓸 때 `VBoREnsureCueAudio`(`VBoRSoundCue.h:30`, 쓰는 예 `VBoRPoweredPanel.cpp:331-332`)로 만든다. 예전에 저장된 BP는 `SoundAudio` 배열이 짧다 — `BeginPlay`의 `SetNum`(`:100-101`)이 이미 처리한다.<br>- `GetSoundCue`(`:298-312`)에 새 슬롯을 더한다.<br>- `AdvanceSounds`의 "지속 칸은 건너뜀" 목록(`:347-348`)과 `StopHeldSounds`(`:353-358`)에 새 슬롯을 더한다 |
| D4 | **끼여서 다시 열릴 때 — 열림 경고음으로 바꾼다** | 닫히다 끼여 `Opening`으로 바뀌는 frame(`:404-421`)에서:<br>- **닫힘 경고음 슬롯을 쓰고 있었으면:** 그 슬롯을 `EndHeld`하고 같은 frame에 `Warning` 슬롯을 `BeginHeld`한다. R11의 `ClosingCue` → `OpeningCue` 전환과 같은 규칙이다(R11 §1 "방향이 바뀌면 지금 칸을 멈추고 다른 방향 칸을 같은 frame에 시작").<br>- **`Warning` 슬롯을 쓰고 있었으면(닫힘 칸이 빔):** 아무것도 바꾸지 않는다. 지금처럼 끊기지 않고 이어진다(P4b E6) |
| D5 | **시험 접근** | 닫힘 경고음 요청 수를 읽는 방법은 구현자가 정한다(예: `GetCloseWarningRequests()` 또는 기존 `GetSoundPlaybackForTest(EVBoRPoweredSound::CloseWarning)` `:85`) |

## 1. 범위

### 만들 것

1. D1–D4: 칸, 슬롯, 시작·전환·정지
2. §4 자동화 시험(새 파일 또는 `P3Probe/R11RecordedSoundTests.cpp` 옆, 이름 `vBoR.R16.*`)

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| `WarningCue`·`WarningAudio`·`GetWarningRequests` 이름 바꾸기, `OpenWarningCue` 새로 만들기 | D1 — BP 값·기존 시험 보존 |
| 다른 소리 칸(`OpenAcceptedCue`·`CloseAcceptedCue`·`MotorCue`·`OpeningCue`·`ClosingCue`·도착·끼임) 바꾸기 | 사용자 요청은 경고음뿐 |
| 대기 시간·속도·끼임 판정·상태·이벤트(`OnStateChanged` 등) 바꾸기 | 소리만 바꾼다 |
| 실내·실외 경고음 나누기(R11 D1 실내 상자) | 요청 없음. 필요하면 따로 |
| 2열 시트·미러 등 다른 부품 | 요청 없음 |
| 소리 에셋 추가, BP·레벨 수정 | 에셋은 회사 프로젝트에만(R11). 에디터 작업 없음 |
| `Docs/Blueprint-Guide.md` 수정 | Claude가 리뷰 때 고친다 |

## 2. 지켜야 할 조건 (바꾸면 안 되는 것)

- **`CloseWarningCue`가 비어 있으면 모든 소리 요청 수·frame이 지금과 같다.** 기존 단언은 바꾸지 않는다. 그대로 통과해야 하는 것:
  - `vBoR.P4b.*`(`P1Probe/P4bPoweredPanelTests.cpp`) — E1 대기 중 경고·도착 정지(`:92-103`), E2(`:120-124`), E4 무시된 입력 무음(`:164`), **E6 끼여도 경고 이어짐(`:201-207`)**, E11 콜백 무음·다음 Tick(`:290-295`), B·R 초기화 수(`:334-350`)
  - `vBoR.P5r.*`(`P1Probe/P5rSoundCueTests.cpp`) — 경고 기본값(`:67-69`), 반복·지연·경계 수(`:126-135`), 초기화(`:241`), 반복 하한(`:268-270`), 예전 BP 슬롯(`:318-321`)
  - `vBoR.P5s.*`(`P3Probe/P5sPartSoundTests.cpp:290`), `vBoR.R11.*`(`P3Probe/R11RecordedSoundTests.cpp`)
  - 이 목록 밖에서 실패하는 단언이 있으면 고치지 말고 리뷰 파일 "질문"에 적고 멈춘다.
- 경고음의 시작 시점은 지금과 같다: 요청을 받은 **다음 Tick**(콜백 안에서 울리지 않음, P4b D3/E11), 대기와 움직임 내내, 도착 frame에 멈춤.
- `ResetPanel`(B·R)은 새 슬롯도 다른 슬롯과 같이 멈추고, R이면 수를 지우고 B면 남긴다(`:187`의 반복문이 이미 모든 슬롯을 돈다 — 그대로 쓰면 된다).

## 3. 왜 이렇게

- 열림 칸을 새로 만들지 않는 이유: `WarningCue`를 그대로 열림으로 쓰면 BP·시험을 하나도 안 건드린다. 사용자는 회사 BP의 `Close Warning Cue`에 닫힘 소리 하나만 넣으면 된다.
- 끼임 때 바꾸는 이유: 실제 차는 다시 열릴 때 열림 경고를 낸다고 보는 것이 자연스럽고, R11이 같은 상황의 이동음을 이미 그렇게 바꾼다. 실차가 다르면 사용자 확인 뒤 고친다(§7-3).

## 4. 경계 상황과 그 시험

| # | 상황 | 기대 동작 | 자동화 시험 |
|---|---|---|---|
| E1 | 닫힘 칸 빔 | 열기·닫기 모두 `Warning` 슬롯. `CloseWarning` 요청 0, 오디오 컴포넌트도 만들지 않음 | 근사 리프트게이트 |
| E2 | 닫힘 칸 있음 — 닫기 | 다음 Tick부터 도착까지 `CloseWarning`만 울리고 반복(`RepeatSeconds`)·지연(`StartDelaySeconds`)은 그 칸 값. 같은 동안 `Warning` 요청 0. 도착 frame에 멈춤 | 같음 |
| E3 | 닫힘 칸 있음 — 열기 | `Warning`만 울림, `CloseWarning` 요청 0 | 같음 |
| E4 | 닫힘 칸 있음 — 끼여 다시 열림 | 끼임 frame에 `CloseWarning` 멈춤·`Warning` 시작(같은 frame). 열림 도착에 `Warning` 멈춤 | 끼임 상자 + 박스(P4b E6 고정물) |
| E5 | 닫힘 칸 빔 — 끼여 다시 열림 | `Warning`이 끊기지 않고 이어짐(시작 다시 안 함) | 같음 |
| E6 | 닫는 중 B / R | `CloseWarning` 즉시 멈춤. R은 수 0, B는 수 유지 | 근사 리프트게이트 |
| E7 | 예전 BP(`SoundAudio` 짧음) + 닫힘 칸 있음 | 첫 닫기에서 컴포넌트가 생기고 울림. 크래시·경고 없음(`ShrinkSoundAudioForTest` `:88`) | 같음 |
| E8 | `DelaySeconds` 0 | 받은 다음 Tick에 경고 시작과 움직임 시작이 같은 frame — 지금과 같은 순서 | 같음 |
| E9 | 기존 시험 | §2 목록 그대로 통과, `vBoR.*` 전체 통과 | 전체 시험 |

## 5. 구현자가 정할 것

- D5 시험 접근 방법.
- 시작할 칸을 고르는 코드 위치(작은 함수로 빼도 되고 `Tick` 안에 둬도 됨).
- 시험 파일 위치와 시험 이름(`vBoR.R16.` 접두는 지킴).

## 6. 완료 조건

1. §1의 1–2가 들어 있다.
2. §4 시험이 자동화로 통과한다. 기존 `vBoR.*`도 통과한다(§2 — 예외 없음).
3. non-unity(`-Module=vBoR`)·unity 빌드에서 vBoR 경고·오류 0. 전체 시험은 최종 트리로 한 번.
4. 코드·주석·ToolTip은 영어(AGENTS.md).
5. AGENTS.md 보고 ①–⑤를 커밋 본문과 `HANDOFF.md` "Codex 결과"에 남긴다.
6. 시험 상태는 `not-tested`다.

## 7. 로컬에서 확인할 것 — 사용자

1. **회사 PC:** `BP_Liftgate` → `Powered Panel | Sound` → `Close Warning Cue`에 닫힘 경고음을 넣는다. 열림 경고음은 기존 `Warning Cue`에 넣는다. 녹음이 한 번 길게 울리는 소리면 `Repeat Seconds` 0, 짧은 삑 소리를 반복하면 녹음 간격에 맞춘다.
2. **PIE+VR:** 열기·닫기에서 각 소리가 대기부터 도착까지 나는지, 도착하면 멈추는지 본다.
3. **박스를 걸고 닫기:** 다시 열릴 때 열림 경고음으로 바뀌는 것이 실차와 같은지 본다. 다르면 알려 준다(D4를 고친다).
4. Quest 3 시험 ID: 리프트게이트(계획서 §7, 04·06단계) — `not-tested`.

## 8. 확인한 API 근거

경로: `Source/vBoR/` 아래(`main` @ `1a17da9`).

| 쓸 것 | 위치 | 쓰임 |
|---|---|---|
| 소리 슬롯 enum | `Interaction/VBoRPoweredPanel.h:20` | D3 |
| 경고음 칸·오디오·시험 접근 | `Interaction/VBoRPoweredPanel.h:83-89`, `:95`, `:109` | D1·D5 |
| 슬롯 생성·예전 BP 배열 | `Interaction/VBoRPoweredPanel.cpp:44-57`, `:100-101` | D3·E7 |
| 칸 찾기 | `Interaction/VBoRPoweredPanel.cpp:298-312` | D3 |
| 이동음 지연 생성·시작·끝(따라 할 예) | `Interaction/VBoRPoweredPanel.cpp:326-339` | D3·D4 |
| 지속 칸 건너뜀·정지 | `Interaction/VBoRPoweredPanel.cpp:341-358` | D3 |
| 경고음 시작 | `Interaction/VBoRPoweredPanel.cpp:378-382` | D2 |
| 끼임 되돌기 | `Interaction/VBoRPoweredPanel.cpp:404-421` | D4 |
| 도착 | `Interaction/VBoRPoweredPanel.cpp:431-439` | E2 |
| 초기화 | `Interaction/VBoRPoweredPanel.cpp:179-191` | E6 |
| 지속 소리 구조체·지연 생성 | `Interaction/VBoRSoundCue.h:22-30`, `Interaction/VBoRSoundCue.cpp:6-14`, `:40-47` | D3 |
| 요청은 닫힘·열림 상태에서만(움직이는 중 방향 바꿈 없음 — 바뀌는 길은 끼임뿐) | `Interaction/VBoRPoweredPanel.h:37-43` | D4 |

## 개정 이력

| 날짜 | 개정 | 내용 |
|---|---|---|
| 2026-11-19 | 처음 | 작성(사용자 요청: 열림·닫힘 경고음이 다름). 경고음을 쓰는 기존 시험을 모두 대조함 — 닫힘 칸이 비면 바뀌는 단언 없음 |
