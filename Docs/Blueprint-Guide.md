# Blueprint 가이드 — 실제 차 CAD를 L_Main에 넣기

> 읽는 사람: 회사 PC에서 실제 차 CAD로 L_Main을 채우는 사람과 그 PC의 AI.
> 먼저 읽을 것: [Company-PC-Guide.md](Company-PC-Guide.md)(지금 상태, CAD에서 뽑을 값 §4).
> 기준: `main` 2026-10-21(PR #59 뒤 — L_Main이 BP 차, 선바이저·시동 버튼·사이드미러 추가, 부품별 소리 칸). 코드 위치는 `Source/vBoR/` 아래다.

## 0. 상태 — 무엇이 확인됐고 무엇이 아닌가

- **L_Main의 차는 이미 BP 틀이다(P4i, 2026-10-21).**
  - `Content/Main/Car/`에 BP 23개가 있다(§3·§4.2 표의 BP 이름). 메시는 모두 엔진 Cube다.
    - 18개(P4i)는 근사 차와 같은 값이다.
    - 선바이저 2개(P5p)와 시동 버튼·사이드미러 3개(P5q)는 근사 차에 없는 부품이다. 값은 명세에 적은 근사 값이다.
  - L_Main에 차 원점으로 놓여 있다. 근사 판·근사 트렁크·"근사 형상" 표시 글자는 Outliner **`Approx` 폴더**에 모여 있다.
  - 근사 차 C++(`AP3VehicleMock`)는 이제 `Development/P3/L_P3_Flow`에만 있다.
  - **회사에서 할 일:** `Approx`를 지우고(§2-3), CAD 정적 메시를 넣고, BP마다 메시와 축 자리를 CAD로 바꾼다(§3·§4). BP를 새로 만들 필요는 없다.
- **확인된 것 — 자동화·PIE, VR 전:**
  - BP 18개가 근사 차와 같은 자리·설정이다(P4i E2).
  - 손잡이 연결(P2e), 기어 버튼 Child Actor, 키 BP와 태그, 받침 → 적재 영역(P4i E3·E4).
  - L_Main PIE에서 흐름 시작 차단이 없다(P4i E6, 정렬 없이 `O`로 시작).
  - 메시로 잡기·밀기(P2f·P2g, §4.3)는 엔진 기본 메시로만 자동화 시험했다.
  - 선바이저 잡기·밀기와 놓은 자리 유지(P5p), 시동 버튼 → 전원 → 사이드미러 접기·펴기(P5q, §6.5).
  - 소리 칸(P5r·P5s, §4.4): 이벤트마다 재생 요청이 나는 수와 frame만 시험했다. 소리는 끈 채로 돌렸다.
- **아직 안 된 것:** VR(Quest 3) 확인 전부, CAD 메시로 바꾼 상태, 버튼 두 개의 CAD 피벗 처리(§4.2 버튼 줄), 실제로 들리는 소리·타이밍·위치감.
  - 막히면 그 부품 하나만 Claude에게 알려 명세로 고친다. 부모 C++를 BP 로직으로 우회하지 않는다.
- **CAD를 넣은 뒤의 BP·L_Main을 리포에 올릴지는 미정이다.** 지금 BP 틀은 CAD가 없어서 리포에 있다. CAD 메시를 넣으면 BP와 L_Main이 CAD를 참조하게 된다. 정하기 전에는 커밋하지 않는다(Company-PC-Guide §5).

## 1. 큰 그림

```
L_Main
├─ World Settings: GameMode Override = BP_vBoRGameMode  (손 Pawn — §8)
├─ VBoRScenarioRunner  (Scenario = Main/Data/DA_Main_Scenario, 19단계)
├─ BP_Car_Core         차 기준점 BP — 정렬·좌석점·구역·태그 (§3)
├─ 부품 BP             문·손잡이·콘솔·글러브박스·레버·스티어링·룸미러·노브·버튼·리프트게이트·2열 시트
│                      ·선바이저·시동 버튼·사이드미러 (§4)
├─ 레벨 배치            바닥 경로 2개, 키 테이블 2개, 박스 받침·적재 영역 (§5)
├─ Approx 폴더         근사 판·근사 트렁크·표시 글자 — 회사에서 통째로 지움 (§2-3)
└─ CAD 정적 메시        (회사) 움직이지 않는 것은 BP로 만들지 않고 레벨에 그대로 (U-14)
```

- **시나리오는 태그로 부품을 찾는다.** 단계마다 "어떤 태그의 무엇"을 보므로, 실제 부품에 근사 차와 **같은 태그**를 붙이면 시나리오를 고치지 않아도 된다(§7 표).
- 손 또는 키보드 → 같은 명령 → 차량 상태 → 단계 판정이다(AGENTS.md 구조 원칙). BP에 로직을 넣지 않는다. **BP는 메시와 값만 바꾼다.**

## 2. 시작 전에

1. **좌표:** UE는 X 앞, Y 오른쪽, Z 위, cm다. CAD는 흔히 X가 뒤쪽이고 mm다. 가져올 때 축과 단위를 맞춘다(Company-PC-Guide §4 #1).
2. **차 원점 = 월드 원점, 지면 = 월드 Z 0으로 둔다.** 차를 옮길 일이 없다 — 4점 정렬이 Pawn을 옮긴다(P4h).
   - 지면을 0에 둘 수 없으면 정렬 컴포넌트의 `FloorWorldZCm`에 지면 높이(cm)를 넣는다. 다만 경로·키 테이블·받침도 그 높이에 둬야 하고, 이 경우는 시험하지 않았다.
3. **`Approx` 폴더를 먼저 통째로 지운다.** BP 메시를 바꾸기 전에 한다.
   - CAD 차체와 겹치기 때문이다.
   - 자동화 시험 `vBoR.P4i.E2`와 E4 일부는 `Approx`가 있을 때만 BP를 근사 차와 비교한다. 지우지 않고 BP를 바꾸면 그 시험이 실패한다(P4i R1-1).
   - **근사 트렁크가 박스 착지 충돌을 주고 있었다**(`P1Probe/P1TrunkMock.cpp:25-26`). 그래서 CAD 트렁크 바닥·벽 메시에는 막는 충돌이 있어야 박스가 착지한다. 메시 충돌은 simple이나 Use Complex Collision As Simple로 두고, 프리셋은 막음(BlockAll 등)으로 둔다(CAD로는 확인 안 함 — §9-5에서 본다).
   - 정렬 컴포넌트는 `BP_Car_Core`에 하나 있다. 레벨에 정확히 하나여야 흐름이 시작되므로 더 넣지 않는다("정렬 컴포넌트 1개 필요", `Interaction/VBoRScenarioRunner.cpp:221`).
4. **폴더:** 산출물은 `Content/Main/` 아래에 둔다. 예: `Main/Car/Core/`, `Main/Car/Parts/`, CAD는 `Main/Car/CAD/`. `Development/` 에셋을 참조하지 않는다(AGENTS.md 구조 원칙).

## 3. 차 기준점 BP — `BP_Car_Core`

**이미 있다:** `Content/Main/Car/Core/BP_Car_Core`. 부모는 `Actor`이고, L_Main의 월드 원점에 놓여 있다. 메시는 넣지 않는다.
- 아래 컴포넌트가 근사 차 값으로 들어 있다. BP를 열어 자리를 CAD 값으로 옮긴다.
- 근사 차와 달리 `Car.DriverView`도 들어 있다. 지금은 눈 점과 같은 자리다. 회사에서는 세 시점을 각 좌석 SgRP로 옮긴다. 눈 점은 설계 눈으로 옮기거나 지운다(선택).

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
| `VehiclePower` (`UVBoRVehiclePowerComponent`) | **정확히 1** | — | 넣을 값 없음 | 시동 버튼·진행자 `E`·사이드미러(§6.5) | 근사 차에 없음(P5q) |

**주의:**
- **좌석점 셋은 같은 종류의 점이어야 한다.** 회사 차는 셋 다 SgRP이고 운전석에 `Car.DriverView`를 붙인다. 근사 차는 `Car.DriverView` 없이 눈 위치를 쓴다. 섞으면 좌석 사이 거리가 틀어진다.
- `Car.DriverView`도 눈 점도 없으면 좌석 전환이 거절된다(`Driver seat reference missing`, P4h R1-1).
- 앉음 구역은 머리 기준이다. 실제 차에서는 설계 눈 위치(있으면)나 SgRP 위 앉은 머리 높이로 둔다. 너무 좁으면 키 큰 사람이 "앉음"으로 안 잡힌다. 넓이는 평가 담당자와 정할 값이다.
- 정렬 점은 에디터에서는 작은 구와 노란 번호로 보이고, 게임에서는 진행자가 `P`를 누를 때만 보인다.

## 4. 부품 BP

### 4.1 공통 규칙

1. **이미 있는 BP를 연다**(§4.2 표의 BP 이름, `Content/Main/Car/Parts/`).
   - 값은 근사 차 값이고 메시는 엔진 Cube다. 메시·축·값만 바꾼다.
   - 표에 없는 부품을 더할 때만 Content Browser 우클릭 → **Blueprint Class** → All Classes에서 부모 C++ 클래스를 골라 새로 만든다.
2. **BP를 레벨에 놓을 때 Actor 위치는 차 원점(월드 원점)이다.** CAD 메시는 피벗이 차 원점에 묶여 있으므로, 메시를 원래 자리에 둔 채 **축 컴포넌트만** 경첩·회전축 자리로 옮긴다(U-09, P2b D3). 근사 차가 같은 방식이다("Actor 원점은 차 원점, 메시·축만 부품 자리로", `P3VehicleMock.cpp:145`).
3. **메시:** 부품 메시 컴포넌트(표의 "메시")에 CAD 메시를 넣는다. 문에 붙어 같이 도는 것(유리·미러·트림)은 하나로 merge해서 넣는다(U-14). 손잡이처럼 따로 움직이는 것은 따로 BP다.
4. **`MeshRestFromRoot`는 차 원점에서 본 메시 자리다.** 메시 피벗이 차 원점 그대로면 **단위 변환**(위치 0, 회전 0, 크기 1)으로 둔다.
   - 지금 BP의 값은 근사 상자용 크기·위치다. CAD 메시로 바꿀 때 같이 바꾼다. 문·패널의 "설정 검사"가 바꾸지 않은 것을 경고한다("Non-cube DoorMesh still uses mock MeshRestFromRoot", `Interaction/VBoRHingedPanel.cpp:292`).
   - 메시 충돌로 잡는 부품은 피벗을 옮기고, 옮긴 만큼을 여기에 넣는다(§4.3).
5. **축 컴포넌트:** 표의 "축"에 CAD의 경첩·회전 중심 위치와 방향을 넣는다. 방향 규칙은 근사 차 줄(표의 "예")의 주석에 있다. 처음 하나는 그 줄을 그대로 따라 하고, Play로 열리는 방향을 확인한 뒤 나머지를 맞춘다.
6. **태그**는 Actor의 Tags(Details → Actor → Tags)에 넣는다. **`SpectatorName`**은 TV 띠에 보일 영어 이름이다(예 `Driver Door`).
7. BP에 Event Graph 로직을 넣지 않는다. 동작이 모자라면 부모 C++를 고친다(명세).

### 4.2 부품표

| 부품(INT) | 부모 클래스 | 메시 / 축 | 꼭 넣을 값 | 태그·연결 | 근사 차 예 | 확인 |
|---|---|---|---|---|---|---|
| 운전석 문 — `BP_Door_FL` | `AVBoRHingedDoor` | `DoorMesh` / `HingePivot`(+Z가 경첩축) | `MaxAngleDeg`, `bUseLatch`, 미는 면 — 메시 표면(`bPushOnMeshSurface`, §4.3) 또는 미는 면 상자(`bUsePushFaceBox`) | `Car.DriverDoor` — 07·08·11·18·20단계 | `:141-144` | 자동화(P2e·P2g·P4i) |
| 문 손잡이 바깥·안쪽(001·002) — `BP_DoorHandle_FL_Ext`·`_Int` | `AVBoRAxisPart` | `PartMesh` / `AxisPivot` + `Axis`·`bReverse` | **`DoorHandleSide` = 바깥/안쪽**, `MaxAngleDeg`, `Release`, 메시로 잡기(`bGrabWithMesh`, §4.3) | 문 BP 밑에 붙인다(§6.1) | `:146-162` | 자동화(P2e·P2f·P4i) |
| 콘솔박스(005) — `BP_ConsoleLid` | `AVBoRHingedPanel` | `DoorMesh` / `HingePivot` | `bUseLatch` 켬, `bAllowPalmOpening` 끔, `MaxAngleDeg` 100 | 손잡이 자리(`OutsideHandle` 상자) | `:164-179` | 자동화(P4i) |
| 글러브박스(006) — `BP_Glovebox` + `BP_GloveboxHandle` | `AVBoRHingedPanel` + 손잡이 `AVBoRAxisPart` | 같음 | `bUseLatch` 켬, `bAllowPalmOpening` 끔, 손잡이 `Release` = Return | 손잡이는 문과 같은 방식(§6.1, 바깥) | `:181-198` | 자동화(P4i) |
| 기어 레버(008) — `BP_GearLever` | `AVBoRAxisPart` | `PartMesh` / `AxisPivot`, `Axis` X | `Release` = Detent, `DetentAnglesDeg`, 시작 1, pinch 켬·손바닥 끔·밀기 켬(U-13) | 끝 버튼은 §6.2 | `:200-209` | 자동화(P4i) |
| 기어 끝 버튼 — `BP_GearButton` | `AVBoRPokeButton` | `ButtonMesh` | `HitAreaScale` 1 | `BP_GearLever` 안 Child Actor로 이미 들어 있음(§6.2). Actor가 버튼 자리에 있음 — CAD 피벗 처리 확인 안 함 | `:211-216` | 자동화(P4i) |
| 방향지시 레버(009) — `BP_TurnSignalLever` | `AVBoRAxisPart` | 같음, 왼쪽이면 `bReverse` | Detent + `DetentNudgeDeg`(자동 복귀) | — | `:218-228` | 자동화(P4i) |
| 스티어링 휠 — `BP_SteeringWheel` | `AVBoRSteeringWheel` | `WheelMesh` / `AxisPivot` | `LimitAngleDeg`, 잡는 반지름 최소·최대 | — | `:230-241` | 자동화(P4i) |
| 룸미러(U-12: 반사 없음) — `BP_RearviewMirror` | `AVBoRTwoAxisPart` | `PartMesh` / `Pivot` | `YawLimitDeg`, `PitchLimitDeg` | 거울면은 CAD 재질 그대로 | `:242-256` | 자동화(P4i) |
| 콘솔 노브 — `BP_ConsoleKnob` | `AVBoRKnob` | `KnobMesh` / `Pivot`(+Z가 윗면·회전축) | `TwistGain`, `StepDeg` | — | `:258-267` | 자동화(P4i) |
| 13번 도달 버튼 — `BP_ReachButton` | `AVBoRPokeButton` | `ButtonMesh` | **`HitAreaScale` 1**(13번은 판정 확대 없음, AGENTS.md) | `Car.ReachButton` — 13단계. Actor가 대시보드 위 버튼 자리에 있음(차 원점 아님) — CAD 피벗 처리 확인 안 함 | `:268-273` | 자동화(P4i) |
| 리프트게이트(011) — `BP_Liftgate` | `AVBoRPoweredPanel` | `PanelMesh` / `HingeLocationCm` | `OpenAngleDeg`, `DelaySeconds`, 열기·닫기 버튼 자리(`OpenButtonFromHinge`·`CloseButtonFromHinge`) | `Car.Liftgate` — 04·06단계. 버튼은 스스로 만듦 | `:350-357` | 자동화(P4i) |
| 2열 등받이 — `BP_RearSeatBack` | `AVBoRFoldingSeat` | `SeatMesh` / `HingeLocationCm` | `FoldAngleDeg` | `Car.RearSeat`. 진행자 F·U가 모든 접는 시트를 찾음 | `:128-132` | 자동화(P4i) |
| 선바이저 L·R — `BP_SunVisor_L`·`_R` | `AVBoRAxisPart` | `PartMesh` / `AxisPivot`, `Axis` Y | `Release` = Stay, palm·pinch·밀기 켬, `MaxAngleDeg` 95(근사). **값 +가 내림이다** — 경첩 방향은 아래 주의 | 없음(자유모드 부품, 단계와 안 이음) | 근사 차에 없음. 근사 값: 경첩 (18, ∓37, 142), 판 16×35×1.5 cm | 자동화(P5p) |
| 시동 버튼 — `BP_StartButton` | `AVBoRPokeButton` | `ButtonMesh` | `HitAreaScale` 1(기본값) | **`Car.StartButton`** — 전원(§6.5). Actor가 대시보드 위 버튼 자리 (27, −15, 91.6)에 있음(차 원점 아님) — CAD 피벗 처리 확인 안 함 | 근사 차에 없음 | 자동화(P5q) |
| 사이드미러 L·R — `BP_SideMirror_L`·`_R` | `AVBoRFoldingMirror` | `MirrorMesh` / **`HingeLocationCm`·`HingeRotation`**(회전한 +Z가 접는 축) | `MeshRestFromRoot` = **펼친** 자리, `FoldAngleDeg` L −65·R +65(부호로 방향 — 차 뒤로 접힘), `SpeedDegPerSecond` 30 | L은 `BP_Door_FL` 밑(§6.5). 전원으로만 움직임, 손으로는 안 만짐 | 근사 차에 없음. 근사 값: 경첩 (40, ∓90, 100), 메시 (40, ∓100, 100) | 자동화(P5q) |

- "자동화(P4i)"는 BP가 근사 차와 같은 자리·설정이고 연결이 된다는 뜻이다(엔진 Cube 메시, VR 전). CAD로 바꾼 뒤의 동작은 아직이다.
- "자동화(P5p)"·"자동화(P5q)"는 BP 값으로 동작 시험을 했다는 뜻이다(엔진 Cube 메시, VR 전).
- **선바이저 경첩 방향:**
  - 지금 `AxisPivot`은 Yaw 180°다. 그래서 로컬 +X가 차 뒤를 향하고, 판이 경첩 뒤쪽에 놓인다.
  - CAD 경첩으로 옮길 때 이 Yaw를 그대로 두고 위치·기울기만 바꾸면 쉽다.
  - 경첩을 반대 방향(Yaw 0 근처)으로 넣으면 `bReverse`를 뒤집어야 값 +가 내림이 된다.
  - 방향이 틀리면 자동화 `vBoR.P5p.E2`("Actual mesh bottom moves downward")가 실패한다. 자동화는 경첩 방향이 바뀌어도 따라간다(P5p R1-1).
- 선바이저는 얇은 판이라 메시로 잡기(§4.3)를 권장한다.
- **사이드미러 경첩은 값으로 옮긴다.**
  - BP 뷰포트에서 `HingePivot` 컴포넌트를 끌어 옮기면 construction이 `HingeLocationCm`·`HingeRotation`으로 덮어쓴다. 리프트게이트·2열 시트와 같은 방식이다.
  - CAD 미러 경첩이 기울어 있으면 `HingeRotation`으로 기울인다.
  - **하우징과 거울면을 merge해서 `MirrorMesh` 하나로 넣는다.** 안쪽 거울면은 따로 움직이지 않는다(사용자 2026-10-21).
- 콘솔박스·글러브박스도 `bPushOnMeshSurface`를 켤 수 있다. 1축 부품(레버 포함)·스티어링 휠·룸미러·노브도 `bGrabWithMesh`를 켤 수 있다. 방법은 §4.3이다.
- 소리 파일과 타이밍은 §4.4에서 바꾼다.

### 4.3 메시로 잡기·밀기 — 선택(P2f·P2g, 자동화로만 확인)

기본은 상자로 판정한다. 잡는 상자(`GrabBox`)는 메시 bounds에 여유를 더한 것이다. 미는 면은 bounds 평면(또는 미는 면 상자)이다. 그래서 곡면이나 비스듬한 부품은 메시 앞의 허공에서도 잡히거나 밀린다. 아래 옵션을 켜면 메시 모양 그대로 판정한다. 둘 다 기본은 꺼짐이다.

**작업 전 확인 — 메시 에셋만 열어 본다.** 피벗·충돌 작업 전에 그 메시를 쓸 수 있는지 본다.
1. Static Mesh 에디터 툴바의 Collision 보기에서 **Complex Collision**을 켠다. 보이는 메시와 똑같이 겹치면 그대로 쓸 수 있다.
   - 아무것도 안 보이면 Details → LOD0 → Sections에서 그 섹션의 `Enable Collision`이 꺼져 있는 것이다.
   - 보이는 메시보다 거칠면 Nanite 대체 메시로 만들어졌을 수 있다(확인 안 한 엔진 동작). 잡기·밀기도 그 거친 모양으로 판정된다.
2. **Simple Collision** 보기로 이미 simple 충돌이 있는지 본다. 있어도 큰 상자 하나 수준이면 지금 잡는 상자와 다를 게 없다. 그러면 아래 잡기 3의 Use Complex Collision As Simple로 바꾼다.
3. 문은 `Double Sided Geometry`를 켠 뒤 문 BP의 "설정 검사"를 누른다. Play 없이 trace되는지 알려 준다(아래 밀기 4).
4. 잡는 부품에는 "설정 검사" 버튼이 없다. 옵션을 켜고 Play를 한 번 해서, Output Log에 `grab mesh …` 경고가 없는지 본다.

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
- **Complex As Simple 주의:**
  - 삼각형 충돌은 표면만 있고 안이 없다. 손바닥 감지 구(반지름 4 cm)가 표면에 닿을 때만 hover된다. 그래서 두께 8 cm보다 두꺼운 부품 안으로 손이 완전히 들어가면 hover가 끊긴다. 손잡이·레버·노브·미러·휠 테는 얇아서 거의 해당하지 않는다.
  - 피벗 변경은 메시 에셋에 들어간다. 그래서 CAD를 다시 import하면 사라질 수 있다. **부품마다 옮긴 값을 기록해 둔다.** 다시 옮기고 `MeshRestFromRoot`에 같은 값을 넣는다.

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

### 4.4 소리 바꾸기 — 소리 칸(P5r·P5s, 자동화로만 확인)

부품마다 이벤트별 **소리 칸**이 있다. 칸마다 소리 파일과 타이밍을 BP 값으로 넣는다. Event Graph는 쓰지 않는다(§4.1-7).

**어디서:**
- BP를 열고 Class Defaults → Details에서 아래 표의 "분류" 칸(`…|Sound`)을 찾는다.
- 전원은 `BP_Car_Core`에서 `VehiclePower` 컴포넌트를 골라 본다.
- 소리 파일은 `Content/Main/Audio/`에 둔다(권장). `Development/`의 소리를 쓰면 P4i 참조 시험이 실패한다.

**칸 하나의 값**(`Interaction/VBoRSoundCue.h`):

| 값 | 뜻 | 기본 |
|---|---|---|
| `Sound` | 소리 에셋(SoundWave·SoundCue·MetaSound). 비우면 무음 | 아래 표 |
| `StartDelaySeconds` (s) | 이벤트 뒤 소리가 나기까지. frame 시간으로 센다 | 0 |
| `VolumeMultiplier`·`PitchMultiplier` | 재생할 때 곱하는 값 | 1·1 |
| `Attenuation` | 감쇠 설정. 비우면 소리 에셋의 설정을 쓴다(아래 "소리 자리") | 비움 |
| `RepeatSeconds` (s) — 지속 칸만 | 0이면 시작 때 한 번 낸다. 루프 에셋이면 상태 동안 계속 난다. 0보다 크면 그 간격으로 다시 낸다. 0.05 s보다 작으면 0.05 s로 쓴다 | 0 |
| `FadeOutSeconds` (s) — 지속 칸만 | 상태가 끝날 때 줄이며 멈추는 시간. 0이면 바로 멈춘다 | 0 |

**칸 표:**
- "ISDK"는 지금 들리는 ISDK poke 누름 소리(지연 0)다. 기본값이 지금 소리 그대로다.
- "무음"은 새로 생긴 칸이다.
- "순간" 칸은 이벤트마다 한 번 난다. "지속" 칸은 상태가 이어지는 동안 난다.

| 부품 BP | 분류 | 칸 | 종류 | 언제 | 기본 |
|---|---|---|---|---|---|
| 버튼 `BP_GearButton`·`BP_ReachButton`·`BP_StartButton` | `Poke Button\|Sound` | `PressedCue` | 순간 | 누를 때마다. 부품이 받지 않은 누름도 낸다(키를 쥔 손 등) | ISDK |
| | | `ReleasedCue` | 순간 | 뗄 때마다 | 무음 |
| 리프트게이트 `BP_Liftgate` | `Powered Panel\|Sound` | `ButtonPressedCue`·`ButtonReleasedCue` | 순간 | 열기·닫기 버튼을 누름·뗌. 버튼은 Play 때 생기므로 여기서 바꾼다 | ISDK·무음 |
| | | `OpenAcceptedCue`·`CloseAcceptedCue` | 순간 | 열기·닫기 요청을 받은 다음 frame. 대기·움직이는 중 같은 쪽 요청에는 안 난다 | 무음 |
| | | `WarningCue` | 지속 | 받은 다음 frame부터 열림·닫힘 도착까지(대기·움직임 동안) | ISDK, 0.5 s 반복 |
| | | `MotorCue` | 지속 | 움직이기 시작한 frame부터 도착까지. 끼여서 다시 열려도 이어진다 | 무음 |
| | | `OpenedCue`·`ClosedCue` | 순간 | 열림·닫힘 도착 frame | 무음 |
| | | `ObstructedCue` | 순간 | 끼임 감지 frame | 무음 |
| 키 `BP_KeyFob_Front`·`_Rear` | `Key\|Sound` | `ButtonPressedCue`·`ButtonReleasedCue` | 순간 | 열림·잠금 버튼을 누름·뗌 | ISDK·무음 |
| 문·콘솔·글러브박스 `BP_Door_FL`·`BP_ConsoleLid`·`BP_Glovebox` | `Hinged panel\|Sound` | `UnlockedCue` | 순간 | 손잡이로 걸쇠가 풀린 순간 | ISDK |
| | | `LatchedCue` | 순간 | 닫혀 잠긴 순간 | ISDK |
| 손잡이·선바이저 `BP_DoorHandle_FL_*`·`BP_GloveboxHandle`·`BP_SunVisor_*` | `Axis part\|Sound` | `OpenedCue`·`ClosedCue` | 순간 | 열림 기준(`OpenThreshold`)·닫힘 기준(`ClosedThreshold`)을 넘은 frame | ISDK |
| 기어·방향지시 레버 `BP_GearLever`·`BP_TurnSignalLever` | `Axis part\|Sound` | `DetentCue` | 순간 | 걸림 위치가 바뀐 frame. 한 frame에 둘을 넘으면 두 번 | ISDK |
| | | `NudgeCue` | 순간 | 방향지시 레버의 차선 변경 걸림 | ISDK |
| 노브 `BP_ConsoleKnob` | `Knob\|Sound` | `StepCue` | 순간 | 한 칸 넘을 때마다 | ISDK |
| 스티어링 휠 `BP_SteeringWheel` | `Steering wheel\|Sound` | `LimitCue` | 순간 | 한계 각도에 닿은 frame | ISDK |
| 사이드미러 `BP_SideMirror_L`·`_R` | `Mirror folding\|Sound` | `UnfoldMotorCue`·`FoldMotorCue` | 지속 | 펼치는·접는 동안. 도중에 방향이 바뀌면 앞 모터는 그 칸의 페이드대로 멈추고 새 모터가 같은 frame에 시작한다 | 무음 |
| | | `UnfoldedCue`·`FoldedCue` | 순간 | 펼침·접힘 도착 | 무음 |
| 전원 `BP_Car_Core` → `VehiclePower` | `Vehicle power\|Sound` | `PowerOnCue`·`PowerOffCue` | 순간 | 켜짐·꺼짐이 실제로 바뀐 순간(시동 버튼·진행자 `E`). 같은 frame에 손과 진행자가 같이 눌러도 한 번 | 무음 |
| 박스(BP 없음 — 아래) | `Carry\|Sound` | `BlockedCue` | 순간 | 막힘에 들어간 순간. 막힌 동안 다시 안 난다 | ISDK |
| | | `PausedCue` | 지속 | 손에 든 채 추적을 잃은 동안 | ISDK, 0.25 s 반복 |

- 1축 부품은 부모가 같아서 BP마다 네 칸이 다 보인다. 걸림 부품(레버)은 열림·닫힘 칸이 나지 않는다. 나머지(손잡이·선바이저)는 걸림 칸이 나지 않는다.
- 룸미러·2열 시트·키 쥐기·문 여닫는 움직임에는 소리 칸이 없다(P5s 범위 밖).

**재생 규칙:**
- 같은 칸이 소리가 끝나기 전에 다시 울리면 처음부터 다시 난다. 다른 칸끼리는 서로 끊지 않는다.
- 진행자 `R`, 부품 초기화, Play 끝에서는 예약된 소리가 지워지고 나는 소리가 바로 멈춘다(페이드 없음). `R`과 Play 시작 때는 전원·미러 소리가 나지 않는다.
- 각도 지점 소리(예: 닫히기 3° 전)는 없다. 비슷한 효과가 필요하면 소리 파일 앞의 빈 부분을 줄이거나 늘린다.

**소리 자리와 감쇠:**
- **감쇠가 없으면 소리가 위치 없이 들린다.** 칸의 `Attenuation`과 소리 에셋 둘 다 감쇠가 없으면 엔진이 공간화하지 않는다(엔진 `Engine/Private/Components/AudioComponent.cpp:1306-1320`, `Engine/Private/ActiveSound.cpp:1088`). 지금 기본 ISDK 소리도 에셋에 감쇠가 없다(에셋 파일만 봤고 소리로는 확인 안 함).
- **부품 쪽에서 들리게 하려면 감쇠를 넣는다.** 감쇠 에셋 하나를 만들어 여러 칸에 같이 써도 된다.
- **감쇠를 넣으면 이 자리에서 들린다:**
  - 문·콘솔·글러브박스·리프트게이트: 경첩 축(`HingePivot`). 잠김 소리도 걸쇠 쪽이 아니라 경첩 쪽에서 난다.
  - 1축 부품·휠: `AxisPivot`. 노브: `Pivot`.
  - 버튼: 버튼 메시. 미러: `MirrorMesh`. 박스: 박스 메시.
  - 전원: 차 원점(바닥 가운데). 시동 버튼 쪽이 아니다.
- 자리가 거슬리면 Claude에게 알린다. 명세로 고친다.

**박스 소리를 바꾸려면 박스 BP가 필요하다 — 확인 안 함.**
- 박스는 받침(`AVBoRBoxStand`)이 Play 때 만든다. 지금 받침의 `BoxClass`는 C++ 기본(`AVBoRCarryObject`)이라 소리를 바꿀 곳이 없다.
- 방법:
  1. `Content/Main/Car/Parts/`에 `AVBoRCarryObject` 자식 BP를 만든다(예 `BP_Box`). 소리 칸만 바꾼다.
  2. L_Main의 받침 Details에서 `BoxClass`로 그 BP를 고른다.
- 크기·적재 영역·바닥 높이는 받침이 덮어쓴다(`Interaction/VBoRBoxStand.cpp:47-49`).
- Play로 박스가 생기고 잡히는지 본다. 그 뒤 `vBoR.*`를 돌린다.

**집 PC에서 바꿀 때 — `Approx`가 있으면 `vBoR.P4i.E2`가 실패한다.**
- E2는 `Approx`가 있으면 부품 BP의 설정 값을 근사 차와 하나씩 비교하고, 소리 칸도 비교한다(`P3Probe/P4iCarBlueprintTests.cpp:93-103`). 그래서 "`BP_Door_FL setting LatchedCue`"처럼 실패한다.
- §2-3과 같은 이유다. 회사에서는 `Approx`를 먼저 지우므로 해당 없다.
- 전원 칸은 컴포넌트 값이라 이 비교에 들지 않는다. 소리를 넣어도 E2가 통과한다(P5s R1-1).

## 5. 레벨에 놓는 것 — 이미 놓여 있음(P4i), 자리만 옮김

| 무엇 | 클래스 | 놓는 법 | 태그·연결 | 근사 차 예 |
|---|---|---|---|---|
| 바닥 경로 — 차 둘레 | `AVBoRFloorRoute` | 레벨에 놓고 Spline 점을 바닥에 찍음(점 2개 이상) | Actor 태그 `Route.AroundCar` — 03단계 | `:133-136` |
| 바닥 경로 — 운전석 문까지 | `AVBoRFloorRoute` | 같음. 끝 점은 문 앞 | `Route.ToDriverDoor` — 07단계 | `:137-140` |
| 앞·뒤 키 테이블 | `AVBoRKeyTable` | 바닥에 놓음. `KeyClass` = `BP_KeyFob_Front`·`BP_KeyFob_Rear` | 키 태그는 §6.3. 뒤 키(`Car.RearKey`)는 단계가 안 쓰지만 근사 차와 같게 둠 | `:117-118`, `:283-298` |
| 박스 적재 영역 | `AVBoRLoadTarget` | 트렁크 안. `LoadingRegion` 상자를 트렁크 안 공간 크기로 | — | `:358-364` |
| 박스 받침 | `AVBoRBoxStand` | 차 뒤 동승석 쪽 바닥 | 태그 `Car.BoxStand`, **`LoadTarget`에 위 적재 영역을 고름**(§6.4) | `:365-368` |

- **경로는 `Car.Footprint`를 지나면 안 된다.** 흐름 시작 때 검사해서, 지나면 이유를 남기고 시작하지 않는다(`VBoRScenarioRunner.cpp:256`·`:265`).
- 경로 점은 나중에 실제 방 크기에 맞춰 옮긴다. 플레이 영역 밖이면 경로를 숨긴다(P3b D6).

## 6. 연결 규칙

### 6.1 손잡이 → 문·콘솔·글러브박스 (확인됨 — 자동화, P2e·U-14)

- 손잡이 BP의 **`DoorHandleSide`**를 바깥 또는 안쪽으로 정한다. 기어 레버처럼 판과 상관없는 1축 부품은 "없음"이다.
- 레벨 Outliner에서 손잡이를 **문(판) Actor 밑으로 끌어다 붙인다.** Play 때 판이 자기 밑의 손잡이를 찾아 연결하고, 경첩 축(`HingePivot`)에 붙여 같이 돌게 한다(`Interaction/VBoRHingedPanel.cpp:200-210`, `:328`).
  - Child Actor 컴포넌트로 넣는 경우에는 그 컴포넌트를 BP 안에서 **`HingePivot` 밑에** 둔다. 아니면 연결하지 않고 경고한다(`:236-238`).
- 손잡이 하나를 두 판이 같이 쓰면, 먼저 연결한 판이 갖는다(경고).
- 판 Details의 **"설정 검사"** 버튼(Hinged panel → Setup)이 Play 전에 같은 판정을 보여 준다. 판마다 한 번 누른다.

### 6.2 기어 끝 버튼 → 레버 (자동화로 확인 — P4i, VR 전)

- 근사 차는 버튼을 레버의 **`AxisPivot`**에 코드로 붙인다(`P3VehicleMock.cpp:216`). 레버가 돌면 버튼이 같이 간다.
- 레벨 Outliner로 붙이면 레버 Actor의 **루트**에 붙어서 레버가 돌아도 버튼이 제자리다. 그래서 다음처럼 한다.
  - `BP_GearLever` 안에 **Child Actor 컴포넌트**(클래스 `BP_GearButton`)가 `AxisPivot` 밑에 이미 있다.
  - 레버를 20° 돌리면 버튼이 근사 차와 같이 따라온다(P4i E2). VR에서 한 번 더 본다.
- 이 버튼은 단계와 연결하지 않는다(자유모드 부품, P5b D8).

### 6.3 키 태그 (자동화로 확인 — P4i, VR 전)

- 키는 키 테이블이 Play 때 만든다(`KeyClass`, `Interaction/VBoRKeyTable.cpp:42`). 그래서 레벨에서 키에 태그를 직접 달 수 없다.
- **키 BP**가 이미 있다: `BP_KeyFob_Front`(Actor Tags `Car.FrontKey`), `BP_KeyFob_Rear`(`Car.RearKey`). 키 테이블의 `KeyClass`가 그 BP다. 키 메시는 키 BP에서 바꾼다.
- 01단계(키 집기, 표시도 이 태그)와 02단계(키 버튼 열림·잠김)가 이 태그를 쓴다.
- 근사 차는 키 버튼에도 태그(`Car.FrontKey.Unlock`·`.Lock`)를 붙이지만, 지금 단계는 쓰지 않는다.

### 6.4 박스 받침 → 적재 영역 (자동화로 확인 — P4i, VR 전)

- `AVBoRBoxStand`의 `LoadTarget`은 **레벨에 놓은 인스턴스에서** 고른다(Details의 스포이트). BP 기본값으로는 레벨 Actor를 고를 수 없다. L_Main에는 이미 골라져 있다. 적재 영역을 지우고 새로 놓으면 다시 고른다.
- 05단계(박스 적재)는 진행자 확인으로 끝난다. 박스가 들어갔는지는 진행자가 본다(U-15).

### 6.5 전원 → 시동 버튼·사이드미러 (자동화로 확인 — P5q, VR 전)

- **전원은 `BP_Car_Core`의 `VehiclePower` 컴포넌트 하나다.**
  - 레벨에 정확히 하나여야 한다. 0개나 2개 이상이면 Output Log에 경고가 한 번 뜨고, 시동 버튼과 진행자 `E`가 둘 다 듣지 않는다.
  - 진행자 창 `Power` 버튼은 꺼지고 이유 "No vehicle power"가 보인다.
- **시동 버튼**은 Actor Tags `Car.StartButton`인 poke 버튼이다.
  - Play 때 전원이 한 번 찾아서 묶는다(`Interaction/VBoRVehiclePowerComponent.cpp:45`). 여러 개면 모두 묶는다.
  - 버튼이 없으면 경고가 한 번 뜬다. 진행자 `E`는 그대로 된다.
- **동작:**
  - 시작은 꺼짐이고 미러는 접혀 있다. 에디터에서는 펼친 자리로 보인다.
  - 누르면 켜지고 미러가 펼쳐진다. 다시 누르면 꺼지고 접힌다. 움직이는 중에 누르면 그 각에서 바로 방향을 바꾼다.
  - 진행자 창 "Power on/off"와 키 `E`도 같은 명령이다. 같은 frame에 손과 진행자가 같이 눌러도 한 번만 바뀐다.
  - 진행자 `R`은 전원을 끄고 미러를 바로 접는다.
- **운전석 미러는 레벨 Outliner에서 `BP_Door_FL` 밑에 붙인다**(손잡이와 같은 방법, §6.1). Play 때 문의 `HingePivot`에 붙어 문과 같이 돈다(`Interaction/VBoRFoldingMirror.cpp:36`). 동승석 미러는 문 BP가 없어 차 원점에 둔다.
- **시동에 다른 동작을 붙일 때는 버튼을 고치지 않는다.**
  - 그 부품이 전원 컴포넌트의 이벤트 `OnPowerChanged(bOn)`을 받게 한다(`VBoRVehiclePowerComponent.h:22`). 미러가 첫 예다.
  - 이벤트는 BP에서도 받을 수 있지만, BP에 로직을 넣지 않는 규칙(§4.1-7)이 있다. 새 동작은 Claude에게 알려 명세로 만든다.
- `Car.StartButton`은 시나리오 단계가 쓰지 않는다(§7 표에 없음). 평가 단계 중에 눌러도 미러만 움직인다.

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

- 태그가 없거나 종류가 다르면 흐름이 시작되지 않는다. Output Log와 VR 진행자 정보(`I`)에 `시작 차단:` 아래 `<단계>: 태그 <이름> 종류 불일치 또는 없음`으로 보인다(`VBoRScenarioRunner.cpp:240`·`:295`).
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
3. **흐름 시작 막힘이 없을 것:** Output Log(또는 VR 진행자 정보 `I`)에 `시작 차단:`이 없어야 한다(§7). "정렬 컴포넌트 1개 필요"면 `BP_Car_Core` 말고 정렬 컴포넌트를 또 넣었거나 `BP_Car_Core`를 지운 것이다(§2-3).
4. **진행자 창**(툴바 `Operator`):
   - **4점 정렬:** 실물 좌석 점 4개를 `P`로 짚고 → 차이(cm)·높이 차이 확인 → `S` 저장 → 표시를 다시 짚어 거리 확인 → `A`로 시작(P4h). 높이 경고가 뜨면 실물 좌석 높이를 확인한다(U-17).
   - **좌석 전환:** `1`·`2`로 동승석·2열에 가서 SgRP 차이만큼 옮겨졌는지, `Home`으로 돌아오는지 본다.
5. **부품마다:** 열리는 방향·최대 각, 손잡이가 문과 같이 도는지(§6.1), 기어 버튼이 레버를 따라오는지(§6.2), 리프트게이트 버튼, 2열 F·U, 키를 집고 버튼 누르기(§6.3).
   - **박스를 트렁크에 놓아 바닥에 착지하는지** 본다. `Approx`의 근사 트렁크를 지운 뒤에는 CAD 트렁크 충돌이 받친다(§2-3).
   - 메시로 잡는 부품은 hover가 메시에 닿을 때만 켜지는지 본다. 얇은 손잡이가 잡히는지, 가까운 부품끼리 맞게 골라지는지도 본다. 콘솔 변수 `Meta.InteractionSDK.DebugInteractionVisuals 1`은 잡는 충돌의 bounds 상자를 그린다. 메시 충돌 모양은 뷰포트 Show → Collision으로 본다.
   - 메시 표면으로 미는 문은 바깥 면과 안쪽 트림을 밀어 본다. 보이는 면에 닿을 때 밀리기 시작하는지, 돌출부와 창문 자리에서 어떻게 되는지 본다.
   - **선바이저:** 잡아 내리고 놓은 자리에 남는지 본다. 앞·뒤 면을 밀어 보고, 잡으려다 밀려 버리지 않는지 본다.
   - **시동 버튼·미러:** 누르면 두 미러가 펼쳐지고 다시 누르면 접히는지 본다. 운전석 문을 연 채로도 해 본다. 진행자 `E`와 `R`도 본다(§6.5). 미러가 접히며 차체·창문과 겹치지 않는지 본다.
   - **소리(§4.4):** 넣은 칸마다 이벤트를 한 번씩 일으켜 들어 본다. 지연이 움직임과 맞는지, 감쇠를 넣은 칸이 부품 쪽에서 들리는지 본다. 박스는 든 채 손을 시야 밖으로 빼 보류음 간격(0.25 s)을 듣는다.
6. **자동화 시험** `vBoR.*`를 돌린다(Agent-Workflow §2.4). `Approx`를 지운 뒤에는 `vBoR.P4i.E2`가 근사 차 비교를 건너뛴다(Info 한 줄). 다른 P4i 시험(BP 부모, 차 원점 배치, 정렬 점 4개, 연결, 참조 규칙)은 그대로 통과해야 한다.
   - `vBoR.P5p.*`는 선바이저 BP의 값을 읽어 시험한다. 실패하면 경첩 방향·`bReverse`를 본다(§4.2 주의).
   - `vBoR.P5q.E13`은 L_Main에 전원 하나, 시동 버튼 하나, 미러 L·R과 L의 부모(`BP_Door_FL`)가 있는지 본다.
7. **VR(Quest 3 Link):** 계획서 §7 시험 ID로 기록한다. 확인 전까지 상태는 `not-tested`다.

막힌 곳은 부품 이름, Output Log 줄, 진행자 창 이유를 그대로 적어 Claude에게 넘긴다. Claude가 명세로 고친다.
