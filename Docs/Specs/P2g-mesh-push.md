# P2g 메시 표면으로 밀기 — 문·뚜껑을 보이는 면에서 밀기

| 항목 | 값 |
|---|---|
| 단계 | P2 보강 — 실제 차 CAD 문을 Blueprint 자식으로 넣기(U-14 후속). 시험 T-03 |
| 결정 근거 | **사용자 요청·문답(2026-10-21):**<br>- 사용자: "미는 면 상자 pushfacebox도 그렇게 해야 자연스러울거 같아", "문의 mesh의 모형으로 collision을 만들어서 그걸 사용하면 어때?"<br>- 지금 밀기는 bounds(또는 `PushFaceBox`)를 **평평한 판**으로 본다. 곡면 문은 보이는 면 앞의 허공에서 밀리기 시작한다.<br>- Claude 조사: `LineTraceComponent`는 채널 응답을 보지 않고 메시 삼각형(복합 충돌)에도 맞는다(§8). 그래서 손 점마다 그 자리의 실제 면을 trace로 읽으면, 나머지 밀기 규칙은 그대로 두고 보이는 면에서 밀 수 있다. **메시에 충돌을 따로 만들 필요가 없다** |
| 베이스 브랜치 | `main` @ P2f 머지 뒤 |
| 작업 브랜치 | `codex/P2g-mesh-push` |
| 리뷰 파일 | `Docs/Reviews/P2g-mesh-push.md` (첫 리뷰 때 생김) |
| 상태 | 명세 — 구현 전 |
| 앞 명세 | [P2f-mesh-grab.md](P2f-mesh-grab.md). P2f가 머지된 뒤 시작한다 |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **"메시 표면으로 밀기" 옵션, 기본 끔** | `AVBoRHingedPanel`에 `bool` 프로퍼티 하나(권장 이름 `bPushOnMeshSurface`, DisplayName "메시 표면으로 밀기", 기본 `false`, 카테고리 `Hinged panel\|Push face`). 문·콘솔·글러브박스 모두 이 부모다 |
| D2 | **면 평면 대신, 손 점마다 그 자리의 면 높이** | 지금 밀기는 면을 문 좌표 Y 하나로 본다. 바깥 면은 `FaceMaxCm.Y`, 안쪽 면은 `FaceMinCm.Y`다(`VBoRDoorMotion.cpp:79`). 켜면 이 값을 **점마다** 다음으로 바꾼다.<br>**면 높이 질의** — 문 좌표 (X, Z)와 면을 받아 그 면의 Y를 돌려준다.<br>- 바깥 면(0): 문 좌표 `(X, FaceMaxCm.Y + 여유, Z)`에서 `(X, FaceMinCm.Y − 여유, Z)`로 `DoorMesh`에 trace한다. 첫 맞은 점의 문 좌표 Y가 면 높이다.<br>- 안쪽 면(1): 반대 방향으로 trace한다.<br>- **문 좌표 = `HingePivot` 지역 좌표**다(P2 D6: +X 판 끝, +Y 바깥, +Z 경첩 축).<br>  - 근거: `HingePivot` 회전 = 닫힌 회전 × 각(`VBoRHingedPanel.cpp:515`). 손 점은 닫힌 `HingeFrame` 좌표로 들어온다(`:378-396`). `ToDoor` = −각 회전이다(`VBoRDoorMotion.cpp:23-26`).<br>  - 그래서 world = `HingePivot->GetComponentTransform()`으로 바꾸면 된다. 면은 문 좌표에서 움직이지 않으므로 문이 몇 도 열려 있든 같은 답이 나온다.<br>- trace: `DoorMesh->LineTraceComponent`, `bTraceComplex = true`. 메시 삼각형(복합 충돌)을 그대로 쓴다.<br>- **맞으면:** 그 점의 면 Y = 맞은 Y. "면 안"이 참이다(지금 `:105`의 사각형 검사 대신).<br>- **안 맞으면**(창문 구멍, 윤곽 밖): 면 Y = 지금처럼 bounds 평면, "면 안"은 거짓이다. 지금처럼 Outside 이유로 버린다 |
| D3 | **면 높이를 쓰는 곳 네 군데 — 나머지는 그대로** | ① 깊이 `D`(`VBoRDoorMotion.cpp:92`)<br>② 앞에서 왔는지 `bFromFront`(`:95-96`). **이전 점의 (X, Z)**로 질의한다<br>③ 목표 각(`:124-129`). 가장 깊은 점의 면 Y를 `PlaneY` 자리에 넣는다<br>④ 면 고르기(`:205-213`). 이전 손바닥 점의 Y가 그 자리 바깥 면 Y 이상이면 바깥, 안쪽 면 Y 이하면 안쪽이다. 질의가 안 맞으면 지금 bounds 규칙을 쓴다<br>**그대로:** 가장자리 여유 사각형(`:80`·`:91`, bounds), 두께 `ThicknessCm`(`:98`, bounds), 손바닥 방향(면 법선 = 문 좌표 ±Y, `:111-112`), 축 가까움(`:103`), 잠김·손바닥 열기 금지(`:114-116`), 속도 상한·최대 각·걸쇠(`MoveTo`, `:133-149`), 추적 손실·소유·잡기 우선(`Advance` 1·2단계, `:151-225`), 집계(`Metrics`)의 뜻 |
| D4 | **한 frame에 한 걸음** | 목표 각 공식(`:126-127`)에 그 점의 면 Y만 넣는다.<br>- 곡면이면 돈 뒤 그 점의 문 좌표 X가 조금 바뀌어 면 Y가 달라진다. 다음 frame에 다시 맞춘다.<br>- 반복 풀이나 면 기울기 보정은 하지 않는다(가장 단순한 방법, AGENTS.md 토큰 규칙). E4가 수렴과 떨림 없음을 본다 |
| D5 | **`FVBoRDoorMotion`은 엔진을 모르는 계산으로 남긴다** | 면 높이 질의는 밖(`AVBoRHingedPanel`)에서 넘긴다. 넘기는 방법은 구현자가 정한다(§5).<br>- **질의가 없으면 지금 코드와 같은 결과다.** 기존 `vBoR.P2Door.*`가 그대로 통과해야 한다.<br>- 질의는 켬일 때만 부른다 |
| D6 | **`DoorMesh` 충돌** | **켬:** `QueryOnly`, 모든 채널 `Ignore`, overlap 이벤트 끔. trace만 받고, 손 감지 구·짐·월드 질의에는 걸리지 않는다.<br>**끔:** 지금처럼 `NoCollision`(`VBoRHingedPanel.cpp:50`).<br>`NoCollision`이면 물리 몸이 안 생겨 trace가 안 된다(§8). 에디터의 "설정 검사"도 맞게 답하도록, 켬 설정은 `OnConstruction`에서도 적용한다 |
| D7 | **시작 검사·설정 검사** | 켰을 때 `DoorMesh` bounds 안의 몇 점(예: X·Z 3×3)에서 바깥→안 trace를 한다.<br>- 하나도 안 맞으면 경고 한 번. 뜻: "DoorMesh has no traceable collision; push uses bounds". 그리고 끈 것과 같이 bounds로 동작한다.<br>- `bUsePushFaceBox`도 켜져 있으면 알림 한 줄. 뜻: "PushFaceBox ignored while pushing on the mesh surface". 면 사각형·두께는 메시 bounds를 쓴다.<br>- 메시 두께 경고(`VBoRHingedPanel.cpp:247-250`)는 켬일 때 내지 않는다.<br>- 결과는 `EvaluateSetup`에 넣는다. BeginPlay 경고와 "설정 검사" 버튼이 같은 판정을 쓴다(P2e) |
| D8 | **근사 차·근사 콘솔·글러브박스는 끔 그대로** | — |

## 1. 왜 만드는가

- 실제 문은 바깥이 볼록하고 안쪽 트림이 오목하다. 평평한 판 모델에서는 보이는 면과 미는 면이 다르다. 그래서 허공에서 밀리거나, 손이 트림에 묻힌 뒤에야 밀린다.
- merge한 문에 사이드미러처럼 튀어나온 것이 있으면 bounds가 두꺼워진다. 그래서 `PushFaceBox`를 따로 두었다(P2e).
  - 메시 표면으로 밀면 미러도 실제 면이 된다. 미러를 밀면 문이 밀린다.
  - 미러를 빼고 싶으면 그 메시의 충돌 설정을 Use Simple Collision As Complex로 바꾸고 미러 없는 simple 충돌을 만든다. 그러면 trace가 그 충돌을 쓴다(§8).
- 단순 충돌(convex)이 아니라 삼각형 trace를 쓰는 이유:
  - convex 하나는 오목한 안쪽 트림을 메운다.
  - trace는 보이는 삼각형에 그대로 맞는다.
  - 사용자가 충돌을 따로 만들 필요가 없다. 엔진 메시에는 기본으로 복합 충돌이 있다.

## 2. 범위

### 만들 것

1. `AVBoRHingedPanel`의 옵션과 D2–D7.
2. `FVBoRDoorMotion`의 면 높이 질의 자리(D5).
3. 자동화 시험 §4.

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| 레버 손가락 밀기(P5m)·룸미러 밀기(P5e·P5l) 바꾸기 | 부품 bounds 기준 그대로. 필요하면 따로 명세 |
| 손바닥 방향 판정을 면 법선(trace 법선)으로 바꾸기 | 한계 60°라 곡률에 비해 넉넉하다. 바꾸면 기존 시험·집계가 달라진다 |
| 반복 풀이·면 기울기 보정 | D4 |
| 잡기 바꾸기 | P2f |
| 근사 차 값, Blueprint-Guide | 근사 차는 끔. 안내 문서는 Claude가 리뷰 뒤 |
| ISDK 플러그인 고치기 | — |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- **끄면(기본) 지금과 같다.** 근사 차와 모든 기존 시험이 그대로 통과한다.
- **켜도 평평한 상자 문이면 같은 결과다**(E2). 메시 면 = bounds 면이기 때문이다.
- 보이는 메시·재질·자리는 그대로다.
- 손잡이 연결(P2e)·잠김·버튼 Unlock(P2d)·손잡이 넘김(P2c)은 그대로다.
- 한 frame의 trace 수에 상한이 있다(손 2 × 점 5 × 지금·이전·면 고르기, 대략 30 이하). 문마다 frame마다 전체 월드를 훑지 않는다.

**기존 시험 감사 — 면 bounds·두께를 직접 보는 단언(모두 옵션 끔이라 그대로):**
- `P2DoorMotionTests.cpp:25`(계산만, 질의 없음)
- `P2bAxisPartTests.cpp:184`·`:376`
- `P2cHingedPanelTests.cpp:194`
- `P2dLatchedPanelTests.cpp:366`
- `P2ePartAssemblyTests.cpp:287`·`:288`·`:303`·`:319`
- `P5aGloveboxTests.cpp:183`

밀기 동작을 보는 묶음(`vBoR.P2Door.*`, `vBoR.P2b.Door.*`, `vBoR.P2c.*`, `vBoR.P2d.*`, `vBoR.P2e.PushFace.*`·`Setup.*`, `vBoR.P5a.*`, P3 흐름의 문 단계)도 끔이라 그대로다.

## 4. 경계 상황과 그 시험 (`vBoR.P2g.*`)

손 입력은 시험용 판 `AP2cTrackedPanel`(`P2Probe/P2cTestFixture.h:9`)의 손 배열로 넣는다. 닫힌 경첩 좌표 그대로 들어간다(`:16`). 사용 예는 `P5hSpectatorTests.cpp:318-324`. 계산만 하는 시험은 `P2DoorMotionTests.cpp:15-40` 방식이다.

| ID | 상황 | 확인 |
|---|---|---|
| E1 | 계산만 — 질의가 bounds 평면을 돌려줌 | `FVBoRDoorMotion`에 "어느 점이든 bounds 면 Y" 질의를 넘긴다. `vBoR.P2Door.AsymmetricFacePlanes`·`PushGeometryAndWithdrawal`의 입력 묶음을 넣으면 frame마다 각이 질의 없을 때와 같다(1e-6 deg) |
| E2 | 세계 + 근사 Cube 문 | 같은 손 입력으로 켬과 끔의 각이 frame마다 같다(1e-3 deg). 바깥 밀어 닫기, 안쪽 밀어 열기, 스침(PalmDirection), 축 가까움(NearAxis) |
| E3 | **비스듬한 면** | Cube 메시를 문 좌표 Z 둘레로 10° 돌려 둔다(`MeshRestFromRoot` 회전). 바깥 면 Y가 X에 따라 달라진다.<br>- 손을 **bounds 평면과 실제 면 사이**에 넣는다: 끔 → 밀림, 켬 → 안 밀림.<br>- 실제 면을 지나면 켬 → 밀림.<br>- 손을 그 자리에 두고 몇 frame 뒤, 가장 깊은 점이 실제 면에서 0.1 cm 안에 있다 |
| E4 | **곡면 수렴** | 엔진 Sphere 또는 Cylinder 메시 문, 켬. 손을 면 안으로 넣고 그대로 둔다.<br>- 각이 한 방향으로만 변한다(떨림 없음).<br>- 속도 상한을 지킨다.<br>- 가장 깊은 점이 면에서 0.5 cm 안으로 수렴한다(삼각형 조각 오차) |
| E5 | 안 맞는 자리 | bounds 사각형 안이지만 메시 윤곽 밖(구의 bounds 모서리 쪽)에서 민다 → 안 밀림, `Ignored[Outside]`가 접촉 에피소드당 1회 |
| E6 | 안쪽 면 | 켬, 풀린 문. 안쪽에서 민다 → 안쪽 실제 면에서 시작해 열린다 |
| E7 | 면 고르기 | 이전 손바닥 점이 bounds 평면보다 안쪽이지만 실제 바깥 면보다는 바깥이다 → 바깥 면(0)으로 고른다 |
| E8 | trace 안 되는 메시 | `DoorMesh`에 빈 `BodySetup` 복제 메시를 넣는다(`P1CarryWorldTests.cpp:235-237`). 켬 → 경고 1회, 끈 것과 같은 각 |
| E9 | 충돌 설정 | 켬 → `DoorMesh`가 `QueryOnly`·모든 채널 Ignore·overlap 끔.<br>- ISDK 손바닥과 같은 구(WorldDynamic, 모든 채널 Overlap)가 `DoorMesh`와 overlap되지 않는다.<br>- `Visibility` 채널 월드 trace가 `DoorMesh`에 맞지 않는다.<br>끔 → `NoCollision` |
| E10 | 설정 검사 | 켬 + `bUsePushFaceBox` 켬 → 알림 줄이 있고 두께 경고는 없다. 켬 + trace 안 되는 메시 → 경고. 에디터 버튼과 BeginPlay가 같은 결과다 |
| E11 | 그대로인 규칙 (켬) | 잠긴 문 밀기 = Locked. 손바닥 방향이 한계를 넘음 = PalmDirection. 축 가까움 = NearAxis. 미는 손 추적 손실 → `PushTrackingLosses` +1, 다른 손이 바로 밀 수 있음(P2 E15b). 잡기가 밀기보다 먼저 |
| E12 | 열린 문에서 | 문을 40° 연 뒤 바깥에서 민다 → 실제 면 기준으로 닫히는 쪽으로 밀린다(질의가 각과 상관없음) |
| E13 | trace 수 | 한 frame의 질의 수가 상한 안이다(시험용 계수로 확인) |

## 5. 구현자가 정할 것

- 질의를 `FVBoRDoorMotion`에 넘기는 방법(콜백 등)과 그 이름.
- trace 여유(cm), 시작 검사의 점 수, 경고의 정확한 영어 문장(위 뜻 유지), 시험용 계수 방식.
- E4의 메시·크기와 허용 오차(0.5 cm에서 시작, 근거를 보고에 적음).

## 6. 완료 조건

1. D1–D8을 지킨다.
2. E1–E13이 통과한다.
3. 기존 `vBoR.*` 시험이 모두 통과한다.
4. non-unity·unity 빌드에서 vBoR 경고가 없다.
5. 보고는 AGENTS.md "작업 방식" 형식으로, 커밋 본문과 HANDOFF "Codex 결과"에 쓴다.

## 7. 로컬에서 확인할 것 — 사용자(에디터·VR, 끝에 한꺼번에)

1. 회사 문 BP에서 옵션을 켜고 "설정 검사"를 누른다. 경고가 없어야 한다.
2. Play·VR에서 본다:
   - 바깥 면·안쪽 트림 밀기가 **보이는 면에 닿을 때** 시작되는지.
   - 사이드미러 같은 돌출을 밀 때 어떻게 되는지.
   - 창문(유리 있음·없음) 자리에서 어떻게 되는지.
3. **Nanite 메시라면** 복합 충돌이 원본보다 거친 대체 메시로 만들어질 수 있다. 그러면 밀리는 면이 보이는 면과 조금 다를 수 있으니 같이 본다(확인 안 한 엔진 동작).
4. 시험 상태는 VR 전까지 `not-tested`(T-03).

## 8. 확인한 API 근거

| 무엇 | 근거 | 쓰는 곳 |
|---|---|---|
| 한 컴포넌트에만 trace | `UPrimitiveComponent::LineTraceComponent` `Engine/Classes/Components/PrimitiveComponent.h:2961`. 구현은 채널 응답 없이 `BodyInstance->LineTrace(…, bTraceComplex, …)` `Engine/Private/Components/PrimitiveComponent.cpp:3513-3525` → `FBodyInstance::LineTrace` `Engine/Private/PhysicsEngine/BodyInstance.cpp:4192-4197` | D2 |
| 복합 trace가 무엇에 맞나 | `FPhysInterface_Chaos::LineTrace_Geom`은 shape의 complex·simple 표시만 거른다(`bTraceComplex && complex` 또는 `!bTraceComplex && simple`). Query 응답은 보지 않는다 `Engine/Private/PhysicsEngine/Experimental/PhysInterface_Chaos.cpp:905-955` | D2·D6 |
| `NoCollision`이면 물리 몸 없음 | `ShouldCreatePhysicsState` = 충돌이 `NoCollision`이 아닐 때(또는 `bAlwaysCreatePhysicsState`) `Engine/Private/Components/PrimitiveComponent.cpp:2018` | D6 |
| 단순 충돌을 복합으로 쓰기(미러 빼기 대안) | `CTF_UseSimpleAsComplex` `PhysicsCore/Public/BodySetupEnums.h:17` | §1 |
| 밀기 계산 | 손 점(손바닥 + 네 손끝) `Interaction/VBoRDoorMotion.h:5-10`. `FindPush` `VBoRDoorMotion.cpp:76-131`, `MoveTo` `:133-149`, `Advance`·면 고르기 `:151-229`, `ToDoor` `:23-26` | D2–D5 |
| 판 쪽 | `DoorMesh` 생성·`NoCollision` `Interaction/VBoRHingedPanel.cpp:48-55`, 면 bounds `:107-117`, `OnConstruction` `:125-136`, `MeshFaceBounds` `:148-151`, 설정 판정 `:207-256`, `HingeFrame`·`ReadHand` `:378-397`, `Tick` `:474-511`, `HingePivot` 회전 `:515` | D2·D6·D7 |
| 시험 본보기 | 시험용 판 `P2Probe/P2cTestFixture.h:9`·`:16`, 손 넣기 `P3Probe/P5hSpectatorTests.cpp:318-324`, 계산만 `P2Probe/P2DoorMotionTests.cpp:15-40`, 충돌 없는 복제 메시 `P1Probe/P1CarryWorldTests.cpp:235-237` | §4 |

## 개정 이력

| 날짜 | 내용 |
|---|---|
| 2026-10-21 | 처음 작성(Claude) |
