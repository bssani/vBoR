# P5s 부품 소리 칸 — 리뷰

명세: [P5s-part-sounds.md](../Specs/P5s-part-sounds.md) · 브랜치: `codex/P5s-part-sounds` · 베이스: `main@630eea8`

상태: **1회차 리뷰 — 문제 1개(R1-1, 내 명세 감사의 빈틈), Claude가 직접 반영함(사용자 2026-10-21 "Claude가 고침"). 참고 1도 같이 막음. 열린 항목 없음. 머지 승인 대기.** 질문 없음.

## 리뷰 1회차 (2026-10-21, Claude — `179972b`)

**결과: 문제 1개.** R1-1은 팀이 시동음을 넣으면 `vBoR.P4i.E2`가 실패하는 문제다. 원인은 내 명세 D11·§3이다. 나머지는 명세대로다.

**바뀐 것:**
- 부품 9종에 소리 칸이 들어갔다: 버튼, 게이트·키의 버튼 칸, 1축 부품, 경첩 판, 노브, 휠, 박스, 미러, 전원.
- `FVBoRSoundPlayback`은 지연 0 소리를 이벤트마다 실제로 낸다(D2). 긴 frame의 합침은 `Advance`에만 남았다.
- 기존 시험은 관찰 도우미 본문 다섯 곳과 허용한 두 줄(`P5fKnobTests.cpp:187`, `P4iCarBlueprintTests.cpp:143`)만 바뀌었다. 기대 수는 그대로다.
- BP·레벨·Content는 바뀌지 않았다.

**재현.** 에디터가 닫힌 것을 확인했고, 단계마다 에디터가 다시 열렸는지 보며 돌렸다.
- non-unity·unity `Result: Succeeded`, vBoR 경고·오류 0.
- `vBoR.*` **528/528**, `TEST COMPLETE. EXIT CODE: 0`. 로그 `Saved/Logs/P5sReview*.log`.
  - P5s 12, P5r 14, P4i 6, P5p 11, P5q 13개 포함.
  - 도우미가 바뀐 기존 시험도 모두 통과했다: P2d 16, P5b 16, P5c 14, P5d 20, P5f 16, P5m 14.
- R1-1 반영 뒤: non-unity·unity 성공(고친 세 파일이 다시 컴파일됨), vBoR 경고·오류 0. `vBoR.*` **528/528**(P5s 12, P4i 6 포함), EXIT CODE 0. 로그 `Saved/Logs/P5sR1*.log`.

**Codex 토큰:** 한 세션에 **8.85M**이고, 하위 에이전트는 0이다.
- 17:58에 Codex 사용량 한도로 멈췄다. 커밋·최종 전체 시험 전이었다. 한도가 풀린 뒤 같은 대화에서 이어서 끝냈다.
- 빌드는 non-unity 4번(첫 번은 새 시험 컴파일 오류), unity 3번 돌렸다. 전체 시험은 2번이다. 첫 전체 시험에서 P4i E2와 새 시험 결함이 나왔다.
- 보고 ③에 실패와 원인을 그대로 적었다.

**확인한 것:**
- **지금 들리는 소리 그대로(D3).** 기존 소리 칸 13개의 기본값이 ISDK 소리·지연 0이다(E1).
  - 칸은 기존 `Play()`가 있던 그 자리에서 `Queue`된다.
    - 1축 부품: `EvaluateState`와 `Tick`의 걸림 알림
    - 판: `ApplyState`
    - 노브·휠: `Tick` 알림
    - 박스: 막힘 진입
  - 걸림·한계·칸 넘김은 지금처럼 `Tick`에서만 난다(E2 "waits Tick").
- **지연 0 연속(D2).** `Tick` 없이 두 번 누르면 `Play`가 2번이고, 한 `Tick`의 걸림 둘도 2번이다(E3). P5r 시험은 그대로 통과한다.
- **버튼(D4).**
  - 누름 소리는 `PressFilter` 앞에서 난다. 거부된 누름도 소리가 난다(E5).
  - `PointerAudio->SetSelectAudio(nullptr)`이고 `SetPointable`은 그대로다. 그래서 P4i E7의 미리보기 보호 단언 `:137`·`:142`가 그대로 통과한다.
  - 지연이 예약된 동안만 `Tick`이 켜진다(E4 "button idle Tick off").
- **게이트·키(D5).** `FinishSpawning` 전에 칸을 넣는다(E6). 게이트 받음 확인음과 버튼 소리는 각자 칸이다.
- **박스 보류(D9).** `UpdateVisual`에서 보류에 들어가고 나가는 것만 보고 `BeginHeld`·`EndHeld`를 부른다. 반복은 `Tick` 끝의 `Advance`가 한다. 1.0 s 한 번과 0.1 s 열 번이 둘 다 5번이다(E10). 복구·놓음·`ResetToPose`에서 멈춘다.
- **미러(D10).** 방향이 바뀌면 앞 모터를 그 칸의 페이드로 끝내고 새 모터를 같은 frame에 시작한다. `ResetMirror`는 바로 멈추고 아무 소리도 내지 않는다(E8). R 순서(전원 끔 → `ResetMirror`)에서도 남는 소리가 없다.
- **전원(D11).** 실제로 바뀔 때만 소리가 나고, 같은 frame 요청은 한 번이다. R은 소리를 내지 않고 지연 예약도 지운다. 사용 불가면 소리가 없다(E9).
- **정리(E11).** 부품 8종의 `EndPlay`에서 지연 중인 소리가 모두 지워지고 바로 멈춘다(미러·전원도 실제 예약을 만든 뒤에 본다).
- 시험 전용 멤버는 `WITH_DEV_AUTOMATION_TESTS` 안에 있다. 주석 말투는 규칙대로다.

### R1-1 시동음을 넣으면 `vBoR.P4i.E2`가 실패함 — 반영함 (Claude, 이 커밋)

**문제:**
- 위치: `Interaction/VBoRVehiclePowerComponent.cpp`(`179972b`) `BeginPlay`의 `for (I < 2) EnsureSoundAudio(I)`.
- **발생 조건:** 집처럼 `L_Main`에 `Approx`가 있는 상태에서, `BP_Car_Core` 전원 컴포넌트의 `PowerOnCue`나 `PowerOffCue`에 소리를 넣는다.
  - 그러면 `BeginPlay`가 Core에 `UAudioComponent`를 만든다.
  - `vBoR.P4i.E2`는 BP를 Play로 만든 뒤 Core의 scene 컴포넌트마다 근사 차에서 같은 이름의 짝을 찾는다(`P3Probe/P4iCarBlueprintTests.cpp:181-198`).
  - 오디오 컴포넌트는 짝이 없으므로 "Core counterpart AudioComponent_0"으로 실패한다. Codex의 첫 전체 시험에서 나온 실패와 같다.
- **영향:** 사용자가 원한 정상 작업(소리 넣기)이 시험 실패로 보인다. P4i R1-1과 같은 종류다. 지금 통과하는 것은 기본값이 무음이기 때문일 뿐이다.
- **원인은 내 명세다.** D11에 "소리 자리는 소유 Actor 루트"라고 썼다. §3은 "E2는 설정 값만 비교하니 영향 없음"이라고 감사했다. E2가 Core 컴포넌트를 짝짓는 것을 놓쳤다. Codex는 첫 실패로 이것을 찾아 무음일 때만 막았다.

**제안:**
- `BeginPlay`에서 미리 만들지 않는다. 첫 요청 때 만든다(`SetPowerOn`이 이미 `Queue` 전에 `EnsureSoundAudio`를 부른다). E2는 전원을 바꾸지 않으므로 그 시험에서는 컴포넌트가 생기지 않는다.
- 첫 재생 frame에 컴포넌트 하나를 등록하는 비용은 무시할 만하다.
- `UGameplayStatics::SpawnSoundAttached`로 바꾸지 않는다. 장치가 없으면 null이라 P5r D3의 시험 조건에 맞지 않는다.

**반영한 것:**
- `VBoRVehiclePowerComponent.cpp` `BeginPlay`에서 미리 만드는 줄을 지웠다. 오디오는 첫 요청(`SetPowerOn`의 `EnsureSoundAudio`) 때만 생긴다. 주석도 고쳤다.
- 참고 1도 같이 막았다. `FVBoRSoundPlayback::Advance` 첫 줄에서 frame 시간이 유한하지 않으면 0으로 본다.
- P5s E9: 소리를 넣고 Play한 뒤 차체 오디오가 0개이고, 첫 켬 뒤 1개가 차 루트에 붙는지 본다. 그 뒤 R로 꺼서 이어지는 기존 단언의 출발 상태를 맞췄다.
- P5s E4 끝: 1축 부품 지연 중 `Tick(NaN)` 뒤에도 0.3 s에 소리가 난다.
- 기존 시험(P4i E2 포함)은 바꾸지 않았다.

**확인할 시험(처음 적은 것):**
- P5s E9를 고친다. 소리를 넣고 Play한 뒤에는 소유 Actor의 scene 컴포넌트 수가 그대로다(오디오 없음).
- 첫 켬 뒤에는 오디오 컴포넌트가 하나 생기고 차 루트에 붙는다. 지금 `:218` 단언을 첫 켬 뒤로 옮긴다.
- `vBoR.P4i.E2`는 그대로다.

### 참고

1. **frame 시간이 NaN이면 그 칸의 지연 소리가 영영 안 난다 — 반영함 (Claude, 이 커밋, R1-1과 같이).**
   - 1축 부품·노브·휠은 `AdvanceSounds`를 유한성 검사(`if (!FMath::IsFinite(DeltaSeconds) …) return;`)보다 먼저 부른다.
   - `FMath::Max(0.0, NaN)`은 NaN을 돌려준다. 그래서 `Pending`과 `HeldRemaining`이 NaN이 되고, 그 예약은 끝나지 않는다(`VBoRSoundCue.cpp` `Advance` 첫 줄).
   - 실제 엔진의 frame 시간은 유한이라 영향은 거의 없다.
   - `Advance` 첫 줄에서 `!FMath::IsFinite(Seconds)`면 0으로 두면 한 곳에서 막힌다. R1-1과 같이 하면 좋다.
2. **버튼 `PressAudio`가 붙는 자리가 `FixedRoot`에서 `ButtonVisual`로 바뀌었다**("움직이는 쪽", D1). 차이는 3 mm라 영향이 없다. P4i E2의 버튼 비교도 통과한다.
3. **박스 보류음은 의도한 동작 변경이다**(D9: frame마다 → 0.25 s). VR에서 간격을 듣는다.
4. **재생 중인 칸을 다시 내면 처음부터 두 번 다시 시작한다**(P5r 참고 1). 그대로 두기로 했다.
5. **Blueprint-Guide**에 부품별 소리 칸을 적는다. P5r 게이트 칸도 같이 적는다. 머지 뒤 Claude가 한다.
6. **실제로 들리는지는 확인하지 않았다**(`-nosound`). T-04·T-08은 `not-tested`다.

## 질문

(없음)
