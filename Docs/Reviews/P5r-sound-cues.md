# P5r 소리 칸 — 리뷰

명세: [P5r-sound-cues.md](../Specs/P5r-sound-cues.md) · 브랜치: `codex/P5r-sound-cues` · 베이스: `main@fa43228`

상태: **1회차 리뷰 통과 — 문제 없음. 열린 항목 없음. 머지 승인 대기.** 질문 없음.

## 리뷰 1회차 (2026-10-21, Claude — `cf59ae6`)

**결과: 통과.** 명세 D1–D6과 E1–E14를 지켰다. 범위 밖으로 커진 것은 없다.

**바뀐 것:**
- `Interaction/VBoRSoundCue.h/.cpp`: 구조체 `FVBoRSoundCue`·`FVBoRHeldSoundCue`와 칸 하나의 재생 도우미 `FVBoRSoundPlayback`.
- `AVBoRPoweredPanel`: 칸 7개, 칸마다 `HingePivot`에 붙은 오디오 컴포넌트.
- 기존 P4b 시험은 `:92` 한 줄만 바뀌었다(§3 감사 그대로).
- BP·레벨·Content는 바뀌지 않았다.

**재현.** 에디터가 닫힌 것을 확인했고, 단계마다 에디터가 다시 열렸는지 보며 돌렸다.
- non-unity·unity `Result: Succeeded`, vBoR 경고·오류 0.
- `vBoR.*` **516/516**(P5r 14개, P4b 12개 포함), `TEST COMPLETE. EXIT CODE: 0`. 로그 `Saved/Logs/P5rReview*.log`.

**Codex 토큰:** 한 세션에 **4.28M**이고, 하위 에이전트는 0이다.
- 빌드와 전체 시험을 두 번씩 돌렸다. 첫 전체 시험에서 새 시험의 결함 두 개(관찰자가 `SetSound` 안의 `Stop`까지 셈, E7의 float 누적)가 나와서 시험만 고쳤다.
- 첫 실패를 숨기지 않고 보고했다(보고 ③).

**확인한 것:**
- **경고음이 지금과 같다(D5).** 옛 `WarnFor`와 새 `BeginHeld`·`Advance`를 같은 입력으로 계산해 봤다.
  - 받은 첫 `Tick`에 1번, 그 뒤 0.5 s마다 난다. 긴 frame이면 넘은 간격 수만큼 센다.
  - 끼임 frame의 남은 시간은 경고에 넣지 않는다. 옛 동작 그대로다(`VBoRPoweredPanel.cpp` 끼임 쪽 `AdvanceSounds(..., false)`).
  - P4b 단언은 하나도 바뀌지 않고 통과한다.
- **멈춘 상태에서도 지연이 흐른다.** 원래 `Tick`은 멈춰 있으면 바로 return했다. 이제 그때도 `AdvanceSounds`를 부른다. 그래서 도착 뒤 지연 소리(`OpenedCue` 0.25 s)도 난다(E7).
- **소리 시간은 움직임 시간과 같은 기준이다.**
  - 받은 첫 `Tick`의 frame 시간부터 센다. 그래서 대기 1 s짜리 게이트는 지연 1 s짜리 받음 확인음과 같은 frame에 움직이기 시작한다.
  - 모터 지연은 움직임이 시작된 frame 안의 남은 시간부터 센다.
- **콜백 안에서는 소리가 없다(D6).** 받음 칸과 경고는 `bPendingCommand`를 처리할 때만 예약한다(E2·P4b E11).
- **frame 시간(E11).** 1.0 s 한 번과 0.1 s 열 번으로 돌리면 칸마다 요청 수가 같다. 긴 frame에서도 실제 `Play`는 칸마다 frame당 한 번이다. 0.05 s 하한도 시험한다.
- **초기화(E10).** 대기·움직임·지연 중에 `ResetPanel(true/false)`와 `EndPlay`를 하면 예약이 지워지고 바로 멈춘다. 그 뒤 10 s 동안 요청이 0이다. B·R의 경고 횟수 규칙은 지금과 같다.
- **칸끼리 안 끊는다(E13).** 같은 frame의 닫힘·받음 칸이 다른 칸의 멈춤에 지워지지 않는다. 같은 칸의 지연 예약도 이벤트마다 남는다.
- **무음 칸**은 요청을 세지 않는다(E1).
- **시험 전용 멤버**(`RequestFrames`·`StopRequests`·접근자·`FP5rTestAccess`)는 모두 `WITH_DEV_AUTOMATION_TESTS` 안에 있다.
- **관찰자의 셈법이 맞다.** 관찰자는 "실제 멈춤 = `Stop` 수 − `Play` 수"로 센다. 엔진 `UAudioComponent::SetSound`는 `bCanPlayMultipleInstances`가 꺼져 있으면 늘 `Stop()`을 한 번 부르므로 이 셈이 맞다(`Engine/Private/Components/AudioComponent.cpp:333-336`).
- 주석 말투는 규칙대로다. 툴팁에 시간 기준점이 적혀 있다.

### 참고

1. **소리가 나는 중에 같은 칸을 다시 내면, 같은 frame에 두 번 처음부터 다시 시작한다.**
   - `SetSound`가 재생 중이면 안에서 `Play()`로 다시 시작한다(`AudioComponent.cpp:345-348`). 그 뒤 `FVBoRSoundPlayback::Play`가 `Play()`를 한 번 더 부른다.
   - 결과는 "처음부터 다시"(D2)로 같아서 문제는 아니다.
   - 반복 경고음처럼 같은 소리를 다시 낼 때는 `SetSound`를 건너뛰는 것이 깔끔하다. 그러면 관찰자 셈법(위 확인 항목)이 바뀌므로 P5s에서 같이 정한다.
2. **칸별 오디오 컴포넌트는 Details에서 편집할 수 없다**(`UPROPERTY()` 배열). 팀은 칸(`Powered Panel|Sound`)만 고치면 되므로 의도대로다. BP 컴포넌트 목록에는 오디오 컴포넌트 7개가 보일 수 있다.
3. **런타임에 `Attenuation`을 비워도 앞서 넣은 감쇠가 남는다.** 칸마다 컴포넌트가 하나라서 그렇다. BP에서 미리 정하는 값이라 영향이 없다(E12가 이 동작을 고정한다).
4. **게이트 버튼 두 개의 딸깍 소리는 아직 ISDK 기본이다.** 명세 범위 밖이고 P5s에서 다룬다.
5. **Blueprint-Guide**의 리프트게이트 줄에 소리 칸을 적어야 한다. P5s와 함께 Claude가 고친다.
6. **실제로 들리는지는 확인하지 않았다.** 자동화는 `-nosound`로 요청만 센다. 소리 순서·타이밍·위치감·루프·페이드는 VR에서 듣는다(명세 §7). T-02·T-04는 `not-tested`다.

## 질문

(없음)
