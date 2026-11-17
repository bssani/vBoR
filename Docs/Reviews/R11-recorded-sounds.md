# R11 리뷰 — 녹음한 차 소리 칸

## 1회차 (Claude 2026-11-17) — `e01141f`, 통과

`git diff main...codex/R11-recorded-sounds`. 순서: AGENTS.md 제약 → 명세 범위·완료 조건 → 빌드 → 버그.

- **범위:** 명세 §0 표의 칸·값만 더했다. 비상등·방향지시·에셋·BP 수정 없음. 제품 밖 변경은 `P0ProbeDialTests.cpp` 지역 변수 이름 하나(unity 이름 충돌, 단언 그대로) — 보고 ③에 적혀 있고 받아들인다.
- **지켜야 할 조건:** 새 칸은 모두 비움, 새 값 0. 오디오는 첫 요청 때 만든다(`VBoREnsureCueAudio`, `VBoRSoundCue.cpp`) → P5s E9 "첫 요청 전 0개" 유지. 기존 슬롯 번호 유지, 새 슬롯은 뒤(`EVBoRPoweredSound` `Count` 뒤 `Opening`·`Closing`, 문 2·3, 전원 2–4). 노브 `TwistGain`은 분류만 `Knob|Input`으로.
- **빌드·시험(Codex 보고):** non-unity·unity 성공, `vBoR.*` 774/774. 다시 돌리지 않았다.
- **확인한 동작:**
  - 실내 판정은 이벤트 frame에 머리를 다시 읽는다(`VBoRParticipantZones.cpp` `IsHeadInside`) → 상자 Tick보다 먼저 온 이벤트도 맞다. 추적 끊김 frame은 이전 값.
  - 문: 실내 칸이 비면 기존 슬롯(`VBoRHingedPanel.cpp` `QueueLatchSound`). 지연 중 이동해도 처음 슬롯(E3).
  - 전원: 켜짐 → 시동 슬롯 고르고 공회전 시작(지연 적용), 실내↔실외 전환 시 지연 없이 바꿈, 꺼짐 fade, Restart 즉시 정지(E4).
  - 리프트게이트: 대기 중 방향 소리 없음, 끼임 되돌기 frame에 닫힘 fade·열림 시작(E6). 속도 = `OpenAngleDeg / 초`라 남은 각도도 같은 속도.
  - 2열: 반대 명령 거부 그대로, 도착 fade(E7).
  - 키: 차 루트에 붙음, 차 없으면 키(E8).
- Check setup의 "Cabin zone: found N; using first"는 `Problems`에 들어가지만 Check setup 보고일 뿐 시작을 막지 않는다(`ValidationProblems`와 별개, `VBoRScenarioRunner.cpp:567`). 명세 "경고 한 줄"과 맞다.

### 참고 (고치지 않아도 됨)

1. 노브 `SoundRemainderDeg`는 놓아도 지우지 않는다(`VBoRKnobMotion.cpp` `ReleaseGrab`). 다음에 잡았을 때 첫 소리가 한 칸보다 조금 일찍 날 수 있다(한 칸 미만). 들리면 다음에 고친다.
2. 시동 직후 공회전 지연(`IdleCue.StartDelaySeconds`) 중에 실내↔실외를 넘으면 공회전이 지연 없이 바로 시작돼 시동 소리와 겹친다. 명세 D3대로이고, 좌석에 앉은 채 시동을 거는 평소 흐름에서는 생기지 않는다.

시험 상태: Quest 3 T-08 `not-tested`. 회사 PC에서 소리·`Cabin Zone`·초 값을 넣은 뒤 확인.

## 2회차 (Claude 2026-11-17) — 머지 뒤 회사 PC 크래시

### 문제 1 — R11 전에 저장한 BP에서 슬롯 배열이 작아 Play 첫 Tick에 크래시 — 반영함 (fix: R11 전에 저장한 BP의 소리 슬롯 배열 크래시)

- **문제:** 회사 PC PIE 첫 Tick에 `Array index out of bounds: 2 into an array of size 2`, `AVBoRHingedPanel::AdvanceSounds` (`VBoRHingedPanel.cpp:575`) ← `Tick` (`:457`).
  - 원인은 `SoundAudio`가 `UPROPERTY() TArray`라 저장된다는 점이다(`VBoRHingedPanel.h:116`). 생성자에서 `SetNum(4)`를 해도(`VBoRHingedPanel.cpp:48`), R11 전에 저장한 BP CDO·레벨 인스턴스를 불러오면 저장된 크기 2가 덮어쓴다. 그 뒤 `SoundAudio[2]`·`[3]`을 고정 번호로 읽는다(`:575-576`, `:585-586`).
  - 같은 구조로 슬롯이 늘어난 곳이 두 군데 더 있다. 둘 다 같은 크래시가 난다.
    - `UVBoRVehiclePowerComponent`: 2 → `SoundSlotCount` 5(`VBoRVehiclePowerComponent.h:50`). `Tick`이 `:127-138`에서 고정 번호로 읽는다.
    - `AVBoRPoweredPanel`: `Count` → `SlotCount`(`VBoRPoweredPanel.cpp:47`). `:277·283·291·298·315-316`.
  - 자동 시험은 새로 Spawn한 액터만 써서 이 경우를 못 잡았다.
- **제안:**
  - 저장값을 읽은 뒤 배열 크기를 슬롯 수로 맞춘다. 새 칸은 `nullptr`로 둔다. 새 칸은 원래 첫 요청 때 만든다(`VBoREnsureCueAudio`, `VBoRSoundCue.cpp:6`). `FVBoRSoundPlayback::Play`도 `Audio`가 없으면 돌아간다(`VBoRSoundCue.cpp` `if (!Audio ...) return`). 따라서 null 칸은 안전하다.
  - 크기를 맞추는 곳은 `PostLoad`가 아니라 `PostInitProperties`나 `BeginPlay` 첫 줄을 권장한다. `PostLoad`는 레벨 인스턴스 복제(PIE 복사) 경로를 다 덮는지 확신이 없다. 가장 단순한 것은 세 클래스의 `BeginPlay` 첫 줄이다. 시험에서 쓰는 `Tick` 전 경로도 `BeginPlay`를 거치는지 확인한다.
  - 줄이지는 않는다(`SetNum`은 늘릴 때만). 기존 0·1번 칸은 저장된 서브오브젝트 그대로 둔다.
  - `UPROPERTY(Transient)`로 바꾸는 방법도 있다. 다만 BP 인스턴스에서 기본 서브오브젝트 포인터 복사가 어떻게 되는지 확인이 더 필요하다. 그래서 권장하지 않는다.
  - 고정 번호로 읽는 다른 부품(`AxisPart` 4, `FoldingMirror` 4)은 R11에서 늘지 않았다. 그래도 같은 줄을 넣어 다음 슬롯 추가에 대비한다. 이것은 명세 밖 확장이 아니라 같은 결함의 예방이다. 넣을지는 구현자가 정한다(보고 ③).
- **확인할 시험:** `vBoR.R11.E10` — 세 클래스 각각에 대해 액터를 Spawn한다. `BeginPlay` 전에 `SoundAudio`를 옛 크기(문 2, 전원 2, 리프트게이트 `Count`)로 줄인다(시험 접근자). 그다음 `BeginPlay`와 몇 Tick을 돌린다. 실내 문 소리나 공회전 같은 새 슬롯 소리를 하나 요청해 크래시가 없는지, 배열 크기가 슬롯 수인지, 그 슬롯 오디오가 만들어지는지 본다.
- **지켜야 할 조건:**
  - 기존 E1–E9 단언은 바꾸지 않는다.
  - P5s E9 "첫 요청 전 오디오 0개"를 유지한다(크기만 늘리고 컴포넌트는 만들지 않는다).
  - BP·레벨 에셋은 고치지 않는다(다시 저장해서 피하는 방법은 쓰지 않는다 — 다른 PC의 옛 BP도 있다).

**반영(사용자 요청으로 Claude가 직접 구현, 독립 리뷰 없음):** 세 클래스 `BeginPlay` 첫 줄에서 모자라면 `SetNum`(늘리기만, 새 칸 null). 시험 접근자 `ShrinkSoundAudioForTest`·`GetSoundAudioNumForTest`. `vBoR.R11.E10OldSavedSlots` 추가. `AxisPart`·`FoldingMirror`는 R11에서 늘지 않아 넣지 않았다. non-unity·unity 빌드 성공, `vBoR.*` 784/784. Quest·회사 PC 확인 `not-tested`.
