# P5l 룸미러 개정 — 리뷰

명세: [P5l-mirror-follow.md](../Specs/P5l-mirror-follow.md) · 브랜치: `codex/P5l-mirror-follow`

## 리뷰 1회차 (2026-10-17, Claude — `4a89c48`)

**결과: 통과. 열린 항목 없음.** D1–D4가 명세대로 들어갔다.
- 기존 단언은 명세 §3이 허용한 곳(손목으로 잡기를 시험하던 E2·E3·E4·E6·E8·E12)만 새 기대값으로 바뀌었다.
  - E5·E7은 공통 helper(`P5eMove`)가 위치 입력으로 바뀌어 같이 영향을 받았다. 단언 값은 그대로이고, E7은 문구만 "wrist" → "grab point"로 바뀌었다(보고 ③).
  - E20은 `BeginGrab` 인자 형만 바뀌었다.
  - 옛 이름 → 새 이름은 보고 ③에 있다: `E2WristYawAnywhere` → `E2GrabPointYaw`, `E6TranslationRollAndJump` → `E6WristIgnoredAndDistanceJump`.
- 앞면 밀기 시험(P5e E9·E10·E11·E14·E20)과 추적 시험(E13)은 입력·단언 그대로 통과한다.
- `Content/`에 이 클래스를 쓰는 Blueprint는 없다(grep). 그래서 D4 때문에 깨지는 에셋은 없다.
- Codex 보고는 커밋 본문에 있다. 첫 구현이라 리뷰 파일이 없어서 Claude가 이 파일을 만들었다.

**재현(에디터가 닫힌 것을 확인한 뒤, 바뀐 C++ 7개의 시각을 갱신):**
- non-unity(`-DisableUnity -Module=vBoR`): `Result: Succeeded`, 경고·오류 0. 로그 `Saved/Logs/ReviewP5lNonUnity.log`.
- unity: `Result: Succeeded`, 경고·오류 0. 로그 `ReviewP5lUnity.log`.
- `vBoR.*`: 345/345(P5l 10개 포함), `TEST COMPLETE. EXIT CODE: 0`. 로그 `ReviewP5lAll.log`. 끝난 뒤 `UnrealEditor` 프로세스 없음.

**Codex 토큰:** 6.03M(입력 6.01M 중 캐시 5.82M, 약 97%), 2026-10-17 13:56–14:08 UTC로 약 12분. 하위 에이전트는 0이다. 재실행은 오류를 고치기 위한 것이라 규칙 안이다(보고 ③).
- 전체 시험이 처음 343/345였다. 실패 둘은 P5e E13(뒷면 시작 조건)과 P5l E13(경고 집계)이다. 고친 뒤 관련 33개, 다시 전체를 돌렸다.

**확인한 것:**
- **D1 잡기**(`Interaction/VBoRTwoAxisMotion.cpp:22-48`):
  - 입력은 잡기 자세의 위치뿐이다(`Interaction/VBoRTwoAxisPart.cpp:137`, `:145`). 손목 회전은 어디에도 들어가지 않는다. E6이 X·Y·Z 축 20° 회전으로 확인한다.
  - 좌우: 피벗 좌표 XY 방위각 `atan2(Y,X)`의 차이에 `L/max(L,MinLeverCm)`를 곱한다(`:30-35`). 위아래: 두 점을 **새** 좌우 각으로 되돌린 뒤 `atan2(Z,−X)`의 차이로 잰다(`:36-44`). 잡기 자세는 회전하지 않는 `BallJoint` 기준이라(`VBoRTwoAxisPart.cpp:103`) 좌우 축과 같은 좌표다.
  - 값을 직접 맞춰 봤다:
    - (−4,0,0) +Z 1 cm: 앙각 0 → 14.04°, 지렛대 4 → ×0.4, 위아래 각 −5.61°. 법선 Z = −sin(−5.61°) > 0이라 유리가 위를 본다.
    - +Y 1 cm: 좌우 각 −5.61°. 법선이 +Y로 돈다.
    - (−4,12,0) −X 1 cm: 지렛대 12.65라 줄이지 않고 +4.18°. +X 1 cm는 −4.40°다.
  - 각도 뜻은 그대로다: `Rotation()`·`ApplyNormal`(`:3-12`)은 바뀌지 않았다(§3).
  - P5l E1E2는 Actor를 37° 돌려 놓고 법선을 실제 `GetActorRightVector`·`GetActorUpVector`와 비교한다.
- **D2 튐·추적**(`:26-28`, `:47`): 한 frame 이동이 5 cm를 넘으면 각도에 쓰지 않고 기준만 옮긴다. 복구 frame은 이동 거리를 `RecoveryDiscardCm`에 남기고 돌지 않는다. 디버그 문구도 cm다(`VBoRTwoAxisPart.cpp:198-201`). P5l E8이 버린 3 cm와 그 뒤 1 cm(지렛대 5 → −4.07°)를 본다.
- **D3 뒷면 밀기**(`:74-85`, `:106-110`):
  - 면 번호를 `FindPush` 안에서 고른다. 이어지는 밀기는 시작한 점·면만 본다(`:74-76`). `ClearPush`가 면 번호도 지운다(`:52`).
  - 뒷면 면 거리는 `−FaceMaxCm.X`다. 부호를 따져 봤다: 손가락이 뒷면을 넘으면 N·P > d가 되어 각이 음수다. 법선이 손가락에서 멀어지고, 닿은 끝이 운전자 쪽으로 온다. P5l E9가 좌우 끝·위 끝 둘 다 확인한다.
  - 무효 진입 다섯(가운데·사각형 밖·두께보다 깊이·옆에서 미끄러짐·면 위에서 출발)은 P5l E10이 본다.
- **D4 재질:**
  - 부모 `BeginPlay`는 덮개 재질만 만든다(`VBoRTwoAxisPart.cpp:84-93`). `UpdateVisual`도 덮개만 바꾼다(`:191-196`).
  - 근사 차는 `FinishSpawning` 뒤에 칠한다(`P3Probe/P3VehicleMock.cpp:211-217`). 이미 실행 중인 월드에서는 `FinishSpawning` 안에서 거울 `BeginPlay`가 끝나므로, 근사 차가 칠한 색이 마지막이다. P5l E15가 확인한다.
- **설정 검사**(`VBoRTwoAxisPart.cpp:65`): `MinLeverCm`·`MaxGrabJumpCm`는 유한하고 0보다 커야 한다. 기존 항목(0 허용)과 따로 묶었다. P5l E13이 0·−1·NaN을 본다.
- **시험 집계 보정**(`P3Probe/P5eRearviewMirrorTests.cpp:61-72`): `AddExpectedMessagePlain`으로 등록한 메시지는 UE가 Verbose로 낮춰 넘긴다(엔진 `Runtime/Core/Private/Misc/AutomationTest.cpp:289-291`, Claude 확인). 그래서 같은 문구의 Warning·Verbose를 세게 바꿨다. P5e E15의 "construction 경고 0"은 더 엄격해질 뿐이다.

### 참고 1 — P2e로 넘김: 거울 면도 메시 bounds에서 나온다

- **위치:** `Interaction/VBoRTwoAxisPart.cpp:66-72`. 앞·뒷면, 면 사각형, 잡기 상자가 모두 `PartMesh` 정적 메시의 bounds를 피벗 좌표로 옮긴 상자다. 문(`VBoRHingedPanel.cpp:101-106`)과 같은 방식이다.
- **영향:** 실제 거울 메시에 받침대(stem)·하우징이 같이 들어오면 상자가 받침대 끝까지 커진다.
  - 뒷면(`FaceMaxCm.X`)이 피벗 근처나 그 뒤로 간다. 유리 뒤에 넣은 손가락은 상자 안에 있어서 뒷면을 넘지 못한다. 그러면 D3 뒷면 밀기가 사라진다.
  - 잡기 상자도 같이 커진다.
- **넘길 곳:** P2e의 "밀기 면 상자 옵션"을 문만이 아니라 거울에도 쓰게 한다. 예: 면을 정하는 상자 컴포넌트를 두고, 비어 있으면 지금처럼 bounds를 쓴다. 지금은 근사 상자뿐이라 문제가 나지 않는다.

### 참고 2 — VR 확인 때 볼 것 (명세 §7에 더함)

1. **뒷면은 바로 앞 frame에 확실히 뒤에 있어야 시작한다**(`VBoRTwoAxisMotion.cpp:81`).
   - 앞면은 0.5 cm 안쪽까지 허용하고, 뒷면은 허용하지 않는다. 보고 ③의 이유를 받아들인다: 같은 허용을 주면 뒷면 위에서 복구한 점이 앞으로 3 cm 가는 것을 뒷면 밀기로 받는다(P5e E13).
   - 그래서 손가락이 하우징 안쪽에 살짝 들어가 있으면 밀어도 안 돈다. 손가락을 뒤로 뺐다 다시 밀면 된다.
   - VR에서 뒷면 밀기가 잘 안 걸린다고 느끼면 그때 허용 폭과 깊이 한도를 같이 고친다.
2. **앞에서 거울을 뚫고 뒤로 손을 넣으면 먼저 앞면 밀기가 걸린다.** VR이라 거울이 손을 막지 않는다.
   - 가운데 2 cm 밖을 뚫으면, 앞면 밀기가 손가락을 따라 거울을 한계까지 돌릴 수 있다. 끝나는 때는 두께 + 여유(3 cm)보다 깊어질 때다. 이것은 P5e 앞면 규칙 그대로다.
   - 사용자가 가장자리를 돌아 뒤로 손을 넣는지, 뚫고 넣는지 본다. 뚫고 넣는 일이 많고 거슬리면 따로 정한다.
3. **실제 SDK 잡기 위치의 피벗 변환은 자동화로 보지 않는다.** 시험 fixture가 `ReadGrabPose`를 바꿔 끼우기 때문이다(`P3Probe/P5eTestFixture.h:16`). 변환 줄(`VBoRTwoAxisPart.cpp:103`)은 P5e 그대로이고 위치만 더 꺼내 쓴다. 명세 §7-1(손을 올리면 면이 위를 보는지)이 이것을 확인한다.

## 개정 이력

| 날짜 | 회차 | 내용 |
|---|---|---|
| 2026-10-17 | 1회차 | 통과, 열린 항목 없음. 참고 1(P2e — 거울 면도 메시 bounds), 참고 2(VR 확인 셋) |
