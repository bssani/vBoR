# P5r 소리 칸 — 이벤트별 소리·타이밍을 BP에서 편집, 리프트게이트부터

| 항목 | 값 |
|---|---|
| 단계 | P5 자유모드·리허설 준비(사용자 요청 2026-10-21). 시험 T-02·T-04(리프트게이트 소리) |
| 결정 근거 | **사용자(2026-10-21):** "오디오를 수정할 수 있어야 해. 단순히 오디오 파일만 수정하는 게 아니라, 타이밍 이런 것도. 예를 들어 liftgate 열고 닫을 때."<br>**Claude 질문 → 사용자 답:**<br>① 범위 → **리프트게이트, 문·콘솔·글러브박스, 버튼·레버·노브·핸들·박스, 시동·사이드미러 전부**<br>② 타이밍 방식 → **이벤트별 소리 칸**(권장안: 소리·시작 지연·반복·멈춤·볼륨, 각도 지점은 안 함)<br>③ 리프트게이트 소리 → **버튼 받음 확인음, 경고음 반복, 모터음 루프, 끝 소리 전부**<br>AGENTS.md "모든 접촉·조작 확인은 시각·청각 피드백으로만" — 소리는 촉감을 대신하는 주요 단서다 |
| 나눔 | 범위가 커서 둘로 나눈다. **이 명세(P5r):** 공통 구조 + 리프트게이트. **다음 명세(P5s):** 같은 구조를 나머지 부품에 적용한다. P5s는 P5r 머지 뒤에 Claude가 쓴다 |
| 베이스 브랜치 | `main` @ 이 명세 머지 뒤 |
| 작업 브랜치 | `codex/P5r-sound-cues` |
| 리뷰 파일 | `Docs/Reviews/P5r-sound-cues.md` (첫 리뷰 때 생김) |
| 상태 | 명세 — 구현 전 |
| 에디터 작업 | **없음.** BP·레벨은 바꾸지 않는다. `BP_Liftgate`에는 새 칸이 기본값으로 보인다 |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **소리 칸 = 구조체 둘** | 새 헤더 `Interaction/VBoRSoundCue.h`. 엔진(`Engine`)에만 의존한다. 리프트게이트 전용 이름을 넣지 않는다(P5s가 그대로 쓴다).<br>**`FVBoRSoundCue`**(USTRUCT BlueprintType) — **순간 소리**(한 번 나는 소리):<br>- `Sound`(`USoundBase*`): 비우면 무음이다. SoundWave·SoundCue·MetaSound 무엇이든 넣을 수 있다.<br>- `StartDelaySeconds`(s, ≥ 0, 기본 0): 이벤트가 일어난 뒤 소리가 나기까지.<br>- `VolumeMultiplier`(≥ 0, 기본 1), `PitchMultiplier`(> 0, 기본 1).<br>- `Attenuation`(`USoundAttenuation*`): 비우면 소리 에셋의 설정을 쓴다. VR에서 소리가 부품 쪽에서 들리게 할 때 쓴다.<br>**`FVBoRHeldSoundCue`**(`FVBoRSoundCue`를 상속) — **지속 소리**(상태가 이어지는 동안 나는 소리):<br>- `RepeatSeconds`(s, ≥ 0, 기본 0): 0이면 시작 때 한 번 재생한다(루프 에셋이면 상태 동안 계속 난다). 0보다 크면 그 간격으로 다시 낸다. 0.05 s보다 작은 값은 0.05 s로 쓴다(지금 경고음과 같음 `VBoRPoweredPanel.cpp:157`, 긴 frame에서 요청이 폭주하지 않게).<br>- `FadeOutSeconds`(s, ≥ 0, 기본 0): 상태가 끝날 때 이만큼 줄이며 멈춘다. 0이면 바로 멈춘다.<br>- 필드 이름은 권장이다. 툴팁에 **시간 기준점과 단위**를 쓴다 |
| D2 | **재생 규칙 — 모든 소리 칸 공통** | - **frame 시간으로만 잰다.** 부품의 `Tick` 안에서 쌓은 시간으로 지연·반복을 센다. 타이머(`FTimerManager`)를 쓰지 않는다 — 시험이 `Tick`을 직접 부르고, frame 시간 규칙과 같아야 한다.<br>- **지연이 frame 중간에 끝나면 그 frame에 낸다.** frame 안의 시각까지 맞추지는 않는다.<br>- **긴 frame에서 반복 간격을 여러 번 넘으면 넘은 수만큼 요청을 센다.** 지금 경고음과 같다(`VBoRPoweredPanel.cpp:154-164` `WarnFor`). 실제 재생은 그 frame에 한 번이면 된다.<br>- **순간 소리:** 이벤트가 일어날 때마다 하나씩 예약된다. 같은 칸이 다시 울리면 앞 소리는 처음부터 다시 난다(지금 `Play()`와 같음). 다른 칸끼리는 서로 끊지 않는다.<br>- **지속 소리:** 상태에 들어간 순간이 기준이다. 지연이 끝나기 전에 상태가 끝나면 나지 않는다. 상태가 끝나면 예약을 지우고, 나는 소리는 `FadeOutSeconds`대로 멈춘다.<br>- **무음 칸**(`Sound` 비움)은 요청도 세지 않는다. 경고도 남기지 않는다.<br>- **초기화**(부품의 Reset, `EndPlay`): 예약된 소리를 모두 지우고, 나는 소리를 바로 멈춘다(페이드 없음). 초기화 뒤에는 아무 소리도 나지 않는다.<br>- 볼륨·피치·감쇠는 재생할 때 그 칸의 오디오 컴포넌트에 넣는다 |
| D3 | **재생 장치는 구현자가 정한다 — 조건만 정한다** | - 칸마다 오디오 컴포넌트 하나를 권장한다. 다른 칸끼리 끊지 않기 위해서다(D2). 소리가 나는 자리는 그 부품의 움직이는 판이다(지금 `WarningAudio`가 `HingePivot`에 붙은 것과 같음, `VBoRPoweredPanel.cpp:36-37`).<br>- 시험이 칸마다 요청 수와 요청이 난 시각(frame)을 읽을 수 있어야 한다.<br>  - 방법 예: 칸마다 센 수를 돌려주는 함수, 또는 오디오 컴포넌트를 시험용 관찰자로 바꿔 끼우기(`P2Probe/P2dTestFixture.h:11-17`, `:61`과 같은 방식).<br>  - 시험 전용 멤버는 `WITH_DEV_AUTOMATION_TESTS` 안에 둔다(`VBoRAxisPart.h:45-47` `CancelCalls`와 같음).<br>- 헤드리스 시험에는 오디오 장치가 없을 수 있다. 시험은 실제 재생이 아니라 **요청**을 센다. `UGameplayStatics::SpawnSound*`처럼 장치가 없으면 null이 되는 경로에 시험이 기대지 않게 한다.<br>- 소리 관리자·서브시스템·데이터 에셋 표는 만들지 않는다(AGENTS.md "범용 프레임워크부터 만들지 않는다") |
| D4 | **리프트게이트 소리 칸 7개** | `AVBoRPoweredPanel`에 둔다. Category는 `Powered Panel\|Sound`(권장). 표는 아래 "리프트게이트 칸" |
| D5 | **지금 경고음은 `WarningCue`로 옮긴다** | - `WarningSound`·`WarningIntervalSeconds`를 `WarningCue`(`FVBoRHeldSoundCue`)의 `Sound`·`RepeatSeconds`로 옮긴다. 기본값은 지금과 같다: ISDK poke 누름 소리(`VBoRPoweredPanel.cpp:39-40`), 0.5 s, 시작 지연 0.<br>- **지금 들리는 것은 바뀌지 않는다.** 대기 시작 frame에 첫 경고가 나고, 대기·움직이는 동안 0.5 s마다 나며, 열림·닫힘에서 멈춘다.<br>- `WarningAudio`(컴포넌트)와 `GetWarningRequests()`는 이름과 뜻을 그대로 둔다. 기존 시험(§3 감사)과 P1 시험대 기록이 쓴다.<br>- 옛 두 프로퍼티는 지운다. `BP_Liftgate`에 저장된 값은 기본값과 같아서, 지워져도 동작 차이가 없다. CoreRedirects는 필요 없다 |
| D6 | **포인터 콜백 안에서는 소리를 내지 않는다** | 지금 규칙 그대로다(P4b D3·E11, `VBoRPoweredPanel.cpp:100-101`). 요청을 받은 순간의 소리(받음 확인음·경고음)는 **다음 `Tick`**을 기준점으로 한다. 키보드 요청도 같은 경로다(입력 경로가 둘이어도 명령은 하나) |

### 리프트게이트 칸 (D4)

| 칸 | 종류 | 시간 기준점 | 멈춤 | 기본값 |
|---|---|---|---|---|
| `OpenAcceptedCue` | 순간 | 열기 요청을 **받은** 다음 `Tick`. 무시된 요청(대기·움직이는 중, 같은 쪽)에는 안 남 | — | 무음 |
| `CloseAcceptedCue` | 순간 | 닫기 요청을 받은 다음 `Tick` | — | 무음 |
| `WarningCue` | 지속 | 받은 다음 `Tick`(대기 시작) | 열림·닫힘 도착, 초기화 | ISDK poke 소리, `RepeatSeconds` 0.5(지금 값) |
| `MotorCue` | 지속 | 대기가 끝나 움직이기 시작한 frame | 열림·닫힘 도착, 초기화. **끼임으로 다시 열릴 때는 이어서 난다**(움직임이 끊기지 않음) | 무음 |
| `OpenedCue` | 순간 | 열림 각도 도착(`OnOpened`와 같은 frame) | — | 무음 |
| `ClosedCue` | 순간 | 닫힘 도착(`OnClosed`와 같은 frame) | — | 무음 |
| `ObstructedCue` | 순간 | 끼임 감지(`OnObstructed`와 같은 frame) | — | 무음 |

- 열기와 닫기의 대기 시간·속도는 지금도 프로퍼티다(`SpeedDegPerSecond`·`DelaySeconds`, `VBoRPoweredPanel.h:73-74`). 소리 타이밍은 이 움직임에 맞춰 위 기준점에서 센다.
- **"닫히기 3° 전" 같은 각도 지점은 만들지 않는다**(사용자 답 ②). 비슷한 효과가 필요하면 `ClosedCue`의 소리 파일 앞부분을 조정한다.

## 1. 왜 만드는가

- 지금 소리는 모두 ISDK 버튼 딸깍 하나다. 리프트게이트는 그 소리를 0.5 s마다 반복할 뿐이다.
- 팀이 실제 차 소리(확인음·경고음·모터·잠김)를 넣고, 언제 날지를 BP에서 맞출 수 있어야 한다. 코드를 고치지 않고 해야 한다.

## 2. 범위

### 만들 것

1. `Interaction/VBoRSoundCue.h`(필요하면 `.cpp`): D1 구조체 둘과 D2 규칙을 지키는 재생 도우미.
2. `AVBoRPoweredPanel`: 칸 7개(D4), 경고음 옮기기(D5).
3. 자동화 시험 §4(`vBoR.P5r.*`).

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| 다른 부품(문·버튼·레버·노브·핸들·박스·시동·미러)의 소리 | P5s에서 한다. 이 명세는 구조를 굳힌다 |
| 리프트게이트 버튼 둘의 딸깍 소리 | 게이트가 Play 때 기본 `AVBoRPokeButton`으로 만든다(`VBoRPoweredPanel.cpp:56`). 버튼 소리는 P5s에서 정한다 |
| 각도 지점 소리 | 사용자 답 ② |
| 소리 관리자·서브시스템·데이터 에셋 표·믹서 | D3 |
| 실제 소리 파일 만들기·넣기 | 팀이 한다. 새 칸 기본값은 무음이다(경고음만 지금 소리) |
| BP·`L_Main` 바꾸기 | 에디터 작업 없음 |
| 움직임·대기·끼임 판정 바꾸기 | §3 |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- **리프트게이트 움직임은 그대로다.** 상태·대기·속도·끼임 방지·이벤트 순서와 횟수, 버튼·키 입력 규칙이 그대로다.
- **지금 들리는 경고음은 그대로다**(D5). 첫 경고 시점, 0.5 s 간격, 긴 frame의 요청 수 세기, 도착·초기화에서 멈춤이 같다.
- **포인터 콜백에서 소리·이동·이벤트가 없다**(D6).
- **기존 시험 감사** — `P1Probe/P4bPoweredPanelTests.cpp`:
  - `:92` `WarningSound != nullptr` → `WarningCue.Sound != nullptr`로만 바꾼다(D5). **이 줄만 바꿀 수 있다.**
  - 그대로 통과해야 하는 단언(값·허용 오차 그대로): `:92` 요청 ≥ 2, `:100-103` 도착 뒤 요청 없음·`WarningAudio` 멈춤, `:120-124` 대기+이동 ≥ 10·멈춤, `:164` 무시된 입력에 요청 0, `:201-207` 끼임 뒤 경고 계속, `:290`·`:295` 콜백 안 0·다음 Tick > 0, `:334-350` B는 요청 수 유지·멈춤, R은 0.
  - 이 파일의 다른 단언은 바꾸지 않는다.
- **P4i 시험:** `vBoR.P4i.E2`는 `BP_Liftgate`의 편집 가능한 설정을 근사 차 게이트와 비교한다. 새 칸은 양쪽 다 기본값이라 같다. 옛 두 프로퍼티가 없어져도 양쪽이 같이 없어진다. E2 단언은 바꾸지 않는다.
- frame마다 월드 전체를 훑지 않는다(P5o).
- 보이는 메시·재질·버튼 자리는 그대로다.

## 4. 경계 상황과 그 시험 (`vBoR.P5r.*`)

시험 world에 `AVBoRPoweredPanel`을 직접 만든다(`P4bPoweredPanelTests.cpp`의 world 방식). 칸에는 시험이 ISDK poke 소리 같은 아무 소리나 넣는다. **실제 재생이 아니라 요청과 시각(frame)**을 본다(D3).

| ID | 상황 | 확인 |
|---|---|---|
| E1 | 기본값 | 새 칸 6개는 무음, `WarningCue`는 ISDK 소리·0.5 s·지연 0. 무음 칸은 열고 닫는 내내 요청 0 |
| E2 | 받음 확인음 + 지연 | `OpenAcceptedCue` 지연 0.3 s. 버튼 누름 → 콜백 안에서 요청 0 → `Tick` 0.1 s씩: 0.2 s까지 0, 0.3 s frame에 1. 닫기도 같다(`CloseAcceptedCue`). 열기 칸이 닫기에 울리지 않는다 |
| E3 | 무시된 입력 | 대기·움직이는 중의 열기·닫기, 같은 쪽 요청 → 받음 칸·경고 요청이 늘지 않는다 |
| E4 | 경고음 | 지금 동작 그대로: 첫 요청은 받은 다음 `Tick`이고, 0.5 s마다 요청하며, 도착하면 멈춘다. `WarningCue.StartDelaySeconds` 0.2이면 첫 요청이 0.2 s 뒤로 밀린다 |
| E5 | 모터 루프 | `MotorCue`(반복 0): 대기 동안 요청 0. 움직이기 시작한 frame에 1(한 번 재생). 도착 frame에 멈춤(재생 중 아님 또는 Stop 요청). `RepeatSeconds` 0.4이면 움직이는 동안 0.4 s마다 요청 |
| E6 | 페이드 | `MotorCue.FadeOutSeconds` 0.5 → 도착 때 0.5 s 페이드로 멈춤을 요청한다(관찰자로 확인). 0이면 바로 멈춘다 |
| E7 | 끝 소리 | `OpenedCue`·`ClosedCue`는 도착 frame에 1번씩, `OnOpened`·`OnClosed`와 같은 frame이다. 지연을 주면 그만큼 뒤에 난다. 그 사이 상태가 바뀌어도(예: 바로 닫기 요청) 순간 소리는 난다 |
| E8 | 끼임 | 닫히다 박스에 닿음 → `ObstructedCue` 1번(`OnObstructed`와 같은 frame). `MotorCue`는 끊기지 않고 이어진다(새 시작 요청 없음). 다시 열림 끝에 `OpenedCue` |
| E9 | 지연이 상태보다 김 | `MotorCue` 지연 > 움직이는 시간 → 모터 요청 0. 도착 뒤에도 나지 않는다 |
| E10 | 초기화 | 대기 중, 움직이는 중, 순간 소리 지연 중 각각에서 `ResetPanel(true)`와 `ResetPanel(false)` → 예약이 지워지고 바로 멈춘다(페이드 없음). 그 뒤 `Tick` 10 s 동안 요청 0. `GetWarningRequests()`는 지금처럼(true는 0, false는 유지) |
| E11 | frame 시간 | 같은 흐름을 `Tick` 1.0 s 한 번과 0.1 s 열 번으로 돌림 → 칸마다 요청 수가 같다. 받음·끝 소리는 기준 frame이 같다. 반복은 넘은 간격 수만큼 센다(D2) |
| E12 | 볼륨·피치·감쇠 | 칸에 넣은 `VolumeMultiplier`·`PitchMultiplier`·`Attenuation`이 그 칸의 오디오 컴포넌트에 들어간다. `Attenuation`을 비우면 컴포넌트의 감쇠를 덮어쓰지 않는다 |
| E13 | 칸끼리 안 끊음 | 같은 frame에 `ClosedCue`·`WarningCue` 멈춤·다른 순간 칸이 겹쳐도, 순간 칸 요청이 다른 칸의 멈춤에 지워지지 않는다 |
| E14 | 기존 시험 | `vBoR.P4b.*`가 §3 감사대로 통과한다(`:92` 한 줄만 바뀜) |

## 5. 구현자가 정할 것

- 재생 도우미의 모양(구조체 + 함수, 또는 작은 컴포넌트). D2·D3을 지키면 된다.
- 필드·칸 이름(권장 이름과 다르면 보고 ③에 쓴다). 툴팁 문구.
- 오디오 컴포넌트를 칸마다 생성자에서 만들지, BeginPlay에서 만들지. 시험이 요청을 읽을 수 있으면 된다.
- 시험에서 요청·시각을 읽는 방법(D3의 두 예 중 하나 또는 같은 수준의 방법).

## 6. 완료 조건

1. D1–D6을 지킨다.
2. E1–E14가 통과한다.
3. 기존 `vBoR.*` 시험이 모두 통과한다. 바뀐 기존 단언은 §3의 한 줄뿐이다.
4. non-unity·unity 빌드에서 vBoR 경고가 없다.
5. 빌드·전체 시험은 끝에 한 번(AGENTS.md 토큰 규칙). 에디터 작업은 없다.
6. 보고는 AGENTS.md "작업 방식" 형식으로, 커밋 본문과 HANDOFF "Codex 결과"에 쓴다.

## 7. 로컬에서 확인할 것 — 사용자(VR, 끝에 한꺼번에)

1. 팀 소리 파일을 `Content/Main/Audio/`(권장)에 넣는다. `Development/`에 두지 않는다(산출물이 참조하면 안 됨).
2. `BP_Liftgate` → Details → `Powered Panel|Sound`에서 칸마다 소리와 지연·반복·페이드를 넣는다. 모터는 루프 에셋(SoundWave의 Looping 켬, 또는 MetaSound 루프)에 `RepeatSeconds` 0이 맞다.
3. L_Main Play → 리프트게이트 열기·닫기·끼임에서 소리 순서와 타이밍을 듣는다. 버튼 누른 뒤 확인음이 늦거나 빠르지 않은지, 모터가 멈출 때 끊기는 느낌이 없는지 본다.
4. VR에서 소리가 게이트 쪽에서 들리는지 본다. 아니면 `Attenuation`에 감쇠 에셋을 넣는다.
5. 시험 상태는 VR 전까지 `not-tested`다.

## 8. 확인한 근거

| 무엇 | 근거 | 쓰는 곳 |
|---|---|---|
| 지금 경고음 | 생성자 `VBoRPoweredPanel.cpp:36-40`, 반복 `WarnFor` `:154-164`, 첫 경고·대기·이동 `Tick` `:177-222`, 도착 멈춤 `:227`, 초기화 `:104-112`, 프로퍼티 `VBoRPoweredPanel.h:66`·`:76-77` | D5·§3 |
| 콜백 규칙 | `Request` `VBoRPoweredPanel.cpp:93-102`(P4b D3·E11) | D6 |
| 게이트 버튼 생성 | `VBoRPoweredPanel.cpp:56`, `:68-71` | 범위 밖 |
| 오디오 컴포넌트 | `SetSound` `Engine/Source/Runtime/Engine/Classes/Components/AudioComponent.h:489`, `Play` `:517`, `Stop` `:583`, `FadeOut` `:511`, `IsPlaying` `:597`, `SetVolumeMultiplier` `:626`, `SetPitchMultiplier` `:630`, `AttenuationSettings` `:379-383` | D1–D3·E5·E6·E12 |
| 루프 에셋 | `USoundWave::bLooping` `Engine/Classes/Sound/SoundWave.h:446`, `USoundBase::IsLooping` `Sound/SoundBase.h:337` | D1 `RepeatSeconds` 0 |
| 시험 관찰자 본보기 | `P2Probe/P2dTestFixture.h:11-17`·`:61`, `P3Probe/P5bTestFixture.h:10-16` | D3 |
| 시험 전용 멤버 본보기 | `Interaction/VBoRAxisPart.h:45-47` | D3 |
| 다른 부품의 지금 소리(P5s 입력) | 1축 부품 `VBoRAxisPart.cpp:156-158`·`:506`·`:589-590`, 판 `VBoRHingedPanel.cpp:91-95`·`:575-576`, 버튼 `VBoRPokeButton.cpp:41-45`·`:80`, 노브 `VBoRKnob.cpp:46-47`·`:147`, 휠 `VBoRSteeringWheel.cpp:60-61`·`:223`, 박스 `VBoRCarryObject.cpp:82-86`·`:377`·`:576`. 시동·미러는 무음 | P5s |

## 개정 이력

| 날짜 | 내용 |
|---|---|
| 2026-10-21 | 처음 작성(Claude) |
