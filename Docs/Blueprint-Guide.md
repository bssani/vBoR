# Blueprint 만드는 법 — 실제 차를 L_Main에 넣기

> 읽는 사람: 회사 PC에서 실제 차 CAD로 L_Main을 채우는 사람과 그 PC의 AI.
> 먼저 읽을 것: [Company-PC-Guide.md](Company-PC-Guide.md)(지금 상태, CAD에서 뽑을 값 §4).
> 기준: `main` 2026-10-21(PR #47 뒤). 코드 위치는 `Source/vBoR/` 아래다.

## 0. 상태 — 무엇이 확인됐고 무엇이 아닌가

- **지금 L_Main의 차는 근사 차 `AP3VehicleMock` 하나다.** 부품 약 20개를 C++ `BeginPlay`에서 만들고, 태그를 붙이고, 서로 잇는다(`P3Probe/P3VehicleMock.cpp`).
  - 이 문서는 그 일을 실제 차에서는 **Blueprint와 레벨 배치로** 하는 방법이다.
- **확인된 것은 문·손잡이 조립뿐이다(P2e).** 자동화 시험으로 확인했고, 에디터·VR 확인은 아직이다.
  - 메시로 잡기·밀기(P2f·P2g, §4.3)도 자동화로만 확인했다. 엔진 기본 메시로 시험했고, CAD 메시로는 아직이다.
- **나머지는 확인되지 않았다.** 부품 부모 클래스는 근사 차에서만 돌았고, BP 자식으로 만들어 레벨에 놓은 적은 아직 없다. 표의 "확인" 칸에 적었다.
  - 막히면 그 부품 하나만 Claude에게 알려 명세로 고친다. 부모 C++를 BP 로직으로 우회하지 않는다.
- **CAD 에셋과 그것을 쓰는 BP를 리포에 올릴지는 미정이다.** 정하기 전에는 커밋하지 않는다(Company-PC-Guide §5).

## 1. 큰 그림

```
L_Main
├─ World Settings: GameMode Override = BP_vBoRGameMode  (손 Pawn — §8)
├─ VBoRScenarioRunner  (Scenario = Main/Data/DA_Main_Scenario, 19단계)
├─ BP_Car_Core         차 기준점 BP — 정렬·좌석점·구역·태그 (§3)
├─ CAD 정적 메시        움직이지 않는 것은 BP로 만들지 않고 레벨에 그대로 (U-14)
├─ 부품 BP             문·손잡이·콘솔·글러브박스·레버·스티어링·미러·노브·버튼·리프트게이트·2열 시트 (§4)
└─ 레벨 배치            바닥 경로 2개, 키 테이블, 박스 받침·적재 영역 (§5)
```

- **시나리오는 태그로 부품을 찾는다.** 단계마다 "어떤 태그의 무엇"을 보므로, 실제 부품에 근사 차와 **같은 태그**를 붙이면 시나리오를 고치지 않아도 된다(§7 표).
- 손 또는 키보드 → 같은 명령 → 차량 상태 → 단계 판정이다(AGENTS.md 구조 원칙). BP에 로직을 넣지 않는다. **BP는 메시와 값만 바꾼다.**

## 2. 시작 전에

1. **좌표:** UE는 X 앞, Y 오른쪽, Z 위, cm다. CAD는 흔히 X가 뒤쪽이고 mm다. 가져올 때 축과 단위를 맞춘다(Company-PC-Guide §4 #1).
2. **차 원점 = 월드 원점, 지면 = 월드 Z 0으로 둔다.** 차를 옮길 일이 없다 — 4점 정렬이 Pawn을 옮긴다(P4h).
   - 지면을 0에 둘 수 없으면 정렬 컴포넌트의 `FloorWorldZCm`에 지면 높이(cm)를 넣는다. 다만 경로·키 테이블·받침도 그 높이에 둬야 하고, 이 경우는 시험하지 않았다.
3. **근사 차를 지운다.** 레벨에 정렬 컴포넌트가 **정확히 하나**여야 흐름이 시작된다("정렬 컴포넌트 1개 필요", `Interaction/VBoRScenarioRunner.cpp:221`).
   - 실제 차를 넣는 레벨에서 `AP3VehicleMock`을 지운다(L_Main만 — `Development/P3/L_P3_Flow`는 그대로 둔다).
4. **폴더:** 산출물은 `Content/Main/` 아래에 둔다. 예: `Main/Car/Core/`, `Main/Car/Parts/`, CAD는 `Main/Car/CAD/`. `Development/` 에셋을 참조하지 않는다(AGENTS.md 구조 원칙).

## 3. 차 기준점 BP — `BP_Car_Core`

부모는 `Actor`, 위치는 월드 원점이다. 메시는 넣지 않는다. 아래 컴포넌트만 넣는다(Add → 이름으로 검색).

| 컴포넌트(클래스) | 개수 | 태그 | 자리·값 | 쓰는 곳 | 근사 차 예 |
|---|---|---|---|---|---|
| `VBoR Vehicle Alignment` (`UVBoRVehicleAlignmentComponent`) | 1 | — | `bHeightFromFloor` 켬, `FloorWorldZCm` 0 | 정렬·좌석 전환 | `P3VehicleMock.cpp:52` |
| `VBoR Align Point` (`UVBoRAlignPointComponent`) | 4 | — | 실물 좌석의 단단한 점 4개 CAD 자리. `Order` 1–4, `Label`(진행자 창에 보임, 예 `Front-left rail end`) | 4점 정렬(P4h, U-18) | `:40-46` |
| `VBoR Viewpoint` (`UVBoRViewpointComponent`) | 3 | `Car.DriverView` / `Car.PassengerView` / `Car.RearView` | 운전석·동승석·2열 **SgRP** | 14·15 좌석 전환(SgRP 차이, P4h D6) | `:48-49`(근사 차는 눈 위치 — 아래 주의) |
| `VBoR Seat Zone` (`UVBoRSeatZoneComponent`) | 1 | `Car.DriverSeat` | 앉은 머리가 들어오는 상자. 근사 차는 설계 눈 아래 7.5 cm 중심, 반크기 25×25×32.5 cm | 10번 "좌석에 앉아보세요"(1.5 s) | `:51`, `:89` |
| Box (`UBoxComponent`) | 1 | `Car.Footprint` | 차체 바닥 윤곽 + 20 cm. **Collision 없음, Hidden in Game** | 바닥 경로가 차를 지나지 않는지 검사 | `:60-62`, `:78-79` |
| Scene 또는 작은 메시 | 1 | `Car.Start` | 차 앞 밖(근사 차는 앞 끝 +250 cm), 차를 바라보게(Yaw 180) | 시작 위치 기록(P3b) | `:63-64`, `:108` |
| `VBoR Eye Point` (`UVBoREyePointComponent`) | 0–1 | — | 운전석 설계 눈 | **선택.** 눈 방식 정렬(집 시험 대체)과 참고 표시. 없으면 기준 좌석점은 `Car.DriverView` | `:47` |
| `VBoR Approach Zone` | 0–1 | `Car.Approach` | 차 둘레 접근 구역 | **지금 단계에서는 안 씀** | `:50`, `:87` |

**주의:**
- **좌석점 셋은 같은 종류의 점이어야 한다.** 회사 차는 셋 다 SgRP이고 운전석에 `Car.DriverView`를 붙인다. 근사 차는 `Car.DriverView` 없이 눈 위치를 쓴다. 섞으면 좌석 사이 거리가 틀어진다.
- `Car.DriverView`도 눈 점도 없으면 좌석 전환이 거절된다(`Driver seat reference missing`, P4h R1-1).
- 앉음 구역은 머리 기준이다. 실제 차에서는 설계 눈 위치(있으면)나 SgRP 위 앉은 머리 높이로 둔다. 너무 좁으면 키 큰 사람이 "앉음"으로 안 잡힌다. 넓이는 평가 담당자와 정할 값이다.
- 정렬 점은 에디터에서는 작은 구와 노란 번호로 보이고, 게임에서는 진행자가 `P`를 누를 때만 보인다.

## 4. 부품 BP

### 4.1 공통 규칙

1. Content Browser 우클릭 → **Blueprint Class** → All Classes에서 부모 C++ 클래스를 고른다(아래 표). 이름 예: `BP_Door_FL`, `BP_DoorHandle_FL_Ext`.
2. **BP를 레벨에 놓을 때 Actor 위치는 차 원점(월드 원점)이다.** CAD 메시는 피벗이 차 원점에 묶여 있으므로, 메시를 원래 자리에 둔 채 **축 컴포넌트만** 경첩·회전축 자리로 옮긴다(U-09, P2b D3). 근사 차가 같은 방식이다("Actor 원점은 차 원점, 메시·축만 부품 자리로", `P3VehicleMock.cpp:145`).
3. **메시:** 부품 메시 컴포넌트(표의 "메시")에 CAD 메시를 넣는다. 문에 붙어 같이 도는 것(유리·미러·트림)은 하나로 merge해서 넣는다(U-14). 손잡이처럼 따로 움직이는 것은 따로 BP다.
4. **`MeshRestFromRoot`는 차 원점에서 본 메시 자리다.** 메시 피벗이 차 원점 그대로면 **단위 변환**(위치 0, 회전 0, 크기 1)으로 둔다.
   - 기본값은 근사 상자용 크기·위치다. 문·패널의 "설정 검사"가 이걸 경고한다("Non-cube DoorMesh still uses mock MeshRestFromRoot", `Interaction/VBoRHingedPanel.cpp:292`).
   - 메시 충돌로 잡는 부품은 피벗을 옮기고, 옮긴 만큼을 여기에 넣는다(§4.3).
5. **축 컴포넌트:** 표의 "축"에 CAD의 경첩·회전 중심 위치와 방향을 넣는다. 방향 규칙은 근사 차 줄(표의 "예")의 주석에 있다. 처음 하나는 그 줄을 그대로 따라 하고, Play로 열리는 방향을 확인한 뒤 나머지를 맞춘다.
6. **태그**는 Actor의 Tags(Details → Actor → Tags)에 넣는다. **`SpectatorName`**은 TV 띠에 보일 영어 이름이다(예 `Driver Door`).
7. BP에 Event Graph 로직을 넣지 않는다. 동작이 모자라면 부모 C++를 고친다(명세).

### 4.2 부품표

| 부품(INT) | 부모 클래스 | 메시 / 축 | 꼭 넣을 값 | 태그·연결 | 근사 차 예 | 확인 |
|---|---|---|---|---|---|---|
| 운전석 문 | `AVBoRHingedDoor` | `DoorMesh` / `HingePivot`(+Z가 경첩축) | `MaxAngleDeg`, `bUseLatch`, 미는 면 — 메시 표면(`bPushOnMeshSurface`, §4.3) 또는 미는 면 상자(`bUsePushFaceBox`) | `Car.DriverDoor` — 07·08·11·18·20단계 | `:141-144` | 자동화(P2e·P2g) |
| 문 손잡이 바깥·안쪽(001·002) | `AVBoRAxisPart` | `PartMesh` / `AxisPivot` + `Axis`·`bReverse` | **`DoorHandleSide` = 바깥/안쪽**, `MaxAngleDeg`, `Release`, 메시로 잡기(`bGrabWithMesh`, §4.3) | 문 BP 밑에 붙인다(§6.1) | `:146-162` | 자동화(P2e·P2f) |
| 콘솔박스(005) | `AVBoRHingedPanel` | `DoorMesh` / `HingePivot` | `bUseLatch` 켬, `bAllowPalmOpening` 끔, `MaxAngleDeg` 100 | 손잡이 자리(`OutsideHandle` 상자) | `:164-179` | 안 됨 |
| 글러브박스(006) | `AVBoRHingedPanel` + 손잡이 `AVBoRAxisPart` | 같음 | `bUseLatch` 켬, `bAllowPalmOpening` 끔, 손잡이 `Release` = Return | 손잡이는 문과 같은 방식(§6.1, 바깥) | `:181-198` | 안 됨 |
| 기어 레버(008) | `AVBoRAxisPart` | `PartMesh` / `AxisPivot`, `Axis` X | `Release` = Detent, `DetentAnglesDeg`, 시작 1, pinch 켬·손바닥 끔·밀기 켬(U-13) | 끝 버튼은 §6.2 | `:200-209` | 안 됨 |
| 기어 끝 버튼 | `AVBoRPokeButton` | `ButtonMesh` | `HitAreaScale` 1 | 레버 축을 따라 움직여야 함(§6.2) | `:211-216` | 안 됨 |
| 방향지시 레버(009) | `AVBoRAxisPart` | 같음, 왼쪽이면 `bReverse` | Detent + `DetentNudgeDeg`(자동 복귀) | — | `:218-228` | 안 됨 |
| 스티어링 휠 | `AVBoRSteeringWheel` | `WheelMesh` / `AxisPivot` | `LimitAngleDeg`, 잡는 반지름 최소·최대 | — | `:230-241` | 안 됨 |
| 룸미러(U-12: 반사 없음) | `AVBoRTwoAxisPart` | `PartMesh` / `Pivot` | `YawLimitDeg`, `PitchLimitDeg` | 거울면은 CAD 재질 그대로 | `:242-256` | 안 됨 |
| 콘솔 노브 | `AVBoRKnob` | `KnobMesh` / `Pivot`(+Z가 윗면·회전축) | `TwistGain`, `StepDeg` | — | `:258-267` | 안 됨 |
| 13번 도달 버튼 | `AVBoRPokeButton` | `ButtonMesh` | **`HitAreaScale` 1**(13번은 판정 확대 없음, AGENTS.md) | `Car.ReachButton` — 13단계 | `:268-273` | 안 됨 |
| 리프트게이트(011) | `AVBoRPoweredPanel` | `PanelMesh` / `HingeLocationCm` | `OpenAngleDeg`, `DelaySeconds`, 열기·닫기 버튼 자리(`OpenButtonFromHinge`·`CloseButtonFromHinge`) | `Car.Liftgate` — 04·06단계. 버튼은 스스로 만듦 | `:350-357` | 안 됨 |
| 2열 등받이 | `AVBoRFoldingSeat` | `SeatMesh` / `HingeLocationCm` | `FoldAngleDeg` | `Car.RearSeat`. 진행자 F·U가 모든 접는 시트를 찾음 | `:128-132` | 안 됨 |

- 콘솔박스·글러브박스도 `bPushOnMeshSurface`를 켤 수 있다. 1축 부품(레버 포함)·스티어링 휠·룸미러·노브도 `bGrabWithMesh`를 켤 수 있다. 방법은 §4.3이다.

### 4.3 메시로 잡기·밀기 — 선택(P2f·P2g, 자동화로만 확인)

기본은 상자로 판정한다. 잡는 상자(`GrabBox`)는 메시 bounds에 여유를 더한 것이다. 미는 면은 bounds 평면(또는 미는 면 상자)이다. 그래서 곡면이나 비스듬한 부품은 메시 앞의 허공에서도 잡히거나 밀린다. 아래 옵션을 켜면 메시 모양 그대로 판정한다. 둘 다 기본은 꺼짐이다.

**잡기 — `메시 충돌로 잡기`(`bGrabWithMesh`).** 1축 부품·스티어링 휠·룸미러·노브의 Input 칸에 있다.
1. **메시 피벗을 손이 잡는 부분 가운데로 옮긴다.**
   - 이유: ISDK는 손바닥에 닿은 후보 중 충돌 컴포넌트 **원점**이 가장 가까운 것을 고른다(`Plugins/MetaXRInteraction/Source/OculusInteraction/Private/Interaction/GrabDetectors/IsdkHandGrabDetector.cpp:397-460`). CAD 메시는 피벗이 모두 차 원점이다. 그대로 두면 휠과 레버처럼 가까운 부품 사이에서 무엇이 잡힐지 정해지지 않는다.
   - 옵션을 켰는데 피벗이 Actor 원점에서 1 cm 안이면 Play 때 경고한다("grab mesh pivot is at the actor origin").
   - 회전축과는 상관없다. 도는 중심은 여전히 축 컴포넌트(`AxisPivot` 등)다(U-09, §4.1-2).
   - **방법(에디터 — 확인 안 함):**
     1. 임시 레벨의 원점에 CAD 메시를 하나 놓는다.
     2. Modeling 모드 → Edit Pivot으로 피벗을 잡는 부분 가운데로 옮기고 Accept한다.
     3. 메시가 제자리에 그대로 보여야 한다. 그때 그 Actor의 Location(회전도 했으면 Rotation)이 옮긴 만큼이다. 그 값을 적고 임시 Actor는 지운다.
   - 메시 에셋 자체가 바뀐다. 같은 메시를 레벨에 따로 놓은 곳이 있으면 거기도 옮겨진다. 잡는 부품의 메시만 옮긴다.
2. 부품 BP의 **`MeshRestFromRoot`에 1의 위치(와 회전)를 넣는다.** 그러면 메시가 원래 자리로 돌아온다. 크기는 1이다.
3. **메시 에셋 충돌** — Static Mesh 에디터 → Collision Complexity:
   - **Use Complex Collision As Simple**이 가장 쉽다. 렌더 삼각형 그대로 쓴다.
   - simple 충돌을 만든다면 box·convex를 쓴다. 구·캡슐이 하나라도 있으면, 크기를 축마다 다르게 준 메시는 충돌 전체가 가장 작은 축으로 줄어든다(엔진 `Engine/Private/PhysicsEngine/BodyInstance.cpp:2133-2153`).
   - 둘 다 없으면 경고하고 상자로 잡는다("grab mesh has no simple collision").
4. 옵션을 켠다. **BP에서 메시의 Collision Presets는 바꾸지 않는다.** Play 때 코드가 정한다. 켜면 Query만 하고 WorldDynamic overlap만 받으며, 끄면 충돌이 없다.
- 그대로인 것: 휠의 잡는 반지름 검사, 레버 손가락 밀기(메시 bounds 기준), hover·잡기 색.

**밀기 — `메시 표면으로 밀기`(`bPushOnMeshSurface`).** 문·콘솔·글러브박스(`AVBoRHingedPanel`)의 Push face 칸에 있다.
1. **피벗은 옮기지 않는다.** `DoorMesh`는 차 원점 피벗 그대로 두고, `MeshRestFromRoot`는 단위 변환이다. 손 점마다 그 자리의 면을 trace로 읽기 때문이다(P2g D2).
2. **메시 에셋에서 `Double Sided Geometry`를 켠다**(Static Mesh 에디터 → Collision).
   - 복합 trace는 기본으로 뒷면을 무시한다. 그래서 한 장짜리 겉판이나 면 방향이 뒤집힌 곳에서는 trace가 빠진다(P2g 리뷰 참고 1).
3. 충돌은 따로 만들지 않아도 된다. 엔진이 렌더 삼각형으로 복합 충돌을 만든다.
4. 옵션을 켜고 **"설정 검사"**를 누른다.
   - "DoorMesh has no traceable collision"이 나오면 trace가 안 되는 것이다. 그러면 bounds로 민다.
   - 켜면 미는 면 상자(`bUsePushFaceBox`)는 쓰지 않는다("PushFaceBox ignored …" 알림). 두께 경고도 내지 않는다.
- merge한 사이드미러 같은 돌출도 미는 면이 된다. 미러를 밀면 문이 밀린다.
  - 빼려면 그 메시의 Collision Complexity를 Use Simple Collision As Complex로 두고, 미러가 없는 simple 충돌을 만든다(P2g §1).
- Nanite 메시는 복합 충돌이 원본보다 거친 대체 메시로 만들어질 수 있다(확인 안 함). 밀리는 면이 보이는 면과 다르면 알려 달라.
- 문에 붙은 손잡이는 위 "잡기"대로 따로 한다.

## 5. 레벨에 놓는 것

| 무엇 | 클래스 | 놓는 법 | 태그·연결 | 근사 차 예 |
|---|---|---|---|---|
| 바닥 경로 — 차 둘레 | `AVBoRFloorRoute` | 레벨에 놓고 Spline 점을 바닥에 찍음(점 2개 이상) | Actor 태그 `Route.AroundCar` — 03단계 | `:133-136` |
| 바닥 경로 — 운전석 문까지 | `AVBoRFloorRoute` | 같음. 끝 점은 문 앞 | `Route.ToDriverDoor` — 07단계 | `:137-140` |
| 앞 키 테이블 | `AVBoRKeyTable` | 바닥에 놓음 | 키 태그는 §6.3 | `:117`, `:283-298` |
| 박스 적재 영역 | `AVBoRLoadTarget` | 트렁크 안. `LoadingRegion` 상자를 트렁크 안 공간 크기로 | — | `:358-364` |
| 박스 받침 | `AVBoRBoxStand` | 차 뒤 동승석 쪽 바닥 | 태그 `Car.BoxStand`, **`LoadTarget`에 위 적재 영역을 고름**(§6.4) | `:365-368` |

- **경로는 `Car.Footprint`를 지나면 안 된다.** 흐름 시작 때 검사해서, 지나면 이유를 남기고 시작하지 않는다(`VBoRScenarioRunner.cpp:255`·`:265`).
- 경로 점은 나중에 실제 방 크기에 맞춰 옮긴다. 플레이 영역 밖이면 경로를 숨긴다(P3b D6).

## 6. 연결 규칙

### 6.1 손잡이 → 문·콘솔·글러브박스 (확인됨 — 자동화, P2e·U-14)

- 손잡이 BP의 **`DoorHandleSide`**를 바깥 또는 안쪽으로 정한다. 기어 레버처럼 판과 상관없는 1축 부품은 "없음"이다.
- 레벨 Outliner에서 손잡이를 **문(판) Actor 밑으로 끌어다 붙인다.** Play 때 판이 자기 밑의 손잡이를 찾아 연결하고, 경첩 축(`HingePivot`)에 붙여 같이 돌게 한다(`Interaction/VBoRHingedPanel.cpp:200-210`, `:328`).
  - Child Actor 컴포넌트로 넣는 경우에는 그 컴포넌트를 BP 안에서 **`HingePivot` 밑에** 둔다. 아니면 연결하지 않고 경고한다(`:236-238`).
- 손잡이 하나를 두 판이 같이 쓰면, 먼저 연결한 판이 갖는다(경고).
- 판 Details의 **"설정 검사"** 버튼(Hinged panel → Setup)이 Play 전에 같은 판정을 보여 준다. 판마다 한 번 누른다.

### 6.2 기어 끝 버튼 → 레버 (확인 안 됨)

- 근사 차는 버튼을 레버의 **`AxisPivot`**에 코드로 붙인다(`P3VehicleMock.cpp:216`). 레버가 돌면 버튼이 같이 간다.
- 레벨 Outliner로 붙이면 레버 Actor의 **루트**에 붙어서 레버가 돌아도 버튼이 제자리다. 그래서 다음처럼 한다.
  - 기어 레버 BP 안에 **Child Actor 컴포넌트**(클래스 = 버튼 BP)를 넣고 `AxisPivot` 밑에 둔다.
  - Play해서 레버를 움직일 때 버튼이 따라오는지 본다. 안 되면 Claude에게 알린다(명세로 고침).
- 이 버튼은 단계와 연결하지 않는다(자유모드 부품, P5b D8).

### 6.3 키 태그 (확인 안 됨)

- 키는 키 테이블이 Play 때 만든다(`KeyClass`, `Interaction/VBoRKeyTable.cpp:42`). 그래서 레벨에서 키에 태그를 직접 달 수 없다.
- **키 BP**(`AVBoRKeyFob` 자식)를 만들고 Class Defaults의 Actor Tags에 `Car.FrontKey`를 넣는다. 앞 키 테이블의 `KeyClass`를 그 BP로 바꾼다.
- 01단계(키 집기, 표시도 이 태그)와 02단계(키 버튼 열림·잠김)가 이 태그를 쓴다.
- 근사 차는 키 버튼에도 태그(`Car.FrontKey.Unlock`·`.Lock`)를 붙이지만, 지금 단계는 쓰지 않는다.

### 6.4 박스 받침 → 적재 영역 (확인 안 됨)

- `AVBoRBoxStand`의 `LoadTarget`은 **레벨에 놓은 인스턴스에서** 고른다(Details의 스포이트). BP 기본값으로는 레벨 Actor를 고를 수 없다.
- 05단계(박스 적재)는 진행자 확인으로 끝난다. 박스가 들어갔는지는 진행자가 본다(U-15).

## 7. 시나리오 태그 표 — `Main/Data/DA_Main_Scenario`

| 단계 | 판정 | 대상 태그 | 대상 종류 |
|---|---|---|---|
| 01 | 키를 쥠(표시도 키) | `Car.FrontKey` | 키(`AVBoRKeyFob`) |
| 02 | 키 버튼 열림·잠김 | `Car.FrontKey` | 키 |
| 03 | 확인(경로 `Route.AroundCar`) | — | — |
| 04·06 | 리프트게이트 열림·닫힘 | `Car.Liftgate` | `AVBoRPoweredPanel` |
| 05·09·16·19 | 진행자 확인 | — | — |
| 07 | 손잡이로 풀림(경로 `Route.ToDriverDoor`) | `Car.DriverDoor` | `AVBoRHingedDoor` |
| 08·18 | 문 45° 이상 열림 | `Car.DriverDoor` | 같음 |
| 10 | 앉음 1.5 s | `Car.DriverSeat` | `UVBoRSeatZoneComponent` |
| 11·20 | 문 닫혀 잠김 | `Car.DriverDoor` | 같음 |
| 12 | 확인 | — | — |
| 13 | 도달 버튼 누름 | `Car.ReachButton` | `AVBoRPokeButton` |
| 14·15 | 좌석 시점 | `Car.PassengerView`·`Car.RearView` | `UVBoRViewpointComponent` |

- 태그가 없거나 종류가 다르면 흐름이 시작되지 않는다. Output Log와 VR 진행자 정보(`I`)에 `시작 차단:` 아래 `<단계>: 태그 <이름> 종류 불일치 또는 없음`으로 보인다(`VBoRScenarioRunner.cpp:238`·`:293`).
- 시나리오 에셋은 하나만 쓴다. 근사 차와 실제 차가 같은 태그를 쓰므로 복사하지 않는다.

## 8. 손 메시 바꾸기 — `BP_vBoRPawn`

- `Main/Core/BP_vBoRPawn` → Class Defaults → **손 표시**: `LeftHandMesh`·`RightHandMesh`·`LeftHandMaterial`·`RightHandMaterial`. 비우면 Meta 기본 손이다(P4f).
- **본 이름이 Meta 손(`XRHand_Palm`·`XRHand_Wrist`…)과 같아야** 손가락이 따라 움직인다. 다르면 기본 손으로 돌아가고 Output Log에 Warning이 한 번 뜬다.
- 재질만 넣으면 그 손의 모든 재질 칸에 들어간다.

## 9. 넣은 뒤 확인 — 순서대로

1. **판마다 "설정 검사"**를 눌러 Output Log에 문제가 없는지 본다(§6.1).
2. **Play(PIE) 한 번** — Output Log에서 찾을 것:
   - `Template Mismatch during attachment`(ensure)가 없을 것. P4f에서 네이티브 부모의 BP 자식이 이것을 냈다. 정렬 점(`UVBoRAlignPointComponent`)도 같은 구조라 처음 한 번은 꼭 본다(P4h 리뷰 참고 2).
   - `P2e setup:` 경고가 없을 것(손잡이 연결).
   - 메시로 잡기·밀기를 켠 부품에서 `grab mesh …`·`DoorMesh has no traceable collision` 경고가 없을 것(§4.3).
3. **흐름 시작 막힘이 없을 것:** Output Log(또는 VR 진행자 정보 `I`)에 `시작 차단:`이 없어야 한다(§7). "정렬 컴포넌트 1개 필요"면 근사 차가 남아 있다(§2-3).
4. **진행자 창**(툴바 `Operator`):
   - **4점 정렬:** 실물 좌석 점 4개를 `P`로 짚고 → 차이(cm)·높이 차이 확인 → `S` 저장 → 표시를 다시 짚어 거리 확인 → `A`로 시작(P4h). 높이 경고가 뜨면 실물 좌석 높이를 확인한다(U-17).
   - **좌석 전환:** `1`·`2`로 동승석·2열에 가서 SgRP 차이만큼 옮겨졌는지, `Home`으로 돌아오는지 본다.
5. **부품마다:** 열리는 방향·최대 각, 손잡이가 문과 같이 도는지(§6.1), 기어 버튼이 레버를 따라오는지(§6.2), 리프트게이트 버튼, 2열 F·U, 키를 집고 버튼 누르기(§6.3).
   - 메시로 잡는 부품은 hover가 메시에 닿을 때만 켜지는지 본다. 얇은 손잡이가 잡히는지, 가까운 부품끼리 맞게 골라지는지도 본다. 콘솔 변수 `Meta.InteractionSDK.DebugInteractionVisuals 1`은 잡는 충돌의 bounds 상자를 그린다. 메시 충돌 모양은 뷰포트 Show → Collision으로 본다.
   - 메시 표면으로 미는 문은 바깥 면과 안쪽 트림을 밀어 본다. 보이는 면에 닿을 때 밀리기 시작하는지, 돌출부와 창문 자리에서 어떻게 되는지 본다.
6. **VR(Quest 3 Link):** 계획서 §7 시험 ID로 기록한다. 확인 전까지 상태는 `not-tested`다.

막힌 곳은 부품 이름, Output Log 줄, 진행자 창 이유를 그대로 적어 Claude에게 넘긴다. Claude가 명세로 고친다.
