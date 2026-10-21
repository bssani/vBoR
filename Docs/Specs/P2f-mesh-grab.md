# P2f 메시 충돌로 잡기 — 상자 대신 부품 메시에 닿을 때만 잡힘

| 항목 | 값 |
|---|---|
| 단계 | P2 보강 — 실제 차 CAD 부품을 Blueprint 자식으로 넣기(U-14 후속). 시험 T-03·T-08 |
| 결정 근거 | **사용자 요청·문답(2026-10-21):**<br>- 사용자: "interaction 할 때 지금 다 box collision으로 하고 있는데 … mesh collision을 써서 그 mesh만 잡히게끔"<br>- Claude: ISDK는 손바닥에 닿은 후보 중 **충돌 컴포넌트 원점**이 가장 가까운 것을 고른다. CAD 메시는 피벗이 모두 차 원점이라 부품끼리 거리가 같아진다(§8).<br>- 사용자: "mesh 자체 pivot point를 바꾼다면? … 그 값만큼 mesh의 location 값을 주면 문제 없어 보여" → **채택.** 잡는 부품 메시만 피벗을 부품 쪽으로 옮기고, 옮긴 값을 `MeshRestFromRoot`에 넣는다(Blueprint 작업, 이 명세 밖) |
| 베이스 브랜치 | `main` @ 이 명세 머지 뒤 |
| 작업 브랜치 | `codex/P2f-mesh-grab` |
| 리뷰 파일 | `Docs/Reviews/P2f-mesh-grab.md` (첫 리뷰 때 생김) |
| 상태 | 명세 — 구현 전 |
| 다음 명세 | [P2g-mesh-push.md](P2g-mesh-push.md)(밀기). 이 명세가 머지된 뒤 시작한다 |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **부품마다 "메시 충돌로 잡기" 옵션, 기본 끔** | 네 부모에 `bool` 프로퍼티 하나씩(권장 이름 `bGrabWithMesh`, DisplayName "메시 충돌로 잡기", 기본 `false`). 각 부모의 Input 카테고리에 둔다.<br>- `AVBoRAxisPart` → `PartMesh`<br>- `AVBoRSteeringWheel` → `WheelMesh`<br>- `AVBoRTwoAxisPart` → `PartMesh`<br>- `AVBoRKnob` → `KnobMesh`<br>Play 중에 바꾸는 것은 받지 않는다. `PreInitializeComponents`에서 한 번 읽는다 |
| D2 | **켜면 메시가 ISDK 잡는 충돌** | 지금 `SetGrabCollider(GrabBox)`를 부르는 자리에서 처리한다(`AxisPart.cpp:192`, `SteeringWheel.cpp:85`, `TwoAxisPart.cpp:78`, `Knob.cpp:76`).<br>- **메시 충돌:** `QueryOnly`, 객체 종류 `ECC_WorldDynamic`, 모든 채널 `Ignore`, `ECC_WorldDynamic`만 `Overlap`, overlap 이벤트 켬. 지금 `GrabBox`와 같은 응답이다. 그래서 짐 착지 검사가 막힘으로 보지 않는다(짐은 막힘만 셈, `VBoRCarryObject.cpp:184-187`).<br>- `Grabbable->SetGrabCollider(메시)`를 부른다.<br>- `GrabBox`는 `NoCollision`, overlap 이벤트를 끈다. `OnConstruction`의 상자 자리·크기 계산은 그대로 둔다(옵션을 끄면 바로 쓰게).<br>- **끄면** 지금 그대로다: 메시 `NoCollision`, 상자가 잡는 충돌 |
| D3 | **쓸 수 있는 메시 충돌이 없으면 상자로** | 쓸 수 있는 충돌이란 메시 `BodySetup`에 simple 요소가 하나 이상 있거나, `CollisionTraceFlag`가 `CTF_UseComplexAsSimple`인 것이다. ISDK는 overlap으로 hover를 알고, overlap은 simple 충돌(또는 complex-as-simple)만 쓰기 때문이다.<br>- 둘 다 아니면 경고를 한 번 남기고 D2를 하지 않는다(상자로 잡음). 뜻: "grab mesh has no simple collision (or Use Complex As Simple); using GrabBox".<br>- 판정 방식은 짐의 `HasSimpleCollision`과 같다(`VBoRCarryObject.cpp:169-173`). 여기에 complex-as-simple을 더한다 |
| D4 | **피벗이 Actor 원점이면 경고** | 켰는데 메시의 쉬는 자리 위치(`MeshRestFromRoot`의 위치)가 Actor 원점에서 1 cm 안이면 경고를 한 번 남긴다. 뜻: "grab mesh pivot is at the actor origin; ISDK picks the nearest collider origin — recentre the mesh pivot and put the offset in MeshRestFromRoot".<br>- 잡기는 막지 않는다.<br>- 이유: 실제 차 부품 BP는 Actor가 차 원점에 있다(Blueprint-Guide §4.1). 메시 피벗도 차 원점이면 가까운 부품끼리 고르기가 정해지지 않는다(§8 고르기) |
| D5 | **잡기를 끄는 기존 경로는 쓰는 충돌을 끈다** | 메시 없음(`AxisPart.cpp:173-177`), 걸림 설정 잘못(`:183-191`), 휠·미러·노브 설정 잘못(`SteeringWheel.cpp:84`, `TwoAxisPart.cpp:76-77`, `Knob.cpp:75`)에서, 켬이면 **메시** 충돌·overlap을 끈다. 끔이면 지금처럼 상자를 끈다.<br>- 미러의 `GrabBox->SetVisibility(bValid, true)`(`TwoAxisPart.cpp:77`)는 상자에만 한다. **메시의 보임은 건드리지 않는다** |
| D6 | **그대로 두는 것** | 휠 잡는 반지름 검사(P5d D5, 손바닥 잡기 점 기준, `SteeringWheel.cpp:113`·`:128-129`). 레버 손가락 밀기(P5m, 메시 bounds 기준, `AxisPart.cpp:352`). hover·잡기 색(메시 overlay). 입력 허용(palm·pinch). 축 변환기·놓기·추적 손실 보류·문 넘김·걸림 |

## 1. 왜 만드는가

- 잡는 상자는 메시 bounds에 여유를 더한 것이다. 곡면이거나 비스듬한 부품은 상자 모서리의 허공에서도 hover되고 잡힌다.
- ISDK의 기본은 메시 충돌로 잡는 것이다(`ColliderMode = FindStaticMesh`). 짐(박스)도 이미 메시로 잡는다(`VBoRCarryObject.cpp:114`).
- 다만 그대로 메시로 바꾸면, 차 원점 피벗 메시끼리 ISDK의 "가장 가까운 원점" 고르기가 동률이 된다. 그래서 피벗을 옮기는 Blueprint 작업과 D4 경고를 같이 둔다.

## 2. 범위

### 만들 것

1. 네 부모의 옵션과 D2–D5.
2. 자동화 시험 §4.

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| 문·콘솔의 잡는 자리 상자(`OutsideHandle`·`InsideHandle`) 바꾸기 | 손잡이 자리 표시용이다(P2d). 실제 문 손잡이는 `AVBoRAxisPart`다 |
| 키(자유 6DoF)·짐 | 키는 따로 정한 예외(U-10)다. 짐은 이미 메시로 잡는다 |
| 밀기(문·레버·미러) | 문 밀기는 P2g. 레버·미러 밀기는 지금 범위가 아니다 |
| ISDK 플러그인 고치기, 후보 고르기 바꾸기 | 플러그인은 고치지 않는다. 고르기는 피벗으로 푼다 |
| 메시 피벗을 바꾸는 코드·에디터 도구 | 사용자가 에디터에서 한다 |
| Blueprint-Guide 고치기 | Claude가 리뷰 뒤 고친다 |
| 근사 차 값 바꾸기 | 근사 차는 끈 상태로 둔다 |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- **끄면(기본) 지금과 같다.** 근사 차와 모든 기존 시험이 그대로 통과한다.
- 보이는 메시·크기·재질·자리는 그대로다. 바뀌는 것은 충돌 설정뿐이다.
- 메시 충돌은 Query 전용이다. 손·Pawn·짐을 막지 않고, 물리 시뮬레이션은 없다.
- `GrabBox`의 자리·크기 계산은 그대로다.

**기존 시험 감사 — `GrabBox`를 직접 보는 단언(모두 옵션 끔이라 그대로):**
- `P2bAxisPartTests.cpp:110`·`:212`·`:323`·`:388`
- `P2dLatchedPanelTests.cpp:167`
- `P5bGearLeverTests.cpp:337`·`:421`
- `P5cTurnSignalTests.cpp:183`
- `P5dSteeringWheelTests.cpp:214`·`:221`
- `P5eRearviewMirrorTests.cpp:183`·`:307`·`:465`·`:680`
- `P5fKnobTests.cpp:176`
- `P5mLeverTests.cpp:93`·`:106`·`:181`·`:192`·`:231`
- 짐의 "메시가 잡는 충돌" 단언(`P1CarryWorldTests.cpp:115`, `P1cCarrySettleTests.cpp:307`)은 이 명세와 상관없다.

## 4. 경계 상황과 그 시험 (`vBoR.P2f.*`)

| ID | 상황 | 확인 |
|---|---|---|
| E1 | 기본값 | 네 부모 모두 옵션 끔. `GetGrabCollider() == GrabBox`, 메시 `NoCollision` |
| E2 | 켬 + 엔진 Cube 메시 | 네 부모 모두 `GetGrabCollider() == 메시`. 메시 충돌이 D2 설정과 정확히 같음. `GrabBox`는 `NoCollision`·overlap 꺼짐 |
| E3 | **상자 모서리의 허공** — 실제 ISDK 손바닥으로 | `/Engine/BasicShapes/Sphere`를 `AVBoRAxisPart` 메시로 쓴다(예: 지름 40 cm). bounds 모서리는 구 표면에서 약 15 cm 떨어져 있다.<br>- 손바닥을 **상자 안·구 표면에서 4 cm보다 먼 자리**에 둔다: 끔 → hover됨, 켬 → hover 안 됨.<br>- 구 표면에 닿는 자리: 켬 → hover되고 `PalmGrab`으로 선택됨.<br>- 실제 grabber 쓰는 법은 `P2bAxisPartTests.cpp:304-332`.<br>- 휠·미러·노브도 켬에서 메시에 닿으면 hover됨을 한 번씩 본다 |
| E4 | **가까운 부품 고르기** | 같은 Actor 원점에 `AVBoRAxisPart` 둘을 둔다. 메시 피벗을 `MeshRestFromRoot` 위치로 서로 10 cm 떨어진 부품 자리에 둔다. 손바닥 구가 둘 다 닿고 A 원점에 더 가까운 자리 → A가 선택됨. B 쪽으로 옮기면 B가 선택됨 |
| E5 | 충돌 없는 메시 | 복제 메시에 빈 `BodySetup`을 넣는다(`P1CarryWorldTests.cpp:235-237` 방식). 켬 → 경고 1회, `GetGrabCollider() == GrabBox`, 상자로 잡힘 |
| E6 | Use Complex As Simple | 복제 메시: simple 요소 없음 + `CTF_UseComplexAsSimple`. 켬 → 경고 없음, 메시가 잡는 충돌, 실제 손바닥 구 overlap으로 hover됨.<br>- 시험에서 복제 메시에 복합 충돌을 만들 수 없으면, 이유를 커밋 보고 ③에 적고 이 줄은 로컬 확인(§7)으로 넘긴다 |
| E7 | 피벗 경고 | 켬 + `MeshRestFromRoot` 위치 0 → 경고 1회(잡기는 됨). 위치가 1 cm보다 멀면 경고 없음. 끔이면 경고 없음 |
| E8 | 잡기 끄는 경로(켬 상태) | (a) 메시 없음 (b) 레버 걸림 설정 잘못(P5b E12 중 하나) (c) 휠 설정 잘못 (d) 미러 설정 잘못 (e) 노브 설정 잘못 → 메시 충돌·overlap 꺼짐, Palm 거부. (d)에서 미러 메시는 계속 보임 |
| E9 | 움직인 뒤 자리 | 켬 `AVBoRAxisPart`를 30° 돌린 뒤, 돈 메시 자리에서 손바닥 구 overlap이 됨. 문 `HingePivot` 밑 손잡이(P2e 경로)도 문을 40° 연 뒤 열린 자리에서 hover됨 |
| E10 | 짐을 막지 않음 | 켬 메시 자리에서 짐과 같은 충돌 설정(WorldDynamic, WorldStatic·WorldDynamic 막힘)의 `ComponentOverlapMulti`에 막힘(`bBlockingHit`)이 없음 |

## 5. 구현자가 정할 것

- 프로퍼티 이름(권장 `bGrabWithMesh`)과 경고의 정확한 영어 문장. 위 뜻은 유지한다.
- 네 클래스의 같은 처리를 도우미 함수 하나로 묶을지. 묶더라도 새 상속 구조는 만들지 않는다.
- E3 구 크기·자리, E4 거리, E6에서 복합 충돌을 만드는 방법.

## 6. 완료 조건

1. D1–D6을 지킨다.
2. E1–E10이 통과한다(E6은 위 예외 규칙대로).
3. 기존 `vBoR.*` 시험이 모두 통과한다.
4. non-unity·unity 빌드에서 vBoR 경고가 없다.
5. 보고는 AGENTS.md "작업 방식" 형식으로, 커밋 본문과 HANDOFF "Codex 결과"에 쓴다.

## 7. 로컬에서 확인할 것 — 사용자(에디터·VR, 끝에 한꺼번에)

1. **회사 CAD 손잡이 하나로:**
   - 메시 피벗을 손이 잡는 부분 가운데쯤으로 옮긴다. 옮겨서 생긴 위치(회전도 했으면 회전)를 `MeshRestFromRoot`에 넣는다.
   - 메시 에셋에 simple 충돌을 만들거나 Collision Complexity를 Use Complex Collision As Simple로 둔다.
   - 옵션을 켠다.
2. **Play·VR에서 본다:**
   - hover가 메시에 닿을 때만 켜지는지.
   - 상자 여유(+1 cm)가 없어져도 문 면에 붙은 얇은 손잡이가 잡히는지.
   - 휠과 레버처럼 가까운 부품끼리 맞게 골라지는지.
   - 콘솔 변수 `Meta.InteractionSDK.DebugInteractionVisuals 1`이면 ISDK가 잡는 충돌의 bounds 상자를 그린다(`OculusInteraction.cpp:39-40`, `IsdkGrabbableComponent.cpp:188-198`). 메시 충돌 모양 자체는 뷰포트 Show → Collision으로 본다.
3. 시험 상태는 VR 전까지 `not-tested`(T-03·T-08).

## 8. 확인한 API 근거

| 무엇 | 근거 | 쓰는 곳 |
|---|---|---|
| ISDK 손 감지 구 | 손바닥 반지름 4 cm `Plugins/MetaXRInteraction/Source/OculusInteraction/Public/Interaction/GrabDetectors/IsdkHandGrabDetector.h:134`, pinch 1.2 cm `:130`, 객체 종류 `ECC_WorldDynamic` `:143`. 모든 채널 Overlap·시작 overlap → hover `Private/Interaction/GrabDetectors/IsdkHandGrabDetector.cpp:35-70`·`:358` | D2·E3 |
| **후보 고르기 = 충돌 컴포넌트 원점 거리** | `IsdkHandGrabDetector.cpp:397-460`(`GrabbableCollider->GetComponentLocation()`) | D4·E4 |
| 충돌 → grabbable 찾기 | 같은 Actor의 grabbable 중 `GetGrabCollider() == Component` `Private/IsdkFunctionLibrary.cpp:320-345` | D2 |
| 잡는 충돌 지정 | `SetGrabCollider` `Public/Interaction/IsdkGrabbableComponent.h:117`. 기본 `ColliderMode = FindStaticMesh` `:198`. `OnRegister`의 `SetupCollider` `Private/Interaction/IsdkGrabbableComponent.cpp:93-112` → vBoR이 `PreInitializeComponents`에서 덮어씀 | D2 |
| 메시 충돌 있는지 | `UStaticMesh::GetBodySetup` `Engine/Classes/Engine/StaticMesh.h:1240`, `UBodySetup::AggGeom` `Engine/Classes/PhysicsEngine/BodySetup.h:138`, `FKAggregateGeom::GetElementCount` `Engine/Classes/PhysicsEngine/AggregateGeom.h:72`, `GetCollisionTraceFlag` `PhysicsCore/Public/BodySetupCore.h:23`, `CTF_UseComplexAsSimple` `PhysicsCore/Public/BodySetupEnums.h:19` | D3 |
| 본보기 — 짐 | 메시 충돌 설정 `Interaction/VBoRCarryObject.cpp:60-67`, 잡는 충돌 `:114`, 충돌 검사 `:169-173`, 막힘만 셈 `:184-187` | D2·D3·E10 |
| 본보기 — 시험 | 충돌 없는 복제 메시 `P1Probe/P1CarryWorldTests.cpp:235-237`, 실제 손바닥 overlap·선택 `P2Probe/P2bAxisPartTests.cpp:304-332` | E3·E5 |
| 지금 부품 자리 | `AVBoRAxisPart` 생성 `Interaction/VBoRAxisPart.cpp:142-152`, 배치 `:164-177`, 잡기 설정 `:182-194` / 휠 `VBoRSteeringWheel.cpp:45-56`·`:68-72`·`:80-85` / 미러 `VBoRTwoAxisPart.cpp:34-40`·`:49-53`·`:76-78` / 노브 `VBoRKnob.cpp:35-43`·`:51-55`·`:75-76` | D1–D5 |

## 개정 이력

| 날짜 | 내용 |
|---|---|
| 2026-10-21 | 처음 작성(Claude) |
