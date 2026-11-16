# R11 녹음한 차 소리 칸 — 실내·실외, 공회전, 소리 길이에 맞춘 움직임

| 항목 | 값 |
|---|---|
| 단계 | 구조 보수. 자유모드(P5) 표현 |
| 결정 근거 | **사용자 2026-11-16:** 실제 차에서 녹음한 소리를 넣는다. **소리 에셋은 회사 프로젝트에만 넣는다 — 이 작업은 칸만 만든다.** 지금 칸과 맞지 않는 것: 실내·실외가 다른 소리(문·시동·공회전), 공회전, 정해진 길이의 자동 동작(리프트게이트 열림 6.773438 s·닫힘 8.675333 s, 2열 접기 3.4 s·펴기 3.0 s), 차가 내는 잠금·해제 소리, 노브 소리 간격. 비상등·방향지시 소리는 이번에 하지 않는다 |
| 베이스 / 작업 브랜치 | `main` / `codex/R11-recorded-sounds` |
| 리뷰 파일 | `Docs/Reviews/R11-recorded-sounds.md` |
| 에디터 작업 | 없음. 소리 가져오기·BP 칸 채우기·값 입력은 사용자가 회사 PC에서 한다(§7) |

## 0. 녹음 → 칸 (구현 뒤 모습)

| 녹음 | 부품 | 칸·값 | 새로 만드는 것 |
|---|---|---|---|
| 콘솔박스 열림·닫힘, 글러브박스 | `AVBoRHingedPanel` | `UnlockedCue`·`LatchedCue` | 없음 |
| 문 열림 실내·실외 | 같음 | `UnlockedCue`(실외) / **`UnlockedInsideCue`** | D2 |
| 문 닫힘 실내·실외 | 같음 | `LatchedCue`(실외) / **`LatchedInsideCue`** | D2 |
| 시동 실내·실외 | `UVBoRVehiclePowerComponent` | `PowerOnCue`(실외) / **`PowerOnInsideCue`** | D2 |
| 공회전 실내·실외 | 같음 | **`IdleCue`·`IdleInsideCue`**(지속) | D3 |
| 시동 끔 | 같음 | `PowerOffCue` | 없음 |
| 노브 | `AVBoRKnob` | `StepCue` + **`SoundStepDeg`** | D4 |
| 리프트게이트 열림·닫힘 | `AVBoRPoweredPanel` | **`OpeningCue`·`ClosingCue`**(지속) + **`OpenSeconds`·`CloseSeconds`**. 대기는 기존 `DelaySeconds` | D5 |
| 2열 접기·펴기 | `AVBoRFoldingSeat` | **`FoldingCue`·`UnfoldingCue`**(지속) + **`FoldSeconds`·`UnfoldSeconds`** | D5 |
| 잠금·해제 (차 소리) | `AVBoRKeyFob` | **`UnlockCarCue`·`LockCarCue`** | D6 |

## 1. 결정

- **D1 실내 판정 = 참가자 머리가 차 실내 상자 안.** 새 컴포넌트 `UVBoRCabinZoneComponent`(상자)를 `BP_Car_Core`에 **하나** 둔다(사용자가 추가). 머리는 추적된 실제 HMD 위치(`UVBoRSeatZoneComponent`와 같은 출처, `VBoRParticipantZones.h:9`). 추적이 끊긴 frame은 마지막 값을 유지한다. 상자가 0개면 늘 실외, 2개 이상이면 첫 번째만 쓰고 Check setup(R5)에 경고 한 줄.
  - 이유: "듣는 사람이 어디 있나"가 소리를 정한다. 문 열림·닫힘·시동·공회전에 같은 규칙 하나로 된다. 동승석·2열 시점(14·15)도 상자 안이면 실내다.
- **D2 실내 칸은 비면 기존 칸을 쓴다.** `…InsideCue`의 `Sound`가 비어 있으면 실내여도 기존 칸을 울린다 → 콘솔박스·글러브박스와 지금 BP는 그대로다. 판정은 **이벤트가 난 frame**의 실내 여부로 한 번(지연 재생 중에 바뀌어도 그대로).
- **D3 공회전 = 전원이 켜져 있는 동안의 지속 소리.** 켜지는 frame에 시작(칸의 `StartDelaySeconds`로 시동 소리 뒤에 이어지게 사용자가 맞춤). 꺼지면 `FadeOutSeconds`로 멈춤. Restart·Reset·EndPlay는 즉시 멈춤(Blueprint-Guide §7.1 규칙).
  - 켜진 채 실내·실외가 바뀌면 지금 공회전을 `FadeOutSeconds`로 멈추고 다른 쪽을 지연 없이 시작한다. 실내 칸이 비면 바꾸지 않는다(D2).
  - 반복은 에셋 Looping(`RepeatSeconds` 0) 또는 `RepeatSeconds` — 기존 `FVBoRHeldSoundCue` 규칙 그대로.
- **D4 노브 소리 간격을 걸림 칸과 따로.** `SoundStepDeg`(deg, 기본 0). 0이면 지금처럼 `StepDeg` 한 칸마다 `StepCue`. 0보다 크면 `StepCue`는 손으로 돌린 노브 각이 `SoundStepDeg`만큼 바뀔 때마다 울리고, `StepDeg`·`OnKnobStep`·걸림 판정은 그대로다(`VBoRKnob.cpp:150-172`).
  - 경계: 한 frame에 여러 칸을 넘으면 여러 번 요청(같은 frame은 기존 규칙대로 합침 여부 그대로), 방향을 바꿔도 같은 규칙, 추적 끊김·튐 판정(`JumpThresholdDeg`)으로 버린 각은 소리를 내지 않는다.
  - 돌린 손 대비 노브가 도는 비율은 이미 `TwistGain`이 있다 — 이 칸들을 `Knob|Sound`·`Knob|Input` 분류로 보이게 정리만 한다(값·의미 그대로).
- **D5 자동 동작 시간을 방향별로.**
  - 리프트게이트 `OpenSeconds`·`CloseSeconds`, 2열 `FoldSeconds`·`UnfoldSeconds`(s, 기본 0). 0이면 지금처럼 `SpeedDegPerSecond`. 0보다 크면 그 방향 전체 이동(끝↔끝)이 그 시간이 되게 속도를 정한다. 리프트게이트가 끼여 방향을 바꾸면 남은 각도에 그 방향 속도. 2열은 움직이는 중 반대 명령을 받지 않는다(`VBoRFoldingSeat.cpp:41-53`) — 그대로.
  - 대기 시간은 **기존 `DelaySeconds`**(`VBoRPoweredPanel.h:100`, 열림·닫힘 공통)를 그대로 쓴다. 바꾸지 않는다.
  - 새 지속 칸 `OpeningCue`·`ClosingCue`(리프트게이트), `FoldingCue`·`UnfoldingCue`(2열)는 **그 방향으로 실제 움직이는 동안** 울린다(리프트게이트 대기 단계 제외 — 대기는 기존 `WarningCue`). 도착하면 `FadeOutSeconds`로 멈춤. 방향이 바뀌면 지금 칸을 멈추고 다른 방향 칸을 같은 frame에 시작.
  - 기존 `MotorCue`(양방향 한 칸, 되돌기에도 이어짐)는 그대로.
- **D6 잠금·해제 차 소리.** `AVBoRKeyFob`에 `UnlockCarCue`·`LockCarCue`(순간). `OnUnlockPressed`·`OnLockPressed`와 같은 frame에 요청. 소리 위치는 **차**(레벨에서 `UVBoRVehiclePowerComponent`를 가진 Actor의 루트), 차가 없으면 키. 기존 `ButtonPressedCue`(키 딸깍)는 그대로.

## 2. 범위

### 만들 것
- `UVBoRCabinZoneComponent`(D1)와 실내 여부를 읽는 한 곳.
- 위 표의 새 칸·값(D2–D6). 새 칸은 모두 **기본 비움(무음)**, 새 값은 기본 0.
- 재생은 기존 `FVBoRSoundPlayback`(`VBoRSoundCue.h:30-50`). 오디오 컴포넌트는 첫 요청 때 만드는 지금 방식(`P5s E9`).
- 자동 시험 `vBoR.R11.*`(§4). Blueprint-Guide §7.1 칸 표는 머지 뒤 Claude가 고친다.

### 범위 밖 — 하지 말 것
- 소리 에셋 추가, BP·레벨 수정, 값 입력(사용자, 회사 PC).
- 비상등·토글 버튼, 방향지시 소리(사용자 2026-11-16 "무시").
- 열림·닫힘 대기를 따로 나누기. 기존 칸의 의미·기본값·재생 규칙 변경. `FVBoRSoundPlayback` 구조 변경(필요하면 질문).
- 실내 차폐·리버브, 감쇠 자동 설정.

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- **새 칸이 비고 새 값이 0이면 지금과 소리·움직임·상태·이벤트가 완전히 같다.** 기존 BP를 고치지 않아도 된다.
- 노브 `StepDeg`·`StepHysteresisDeg`·`JumpThresholdDeg`·`TwistGain`의 의미와 `OnKnobStep` 시점.
- 리프트게이트 `DelaySeconds`·`WarningCue`·`MotorCue`·끼임 동작, 2열 `SpeedDegPerSecond`(값 0일 때).
- 시나리오 완료 판정·진행자 명령·전원 상태는 소리와 무관하다.
- 기존 시험 단언은 하나도 바꾸지 않는다. 특히 `vBoR.P5s.E1Defaults`·`E9Power`(첫 요청 전 차 오디오 0개·뒤 1개, Restart 무음)·`E12RegressionDefaults`, `P5r.*`, `R3.*`, `P5f.*`(노브), `P5ze.*`. 분류(Category) 정리로 깨지는 시험이 있으면 정리를 하지 말고 그대로 둔다. 그 밖에 깨지면 멈추고 질문.

## 4. 시험 — `vBoR.R11.*`

| ID | 내용 | 경계 |
|---|---|---|
| E1 | 새 칸 기본 무음, 새 값 기본 0 | 새 칸을 채우지 않은 world에서 기존 이벤트를 다 내도 새 칸 요청 0, 리프트게이트·2열 도착 시간 지금과 같음 |
| E2 | 실내 판정 | 상자 안·밖, 추적 끊긴 frame은 이전 값, 상자 0개 = 실외, 2개 = 첫 번째 |
| E3 | 문 열림·닫힘 실내·실외 | 실내 칸 비면 기존 칸. 지연 재생 중 실내 여부가 바뀌어도 처음 판정대로 |
| E4 | 시동·공회전 | 켜짐 → 시동 + 공회전, 꺼짐 → 공회전 fade + 끔 소리, Restart → 즉시 멈춤·끔 소리 없음, 켜진 채 실내↔실외 → 공회전 바뀜(실내 칸 비면 안 바뀜), 같은 frame 손·진행자 중복 → 한 번 |
| E5 | 노브 소리 간격 | `SoundStepDeg` 5·`StepDeg` 15로 45° → 소리 9회·`OnKnobStep` 3회. 0이면 지금과 같음. 튐으로 버린 각 무음, 방향 바꿈 |
| E6 | 리프트게이트 초 | `OpenSeconds` 6.773438 → 열림 도착이 대기 + 그 시간(±1 frame), `CloseSeconds` 8.675333 따로, 0이면 지금 속도. 대기 중 `OpeningCue` 없음, 끼임 되돌기 → `ClosingCue` 멈춤·`OpeningCue` 같은 frame |
| E7 | 2열 초 | `FoldSeconds` 3.4·`UnfoldSeconds` 3.0, 지속 칸 시작·도착 멈춤, 움직이는 중 반대 명령 거부 그대로 |
| E8 | 잠금·해제 차 소리 | 각 버튼 → 그 칸만, 오디오가 차 루트에, 차 없으면 키 |
| E9 | Reset·EndPlay | 새 지속 칸 모두 즉시 멈춤·예약 지움(기존 P5s E11 규칙과 같음) |

## 5. 구현자가 정할 것

- 실내 여부를 어디서 읽을지(상자를 찾는 방법, 머리 위치를 Runner·Pawn 중 어디서). 단 limiter·조준과 무관한 추적 원본.
- 새 칸의 슬롯 번호 — 기존 슬롯 뒤에 붙인다(기존 번호를 쓰는 시험이 있다, `VBoRAxisPart.h:95` 같은 "Slot order = property order").
- D5 시간 → 속도 계산 위치.
- `ToolTip`·`Category` 문구(영어 조각문, AGENTS.md).

## 6. 완료 조건

- §4 시험 전부 통과, `vBoR.*` 전체 통과(기존 단언 그대로).
- non-unity(`-Module=vBoR`)·unity 빌드 경고 없음.
- 커밋 본문·`HANDOFF.md` "Codex 결과"에 보고 ①–⑤.

## 7. 로컬에서 확인할 것 — 사용자(회사 PC)

1. 녹음 파일을 프로젝트로 가져온다. 공회전은 SoundWave **Looping** 체크.
2. `BP_Car_Core`에 `Cabin Zone` 추가, 운전석·동승석·2열 머리 위치가 다 들어가게.
3. 칸 채우기(§0 표). 시동 → 공회전 이음: `IdleCue.StartDelaySeconds` ≈ 시동 소리 길이.
4. 리프트게이트 `OpenSeconds` 6.773438·`CloseSeconds` 8.675333, 대기는 `DelaySeconds`. 2열 `FoldSeconds` 3.4·`UnfoldSeconds` 3.0.
5. 노브 `SoundStepDeg`(예: 5)로 소리 간격, 돌아가는 비율은 `TwistGain`.
6. Quest 3(T-08 자유모드): 차 밖·안에서 문 여닫기, 시동 → 공회전 → 차에서 내리면 실외 소리로, 리프트게이트·2열 소리 끝과 도착이 맞는지, 노브 소리. `not-tested`.
