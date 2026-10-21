# P2g 메시 표면으로 밀기 — 리뷰

명세: [P2g-mesh-push.md](../Specs/P2g-mesh-push.md) · 브랜치: `codex/P2g-mesh-push` · 베이스: `main@8427673`

상태: **1회차 리뷰 — 통과. 문제 없음, 열린 항목 없음. 머지 승인 대기.** 명세 §7에 확인 항목 하나를 더했다(참고 1, 개정 1).

## 리뷰 1회차 (2026-10-21, Claude — `a4ef3b6`)

**결과: 통과.** D1–D8이 명세대로 들어갔고 범위 밖 변경은 없다.
- 에디터·에셋·Config·플러그인·근사 차는 바뀌지 않았다.
- 기존 시험 파일은 하나도 바뀌지 않았다(§3 감사 목록의 단언 그대로).
- Codex는 보고를 커밋 본문과 HANDOFF에만 썼고, 리뷰 파일을 만들지 않았다.

**재현(`a4ef3b6`).** 에디터가 닫힌 것을 확인했고, 단계마다 에디터가 다시 열렸는지 보며 돌렸다.
- non-unity(`-DisableUnity -Module=vBoR`): `Result: Succeeded`, vBoR 경고·오류 0. 로그 `Saved/Logs/ReviewP2g-NonUnity.log`.
- unity: `Result: Succeeded`, vBoR 경고·오류 0. 로그 `Saved/Logs/ReviewP2g-Unity.log`.
- `vBoR.*`: **472/472**(P2g 13개 포함), `TEST COMPLETE. EXIT CODE: 0`. 로그 `Saved/Logs/ReviewP2g-All.log`.
- **최종 트리의 전체 시험은 이 재현이 처음이다.** Codex는 전체 시험을 한 번 돌렸고 470/472였다. 새 E4·E8의 시험 구성 문제였다. 시험만 고친 뒤 P2g 묶음만 다시 돌렸다. 보고 ③에 그대로 적었다(참고 4).

**Codex 토큰:** 5.63M(캐시 96%), 2026-10-21 01:40–01:52 UTC(KST 10:40–10:52), 약 12분. 하위 에이전트 0.

**확인한 것:**
- **질의 자리(D5)** — `FVBoRDoorMotion::QueryFace`(`TFunction`)다. 비어 있으면 모든 분기가 옛 식이다.
  - `FindPush`의 면 안: `VBoRDoorMotion.cpp:94`. 비어 있으면 옛 사각형 검사다.
  - 이전 점: `:100`. 질의가 없으면 `BoundsY`다.
  - 목표 각: `:133`. 가장 깊은 점의 면 Y를 쓴다.
  - 면 고르기: `:217`. 질의가 안 맞으면 bounds 값이 남는다.
  - E1이 옛 P2Door 입력 묶음으로 1e-6°를 본다.
- **질의 수(§3 상한)** — `Advance`는 손마다 `FindPush`를 한 번만 부른다(`:197-232`). 그래서 손 하나에 면 고르기 2 + 지금 5 + 이전 5 = 12, 두 손이면 24다. E13이 최악 경로로 24를 센다.
- **면 높이 질의(D2)** — `AVBoRHingedPanel::QueryMeshFace`(`VBoRHingedPanel.cpp:167`).
  - 문 좌표 (X, Z)에서 bounds Y ±1 cm 밖부터 `DoorMesh->LineTraceComponent`로 trace한다(`bTraceComplex`).
  - 좌표 변환은 `HingePivot` world transform이다. 손 Y는 쓰지 않는다. 그래서 손이 면 안에 들어가 있어도 면을 바깥에서 읽는다.
  - E12가 기울인 Actor·`HingePivot`와 40° 열림에서 닫힌 때와 같은 Y(1e-4 cm)를 본다.
- **충돌(D6)** — `ConfigurePushCollision`(`:159`)이 `OnConstruction`과 `PreInitializeComponents`에서 불린다.
  - 켬은 `QueryOnly`, 모든 채널 Ignore, overlap 끔이다. 끔은 `NoCollision`이다.
  - 객체 종류로 묻는 질의는 채널 응답을 보지 않는다. 그래도 vBoR에서 그런 질의는 리프트게이트 끼임 검사 하나다. 그 검사는 짐 메시만 고른다(`VBoRPoweredPanel.cpp:145-150`).
  - ISDK에서는 레이 잡기(`IsdkRayGrabDetector.cpp`)뿐이다. 레이 잡기는 금지라 꺼져 있다.
- **설정 검사(D7)** — 판정은 `EvaluateSetup`(`:296`) 하나다. 내부 3×3 바깥 trace를 하고, 하나라도 맞으면 trace된다고 본다.
  - BeginPlay가 결과로 `bMeshPushReady`를 정하고(`:423`) 질의를 넣는다(`:124`).
  - 켬일 때 `PushFaceBox`는 알림으로 바뀌고, 두께·상자 회전 경고는 내지 않는다(`:110`).
  - E10이 Play 전과 뒤에 같은 보고인지 본다.
- **시험 구성 조사(보고 ③)는 엔진 코드와 맞다:**
  - 빈 `BodySetup`도 렌더 삼각형으로 복합 충돌을 다시 만든다. 그래서 trace 안 되는 메시를 만들려고 `CTF_UseSimpleAsComplex`를 썼다.
  - body에 구가 있으면 scale이 가장 작은 축으로 묶인다(`BodyInstance.cpp:2133-2153`·`:2090-2128`). 그래서 E4는 등방 구를 쓴다(참고 2).

### 참고 (반영 불필요 — 열린 항목 아님)

1. **단일면·면 방향이 뒤집힌 CAD 메시 — 내 명세의 빈틈.** 복합 trace는 기본으로 **뒷면을 무시한다.**
   - 근거: `UBodySetup::bDoubleSidedGeometry`의 기본은 거짓이다(`Engine/Classes/PhysicsEngine/BodySetup.h:160-166`, 생성자에서 정하지 않음). 이 값이 `SetCullsBackFaceRaycast(!bDoubleSided)`로 간다(`BodySetup.cpp:710` → `Experimental/ChaosInterfaceUtils.cpp:323`). 그러면 trace 방향과 같은 쪽을 보는 삼각형은 건너뛴다(`Chaos/Private/Chaos/TriangleMeshImplicitObject.cpp:210-214`).
   - 엔진 Cube·Sphere처럼 닫힌 메시는 결과가 같다. 그래서 시험은 통과한다.
   - CAD 문에서는 다르다. 겉판이 한 장짜리이거나 면 방향이 일부 뒤집혀 있으면 그 자리 trace가 빠진다. 그러면 Outside로 버려지거나 뒤쪽 다른 면에 맞는다. 안쪽 트림이 없는 뚜껑을 안에서 밀 때도 빠진다.
   - 3×3 시작 검사는 전부 빠질 때만 경고한다. 일부만 빠지면 알리지 않는다.
   - **조치:** 메시 표면으로 밀 메시는 에셋에서 `Double Sided Geometry`를 켠다. 그러면 어느 쪽에서든 첫 면에 맞는다. 명세 §7에 이 확인을 더했다(개정 1). 머지 뒤 Blueprint-Guide에도 적는다. 코드 변경은 필요 없다.
   - P2f 잡기는 해당하지 않는다. 손 감지 구는 overlap이라 면 방향을 보지 않는다.
2. **구·캡슐 simple 충돌 + 비균등 scale.** body에 구가 하나라도 있으면 그 body의 모든 shape(삼각형 포함)가 가장 작은 scale로 묶인다. 캡슐이면 XY가 묶인다(`BodyInstance.cpp:2133-2153`).
   - CAD 메시는 보통 scale 1이라 해당하지 않는다.
   - 잡기·밀기 메시에 simple 충돌을 만든다면 구·캡슐 대신 box·convex를 쓴다. Blueprint-Guide에 한 줄 적는다.
3. **`SyncSettings`가 매 `Tick`(`VBoRHingedPanel.cpp:540`)마다 `QueryFace`에 새 람다를 넣는다.** 켬이면 문마다 frame마다 작은 heap 할당이 하나 생긴다. 문 수가 적어 문제는 아니다. 이 함수를 다음에 고칠 때 바뀔 때만 넣으면 된다.
4. **빌드·시험 횟수.** 빌드는 non-unity 5회, unity 3회였다. 시험은 전체 1회(470/472), 진단 1회, P2g 묶음 2회였다.
   - 첫 전체 실행에서 시험 구성 실패가 나와 고쳐야 했으므로 규칙 위반으로 보지 않는다.
   - 다만 마지막 트리로 전체 시험을 다시 돌리지 않고 P2g 묶음만 돌렸다. 그래서 위 재현이 그 확인이 됐다. 보고는 정직했다("단일 전체 실행 472/472 성공으로 보고하지 않음").
5. **켬 + trace 안 되는 메시 + `PushFaceBox` 켬**이면 상자가 아니라 메시 bounds로 민다. 명세 D7("면 사각형·두께는 메시 bounds")을 그대로 읽은 결과다. 경고가 따로 나오므로 그대로 둔다.

### 질문

없음.
