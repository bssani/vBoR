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
