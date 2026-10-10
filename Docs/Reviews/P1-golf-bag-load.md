# P1 리뷰 기록

명세: [Docs/Specs/P1-golf-bag-load.md](../Specs/P1-golf-bag-load.md) · 브랜치: `codex/P1-golf-bag-load`

**Codex가 할 일:**
- 상태가 `열림`인 항목을 위에서부터 반영한다.
- 항목의 세 칸은 이렇게 다룬다:
  - **문제**는 반드시 해결한다.
  - **제안**은 권장이다. 다르게 하면 보고 ③에 이유를 적는다.
  - **확인할 시험**은 새로 넣거나 고칠 자동화 시험이다.
- 같은 브랜치에 새 커밋으로 한다. 반영한 항목의 상태를 `반영함 (<커밋 제목>)`으로 바꿔 같은 커밋에 넣는다. 커밋 본문에 보고 ①~⑤를 쓴다.
- 판단이 필요하면 맨 아래 "질문"에 적고 멈춘다.
- push·`main` 머지는 하지 않는다. 끝나면 `HANDOFF.md`의 "Codex 결과"를 채운다.

명세가 이번 회차에 바뀌었다(명세 머리 표의 "개정" 행). 이 리뷰 파일과 함께 `main`에서 이 브랜치로 머지해 두었다.

## 회차

| 회차 | 날짜 | 리뷰 대상 | 리뷰어 | 결과 |
|---|---|---|---|---|
| 1 | 2026-10-10 | `0824ebf` P1 골프백 실험 | Claude | **수정 후 실측.** 빌드·시험은 통과. 6건 — 트렁크 자리값 오류(Claude 명세 실수), 회전 제한의 Yaw 뒤집힘, 높이 기준, 목표 밖 낙하를 "안착"으로 표시, 막힘 거리 정의, 반복 시험용 되돌리기 |

### 1회차에 재현한 것

- 에디터를 닫은 채 빌드를 두 번 했다. 둘 다 경고·오류 0이다(샘플 플러그인 끔).
  - unity 빌드
  - `-DisableUnity` 빌드: 파일마다 따로 컴파일해 빠진 include를 찾는다
- `vBoR.*` 35개 Success(P0 24 + P1 11), `TEST COMPLETE. EXIT CODE: 0`. 로그: `Saved/Logs/ClaudeP1Review.log`.

**잘 된 것:**
- **보류 처리:**
  - 보류 중 ISDK release를 무시하고, 소유 손 세션을 ISDK 선택과 따로 둔다.
  - 복구 첫 frame은 rebase만 한다. rebase를 거듭해도 처음 잡을 때 기준 기울기 한도를 넘지 않는다(`VBoRCarryMotion.h:91-97`).
  - 합성 시험이 이 경계를 정확히 짚는다.
- **잡기 정책:**
  - 던지기는 생성자에서 끈다(`bIsThrowable=false`, Throwable 컴포넌트 0개 시험).
  - 두 번째 손은 `SingleGrabFirstRetained`와 세션 소유 검사로 이중으로 막는다.
  - 형상 scale을 보존하고, 받침대에 닿은 시작 자세를 0.005 cm 여유로 처리한다.
  - P0 기본값(잡기 꺼짐)이 유지된다는 시험이 있다.
- **손이 골프백의 충돌 검사를 막지 않는다.** Claude가 확인했다.
  - ISDK 손 메시는 `UPoseableMeshComponent` 기반이다. 기본값이 NoCollision이고(`Engine/Source/Runtime/Engine/Private/Components/SkinnedMeshComponent.cpp:462`), ISDK가 이를 바꾸지 않는다.
  - 손바닥 잡기 콜라이더는 모든 채널에 Overlap이다(`Plugins/MetaXRInteraction/Source/OculusInteraction/Private/Interaction/GrabDetectors/IsdkHandGrabDetector.cpp:43-45`).
  - 그래서 `OverlapBlockingTestByChannel`·`SweepSingleByChannel`에 걸리지 않는다.
- 기본값으로는 실을 수 없다는 것을 숨기지 않았다. 값을 임의로 바꾸지 않고 보고 ③과 시험에 드러냈다. 규칙대로 한 것이다.
- 주석 말투가 맞다. 샘플 플러그인 참조가 없고, `Config`·`Content`·`Plugins` 변경도 없다.

## 열린 항목

### R1-1 트렁크 안쪽 폭 자리값을 130 cm로 바꾼다 (Claude 명세 오류) — 열림

**문제**
- **위치:**
  - `Source/vBoR/P1Probe/P1TrunkMock.h:20` `InsideWidthCm = 100`
  - 시험: `P1LoadBenchTests.cpp:20, 25, 73-74`. 폭 100과 "들어가지 않음"을 기본 동작으로 고정한다.
- **발생 조건:** 기본값 그대로 실행하면 실을 수 없다.
  - 가로로 넣으면 120 cm 골프백이 폭 100 cm 입구를 지나지 못한다.
  - 세로로 넣으면 깊이 90 cm보다 길다.
  - 원인은 명세 §2의 자리값이다. Codex 구현은 명세대로다.
- **영향:** 명세 §7의 적재 10회와 보조 안착을 기본값으로는 시험할 수 없다.

**제안**
- `InsideWidthCm` 기본값을 130으로 한다(명세 §2 개정). 양옆이 5 cm씩 남는다. 받침대 폭 130 cm(`StandSizeCm.Y`)는 그대로 둔다.
- "들어가지 않으면 크기를 바꾸지 않고 '안 맞음'을 표시한다"는 지금의 좋은 시험은 지우지 않는다. 시험 안에서 폭을 100으로 설정해 그대로 남긴다.

**확인할 시험**
- `MockGeometry`: 기본 좌우 벽 안쪽 면이 ±65 cm다.
- `PlacementAndReset`: 기본값에서 `IsStaticFit` true, 패널 "clear".
- 폭 100으로 설정한 경우: `IsStaticFit` false, 패널 "overlap / does not fit", 골프백 크기 그대로.

### R1-2 회전 제한이 Pitch ±90° 근처에서 Yaw를 뒤집는다 — 열림

**문제**
- **위치:** `Source/vBoR/Interaction/VBoRCarryMotion.h`. 두 곳이 모두 회전 차이를 `FRotator`로 바꾼 뒤 Pitch·Roll을 축별로 자른다.
  - `:9-11` `LimitedHandDelta`
  - `:92-94` `Update`의 잡기 기준 재제한
- **발생 조건:**
  - `FQuat::Rotator()`의 Pitch는 −90~+90° 범위뿐이다. 그 밖의 회전은 같은 회전을 Yaw+180·Roll+180으로 표현한다. 예: 순수 Pitch 100° → (Pitch 80, Yaw 180, Roll 180).
  - Roll 180을 15로 자르면 앞 frame(Yaw 0)과 비교해 물체 Yaw가 한 frame에 180° 바뀐다.
  - ±90° 근처에서는 손이 조금만 움직여도 Yaw가 크게 흔들린다(짐벌).
  - 손바닥 상대 회전이 이 범위에 드는 경우: 트렁크 안으로 밀며 손목을 꺾고 팔을 드는 동작, 손 방향이 뒤집힌 High frame 하나.
- **영향:** 120 cm 골프백이 공중에서 반 바퀴 돈다. 회전 겹침 검사는 지금 위치에서 겹칠 때만 막으므로 빈 공간에서는 그대로 돈다. §3 "튐 없음"에 어긋난다.

**제안**
- 월드 수직축 기준 swing-twist로 나눈다.
  - `FQuat::ToSwingTwist(FVector::UpVector, Swing, Twist)`(`Engine/Source/Runtime/Core/Public/Math/Quat.h:456`). 구현은 `this = Swing * Twist`다(`Engine/Source/Runtime/Core/Private/Math/UnrealMath.cpp:793-813`).
  - Twist는 그대로 따르고(Yaw), Swing은 각도를 `TiltLimitDeg`로 자른다(축은 유지).
- 두 곳 모두 바꾼다: 손 회전 차이, 잡을 때 기준 재제한.
- 축별 ±15°와 원뿔 15°는 대각 방향에서 조금 다르다. 명세는 어느 쪽이든 받는다. 고른 것을 보고 ③에 적는다.
- swing-twist도 Swing이 정확히 180°일 때는 특이점이 있다(`UnrealMath.cpp:801-805`). 그래도 ±90°와 달리 실제로 거의 생기지 않는다.
- P0-a 다이얼이 같은 분해를 쓴다(`Source/vBoR/P0Probe/P0ProbeDialMeasurement.cpp:78-81`). 부호를 W ≥ 0 반구로 맞추는 처리도 거기 있다.

**확인할 시험**
- 손 Pitch를 80°→100°로 1°씩 바꾸는 frame 열:
  - frame 사이 물체 Yaw 변화가 작다(1° 남짓)
  - 기울기는 한도 안
- 손 방향이 뒤집힌 frame 하나가 들어와도 Yaw가 180° 바뀌지 않는다.
- 기존 시험(Yaw 85° 따라감, 반복 rebase 한도)이 그대로 통과한다.

### R1-3 트렁크 높이를 가정한 눈높이가 아니라 물리 바닥에서 잰다 — 열림

**문제**
- **위치:**
  - `Source/vBoR/P1Probe/P1LoadBench.cpp:129`: 시험대 원점 Z = 머리 Z − `AssumedEyeHeightCm`
  - `P1LoadBench.h:134`: `AssumedEyeHeightCm` 기본 165
- **발생 조건:** 배치 순간(첫 추적 frame, `R`)의 눈높이가 165 cm가 아니면 트렁크 바닥이 물리 바닥에서 75 cm가 아니게 된다. 키가 다른 사람, 앉거나 숙인 자세가 그렇다.
- **영향:** 키·자세에 따라 트렁크 바닥이 수십 cm 오르내린다. P1에서 사용자가 몸으로 느끼는 핵심(들어 올리는 높이)이 시도마다, 사람마다 달라진다. 명세의 "바닥 높이 75 cm"는 물리 바닥 기준이었다(명세 §2 개정으로 명시함).

**제안**
- 수평 위치·Yaw는 지금처럼 머리에서 가져온다. 원점 Z는 Pawn 원점 Z로 한다.
  - Pawn 루트 `VROrigin`이 추적 원점이다(`Source/vBoR/Interaction/VBoRHandRigPawn.cpp:47-50`).
  - UE 5.7 OpenXR은 Stage를 고르고(`Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.cpp:2332-2335`), Stage 원점은 바닥이다(`:491-492`).
- `AssumedEyeHeightCm`는 지운다.
- 레벨 바닥이 물리 바닥과 다를 수 있다(PlayerStart 높이). 목표 밖으로 떨어진 골프백이 물리 바닥보다 아래로 내려가 보일 수 있다. P1에서는 받아들인다. 로그에 Pawn 원점 Z를 남긴다.

**확인할 시험**
- `PlacementAndReset`: Pawn 원점 Z를 고정하고 머리 높이를 120·165·190 cm로 바꿔 배치한다. 매번 트렁크 바닥 윗면 월드 Z = Pawn 원점 Z + 75.

### R1-4 목표 밖에서 멈춘 것을 "안착"(초록)으로 표시한다 — 열림

**문제**
- **위치:** 어디서 멈추든 `bSeated`·`OnSeated`가 된다.
  - `Source/vBoR/Interaction/VBoRCarryObject.cpp:263-269` `FinishPlacement`가 상태를 바꾼다.
  - `:343`에서 "Seated", `:366-367`에서 초록 덧입힘이 된다.
  - 시험대는 "Bag seated", "Final placement snapshot"으로 기록한다(`P1LoadBench.cpp:285-289`).
- **발생 조건:** 목표 밖에서 놓아 바닥·받침대·범퍼에 떨어지면 생긴다.
- **영향:** 실패한 적재가 성공 색으로 보인다. VR에서는 색이 유일한 결과 신호다(AGENTS.md: 접촉·조작 확인은 시각·청각뿐). 진행자 로그도 성공처럼 읽힌다. 명세가 "안착"을 정의하지 않은 탓도 있다 → 명세 §2를 개정해 "목표 안·겹침 없음일 때만"으로 정의했다.

**제안**
- 멈춘 순간 `LoadTarget->IsObjectInside`로 나눈다.
  - 목표 안: "안착" — 초록, `OnSeated`
  - 목표 밖: "놓임(목표 밖)" — 초록 아님, `OnSeated` 없음
- 보조 안착은 끝나면 안착이다.
- 결과 문자열을 셋으로 나눈다: 보조 안착 / 낙하 → 목표 안 / 낙하 → 목표 밖.

**확인할 시험**
- 목표 밖 낙하 정지:
  - 상태 문자열이 "놓임(목표 밖)"에 해당
  - `OnSeated` 호출 0
  - 덧입힘 색이 `SeatedColor`가 아님
- 목표 안, 보조 범위 밖에서 놓아 낙하 정지: 안착, `OnSeated` 1.

### R1-5 막힘 거리가 잡은 자리 오프셋을 포함한다 — 열림

**문제**
- **위치:** `Source/vBoR/Interaction/VBoRCarryObject.cpp:244`. `MaxHandGapCm` = 손바닥 위치와 Actor 위치(원통 중심) 사이 거리다.
- **발생 조건:** 골프백 한쪽 끝을 잡으면 막히지 않아도 손바닥과 중심이 약 50 cm 떨어져 있다.
- **영향:** 패널·로그의 "max hand/object gap"이 잡은 위치에 좌우된다. "막혔을 때 손이 얼마나 더 밀고 들어갔나"를 알 수 없다. 명세 문구("손과 물체 사이 최대 거리")가 모호했다 → 명세 §2를 개정했다.

**제안**
- 막힌 frame에서 잰다: 요청 자세(`Session.Update`가 낸 `Desired`)의 위치와, `TryMoveToPose` 뒤 실제 위치의 거리. 그 최댓값을 쓴다.
- 이름을 "max requested/actual gap"처럼 바꾸면 오해가 없다.

**확인할 시험**
- 손바닥이 중심에서 50 cm 떨어진 채 벽 쪽으로 10 cm 더 밀면 최대 거리 ≈ 10 cm다(50 cm대가 아님).

### R1-6 반복 시험용으로 골프백만 되돌리는 키를 둔다 — 열림

**문제**
- **위치:** 골프백을 받침대로 돌려놓는 방법이 `R`뿐이다(`P1LoadBench.cpp:106-108`, `:126-149`). `R`은 두 가지를 같이 한다.
  - 그 순간 머리 위치·Yaw로 시험대 전체를 다시 놓는다.
  - 기록을 전부 지운다(`VBoRCarryObject.cpp:324-328`, `P1LoadBench.cpp:138-142`).
- **발생 조건:** 명세 §7의 잡기 10회·적재 10회는 시도마다 골프백을 되돌려야 한다.
- **영향:**
  - 서 있는 자리·보는 방향에 따라 매 시도 트렁크 위치가 바뀌어 시도끼리 비교가 안 된다.
  - 패널에 누적이 남지 않는다.
  - 로그에 시도 번호가 없다.
  - 명세에 없던 것이다(Claude 빈틈) → 명세 §2를 개정했다.

**제안**
- `B`(프로퍼티) 키를 둔다.
  - 시험대·트렁크는 그대로 둔다. 골프백만 `ResetToPose(BagStartPose())`로 돌린다. 잡고 있던 손 세션도 지금처럼 먼저 끝낸다.
  - 시도 번호를 1 올리고, 직전 시도 요약을 로그 한 줄로 남긴다: 결과, 잡은 시간, 막힘 횟수·시간, 보류, 목표 오차.
- 누적 값은 골프백 리셋에 지워지지 않는 곳에 둔다(시험대 쪽, 또는 골프백의 시도별·누적 구분).
- `R`은 지금처럼 전부 지운다.

**확인할 시험**
- `B` 뒤 확인할 것:
  - 트렁크·시험대 변환이 전과 같음
  - 골프백이 받침대 시작 자세
  - 소유 손 없음
  - 시도 번호 +1
  - 누적 값 유지
  - 입구 통과 수를 지어내지 않음
- `R`은 여전히 누적까지 지운다.

## 로컬에서 확인할 것 (반영 뒤)

- **에디터:**
  - `Content/P1/L_P1_GolfBag` 레벨(Basic + PlayerStart), GameMode Override = `P1LoadBenchGameMode`
  - 근사 형상 표기, 눕힌 원통, 패널, 반투명 목표, 덧입힘 색
- **Quest 3:** 명세 §7 표 그대로. 이번 반영으로 더 볼 것:
  - 골프백을 잡은 채 손목을 크게 꺾어도 공중에서 돌지 않는지(R1-2)
  - 트렁크 바닥이 허리 아래 적당한 높이로 느껴지는지(R1-3)
  - 목표 밖에 떨어뜨렸을 때 초록이 아닌지(R1-4)
  - `B`로 10회 반복(R1-6)
- **추적 손실 순서는 기기로만 확인된다.** ISDK release와 추적 손실 판정 중 무엇이 먼저 오는지, 그래서 잡은 채 손을 시야 밖으로 뺄 때 골프백이 떨어지지 않고 멈추는지를 본다. Codex가 SDK 소스 순서를 확인했지만(보고 ④) 실제 손 추적 신뢰도가 떨어지는 시점과 다를 수 있다.

## 질문

없음.
