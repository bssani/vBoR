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

명세가 1·3·4·5·6회차에 바뀌었다(명세 머리 표의 "개정" 행). 3회차부터 개정은 이 브랜치에 바로 커밋했다.

## 회차

| 회차 | 날짜 | 리뷰 대상 | 리뷰어 | 결과 |
|---|---|---|---|---|
| 1 | 2026-10-10 | `0824ebf` P1 골프백 실험 | Claude | **수정 후 실측.** 빌드·시험은 통과. 6건 — 트렁크 자리값 오류(Claude 명세 실수), 회전 제한의 Yaw 뒤집힘, 높이 기준, 목표 밖 낙하를 "안착"으로 표시, 막힘 거리 정의, 반복 시험용 되돌리기 |
| 2 | 2026-10-10 | `eb67e20` 1회차 반영 | Claude | **통과. 열린 항목 없음.** R1-1~R1-6 제안대로. 남은 것은 에디터 레벨과 Quest 3 확인(`not-tested`) |
| 3 | 2026-10-10 | 첫 VR 시험(`eb67e20`, 사용자) | Claude | **개정 2건.** 트렁크 안 안착 0/22. 두 손 잡기(U-08, R3-1), 기울기 기준 고정·보이는 형상대로 충돌(R3-2). 사용자 레벨 `Content/P1/L_P1_GolfBag.umap` 커밋 |
| 4 | 2026-10-11 | `f6c5a2e` 3회차 반영 | Claude | **R3-1·R3-2 반영 확인. 새 항목 2건.** 안착 판정이 골프백 크기 상자(±0.01 cm)라 트렁크 안에 조금만 비켜 놓아도 "목표 밖"(R4-1, Claude 명세 빈틈), simple collision 없는 메시 경고(R4-2). **빌드·시험 재현 못 함 — 에디터가 열려 있었다** |
| 5 | 2026-10-11 | `7e6aa46` 4회차 반영 | Claude | **R4-1·R4-2 반영 확인. 빌드·시험 재현(R3·R4분): non-unity·unity 경고·오류 0, `vBoR.*` 47/47.** 새 항목 1건 — 부모 기본값에서 목표 자세(120 cm)가 적재 영역(100 cm)보다 길어 값을 안 바꾼 목표의 보조 안착이 "목표 밖"(R5-1, Claude 명세 빈틈). P1 시험대는 영향 없음 → VR 재시험 진행. **VR 재시험(사용자, 같은 날) 뒤 R5-2 추가:** 걸리면 통째로 멈춤 → 접촉 응답 D7(미끄러짐·걸린 곳 축 회전, 사용자 결정). R5-3 기울기 한도 15° → 45°(사용자) |
| 6 | 2026-10-11 | `2c13c32` 5회차 반영 | Claude | **R5-1·R5-2·R5-3 반영 확인. 빌드·시험 재현: non-unity·unity 경고·오류 0, `vBoR.*` 53/53.** 새 항목 1건 — 회전 밀어냄 상한이 frame당(0.5 cm)이라 이 PC(약 15 fps)에서 걸린 곳 축 회전이 회사 PC의 1/5–1/6 속도, "같은 요청이면 다시 안 풂" 캐시가 시험에서만 정지를 만듦(R6-1, Claude 명세 모순). **VR 재시험 전에 반영** |
| 7 | 2026-10-11 | `963d03e` 6회차 반영 + VR 재시험 2차(사용자) | Claude | **R6-1 반영 확인. 열린 항목 없음.** 코드·시험은 제안대로다. 사용자 VR: "골프백 지금이 훨씬 낫다"(막힘 비율 47% → 30%). 에디터를 닫은 뒤 빌드·시험을 재현했다: non-unity·unity 경고·오류 0, `vBoR.*` 55/55. **머지 준비 완료** |

### 7회차에 확인한 것

- **빌드·시험 재현(머지 전, 에디터를 닫은 뒤, `46c3da3` = 소스 `963d03e`).** vBoR 소스 수정 시각을 갱신해 다시 컴파일했다.
  - non-unity(`-DisableUnity -Module=vBoR`): `.cpp` 21개와 gen 파일을 하나씩 컴파일했다. 경고·오류 0.
  - unity: 경고·오류 0.
  - 시험: `vBoR.*` **55/55 Success**, `TEST COMPLETE. EXIT CODE: 0`. 로그: `Saved/Logs/P1R7ClaudeAutomation.log`.
  - 시험 로그 수치가 Codex 보고와 같다: 15/90 fps 차이 0.005239511 cm·0.005519020°, 바닥 3° 중심 상승 3.125975 cm·막힘.
  - Warning은 알려진 잡음뿐이다(`Unable to retrieve OculusXRHMD` 등, Agent-Workflow §2.5).
- **R6-1:**
  - 밀어냄 예산은 `MaxRotationPushCmPerSecond`(30 cm/s) × Tick dt다(`VBoRCarryContact.cpp`). dt·속도가 0 이하이거나 유한하지 않으면 예산은 0이다.
  - 같은 요청 캐시(`bHasHeldRequest`·`LastHeldRequest`)는 없어졌다. dt는 Tick → `AdvanceHeldPose` → `MoveHeldPose` → `ResolveHeldContact`로 넘어간다. 다른 호출 경로는 없다(시험 제외).
  - 시험이 확인하는 것:
    - 같은 요청이면 회전 오차가 늘지 않고, 요청을 지나치지 않으며, 도달하면 멈춘다.
    - 15/90 fps에서 0.5 s 뒤 차이는 0.005 cm·0.006°다.
    - 갈 수 없는 요청이면 그 자리에 멈춘다.
    - 바닥 떨림(0.3 cm·0.5°)은 막힘이 아니다.
  - 바닥 3° 기울이기는 중심이 3.13 cm 올라가 막힘이 된다. 명세대로 로그만 남겼다(판정은 VR).
- 범위·주석 말투·`Config`/`Content`/`Plugins` 변경 없음 — 맞다.
- **VR 재시험 2차**(조사 기록 "2026-10-11 — P1 VR 재시험 2차"): 사용자 "훨씬 낫다".
  - 추적 손실·`B` 반복은 세 번의 실행 모두 하지 않았다.
  - 바닥 기울이기도 안 봤다.

### 6회차에 확인한 것

- **빌드 재현.** 에디터를 닫고, vBoR 소스 수정 시각을 갱신한 뒤 빌드했다.
  - non-unity(`-DisableUnity -Module=vBoR`): `.cpp` 21개와 gen 파일을 하나씩 컴파일했다. 경고·오류 0.
  - unity: 경고·오류 0.
- **시험:** 로컬 도구 플러그인을 끄고 `vBoR.*` **53/53 Success**, `TEST COMPLETE. EXIT CODE: 0`. 로그: `Saved/Logs/P1R6ClaudeAutomation.log`. Warning은 알려진 잡음뿐이다(MetaXR Content 빈 엔진 버전, XR 확장, 샘플 CDO, 시험 world 정리).
- **R5-1:**
  - 기본 적재 영역 = 기본 목표 상자(28×28×120 cm, `VBoRLoadTarget.cpp:20`)다. 값을 안 바꾼 목표는 정확한 자리만 안착으로 친다. R4-1 이전과 같지만, 영역을 모르는 목표에서는 보수적인 기본값이라 받아들인다.
  - 보조 안착이 영역 밖으로 끝나면 경고 1회를 남긴다(`FinishPlacement`).
  - 시험대 배치·`R`·`B`에서는 거짓 경고가 없다. 시험이 확인한다.
- **R5-3:** `TiltLimitDeg = 45`. 순수 함수 시험은 15를 인자로 넘긴 그대로라 유효하다.
- **R5-2:** 들고 옮길 때만 `ResolveHeldContact`(`VBoRCarryContact.cpp`)를 쓴다. 보조 안착·낙하의 `TryMoveToPose`는 그대로다(`ContactDropOnSlope`).
  - 미끄러짐: sweep → 법선 평면 투영 → 두 번째 면이면 교선 방향, 3번까지 한다. 지나온 면 안쪽으로 향하면 멈춘다. 엔진 `TwoWallAdjust`의 0.01 cm 밀어냄은 쓰지 않았다.
  - 회전: 5° 구간마다 확인하고, 길이 0 sweep의 `Normal`·`PenetrationDepth`로 3번까지 밀어낸다. 실패 구간은 이분 탐색 14회. 결과도 처음 놓인 자세 기준 원뿔 안인지 확인한다.
  - 막힘: 요청-실제 차이 히스테리시스(2 cm·5° 진입, 1 cm·2.5° 해제)이고, 소리는 진입 때만 낸다. 보류 소리도 `UpdateVisual`이 상태가 바뀔 때만 낸다. 시험대 "막힌 채 입구 통과"는 새 정의를 쓴다.
  - 사용자 예(끝면이 옆벽에 닿은 채 Yaw)를 실제 Actor로 시험한다(`ContactRotation`).
- **받아들인 것:** `ResolveHeldContact`는 현재 자세가 이미 겹쳐 있으면 아무것도 하지 않는다(`VBoRCarryContact.cpp:8`). 예전 `TryMoveToPose`는 시작 겹침에서 빠져나가는 이동을 허용했다. 지금 경로는 모두 `IsPoseClear`를 통과한 자세만 만들어서 이 상황은 생기지 않는다. 레벨에서 받침대와 다른 형상이 겹치게 배치되면 생길 수 있어 적어 둔다.
- 범위·주석 말투·`Config`/`Content`/`Plugins` 변경 없음 — 맞다.
- **새 항목 1건(R6-1).** 원인은 Claude 명세의 모순이다.

### 5회차에 확인한 것

- **빌드 재현(R3 `f6c5a2e` + R4 `7e6aa46`분).** 에디터를 닫고, vBoR 소스의 수정 시각만 갱신해 다시 컴파일시켰다. 샘플 플러그인은 끈다.
  - non-unity(`-DisableUnity -Module=vBoR`): vBoR `.cpp` 19개와 `Module.vBoR.gen.cpp`를 하나씩 컴파일했다. 경고·오류 0.
    - 모듈을 한정한 이유: 전체 non-unity는 로컬 도구 플러그인 약 1,200 파일을 다시 컴파일한다(Codex 보고 ③). Agent-Workflow §2.3에 적었다.
  - unity: `Module.vBoR.cpp` 컴파일·링크. 경고·오류 0.
  - 로컬 도구 `McpAutomationBridge`가 내는 `StructUtils` 폐기 예정 안내 2줄은 vBoR 판정에서 뺀다.
- **시험:** `-DisablePlugins=OculusInteractionSamples,McpAutomationBridge,UnrealAgent`로 돌렸다. `vBoR.*` **47/47 Success**, `TEST COMPLETE. EXIT CODE: 0`. 로그: `Saved/Logs/P1R5ClaudeAutomation.log`.
  - Warning·Error는 알려진 잡음뿐이다: 샘플 `InfoCardWidget` CDO, 없는 XR 확장, 시험 world 정리 때 `World has no context`.
  - 충돌 없는 메시 경고는 기대 로그로 잡혀 Verbose로 찍힌다.
- **R4-1 적재 영역:**
  - `AVBoRLoadTarget::LoadingRegion`(UBoxComponent, NoCollision, 게임 중 숨김)을 둔다.
    - `IsObjectInside`만 영역 좌표계(scale을 뺀 component transform)와 scaled extent로 잰다.
    - `IsStaticFit`·보조 안착·안내 표시는 목표 자세를 그대로 쓴다(`VBoRLoadTarget.cpp:61-75`).
  - 시험대 영역을 트렁크 패널 식(`P1TrunkMock.cpp:55-60`)과 대조했다. 영역이 안쪽 공간과 맞는다.
    - X: 0(범퍼 턱 안쪽 면)부터 깊이(앞벽 안쪽 면)까지
    - Y: ±폭/2(옆벽 안쪽 면)
    - Z: 바닥 윗면부터 천장 아랫면까지
    - 범퍼 턱(X < 0)은 영역 밖이다. 그래서 턱 위에 걸친 골프백은 목표 밖이다.
  - 영역 축은 트렁크를 따르고, 목표의 가로 회전은 따르지 않는다. `R`과 트렁크 값 변경이 영역에 반영된다. 시험이 둘 다 확인한다.
  - 낙하 시험은 실제 시험대 Actor로 한다.
    - 깊이 방향으로 20 cm 비킨 낙하(보조 범위 밖) → 초록, `DroppedInside`, `OnSeated` 1
    - 입구에 걸친 낙하 → 회색, `DroppedOutside`
- **R4-2 simple collision:**
  - `HasSimpleCollision`을 `IsPoseClear`와 상태 표시가 같이 쓴다. 경고 로그는 재사용 부품 쪽 `LogVBoRCarry`에 남긴다. `GetStateText`는 원인을 먼저 보인다.
  - 시험은 공유 엔진 asset을 건드리지 않고, 복제 메시의 BodySetup만 비운다.
  - 3회차 시험 중 메시를 잠깐 비우는 단계(`P1LoadBenchTests.cpp:284`)에서 시험대 로그에 `Normal -> No simple collision -> Normal`이 남는다. 새 상태가 시험대 기록에도 나온다는 뜻이고, 문제가 아니다.
- **범위:** `Config`·`Content`·`Plugins` 변경 없음. 주석 말투가 맞고, 명세 밖 기능이 없다.
- **새 항목 1건(R5-1).** 재사용 부모의 기본값 두 개가 서로 맞지 않는다. P1 시험대에서는 드러나지 않는다.

### 4회차에 확인한 것

- **빌드·자동화 재현은 못 했다.** 리뷰 시점에 사용자 에디터가 열려 있었다(PID 40680, MCP 서버 :3000 포함). Codex 보고는 경고·오류 0, `vBoR.*` 46/46(`Saved/Logs/P1R3F1Build.log`, `P1R3F1Automation.log`). 다음 회차에 이 회차 분까지 함께 재현한다.
- R3-1 두 손:
  - 세션이 두 손을 소유한다(`VBoRCarryMotion.h:73-205`). 두 번째 손이 잡는 순간 첫 손 위치를 다시 읽어 rebase한다(`VBoRCarryObject.cpp` Select 처리).
  - 방향은 두 손 위치로만 정한다. Yaw = 두 손 방향의 수평 각 변화, 기울기 = 높이 각 변화이고, 기울기는 처음 놓인 자세 기준 원뿔 안으로 이분 탐색해 줄인다(`LimitedPairDelta`).
  - 위치는 두 손 가운데가 잡은 점 가운데를 따른다. 손 간격이 그대로면 멈춘 손 쪽 잡은 점이 그 손에 붙어 있다. Claude가 식으로 확인했다(사용자 예: 왼손 고정·오른손만 돌림).
  - 수평 간격 5 cm 미만이면 방향을 쓰지 않는다. 방향을 잃거나 되찾는 frame은 rebase만 한다.
  - 한 손 놓기 → 남은 손으로 rebase. 둘 중 하나라도 추적을 잃으면 전체 보류.
  - ISDK는 `MultiGrab`, single·multi 슬롯에 같은 transformer, `MaxGrabPoints() = 2`다.
  - 갱신은 Actor Tick 한 곳에서만 한다. 1회차의 frame당 두 번 갱신이 없어졌다.
- R3-2 기울기 기준과 충돌 형상:
  - rest 회전은 BeginPlay·`ResetToPose`에서만 저장한다.
  - 충돌은 `ComponentOverlapMulti`·`ComponentSweepMulti`로 메시 simple collision을 그대로 쓴다. 접촉 여유 0.05 mm, 시작 겹침에서 멀어지는 이동 허용, 막는 hit만 본다.
  - 회전이 바뀔 때만 회전 겹침을 검사한다. 쉬고 있을 때 생기던 회전 막힘 깜빡임이 줄어든다.
  - 목표 포함·입구 통과도 같은 형상(convex 꼭짓점)으로 잰다.
- Codex가 스스로 찾은 "입구 통과를 렌더 상자로 일찍 셈"(F1)도 같은 형상으로 고쳤다.

### 2회차에 확인한 것

- 에디터를 닫고 바뀐 파일 non-unity 컴파일 + unity 빌드, 둘 다 경고·오류 0(샘플 플러그인 끔). `vBoR.*` **40개 Success**(P0 24 + P1 16), `TEST COMPLETE. EXIT CODE: 0`. 로그: `Saved/Logs/ClaudeP1R1Review.log`.
- R1-1: 기본 폭 130 cm, 벽 안쪽 면 ±65 cm. 폭 100 cm "안 맞음·크기 그대로" 시험은 설정값으로 남겼다.
- R1-2: `LimitWorldTilt`가 월드 Z swing-twist로 나누고 Swing을 W ≥ 0 반구로 맞춘 뒤 원뿔 15°로 자른다(`VBoRCarryMotion.h:7-18`). 손 회전 차이와 잡기 기준 재제한 두 곳 모두 이 함수를 쓴다. 순수 Pitch 100°는 Twist가 단위 회전이 되어 Yaw가 0으로 남는다. `PitchBoundary` 시험이 80°→100° frame 사이 1° 이하 변화와 179° 뒤집힘을 확인한다. 원뿔 선택은 보고 ③에 있다.
- R1-3: 시험대 원점 Z = Pawn 원점 Z. 머리 높이 120·165·190 cm에서 트렁크 바닥 = Pawn Z + 75 cm 시험. 바닥 기준을 로그에 남긴다.
- R1-4: `FinishPlacement`에서 `IsObjectInside`로 나눠 목표 밖은 `Released (outside target)`·회색, `OnSeated` 없음. 결과 문자열 셋(보조 / 낙하→목표 안 / 낙하→목표 밖). 실제 Actor 낙하로 시험.
- R1-5: 요청 자세 위치와 이동 뒤 실제 위치의 차이. 50 cm 오프셋에서 10 cm만 잡히는 시험.
- R1-6: `B`가 골프백만 돌려놓고 시도 요약 한 줄, 번호 +1, 누적 유지, 입구 통과 수만 초기화. `R`은 전부 지움. Codex가 스스로 찾은 "같은 시도 안 재잡기 때 앞선 결과가 사라짐"도 고치고 회귀 시험(`CumulativeRegrab`)을 넣었다.
- **받아들인 것(정리 때 지울 것):** 재사용 부모 `VBoRCarryObject.h`에 P1 시험 클래스 4개의 `friend` 선언이 있다. 컴파일 의존은 아니고(이름만 선언) P1 시험이 내부 상태를 직접 만들기 위한 것이라 이번에는 받아들인다. `Source/vBoR/P1Probe/`를 지울 때 이 `friend` 줄과 `P1CarryTestObserver.h`를 함께 지운다.

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

없음.

## 6회차 항목 (모두 반영함)

### R6-1 회전 밀어냄 상한이 frame당이고, "같은 요청이면 다시 풀지 않음" 캐시가 시험에서만 정지를 만든다 (Claude 명세 모순) — 반영함 (`963d03e`)

**Codex 반영:** `MaxRotationPushCmPerSecond` 기본 30 cm/s로 바꾸고 Tick의 `DeltaSeconds`를 `AdvanceHeldPose` → `MoveHeldPose` → `ResolveHeldContact`로 전달했다. frame의 두 회전 단계가 `속도 × dt` 예산을 공유한다. `bHasHeldRequest`·`LastHeldRequest`와 무효화 코드를 모두 제거했다. 같은 요청도 계속 풀며 회전 오차 비증가·요청 미초과·도달/막힘 뒤 정지를 확인했다.

**확인:** 기존 캐시에서 다음 frame 수렴 assertion이 실패하는 RED를 확인한 뒤 수정했다. `ContactRotation`은 새 수렴/정착 기대와 0.3 cm·0.5° 바닥 떨림으로 개정했다. 신규 `ContactFrameRate`에서 정확히 0.5 s의 15/90 fps 결과 차이는 **0.005239511 cm / 0.005519020°**였다(둘 다 아직 요청에 도달하지 않은 조건). `ContactFloorTilt`의 3° 요청은 중심 상승 **3.125975 cm**, `blocked=true`, 낮은 점 Z **0.005004 cm**를 기록했다. 이 응답을 바꾸지 않았으며 자연스러움은 Quest 3 판정 대상이다.

**검증:** 샘플 비활성 non-unity(`-Module=vBoR`)·기본 빌드 성공, vBoR compiler/linker 경고·오류 0. 샘플·두 로컬 도구 플러그인을 끈 `vBoR.*` **55/55 Success**, Fail 0, EXIT CODE 0. 로그 `Saved/Logs/P1R6NonUnityBuild.log`, `P1R6Build.log`, `P1R6Automation.log`; RED `P1R6Red.log`. 보고 ①~⑤는 마지막 커밋 본문과 HANDOFF.md에 기록한다. Claude 7회차 공식 리뷰와 Quest 3 T-02/T-09는 별도 대기(`not-tested`).

**문제**
- **위치:**
  - `Source/vBoR/Interaction/VBoRCarryContact.cpp:11`: `PushBudgetCm = MaxRotationPushCm`. 시간과 무관한 한 번(= 한 frame) 예산이다.
  - `Source/vBoR/Interaction/VBoRCarryObject.h:98`: `MaxRotationPushCm = 0.5`.
  - `Source/vBoR/Interaction/VBoRCarryObject.cpp:341`: `MoveHeldPose`는 요청이 직전과 `1e-6` 안에서 같으면 다시 풀지 않는다.
- **발생 조건:** 걸린 곳을 축으로 도는 데 필요한 밀어냄이 frame 예산보다 클 때.
  - 최악은 골프백 긴 쪽이 지렛대가 되는 경우다. 한쪽 끝이 바닥에 닿은 채 기울이거나, 옆면이 벽에 닿은 채 Yaw로 돌리면, 중심 기준 1°마다 약 1.05 cm(60 cm × π/180)를 밀어내야 한다.
  - 끝면이 벽에 닿은 Yaw(`ContactRotation`)는 1°에 약 0.25 cm라 덜하다.
- **영향:**
  - **frame 속도에 따라 동작이 달라진다.** 0.5 cm/frame이면 지렛대 경우 frame당 약 0.48°만 돈다.
    - 이 PC(VR 재시험 약 15 fps): 초당 약 7°
    - 회사 PC(72–90 fps): 초당 35–43°
    - 같은 동작이 5–6배 다르게 느껴진다. 이 PC에서 하는 VR 재시험은 D7을 제대로 보여 주지 못하고, 조금만 빨리 돌려도 차이 5°를 넘어 "막힘"이 뜬다.
  - **시험이 기기 동작과 다르다.**
    - 캐시는 요청이 거의 비트 단위로 같을 때만 듣는다. 실제 손 추적 요청은 매 frame 조금씩 달라서 거의 매번 다시 풀고, 그때마다 남은 회전을 이어서 돈다. Codex 주석도 "같은 요청을 다시 풀면 제한된 회전이 누적됨"이라고 적었다.
    - 그래서 `ContactRotation`의 "Same requested pose cannot creep"는 기기에서는 일어나지 않는 정지를 확인한다.
    - 바닥 재잡기의 "remains held"도 캐시 덕이다. 아래 1 cm·3° 요청을 계속 풀면 낮은 쪽 끝을 축으로 3°까지 돌고, 중심이 약 3 cm 올라가 요청과의 차이가 2 cm를 넘는다.
- **원인:** 명세 D7을 Claude가 "frame당 밀어냄 상한"과 "요청이 그대로면 움직이지 않는다"로 썼다. 상한 때문에 한 frame에 다 못 도는 회전이 생기는데, 그 회전을 다음 frame에 이어 돌면 "움직이지 않는다"와 어긋난다. Codex는 캐시로 명세 문장을 맞췄다. 명세를 고쳤다(6회차 개정, §2 D7·§3·§4·§7):
  - 밀어냄은 **초당 상한**으로 한다.
  - 손이 멈춰도 **요청 쪽으로만 이어서 따라가다 멈춘다**(지나치지 않음).

**제안**
- 예산을 `초당 상한 × DeltaSeconds`로 바꾼다.
  - 값은 자리값으로 둔다. 예: 30 cm/s. 60 fps에서 지금 0.5 cm/frame과 같고, 120 cm 골프백 끝을 축으로 약 29°/s다.
  - 프로퍼티 이름과 단위를 초당으로 바꾼다.
  - 갱신은 Tick 한 곳이다(R3-1). `MoveHeldPose`가 dt를 받게 하면 시험에서 frame 속도를 바꿔 볼 수 있다.
- 같은 요청 캐시(`bHasHeldRequest`·`LastHeldRequest`)를 없앤다.
  - 지금 풀이는 같은 요청을 다시 풀어도 요청 쪽으로만 간다. 회전은 현재 → 요청 Slerp 비율이고, 이동은 요청 쪽 sweep·투영이다. 도달하거나 막히면 변화 0이 되어야 하니, 이것을 시험으로 보인다.
- 바닥에서 들지 않고 기울이는 경우의 결과(낮은 쪽 끝을 축으로 들림, 손과 멀어짐)는 이번에는 바꾸지 않는다. 시험 로그에 남기고 VR에서 본다.
  - 거슬리면 다음 회차에 "요청 차이(막힘 판정과 같은 척도)가 가장 작은 자세를 고른다"로 정한다.

**확인할 시험**
- frame 속도: 같은 요청을 15 fps(dt 1/15)로 0.5 s, 90 fps(dt 1/90)로 0.5 s 풀었을 때 결과 자세 차이가 작다(예: 0.2 cm·0.5° 안).
- 손 정지(같은 요청 반복):
  - 요청과의 회전 차가 frame마다 늘지 않는다.
  - 요청 회전을 지나치지 않는다.
  - 도달하거나 더 못 가면 변화가 0이다.
- 바닥 재잡기는 실제 떨림 크기로 한다: 아래 0.3 cm·기울기 0.5°를 여러 frame 요청 → 겹침 없음, 막힘 아님.
- 바닥에서 들지 않고 3° 기울이기를 여러 frame 요청 → 겹침 없음, 낮은 쪽 끝이 바닥을 넘지 않음. 중심 상승량과 막힘 여부는 시험 로그에 남긴다(판정은 VR).
- `ContactRotation`의 "Same requested pose cannot creep"와 바닥 재잡기 1 cm·3° "remains held"는 위 시험으로 바꾼다. 보고 ③에 적는다.

## 5회차 항목 (모두 반영함)

### R5-1 부모 기본값에서 목표 자세가 적재 영역보다 길어, 값을 안 바꾼 목표의 보조 안착이 "목표 밖"으로 끝난다 (Claude 명세 빈틈) — 반영함 (`2c13c32`)

**문제**
- **위치:**
  - `Source/vBoR/Interaction/VBoRLoadTarget.cpp:20`: 적재 영역의 기본 BoxExtent가 50 cm다. 한 변 100 cm 정육면체이고, 중심은 목표 root다.
  - `Source/vBoR/Interaction/VBoRLoadTarget.h:22`: 목표 기본 크기는 28×28×120 cm다. 긴 축은 목표 local Z다.
  - `Source/vBoR/Interaction/VBoRCarryObject.cpp:368` `FinishPlacement`: 보조 안착도 `IsObjectInside`(적재 영역)로 안착 여부를 가른다.
- **발생 조건:**
  - 시험대를 거치지 않고 놓은 `AVBoRLoadTarget`(또는 자식 Blueprint)에서 적재 영역을 바꾸지 않았다.
  - 그 목표에 긴 축이 100 cm를 넘는 짐을 보조 안착했다.
  - 예: 120 cm 골프백이 목표 자세에 정확히 놓여도 Z 반길이가 60 cm라 영역 반길이 50 cm를 넘는다.
- **영향:**
  - 보조 안착이 끝나도 회색 "Released (outside target)"으로 보이고, 안착 이벤트가 없다. R4-1과 같은 오표시가 부모 기본값에서 다시 생긴다.
  - 경고도 없다. R4-2에서 막은 "조용한 실패"와 같은 종류다.
  - P1 시험대는 `PlaceFromHead`가 영역을 트렁크 값으로 덮어쓴다(`P1LoadBench.cpp:186-194`). 그래서 **P1 VR 재시험에는 영향이 없다.** P4에서 부모를 실제 차량 레벨에 놓을 때 드러난다.
- **원인:** R4-1 명세 개정에서 Claude가 "목표 자세는 적재 영역 안에 있어야 한다"는 조건을 적지 않았다. 명세 §2-3·§4를 고쳤다(5회차 개정).

**제안**
- 부모 기본 영역이 기본 목표 상자를 담게 한다. 값은 구현자가 정한다(예: 각 축에서 목표 기본 크기의 반 이상).
- 보조 안착이 끝났는데 `IsObjectInside`가 false면 설정 오류다. 정의상 목표 자세는 영역 안이어야 하기 때문이다.
  - 이때 경고 로그를 남긴다. 목표 크기, 영역 크기, target 이름을 넣고, 시도마다 한 번만 남긴다.
  - `FinishPlacement`에서 남기면 같은 파일의 `LogVBoRCarry`(`VBoRCarryObject.cpp:23`, `DEFINE_LOG_CATEGORY_STATIC`)를 그대로 쓸 수 있다.
- **BeginPlay 검사로 하지 않는다.** 시험대 순서 때문에 거짓 경고가 난다.
  - 시험대는 목표를 `SpawnActorDeferred`(`P1LoadBench.cpp:218`)로 만들고 `FinishSpawning`(`:223`)을 부른다. BeginPlay는 이때 돈다.
  - 영역은 그 뒤 `PlaceFromHead`(`:192-193`)에서 정한다.
  - 그래서 BeginPlay에서 재면 시험대 목표마다 거짓 경고가 난다.
- 영역을 목표 상자나 렌더 경계에 맞춰 자동으로 키우지 않는다. 사용자가 편집한 영역을 덮어쓰게 된다.

**확인할 시험**
- 영역을 바꾸지 않은 기본 목표 + 120 cm 골프백(P1 원통 형상) 보조 안착 → `Seated`, `Assisted`, `OnSeated` 1.
- 영역을 목표보다 작게 설정 → 보조 안착 뒤 `Released (outside target)`, 경고 1회(`AddExpectedMessagePlain`).
- 시험대 배치·`R`·`B` → 이 경고 0회.

### R5-2 걸리면 통째로 멈춘다 — 면을 따라 미끄러지고 걸린 곳을 축으로 돌게 한다 (VR 재시험, 사용자 결정 D7) — 반영함 (`2c13c32`)

근거: 조사 기록 "2026-10-11 — P1 VR 재시험". 명세 D7, §2 "충돌", §3, §4 새 행 9개, §7 "모서리 걸림".

**문제**
- **위치:** `Source/vBoR/Interaction/VBoRCarryObject.cpp`
  - `:177-183`: 요청 회전을 물체 중심 기준으로 현재 위치에서 겹침 검사한다. 겹치면 회전 전체를 버린다.
  - `:184-201`: 이동 sweep이 첫 접촉 시간에서 이동 전체를 멈춘다. 면을 따라가는 성분도 같이 버린다.
  - `:204`: 조금이라도 닿거나 회전을 버리면 false(막힘)를 돌려준다.
  - `:332-339` `MoveHeldPose`: 그 반환값을 그대로 막힘 상태·횟수로 쓴다.
- **발생 조건:** 골프백 일부가 면에 닿은 채 손이 그 면 쪽 성분을 가진 이동·회전을 요청할 때. 예:
  - 한쪽 끝이 입구 옆 모서리에 걸린 채 반대쪽을 밀어 넣음
  - 벽에 비스듬히 밂
  - 트렁크 바닥에 놓인 골프백을 다시 잡았는데 손이 조금 떨림
- **영향(VR 재시험 2026-10-11):**
  - 잡은 시간 41.5 s 중 19.4 s(47%)가 막힘이었다. 진입 26번, 요청-실제 차이 최대 53.6 cm.
  - 트렁크 안 골프백을 다시 잡은 12번 중 8번이 0.1 s 안에 막혔다.
  - 사용자: "오른쪽 모서리가 오른쪽 코너에 걸리면 왼쪽은 각도를 틀어 들어갈 수 있어야 하는데, 걸리면 그냥 안 움직인다."
- **원인:** 명세 §2 "충돌"을 Claude가 "막히면 멈추고, 회전도 겹치면 이전 회전 유지"로 썼다. **코드는 명세대로다.** 명세를 D7로 바꿨다.

**제안**
- `TryMoveToPose`의 기존 동작은 보조 안착(`:406`)·낙하(`:418`)·시험이 그대로 쓰게 둔다. 들고 옮기는 경로(`MoveHeldPose`)만 접촉 응답을 쓰게 나눈다(명세 §2: 보조 안착·낙하는 그대로).
- 접촉 응답 순서 예. 방법은 구현자가 정하고, 다르게 하면 보고 ③에 적는다.
  1. **회전**
     - 요청 회전을 현재 위치에서 시험한다. 겹치면 그 자세에서 길이 0 `ComponentSweepMulti`를 한다. 지금 `IsPoseClear`가 쓰는 방식이다.
     - 시작부터 겹친 hit의 `Normal`·`PenetrationDepth`(`Engine/Source/Runtime/Engine/Classes/Engine/HitResult.h:86-116`)로 밀어낼 벡터를 만든다. 0.005 cm 이하 접촉은 지금처럼 겹침으로 보지 않는다.
     - 밀어낸 자세를 `IsPoseClear`로 확인한다. 실패하거나 frame당 상한을 넘으면 회전 비율을 이분 탐색한다(`FVBoRCarryMotion::LimitedPairDelta`의 기울기 탐색과 같은 방식, `VBoRCarryMotion.h:29`).
  2. **이동**
     - sweep에서 막히면 남은 이동을 hit 법선 평면에 투영해 다시 sweep한다. 엔진 방식: `UMovementComponent::ComputeSlideVector` = `FVector::VectorPlaneProject(Delta, Normal) * Time`, `Engine/Source/Runtime/Engine/Private/Components/MovementComponent.cpp:711`.
     - 두 번째 면이 이전 면과 90° 이하로 만나면 두 법선의 외적 방향(교선)으로 보낸다(`TwoWallAdjust`, `:778`).
     - 되풀이는 2~3번이면 된다.
  3. 최종 자세를 `IsPoseClear`로 확인하고 적용한다.
- **주의:**
  - 시작부터 겹친 hit의 `ImpactPoint`는 의미가 없다(`HitResult.h:50`). 접촉점 좌표를 축으로 쓰지 말고 법선·깊이로 민다. 걸린 끝이 면에 붙어 있으므로 결과는 그 끝을 축으로 돈 것과 같다.
  - 엔진 `TwoWallAdjust`는 같은 면을 다시 맞으면 `HitNormal * 0.01`(cm) 밀어낸다(`:802-806`). 우리 접촉 여유 0.005 cm보다 크므로 그대로 옮기지 않는다.
  - 엔진 movement component를 붙여 `SafeMoveUpdatedComponent`·`SlideAlongSurface`(`Classes/GameFramework/MovementComponent.h:316, 362`)를 쓰는 방법도 있다. 다만 회전은 sweep하지 않고 접촉 여유가 달라서, 직접 쓰는 쪽이 작다고 본다.
- **막힘 판정을 반환값에서 떼어 낸다.** `MoveHeldPose`에서 요청 자세와 실제 자세의 차이로 정한다: 위치 2 cm 또는 회전 5°에서 진입, 1 cm·2.5° 미만에서 해제. 값은 프로퍼티(자리값)로 둔다.
  - 막힘 횟수·소리는 진입 때만 낸다.
  - 최대 차이 측정(R1-5 정의)은 그대로다.
  - 입구 "막힌 채 통과" 판정도 이 상태를 쓴다(`P1LoadBench`).
- 두 손 요청 계산(`FVBoRCarrySession::UpdateHands`)은 건드리지 않는다. 기울기 한도는 접촉 응답 결과에도 적용한다. 밀어냄이 기울기를 바꾸지는 않지만, 회전 비율을 줄인 결과가 한도 안인지는 확인한다.

**확인할 시험** — 실제 Actor 합성 시험. 시험대 트렁크 또는 단순 벽 Actor를 쓴다.
- 옆벽에 45°로 밂 → 실제 이동의 면 방향 성분 ≈ 요청의 면 방향 성분(오차 0.1 cm), 법선 방향 이동 0, 겹침 없음, 막힘 아님.
- 바닥+옆벽 안쪽 모서리로 밂 → 이동이 교선 방향, 두 면 모두 겹침 없음.
- 한쪽 끝이 입구 옆벽 앞 모서리에 닿은 자세에서, 그 끝을 벽 쪽으로 미는 Yaw +10° 요청 → 적용된 Yaw > 0, 겹침 없음, 걸린 끝이 면을 넘지 않음, 반대쪽 끝이 요청 방향으로 움직임.
- 트렁크 바닥에 닿은 골프백에 아래로 1 cm·기울기 3° 요청 → 겹침 없음, 상태 "잡힘".
- 접촉 중 같은 요청 두 frame → 두 번째 frame 위치·회전 변화 0.
- 차이 1.5 cm↔2.5 cm 반복 → 막힘 진입 1번, 1 cm 미만에서 해제.
- 접촉 응답 뒤 기울기 ≤ 15°. 한 frame 밀어냄 ≤ 상한.
- 기존 보조 안착·낙하·막힘 sweep·`RequestedActualGap` 시험이 그대로 통과한다. 막힘 정의가 바뀐 시험은 새 정의로 고치고 보고 ③에 적는다.

### R5-3 기울기 한도 15°가 너무 제한적이다 — 45°로 올린다 (사용자, Claude 자리값 실수) — 반영함 (`2c13c32`)

**문제**
- **위치:**
  - `Source/vBoR/Interaction/VBoRCarryObject.h:96`: `TiltLimitDeg = 15`.
- **발생 조건:** 골프백을 기울일 때 — 두 손 높이 차로 한쪽 끝을 들거나 한 손으로 손목을 꺾을 때.
- **영향:**
  - 손이 기울여도 골프백이 15°에서 멈춰 "각도가 너무 제한적"이다(사용자, 2026-10-11).
  - VR 재시험에서 놓을 때 기울기가 13~15°로 한도에 붙어 있었다.
- **원인:** 15°는 Claude가 P1 첫 초안에서 근거 없이 정한 자리값이었다. 명세를 45°로 고쳤다(§2 회전, §4).

**제안**
- 기본값만 45로 바꾼다. 범위는 `ClampMax = 90`으로 그대로 둔다.
- 기울기 계산(`LimitWorldTilt`·`LimitedPairDelta`·rest 기준)은 바꾸지 않는다.
- 순수 함수 시험(`P1CarryMotionTests.cpp`)은 한도를 인자로 넘기므로 15 그대로 둬도 된다.
- 실제 Actor 시험 가운데 기본값 15°에 기대는 것이 있으면 `TiltLimitDeg`를 읽어 쓰도록 고친다.
- R5-2의 "접촉 응답 뒤 기울기 ≤ 한도" 시험은 기본값 45°로 한다.

**확인할 시험**
- 기본 `AVBoRCarryObject`의 `TiltLimitDeg`가 45다.
- 두 손 높이 차로 30° 요청 → 30° 적용(예전 15°에서 잘리던 범위). 60° 요청 → 45°에서 잘림.
- 기존 기울기·Yaw 연속성 시험이 그대로 통과한다.

## 4회차 항목 (모두 반영함)

### R4-1 "안착·목표 안"이 골프백 크기 상자로 판정되어, 트렁크 안에 조금만 비켜 놓아도 목표 밖이 된다 (Claude 명세 빈틈) — 반영함 (`7e6aa46`)

**Codex 반영:** `AVBoRLoadTarget::LoadingRegion`을 목표 자세·안내 메시와 별도의 편집 가능한 상자 component로 추가했다. `IsObjectInside`는 이 영역 좌표계에서 기존 simple collision 형상 전체의 포함과 겹침 없음을 확인한다. 목표 자세의 정적 적합성·보조 안착·골프백 크기 안내는 유지했다. 시험대는 배치·R 때 트렁크 안쪽 깊이·폭·높이와 바닥 높이에서 영역을 설정한다. 패널·측정·snapshot 로그는 `loading region`과 `target pose`를 구분한다.

**확인:** 수정 전에 `PlacementOutcome`에서 목표에서 깊이 방향으로 20 cm 비킨 낙하가 회색·안착 이벤트 0으로 실패했다(`Saved/Logs/P1R4RedAutomation.log`). 반영 뒤 같은 낙하는 초록·`DroppedInside`·`OnSeated` 1이고 반복 tick에도 이벤트가 늘지 않는다. 입구/범퍼 턱에 걸친 낙하는 회색·목표 밖이며, 기존 목표 밖 낙하도 영역 밖임을 명시했다. `PlacementAndReset`에서 기본 정적 적합성 true·폭 100 cm false, 영역 위치/회전/크기·guide 크기 유지·R 뒤 편집 치수 반영을 확인했다. 원통 자기 축 회전 포함 시험도 유지했다.

**문제**
- **위치:**
  - `Source/vBoR/P1Probe/P1LoadBench.cpp:211`: 목표 크기를 골프백 크기(28×28×120 cm)로 정한다.
  - `Source/vBoR/Interaction/VBoRLoadTarget.cpp:50` 이후 `ContainsPose`: 그 상자 ±0.01 cm 안이어야 "안"이다.
  - `IsObjectInside` → `FinishPlacement`가 안착/놓임을 가른다.
- **발생 조건:** 트렁크 안에 넣었지만 목표 자세에서 1 cm라도 벗어난 채 수직 낙하한 경우 모두 해당한다. 보조 범위(10 cm·20°) 밖이거나 보조 경로가 막힌 경우다.
- **영향:**
  - 실제로 실린 골프백이 회색 "Released (outside target)"으로 보이고 안착 이벤트가 없다. R1-4의 반대 방향 오표시다.
  - 초록은 사실상 보조 안착으로만 나온다.
  - `PlacementOutcome` 시험은 목표 바로 위(0, 0)에서 떨어뜨려 이 경우를 잡지 못한다.
- **원인:** R1-4에서 Claude가 "목표 안"의 범위를 정하지 않았고, 2회차 리뷰에서도 놓쳤다. 명세 §2-3을 개정해 **목표 자세(정적 적합성·보조 안착용)**와 **적재 영역(트렁크 안 공간, 안착 판정용)**을 나눴다.

**제안**
- `AVBoRLoadTarget`에 적재 영역 상자(크기와 위치)를 둔다. `IsObjectInside`는 이 상자에 골프백 충돌 형상이 다 들어가고 겹침이 없는지로 바꾼다. 3회차의 `GetCollisionBoundsInPose`를 적재 영역 좌표계로 쓰면 된다.
- `IsStaticFit`·보조 안착·안내 표시는 지금처럼 목표 자세를 쓴다.
- 시험대는 트렁크 안쪽 폭·깊이·높이(바닥 윗면부터 천장 판 아래까지)를 적재 영역으로 넘긴다. 값은 트렁크 자리값을 따라간다(`AP1TrunkMock`).
- 패널·로그의 "inside and clear"는 적재 영역 기준임이 드러나게 이름을 바꾼다. 목표 자세 오차(cm·deg)는 그대로 남긴다.

**확인할 시험**
- 목표 자세에서 옆으로 20 cm 떨어진 트렁크 안 지점에 낙하 → 안착, `OnSeated` 1, 초록.
- 일부가 범퍼 턱 위·입구 밖에 걸친 채 멈춤 → 목표 밖, 회색.
- 기본 `IsStaticFit` true, 폭 100 cm에서 false — 1회차 시험이 그대로 통과.
- `PlacementOutcome`의 기존 "목표 밖 낙하"가 적재 영역 밖에서 일어나는지 확인한다(영역이 커져 우연히 안이 되지 않게).

### R4-2 simple collision이 없는 메시는 조용히 못 움직인다 — 반영함 (`7e6aa46`)

**Codex 반영:** BeginPlay에서 simple collision을 확인해 재사용 부품의 `LogVBoRCarry` Warning으로 메시 경로·원인·수정 방법을 남긴다. `GetStateText`는 다른 상태보다 `No simple collision`을 먼저 표시하므로 시험대 패널에서도 원인이 보인다. 이동 거부와 기존 메시 simple collision 판정은 유지했다.

**확인:** 수정 전에 충돌 없는 복제 메시의 상태가 `Normal`이라 `MissingSimpleCollision`이 실패했다(`Saved/Logs/P1R4RedF1Automation.log`). 반영 뒤 시작 경고 1개를 기대 로그로 확인하고, 시작/tick 상태 문구·이동 거부·자세 불변·시험대 패널 문구를 확인했다. 공유 엔진 asset은 수정하지 않고 시험용 복제 메시의 BodySetup만 비웠다.

**문제**
- **위치:** `Source/vBoR/Interaction/VBoRCarryObject.cpp` `IsPoseClear`. 메시에 simple collision이 없으면(`AggGeom.GetElementCount() == 0`) 항상 false다. 그래서 `TryMoveToPose`가 모든 이동을 거부한다.
- **발생 조건:** 나중에 자식 Blueprint가 실제 골프백·박스 메시로 바꿨는데 그 메시에 simple collision이 없을 때.
- **영향:** 잡히기는 하는데 전혀 안 움직이고 "Blocked"로만 보인다. 원인을 알 수 없다. 보고 ③에 "자식 메시에는 simple collision이 필요"라고 적은 조건이 코드·화면에 드러나지 않는다.

**제안**
- BeginPlay에서 메시의 simple collision 유무를 확인한다. 없으면 경고 로그를 남기고, 상태 문자열을 "No simple collision"처럼 따로 둔다.
- 렌더 경계로 대신 판정하지는 않는다. 3회차 결정 "보이는 형상대로"를 지킨다.
- 로그 카테고리는 재사용 부품 쪽에 둔다(`LogP1Probe`가 아님).

**확인할 시험**
- simple collision 없는 메시로 시작하면 경고 문구가 상태에 나오고, 이동은 하지 않는다.

### Codex R4 반영 보고 (2026-10-11)

① **바뀐 파일:** `Source/vBoR/Interaction/VBoRLoadTarget.h`, `.cpp`; `Source/vBoR/Interaction/VBoRCarryObject.h`, `.cpp`; `Source/vBoR/P1Probe/P1LoadBench.cpp`, `P1CarryWorldTests.cpp`, `P1LoadBenchTests.cpp`; 이 리뷰 파일. `HANDOFF.md` 결과는 git 무시 파일에 별도 기록했다.

② **명세 §6 완료 조건:**
- 1: 패널·로그에 목표 자세 오차와 적재 영역 포함을 구분하고 충돌 누락 원인을 표시함. 기존 측정 유지.
- 2: 형상·scale·충돌·손 입력/추적 정책 유지. P0 기본 poke-only와 기존 회귀시험 통과.
- 3: R4 경계 시험 추가/수정. 영역 안 비킨 낙하·입구에 걸친 낙하·충돌 없는 메시를 실제 Actor 합성 시험으로 확인.
- 4: 아래 ④의 로컬 엔진/ISDK 헤더 대조 완료.
- 5: 샘플 플러그인을 끈 기본 빌드와 vBoR 모듈 non-unity 빌드 성공, vBoR 컴파일·링크 경고/오류 0. `vBoR.*` **47/47 Success**, 실패 0, `TEST COMPLETE. EXIT CODE: 0`. 자동화는 샘플·두 로컬 도구 플러그인을 모두 끔.
- 6: 아래 ⑤에 빌드·에디터·Quest 확인 사항 기록.

③ **명세와 다르게 한 것:** 없음. 두 제안대로 구현했다. 20 cm 낙하 시험은 폭 여유가 양옆 5 cm인 조건 때문에 깊이 방향으로 비켰다. 영역 component의 transform·BoxExtent가 편집 값이며, 판정용이라 충돌/게임 중 표시를 껐다. 전체 non-unity 빌드가 로컬 도구 파일 약 1,200개를 재컴파일하려 해 중단하고 `-DisableUnity -Module=vBoR`로 범위를 한정했다. 최종 변경 `.cpp` 5개를 다시 개별 컴파일·링크했다. 기본 빌드는 handoff 명령 그대로 완료했다. UnrealAgent/MCP 도구와 junction은 확인했고 이번 R4는 에디터 단계가 없어 MCP 호출하지 않았다. `Config/`, `Content/`, `Plugins/` 소스 변경 없음.

④ **헤더 확인(추정 없음):** 아래 엔진 경로는 `C:/Program Files/Epic Games/UE_5.7/Engine/Source/` 기준이다.
- 영역 상자: `Runtime/Engine/Classes/Components/BoxComponent.h:39,43,47` (`SetBoxExtent`, scaled/unscaled extent), `Runtime/Engine/Classes/Components/SceneComponent.h:593,928` (world transform, 게임 중 숨김).
- 충돌 유무: `Runtime/Engine/Classes/Components/StaticMeshComponent.h:624` (`GetBodySetup`), `Runtime/Engine/Classes/PhysicsEngine/BodySetup.h:138` (`AggGeom`), `Runtime/Engine/Classes/PhysicsEngine/AggregateGeom.h:72` (`GetElementCount`). 기존 메시 검사 `Runtime/Engine/Classes/Engine/World.h:2351,2380` (component sweep/overlap)도 대조했다.
- 시험용 메시/경고: `Runtime/Engine/Classes/Engine/StaticMesh.h:1248` (`SetBodySetup`), `Runtime/Core/Public/Misc/AutomationTest.h:1815` (`AddExpectedMessagePlain`).
- ISDK 호출 변경 없음. 유지한 설정은 `Plugins/MetaXRInteraction/Source/OculusInteraction/Public/Interaction/IsdkGrabbableComponent.h:117,146` (collider/Palm), `Interaction/Grabbable/IsdkGrabTransformerComponent.h:130,171,199,259,266` (transformer target, MultiGrab, Throwable)로 다시 대조했다.

⑤ **로컬에서 확인할 것:**
- 빌드 로그: `Saved/Logs/P1R4FinalBuild.log`, `P1R4FinalNonUnityBuild.log`. 기본 빌드에 로컬 `McpAutomationBridge`의 `StructUtils` 폐기 예정 의존 안내 2줄(플러그인·project)이 있다. vBoR 경고/오류와 구분하며 도구 소스는 수정하지 않았다.
- 합성 자동화 로그: `Saved/Logs/P1R4FinalAutomation.log`. 47개 전체 통과. 충돌 누락 경고 1개는 시험에서 의도적으로 발생시키고 검증한 로그다. Workflow §2.5의 기존 XR/샘플 CDO 잡음과 실제 시험 결과를 구분한다.
- 에디터: 새 Blueprint·asset·레벨 작업 없음. 기존 `Content/P1/L_P1_GolfBag`에서 목표 guide 크기, `loading region` 패널 표기, 영역 안/입구 밖 낙하 색을 확인한다. 실제 자식 메시 교체 시 simple collision 설정과 경고 문구도 확인한다.
- Quest 3 Link/VR Preview: P1 명세 §7의 두 손 pivot·한 손 놓기/추적 손실, 트렁크 안 비킨 낙하 초록·입구 밖 회색, B 10회 반복, 손 시야 밖 보류/복구를 재시험한다. 관련 계획서 **T-02·T-09는 `not-tested`**이며 P1 합성 시험이 정식 기기/적재 평가를 대신하지 않는다. 질문 없음. Claude 5회차 리뷰 대기, push·머지 없음.

## 3회차 항목 (모두 반영함)

3회차는 코드 리뷰가 아니라 **첫 VR 시험(2026-10-10, 사용자) 뒤의 개정**이다. 근거는 조사 기록 "2026-10-10 — P1 골프백 첫 VR 실행", 결정은 U-08(계획서 §2, AGENTS.md 허용 목록). 명세 D6·§2·§3·§4가 바뀌었다.

### R3-1 두 손 잡기 — 두 잡은 점으로 방향을 정한다 (U-08) — 반영함 (`f6c5a2e`)

**Codex 반영:** 세션이 두 손을 소유하고 single/multi 슬롯에 같은 제약 transformer를 연결했다. 두 점의 위치로 Yaw·제한 elevation·중앙 위치를 정하며, 두 손 모드에서는 손목 회전을 쓰지 않는다. 추가 잡기·한 손 놓기·추적 복구 frame은 rebase만 한다. 한 손이라도 무효면 전체 보류, 마지막 손의 유효 release만 낙하/보조를 시작한다. 두 손 진입 횟수·소유 시간·현재 모드를 패널/로그/B 누적에 추가했다.

**확인:** `TwoHandMotionAndTransitions`, `TwoHandPreservesEntryAxialTilt`, `TwoHandActorLifecycle`에서 pivot·scale·전환·손실·기울기·이벤트·B/R을 확인했다. 가까운 손은 수평 간격 5 cm 미만에서 primary 이동만 따르고 회전을 고정한다. 두 손 시간은 추적 보류를 포함한 소유 시간이다. 실제 palm 인식과 SDK release/confidence 순서는 Quest 3 `not-tested`.

**문제**
- **위치:** 지금 구현은 두 번째 손을 세 곳에서 막는다.
  - `Source/vBoR/Interaction/VBoRCarryObject.cpp:25`: `MultiGrabBehavior = SingleGrabFirstRetained`
  - `VBoRCarryObject.h:61`: transformer `MaxGrabPoints() = 1`
  - 세션이 손 하나만 소유한다: `VBoRCarryMotion.h:63` `Begin`, `VBoRCarryObject.cpp:221` Select 처리
  - 시험도 이를 고정한다: `P1CarryMotionTests.cpp:44-45`, `P1CarryWorldTests.cpp:105`
- **발생 조건:** 첫 VR 시험에서 22번 잡고 트렁크 안 안착 0번, 입구 통과 4번 모두 막혔다. 한 손으로 들면 손목 Yaw가 그대로 골프백 Yaw가 되어, 잡을 때마다 목표 방향에서 2~97° 벗어나 있었다.
- **영향:** 120 cm 물체의 방향을 한 손으로 맞추는 것이 사실상 정밀 정렬이다. 사용자 결정 U-08로 두 손 잡기를 넣는다(명세 D6, §2 "두 손").

**제안**
- ISDK 쪽 설정:
  - `MultiGrabBehavior = MultiGrab`. 잡은 손이 2개 이상이면 `SetMultiGrabTransformerObject`로 넣은 transformer를 쓴다(`Plugins/MetaXRInteraction/Source/OculusInteraction/Public/Interaction/Grabbable/IsdkGrabTransformerComponent.h:171, 259`).
  - 잡은 손 수가 바뀔 때마다 ISDK가 `EndTransform`→`BeginTransform`을 다시 부른다(`Private/Interaction/Grabbable/IsdkGrabTransformerComponent.cpp:113-127, 246-299`).
  - 지금 구조(ISDK 선택과 따로 두는 세션)를 유지한다. ISDK 전환 순서에 기대지 말고 세션이 손 둘을 소유하게 하는 것이 추적 손실 처리와 맞다. 1회차의 "보류 중 ISDK release 무시"가 이 구조 덕분이다.
- 두 손 계산은 엔진 없이 시험할 수 있는 순수 함수로 둔다(`FVBoRCarryMotion` 옆).
  - 입력: 두 손 시작 위치, 물체 시작 자세, 두 손 현재 위치, 기울기 한도.
  - 출력: 물체 자세.
  - 계산: 두 손을 잇는 방향의 수평 각 변화로 Yaw를 정하고, 높이 차로 생기는 기울기는 R3-2의 기준(처음 놓인 자세)으로 한도 안에 자른다. 위치는 두 잡은 점의 가운데가 두 손의 가운데를 따르게 한다. 그러면 손 사이 거리가 그대로일 때 멈춘 손 쪽 잡은 점이 그 손에 붙어 있다.
- 손목 회전은 두 손 모드에서 쓰지 않는다. 두 점으로는 자기 축 둘레 회전을 알 수 없으므로 시작 때 값을 유지한다.
- 두 손이 너무 가까우면 방향을 못 믿는다. 기준 거리 아래면 한 손처럼 다룬다. 기준 거리는 구현자가 정하고 보고 ③에 적는다.
- 전환마다(한 손→두 손, 두 손→한 손, 추적 복구) 그 순간 자세로 rebase한다. 첫 frame 물체 변화는 0이다.
- 추적 손실: 두 손 중 하나라도 잃으면 전체를 보류한다(명세 §2). 보류 중 ISDK release 무시도 손별로 지금처럼 한다.
- 측정에 두 손 모드 진입 횟수·누적 시간, 지금 한 손/두 손을 더한다(명세 §2 측정 표). 패널·로그·`B` 시도 요약에 들어간다.

**확인할 시험** (명세 §4 새 행)
- 두 번째 손 잡기 frame의 물체 변화 0.
- 왼손 고정, 오른손이 왼손 둘레로 거리 유지하며 30° 돎:
  - 왼손 잡은 점의 월드 위치 변화 ≈ 0
  - 물체 Yaw 변화 = 30°
- 높이 차가 커도 기울기 ≤ 15°.
- 손 간격이 늘거나 줄어도 scale 불변.
- 손이 거의 붙었을 때 Yaw가 튀지 않음.
- 한 손 놓기 frame의 물체 변화 0. 이어서 남은 손으로 한 손 모드가 된다.
- 두 손 중 한 손 추적 손실:
  - 보류 중 이동 0
  - 복구 frame 변화 0
- 기존 "두 번째 손은 소유 못 함" 시험 2개는 새 동작에 맞게 바꾼다.

### R3-2 기울기 한도가 다시 잡을 때마다 쌓이고, 충돌 판정이 보이는 형상보다 커진다 — 반영함 (`f6c5a2e`)

**Codex 반영:** rest 회전은 생성/`ResetToPose`에서 저장하고 재잡기·복구·손 수 전환에서 바꾸지 않는다. 두 손 elevation만 제한해 시작 axial 회전을 보존한다. 이동/회전 후보 검사는 mesh simple collision의 component sweep/overlap으로 변경했다(self 무시, blocking만). 기존 0.005 cm 접촉 여유를 유지하며 실제 convex vertices로 목표 포함을 검사한다. 내부 검토에서 입구 측정에 남은 render OBB의 조기 통과 집계도 같은 형상 경계로 수정했다. 형상 무효 구간은 이전 입구 샘플과 연결하지 않는다.

**확인:** `RestTiltAcrossRegrabs` 5회 반복, `CylinderShape` axial 15° 원통의 이동/목표 포함 및 기존 접촉·벽·낙하·보조 회귀가 통과했다. `CylinderHullEntrance`는 회전한 시험대에서도 실제 형상이 입구를 넘기 전에는 통과를 세지 않으며, 무효 형상 복구/B 재설정도 확인한다. `BagRepeat`에 조기 막힘 분류 회귀를 추가했다. 샘플 플러그인 비활성 UE 5.7 빌드 경고·오류 0, `vBoR.*` **46/46 Success**, `TEST COMPLETE. EXIT CODE: 0`. 로그: `Saved/Logs/P1R3F1Build.log`, `P1R3F1Automation.log`. 자세한 보고 ①~⑤는 커밋 본문과 `HANDOFF.md`에 있다. Claude 정식 재리뷰와 Quest 3 시험은 별도 대기.

**문제**
- **위치:**
  - 기울기 기준이 잡을 때마다 새로 정해진다: `VBoRCarryMotion.h:69`(`GrabObjectRotation = 잡은 순간 회전`), `:102`(그 기준으로 재제한).
  - 충돌은 메시 경계의 외접 상자(OBB)로 검사한다: `VBoRCarryObject.cpp:107-116`(`OverlapBlockingTestByChannel` + `MakeBox`), `:130-138`(`SweepSingleByChannel` + `MakeBox`).
- **발생 조건:**
  - 놓은 자세(기울어진 채)에서 다시 잡으면 그 자세에서 또 15°가 허용된다. 첫 VR 시험의 잡은 순간 자세에서 자기 축 둘레 회전이 최대 56°, 축 기울기가 최대 16.6°였다.
  - 원통의 자기 축 둘레 회전은 눈에 안 보인다. 그런데 OBB는 같이 돌아 28 cm 정사각 단면의 모서리가 대각 39.6 cm까지 튀어나온다.
- **영향:**
  - 보이는 원통은 닿지 않았는데 받침대·턱·바닥에 막힌다. 첫 VR 시험의 막힘 진입은 38번이었다.
  - 보조 안착의 각도 판정(20°, `VBoRCarryMotion.h:29-35`)에 안 보이는 회전이 들어가 보조가 안 걸린다.

**제안**
- **기울기 기준:** 기준을 "처음 놓인 자세"로 고정한다. 시험대가 골프백을 받침대에 놓을 때(`ResetToPose`, 생성) 그 회전을 기준으로 저장한다.
  - 허용 회전 = (월드 Z 둘레 Yaw) × (기준 대비 swing ≤ 15°) × 기준. 지금의 `LimitWorldTilt`를 기준 회전에 대해 쓰면 된다.
  - 한 손·두 손 모두 같은 기준을 쓴다.
- **충돌 형상:** 보이는 형상대로 판정한다.
  - 엔진 Cylinder에는 단순 충돌(Convex)이 있다(`/Engine/BasicShapes/Cylinder.uasset`의 `ConvexElems`, Claude가 확인).
  - 메시 자체 충돌로 `UWorld::ComponentSweepMulti`·`ComponentOverlapMulti`를 쓴다(`Engine/Source/Runtime/Engine/Classes/Engine/World.h:2351, 2380`). 그러면 자식 Blueprint가 실제 골프백 메시(단순 충돌 포함)로 바꿔도 그 형상대로 막힌다.
  - 주의점:
    - 자기 Actor를 무시한다.
    - 결과 중 막는 hit(`bBlockingHit`)만 본다.
    - 받침대에 딱 닿은 시작 자세에서 시작 겹침(`bStartPenetrating`)으로 모든 이동이 막히지 않게 한다(예: 겹침 법선 반대쪽으로 멀어지는 이동은 허용). 1회차의 0.005 cm 여유와 같은 목적이다.
  - 다른 방법(OBB 유지 + 자기 축 둘레 회전 0 고정 등)을 쓰면 보고 ③에 이유를 적는다.
- **보조 안착 각도 판정:** 원통처럼 자기 축 대칭인 메시를 위한 처리는 이번에 하지 않는다. 기준 고정으로 안 보이는 회전이 15° 안에 묶이면 충분한지 다음 VR 시험에서 본다.

**확인할 시험**
- 잡기(기울임)→놓기→다시 잡기(같은 방향으로 기울임)를 5번 반복해도 기준 대비 swing ≤ 15°.
- 자기 축 둘레로 15° 돈 원통을 바닥 위 1 cm에서 수평 이동해도 막히지 않음. OBB였다면 모서리가 바닥에 걸리는 조건이다.
- 받침대에 닿은 시작 자세에서 들어 올리기·수평으로 빼기가 여전히 됨(1회차 회귀 시험 유지).
- 기존 충돌 시험(벽 앞 정지, 회전 겹침 거부, 낙하 첫 면 정지)이 새 판정으로도 통과.

## 1회차 항목 (모두 반영함)

### R1-1 트렁크 안쪽 폭 자리값을 130 cm로 바꾼다 (Claude 명세 오류) — 반영함 (`eb67e20`)

**Codex 반영:** 기본 폭 130 cm, 좌우 벽 안쪽 면 ±65 cm, 기본 정적 적합성 `clear`로 수정했다. 폭 100 cm 실패와 원통 크기 보존 시험은 별도 설정으로 유지했다. `MockGeometry`, `PlacementAndReset` 통과.

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

### R1-2 회전 제한이 Pitch ±90° 근처에서 Yaw를 뒤집는다 — 반영함 (`eb67e20`)

**Codex 반영:** 두 제한 지점에 월드 Z swing-twist와 원뿔 15° 제한을 적용했다. 순수 Pitch 80°→100°의 연속 frame, 뒤집힌 179° frame, Yaw 85°, 반복 rebase 시험이 통과했다. 정확한 swing 180° 특이점은 제안에 적힌 한계로 남는다.

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

### R1-3 트렁크 높이를 가정한 눈높이가 아니라 물리 바닥에서 잰다 — 반영함 (`eb67e20`)

**Codex 반영:** `AssumedEyeHeightCm`를 제거하고 시험대 원점 Z를 Pawn 원점 Z로 정했다. 머리는 수평 위치와 Yaw에만 사용하며 바닥 기준을 로그에 남긴다. 머리 높이 120·165·190 cm에서 트렁크 바닥이 매번 Pawn Z + 75 cm인 시험이 통과했다.

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

### R1-4 목표 밖에서 멈춘 것을 "안착"(초록)으로 표시한다 — 반영함 (`eb67e20`)

**Codex 반영:** 최종 목표 안·충돌 없음일 때만 안착 색과 `OnSeated`를 보낸다. 목표 밖은 `Released (outside target)`와 회색 덧입힘이며, 진행 중 보조는 잡힘 색을 쓴다. `PlacementOutcome`에서 목표 밖 이벤트 0회, 목표 안 낙하 이벤트 1회와 반복 tick 중복 없음이 통과했다.

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

### R1-5 막힘 거리가 잡은 자리 오프셋을 포함한다 — 반영함 (`eb67e20`)

**Codex 반영:** 제약을 적용한 요청 위치와 충돌 이동 뒤 실제 위치의 최대 차이를 `max requested/actual gap`으로 표시한다. `RequestedActualGap`에서 손바닥 오프셋 50 cm를 제외하고 벽 때문에 못 간 약 10 cm만 기록되는 시험이 통과했다.

**문제**
- **위치:** `Source/vBoR/Interaction/VBoRCarryObject.cpp:244`. `MaxHandGapCm` = 손바닥 위치와 Actor 위치(원통 중심) 사이 거리다.
- **발생 조건:** 골프백 한쪽 끝을 잡으면 막히지 않아도 손바닥과 중심이 약 50 cm 떨어져 있다.
- **영향:** 패널·로그의 "max hand/object gap"이 잡은 위치에 좌우된다. "막혔을 때 손이 얼마나 더 밀고 들어갔나"를 알 수 없다. 명세 문구("손과 물체 사이 최대 거리")가 모호했다 → 명세 §2를 개정했다.

**제안**
- 막힌 frame에서 잰다: 요청 자세(`Session.Update`가 낸 `Desired`)의 위치와, `TryMoveToPose` 뒤 실제 위치의 거리. 그 최댓값을 쓴다.
- 이름을 "max requested/actual gap"처럼 바꾸면 오해가 없다.

**확인할 시험**
- 손바닥이 중심에서 50 cm 떨어진 채 벽 쪽으로 10 cm 더 밀면 최대 거리 ≈ 10 cm다(50 cm대가 아님).

### R1-6 반복 시험용으로 골프백만 되돌리는 키를 둔다 — 반영함 (`eb67e20`)

**Codex 반영:** 편집 가능한 `B` 키로 가방만 돌려놓고 손 소유를 끝내며, 직전 시도 요약을 한 줄 기록하고 번호를 올린다. 시험대·트렁크 변환과 누적 결과·시간은 유지하고 입구 통과 기록은 초기화한다. `R`은 전체 재배치와 누적 초기화를 유지한다. 같은 시도에서 재잡기해도 앞선 결과와 시간이 보존되도록 보완했다. `BagRepeat`, 실제 Actor의 목표 밖 낙하→재잡기→목표 안 낙하→보조 안착을 거치는 `CumulativeRegrab` 통과.

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

### Codex 검증 기록 (2026-10-10)

- 샘플 플러그인을 끈 UE 5.7 `vBoREditor Win64 Development` 빌드 성공, 컴파일·링크 경고와 오류 0. 로그: `Saved/Logs/P1R1Build.log`.
- 샘플 플러그인을 끈 `vBoR.*` 전체 **40/40 Success**(P0 24 + P1 16), `TEST COMPLETE. EXIT CODE: 0`. 로그: `Saved/Logs/P1R1Automation.log`.
- 독립 정적 검토에서 발견한 재잡기 누적 손실은 회귀시험으로 재현한 뒤 수정했고, 재검토에서 추가 차단 문제는 없었다. 내부 기록: `Saved/Logs/P1R1InternalReview.md`. 이는 Claude의 다음 공식 리뷰를 대신하지 않는다.
- 에디터 표시·키보드 입력·Quest 3 손 추적은 미확인. P1 실측과 관련 T-02 정식 시험 상태는 `not-tested`다. 질문 없음.

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

## Codex R5 반영 기록 (2026-10-11)

① 바뀐 파일
- Source/vBoR/Interaction/VBoRCarryObject.h, .cpp: 기본 기울기 45°, 접촉 프로퍼티, 요청 캐시와 생명주기 초기화, 요청/실제 차이 히스테리시스, 막힘 진입 소리, 보조 완료 설정 경고.
- Source/vBoR/Interaction/VBoRCarryContact.cpp (신규): 들고 옮길 때만 쓰는 접촉 응답.
- Source/vBoR/Interaction/VBoRLoadTarget.cpp: 기본 적재 영역을 기본 목표 크기 28×28×120 cm로 맞춤.
- Source/vBoR/P1Probe/P1CarryContactTests.cpp (신규): 실제 Actor/World 합성 회귀시험 6개.
- Docs/Reviews/P1-golf-bag-load.md: R5-1·R5-2·R5-3 상태와 이 보고. HANDOFF.md는 무시 파일에 따로 기록함.

② 명세 §6 완료 조건
1. 충족: 패널/로그의 기존 측정 경로를 유지했고, 새 히스테리시스 상태·진입 횟수·최대 요청/실제 차이·막힌 입구 통과가 그 경로로 전달됨. ContactBlockageState 및 기존 시험대 시험 통과.
2. 충족(코드·합성 검증): 같은 요청의 다음 frame 이동 0, 보류 중 이동 0, scale 보존, simple collision과 0.005 cm 접촉 여유, 처음 놓인 자세 기준 45° 유지. 손 수 변경·새 잡기·보류/복구·reset 때 캐시 초기화. 두 손 요청 계산, 잡기 기본 꺼짐, 보조 안착/수직 낙하 경로는 그대로이며 P0-b 포함 기존 47개 통과. Quest 3 검증은 별도임.
3. 충족: 신규 DefaultGoalAssistance, ContactSlide, ContactRotation, ContactBlockageState, ContactDropOnSlope, DefaultTilt45. 기본 120 cm 보조 안착/이벤트 1회, 작은 명시 영역의 경고 1회·회색·자동 확장 없음, 시험대 최초/R/B 오경고 없음; 면/교선 이동; 걸린 끝 Yaw·바닥 재잡기; 정지·보류·밀어냄 상한·접촉 결과 45°; 거리/각도 히스테리시스와 정확히 2 cm 진입; 막힌 입구 통과; 경사면 낙하의 수평 이동·회전·후속 이동 0; 기본 45°의 두 손 30°/60° 요청과 90° 근처 연속성을 확인함.
4. 충족: 실제 플러그인·엔진 헤더 대조 경로/줄은 ④에 있음.
5. 충족: 샘플을 끈 UE 5.7 vBoREditor Win64 Development 빌드 및 -DisableUnity -Module=vBoR 파일별 재컴파일 성공, vBoR 컴파일·링크 경고/오류 0. 샘플·McpAutomationBridge·UnrealAgent를 끈 vBoR.* 53/53 Success, 실패 0, TEST COMPLETE. EXIT CODE: 0.
6. 충족: ⑤에 에디터·기기 확인을 구분함.

③ 명세와 다르게 한 것과 이유 / 구현자 재량
- 명세 이탈 없음. 기존 시험의 기대값은 바꾸지 않았음. 기존 순수 회전 시험의 명시적 15° 입력은 함수의 설정 가능한 한도 시험이므로 유지했고, 기본값/실제 접촉 결과 45°를 신규 시험으로 확인함(R5-3).
- 엔진 movement component 대신 carry Actor 전용 함수를 둠. 기존 simple collision 쿼리와 0.005 cm 여유를 재사용하고, 보조·낙하의 TryMoveToPose를 수정하지 않기 위함.
- 이동은 최대 3회 sweep, 면 투영과 교선 투영. 이미 만난 면 안으로 다시 향하면 멈춤. 엔진 TwoWallAdjust의 0.01 cm 밀어냄은 쓰지 않음.
- 회전은 최대 5° 간격(최대 36 구간)으로 확인하고, 길이 0 sweep의 Normal/PenetrationDepth로 새 겹침을 최대 3회 해소함. 여러 면의 남은 깊이를 순서대로 더하고 최종 겹침을 재검사함. 실패 구간의 회전 비율은 14회 이분 탐색함. 회전 경로 검사는 이산 검사임.
- 회전 밀어냄 상한은 편집 가능한 기본 0.5 cm. 120 cm 원통의 바닥 재잡기 1 cm/3° 요청이 큰 중심 보정으로 튀지 않게 회전을 일부만 적용하는 자리값이며 실측 합격 기준이 아님.
- 한 요청에서 회전→이동을 최대 2회 풀고, 두 회전 단계는 같은 frame의 남은 밀어냄 예산을 공유함. 내부 독립 검토가 발견한 '바닥에서 50 cm 들어 올리며 30° 기울일 때 공중에서도 회전이 덜 적용된 채 캐시에 고정됨'을 합성 회귀시험으로 재현한 뒤 수정함. 같은 요청 반복 시 이동 0도 유지했고 재검토에서 추가 중요한 문제 없음. Claude 공식 리뷰는 아직임.
- 히스테리시스 자리값은 명세대로 위치 2 cm/각도 5° 진입, 둘 다 1 cm/2.5° 미만에서 해제. 막힘 소리는 상태 표시 함수에서 떼어 진입 분기에서만 재생함.
- 기본 영역은 constructor에서만 목표 기본 크기에 맞춤. 사용자가 줄인 영역을 확장하지 않으며, 보조 완료가 영역 밖일 때만 목표 이름·목표 크기·영역 크기를 경고함. 시험의 로그 집계는 GLog 동기 flush 후 확인해 전달 지연과 실제 경고 누락을 구분함.

④ 헤더/API 확인
- 새 ISDK·MetaXR API 도입 없음. 기존 조립 경로를 실제 헤더로 다시 확인함:
  - Plugins/MetaXRInteraction/Source/OculusInteraction/Public/Interaction/IsdkGrabbableComponent.h:117,146,158 — collider, Palm 입력 허용, transformer.
  - 같은 Public/Interaction/Grabbable/IsdkGrabTransformerComponent.h:110,130,171,199,259,266 — ForceCancel, single/multi transformer, target, MultiGrab, bIsThrowable.
  - 같은 Public/Interaction/Grabbable/IsdkTransformer.h:47,57,65,89 — Begin/End/UpdateTransform, MaxGrabPoints.
  - Plugins/MetaXR/Source/OculusXRInput/Public/OculusXRInputFunctionLibrary.h:216,258 — 신뢰도·위치 유효성.
- 엔진 경로 기준: C:/Program Files/Epic Games/UE_5.7/Engine/Source/
  - Runtime/Engine/Classes/Engine/World.h:2351,2380 — ComponentSweepMulti, ComponentOverlapMulti.
  - Runtime/Engine/Classes/Engine/HitResult.h:50,61,91,116 — initial overlap의 ImpactPoint 한계, Normal, PenetrationDepth, bStartPenetrating.
  - Runtime/Engine/Private/Components/MovementComponent.cpp:711,778 — 면 투영·두 면 처리 참고(직접 component를 붙이지 않음).
  - Runtime/Engine/Classes/Components/BoxComponent.h:39,43,47 — extent 설정/크기 조회.
  - Runtime/Engine/Classes/GameFramework/Actor.h:1803 — 검사한 자세 적용.
  - Runtime/Engine/Classes/Components/AudioComponent.h:517 — 진입 소리 Play.
  - Runtime/Core/Public/Math/Quat.h:660 — Slerp.
  - Runtime/Core/Public/Misc/OutputDeviceRedirector.h:88 및 Misc/AutomationTest.h:1815 — 시험의 동기 로그 flush·기대 경고.
- 위 호출은 헤더 확인 및 실제 로컬 컴파일로 확인함. 기억에만 의존한 신규 API 없음.

⑤ 로컬에서 확인할 것 / 검증 기록
- 빌드 완료: Saved/Logs/P1R5NonUnityBuild.log, P1R5FinalBuild.log. vBoR .cpp 21개를 파일별로 재컴파일한 뒤 기본 빌드. 로컬 도구 플러그인의 StructUtils 폐기 예정 의존 안내 2줄은 vBoR 컴파일 경고와 분리함.
- 자동화 완료: Saved/Logs/P1R5AllAutomation.log — 53/53 Success, 실패 0, EXIT CODE: 0. 최초 회귀 재현 P1R5RedAutomation.log, 복합 lift+tilt 실패 재현 P1R5LiftRedAutomation.log. 알려진 XR 확장/샘플 InfoCard CDO/시험 World 정리 잡음은 Agent-Workflow 기준으로 분리함.
- 에디터: 시작·종료 시 닫힘. HANDOFF의 이번 작업 지시에 따라 MCP 호출·레벨·Blueprint·에셋 수정 없음. 기존 L_P1_GolfBag/ P1LoadBenchGameMode에서 패널·접촉 색·진입 소리를 확인할 것. 실제 자식에서 새 기본값/편집값이 적용되는지도 확인할 것.
- Quest 3 Link/VR Preview: 명세 §7의 모서리 걸림, 바닥 재잡기, 들어 올리면서 기울이기, 45°의 적절성, 기운 채 놓인 자세, 막힘 색/소리 깜빡임, 시야 밖 손 보류·복구, B 10회 반복을 재시험할 것. T-02 적재·T-09 운영 정식 시험과 이번 R5 기기 재시험은 not-tested. 코드·합성 시험 결과를 기기 동작/주관적 조작감 증거로 취급하지 않음.
- 브랜치 codex/P1-golf-bag-load, 질문 없음. R5-1·R5-2·R5-3 반영 완료, Claude 6회차 리뷰 대기. push·머지 없음.


## 질문

없음.
