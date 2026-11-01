# Blueprint 가이드 — 실제 차 CAD로 부품 BP 만들기

> 읽는 사람: 회사 PC에서 실제 차 CAD로 `L_Main`을 채우는 사람(사용자·팀원)과 그 PC의 AI.
> 먼저 읽을 것: [Company-PC-Guide.md](Company-PC-Guide.md)(지금 상태, CAD에서 뽑을 값 §4).
> 기준: `main` 2026-11-02(R5 머지 뒤). 코드 위치는 `Source/vBoR/` 아래다.
> 이 문서의 핵심은 **§3 부품 하나 만드는 순서**다. 처음이면 §1–§3을 읽고, 부품별 값은 §5, 판정 형상은 §6에서 찾는다.

## 0. 상태 — 무엇이 확인됐고 무엇이 아닌가

- **`L_Main`의 차는 이미 BP 틀이다(P4i).** `Content/Main/Car/`에 부품 BP가 있고(§5 표), 메시는 엔진 Cube다. 근사 판·트렁크·"근사 형상" 글자는 Outliner **`Approx` 폴더**에 있다. 회사에서는 `Approx`를 지우고(§2-3) BP마다 메시·축·값을 CAD로 바꾼다. BP를 새로 만들 필요는 거의 없다.
- **자동화 시험·PIE로 확인된 것(VR 전):** BP 자리·설정·연결(P4i), 메시로 잡기·밀기(P2f·P2g, 엔진 기본 메시로만), 선바이저·시동·미러(P5p·P5q), 소리 칸(재생 요청 수와 frame만), 박스 BP·다시 채우기(P5t·P5zf), 리프트게이트 판정 상자(P5zg), 문 그립(P5ze), 뷰포트 표시(R2a·R2c), 손잡이 근처 밀기 금지(R7), 선바이저 밀기 면 상자·손끝 여유(R8a), 미러 전원 연동 끄기·BP 이벤트(R3), Runner `Check setup`(R5).
- **아직 안 된 것:** Quest 3(VR) 확인 전부, CAD 메시로 바꾼 상태 대부분, 실제로 들리는 소리. 확인 전까지 시험 상태는 `not-tested`다.
- **막히면** 그 부품 하나와 Output Log 줄을 Claude에게 넘긴다. 부모 C++의 판정을 BP 로직으로 우회하지 않는다(§1 규칙).
- **CAD를 넣은 BP·`L_Main`을 리포에 올릴지는 미정이다.** 정하기 전에는 커밋하지 않는다(Company-PC-Guide §5).

## 1. 큰 그림과 규칙

```
L_Main
├─ World Settings: GameMode Override = BP_vBoRGameMode  (손 Pawn — §10)
├─ VBoRScenarioRunner  (Scenario = Main/Data/DA_Main_Scenario, 19단계. Details에 Check setup — §3-9)
├─ BP_Car_Core         차 기준점 — 정렬·좌석점·구역·전원·태그 (§4)
├─ 부품 BP             문·손잡이·그립·콘솔·글러브박스·레버·스티어링·룸미러·노브·버튼·리프트게이트
│                      ·2열 시트·선바이저·시동 버튼·사이드미러 (§5)
├─ 레벨 배치            바닥 경로 2개, 키 테이블 2개, 박스 받침·적재 영역, 안내판 표식 5개 (§8·§9)
├─ Approx 폴더         근사 판·트렁크·표시 글자 — 회사에서 통째로 지움 (§2-3)
└─ CAD 정적 메시        움직이지 않는 것은 BP로 만들지 않고 레벨에 그대로 (U-14)
```

- **시나리오는 태그로 부품을 찾는다.** 실제 부품에 지금과 같은 태그를 붙이면 시나리오를 고치지 않아도 된다(§9 표).
- 손 또는 키보드 → 같은 명령 → 차량 상태 → 단계 판정이다(AGENTS.md 구조 원칙).
- **규칙 — C++는 상태와 판정, BP는 표현(사용자 2026-11-02).**
  - BP에서 바꾸는 것: 메시, 값, 소리 칸, 그리고 부품 **이벤트에 붙이는 표현**(조명·소리·다른 부품 움직이기 — §7).
  - BP에 넣지 않는 것: 잡기·밀기·잠금·끼임 같은 **판정**, 시나리오 단계 완료에 영향을 주는 로직. 동작이 모자라면 부모 C++를 명세로 고친다.

## 2. 시작 전에

1. **좌표:** UE는 X 앞, Y 오른쪽, Z 위, cm다. CAD는 흔히 X가 뒤쪽이고 mm다. 가져올 때 축과 단위를 맞춘다(Company-PC-Guide §4 #1).
2. **CAD는 온 자리 그대로 둔다 — CAD 원점 = 월드 원점(사용자 2026-10-25).**
   - CAD 정적 메시, 부품 BP, `BP_Car_Core`의 Actor는 모두 (0,0,0), 회전 0이다. 차 몸체가 원점에서 떨어져 보여도 옮기지 않는다(CAD 피벗이 CAD 좌표 원점이라서 그렇다).
   - 그래서 `BP_Car_Core`의 점·구역과 부품 축 자리는 **CAD 좌표를 cm로 바꾼 값 그대로** 넣는다. 숫자 변환이 헷갈리면 CAD 메시의 점에 버텍스 스냅(V)으로 붙인다.
   - 부품 BP Actor가 (0,0,0)인지는 자동화 시험(`vBoR.P4i.*`)이 본다. 차를 옮기면 실패한다.
   - **Pawn은 직접 맞추지 않는다.** 4점 정렬이 정렬점의 월드 위치로 Pawn 자리를 구한다. 정렬 전(집 시험) PIE 시작 자리인 `PlayerStart`만 차 근처로 옮겨 둔다.
   - **지면(타이어 바닥)은 Z 0이 아니다(사용자 2026-10-25).** `BP_Car_Core` → `VBoR Vehicle Alignment` → **`FloorWorldZCm` = 지면의 월드 Z(cm)**. 정렬이 실제 바닥을 이 높이에 둔다(U-17). 목표 표시 고리도 이 높이를 따른다(P5zb).
   - **바닥에 놓는 것은 모두 그 높이에 둔다:** 레벨 바닥 메시, 바닥 경로 두 개(Actor Z와 Spline 점), 키 테이블 두 개(키가 테이블 Actor Z에 닿으면 돌아옴), 박스 받침(Actor Z = 박스 바닥 판정 높이), `Car.Start` 표시, `PlayerStart`.
3. **`Approx` 폴더를 먼저 통째로 지운다.** BP 메시를 바꾸기 전에 한다.
   - CAD 차체와 겹치기 때문이다. 또 `vBoR.P4i.E2`·`P5t.E6`은 `Approx`가 있을 때만 BP를 근사 값과 비교하므로, 지우지 않고 BP를 바꾸면 실패한다.
   - **근사 트렁크가 박스 착지 충돌을 주고 있었다.** CAD 트렁크 바닥·벽 메시에는 막는 충돌이 있어야 박스가 착지한다(simple 또는 Use Complex Collision As Simple, 프리셋은 BlockAll 등).
   - 정렬 컴포넌트는 `BP_Car_Core`에 하나 있다. 레벨에 정확히 하나여야 흐름이 시작된다.
4. **폴더:** 산출물은 `Content/Main/` 아래(예: `Main/Car/Parts/`, CAD는 `Main/Car/CAD/`, 소리는 `Main/Audio/`). `Development/` 에셋을 참조하지 않는다.

## 3. 부품 하나 만드는 순서

어떤 부품이든 이 순서로 한다. 부품별 값과 주의는 §5 표에 있다.

**1) 부모 클래스를 고른다.** 움직이거나 손으로 다루는 부품만 BP다(U-14). 문에 붙어 같이 도는 것(유리·트림·바깥 미러 하우징 merge 등)은 문 메시 하나로 merge한다.

| 동작 | 부모 C++ | 예 |
|---|---|---|
| 경첩으로 도는 판(손으로 밀고 손잡이로 염) | `AVBoRHingedPanel` / 문은 `AVBoRHingedDoor` | 문, 콘솔 뚜껑, 글러브박스 |
| 1축 회전·직선(손잡이·레버·선바이저·서랍), 문 그립 | `AVBoRAxisPart` | 문 손잡이, 기어·방향지시 레버, 선바이저 |
| 버튼으로 열리는 전동 판 | `AVBoRPoweredPanel` | 리프트게이트 |
| poke 버튼 | `AVBoRPokeButton` | 시동 버튼, 13번 도달 버튼 |
| 전원으로 접는 미러 | `AVBoRFoldingMirror` | 사이드미러 |
| 진행자 키로 접는 시트 | `AVBoRFoldingSeat` | 2열 등받이 |
| 그 밖 | `AVBoRSteeringWheel`, `AVBoRTwoAxisPart`(룸미러), `AVBoRKnob`, `AVBoRCarryObject`(박스) | — |

**2) BP를 연다.** 이미 있는 BP(§5 표)를 연다. 표에 없는 부품만 Content Browser 우클릭 → Blueprint Class → All Classes에서 부모를 골라 만든다. BP를 레벨에 놓을 때 Actor 위치는 차 원점(0,0,0)이다.

**3) 메시를 넣고 `MeshRestFromRoot`를 맞춘다.**
- 부품 메시 컴포넌트(§5 표의 "메시")에 CAD 메시를 넣는다.
- `MeshRestFromRoot` = 차 원점에서 본 메시 자리. 메시 피벗이 차 원점 그대로면 **단위 변환**(위치 0, 회전 0, 크기 1)이다. 지금 BP 값은 근사 상자용이므로 같이 바꾼다(문은 Check setup이 "Non-cube DoorMesh still uses mock MeshRestFromRoot"로 알려 준다).
- 메시로 잡는 부품만 피벗을 옮기고 옮긴 만큼을 여기에 넣는다(§6.2).

**4) 축 자리를 넣고 화살표로 확인한다(R2a).**
- 축 컴포넌트(§5 표의 "축")를 CAD의 경첩·회전 중심 위치와 방향으로 옮긴다. **메시는 제자리에 두고 축만 옮긴다**(U-09). 리프트게이트·시트·미러는 컴포넌트를 끌지 말고 `HingeLocationCm`(미러는 `HingeRotation`도) 값으로 옮긴다 — construction이 덮어쓴다.
- 뷰포트에 **움직임 표시**가 보인다(에디터 전용, Play에는 없음):

| 색 | 뜻 |
|---|---|
| 노랑 선 | 실제 회전축 |
| 초록 호 + 화살촉 | 닫힘 → 최대 각, 화살촉이 열리는 방향. 직선 이동이면 초록 화살표(길이 = 최대 이동) |
| 빨강 짧은 선 | 닫힘(0) 자리 |
| 흰 눈금 | 걸림(detent) 각. 시작 걸림은 굵게 |
| 면 위 초록·빨강 화살표 | "이 면을 손바닥으로 밀면" 열림(초록)·닫힘(빨강) 쪽으로 간다 |

- Details의 **`Preview (0–1)`** 슬라이더로 뷰포트에서 실제로 열고 닫아 본다. 저장되지 않고, Play는 원래 시작 상태(닫힘, 미러 접힘 등)로 시작한다.
- 표시가 거슬리면 `Motion guide`를 `Selected only`(선택했을 때만)나 `Off`로.
- 방향이 반대면 `bReverse`(AxisPart)나 축 회전을 고친다. 표시는 판정과 같은 식에서 나오므로 화살표 = 실제 동작이다.

**5) 판정 형상을 보면서 맞춘다(R2c).** 손이 "어디서 잡히고 어디서 밀리는지"를 뷰포트에 그린다(에디터 전용):

| 색 | 뜻 |
|---|---|
| 하늘색 | 잡기 형상 — `GrabBox` 또는 메시 simple collision(§6.2) |
| 파랑 | 손바닥 밀기 면(문·콘솔·글러브박스), 손가락 밀기 판(AxisPart) |
| 보라 | 손잡이 근처 밀기 금지 범위(§6.4) |
| 주황 | 리프트게이트 판정 상자가 닫힘 → 열림 각까지 쓸고 가는 자리(§6.5) |
| 자홍 | **경고·근사** — 메시 충돌이 없어 상자로 떨어짐, 메시 면 추적 실패, 판정 상자 없음, 회전한 밀기 면 상자 |

- 자홍이 보이면 그 부품의 설정을 고친다(§6). 표시는 `Judgment guide`(Always / Selected only / Off)로 따로 끈다.
- **Show Collision에 기대지 않는다.** 잡기 충돌은 Play 때 정해져 에디터의 Show Collision과 다를 수 있다. 이 색 표시가 실제 판정이다.

**6) 연결한다(§8).** 손잡이·그립·미러는 Outliner에서 문 Actor 밑에 붙인다. 시나리오가 찾는 부품은 Actor Tags에 태그를 단다(§9). `SpectatorName`에 TV 띠에 보일 영어 이름을 쓴다(예 `Driver Door`).

**7) 소리 칸을 채운다(§7.1).** 선택.

**8) 표현을 붙인다(§7.2).** 선택. 조명·소리·다른 부품 움직이기를 부품 이벤트에 붙인다. 판정은 붙이지 않는다(§1 규칙).

**9) Runner `Check setup`을 누른다(R5).**
- `L_Main` Outliner에서 `VBoRScenarioRunner` 선택 → Details → `Guidance | Setup` → **`Check setup`**. Play 없이 레벨 전체를 본다.
- 결과는 `Setup check result` 칸과 Output Log에 **한 줄에 하나씩**(`setup check:`로 검색). 문제는 Warning, 알림은 Log.
- 보는 것:
  - 시나리오 태그 수·종류(Play 때 `Start blocked:`와 같은 문구). Play 때 생기는 키·버튼의 BP 기본 태그까지 센다.
  - 버튼 칸(리프트게이트·키 버튼 클래스)의 BP가 시나리오 태그를 가지면 "어느 BP의 어느 칸" 한 줄(`found 3` 같은 원인).
  - `Car.StartButton`·전원 컴포넌트, 문 밑에 없는 미러, 어느 문에도 연결 안 된 손잡이·그립.
  - 모든 문·콘솔·글러브박스의 Check setup 결과(문마다 누를 필요 없음).
- `No issues`가 목표다. 동승석 미러처럼 일부러 문 밖에 둔 것은 그 줄을 무시한다.

**10) Play로 확인한다(§11).**

## 4. 차 기준점 BP — `BP_Car_Core`

**이미 있다:** `Content/Main/Car/Core/BP_Car_Core`. 부모는 `Actor`, 월드 원점에 놓여 있다. 메시는 넣지 않는다. 컴포넌트 자리를 CAD 값으로 옮긴다.

| 컴포넌트(클래스) | 개수 | 태그 | 자리·값 | 쓰는 곳 |
|---|---|---|---|---|
| `VBoR Vehicle Alignment` | 1 | — | `bHeightFromFloor` 켬, **`FloorWorldZCm` = 지면 월드 Z** | 정렬·좌석 전환 |
| `VBoR Align Point` | 4 | — | 실물 좌석의 단단한 점 4개 CAD 자리. `Order` 1–4, `Label`(진행자 창에 보임) | 4점 정렬(P4h, U-18) |
| `VBoR Viewpoint` | 3 | `Car.DriverView` / `Car.PassengerView` / `Car.RearView` | 운전석·동승석·2열 **SgRP** | 14·15 좌석 전환 |
| `VBoR Seat Zone` | 1 | `Car.DriverSeat` | 앉은 머리가 들어오는 상자 | 10번 "좌석에 앉아보세요"(1.5 s) |
| Box | 1 | `Car.Footprint` | 차체 바닥 윤곽 + 20 cm. **Collision 없음, Hidden in Game** | 바닥 경로가 차를 지나지 않는지 검사 |
| Scene 또는 작은 메시 | 1 | `Car.Start` | 차 앞 밖, 차를 바라보게 | 시작 위치 기록 |
| `VBoR Eye Point` | 0–1 | — | 운전석 설계 눈 | 선택. 눈 방식 정렬(집 시험 대체) |
| `VBoR Approach Zone` | 0–1 | `Car.Approach` | 차 둘레 | 지금 단계에서는 안 씀 |
| `VehiclePower` | **정확히 1** | — | 소리 칸만 | 시동 버튼·진행자 `E`·사이드미러(§8.4) |

- **좌석점 셋은 같은 종류의 점이어야 한다**(회사 차는 셋 다 SgRP, 운전석에 `Car.DriverView`). 섞으면 좌석 사이 거리가 틀어진다. `Car.DriverView`도 눈 점도 없으면 좌석 전환이 거절된다.
- 앉음 구역은 머리 기준이다. 넓이는 평가 담당자와 정할 값이다.
- 정렬 점은 에디터에서는 작은 구와 노란 번호로, 게임에서는 진행자가 `P`를 누를 때만 보인다.

## 5. 부품표

| 부품 — BP | 부모 | 메시 / 축 | 꼭 넣을 값 | 태그·연결 |
|---|---|---|---|---|
| 운전석 문 — `BP_Door_FL` | `AVBoRHingedDoor` | `DoorMesh` / `HingePivot`(+Z가 경첩축) | `MaxAngleDeg`, 잠금(`Lock when closed …`), 밀기 면(§6.3), 손잡이 근처 밀기 금지(§6.4), 선택: 관성(아래) | `Car.DriverDoor` — 07·08·11·18·20단계 |
| 문 손잡이 바깥·안쪽 — `BP_DoorHandle_FL_Ext`·`_Int` | `AVBoRAxisPart` | `PartMesh` / `AxisPivot` + `Axis`·`bReverse` | **`Door side` = Outside / Inside**, `MaxAngleDeg`, `Release`, 선택: 메시로 잡기(§6.2) | 문 밑에 붙인다(§8.1) |
| 문 안쪽 그립 — 예 `BP_DoorGrip_FL_Int`(필요하면 만듦) | `AVBoRAxisPart` | `PartMesh` / 축 안 씀(움직이지 않음) | **`Door side` = Grip**, 잡기 설정(손바닥·pinch, 메시로 잡기) | 문 밑에 붙인다(§8.1). 잡으면 바로 문을 잡아 밀고 당긴다. **잠금은 풀지 않고, 잠긴 동안 잡히지 않는다**(P5ze) |
| 콘솔박스 — `BP_ConsoleLid` + `BP_ConsoleHandle` | `AVBoRHingedPanel` + `AVBoRAxisPart` | 같음 | 잠금 켬, `Allow palm opening` 끔, `MaxAngleDeg` 100. 손잡이: `Movement` = Translate, `MaxTravelCm` 0.75, `Release` = Return, 손바닥 잡기만 | 손잡이 `Door side` = Outside, 뚜껑 밑. 끝까지 눌러야 풀린다(P5x) |
| 글러브박스 — `BP_Glovebox` + `BP_GloveboxHandle` | 같음 | 같음 | 잠금 켬, `Allow palm opening` 끔, 손잡이 `Release` = Return | 손잡이 `Door side` = Outside, 판 밑 |
| 기어 레버 — `BP_GearLever` | `AVBoRAxisPart` | `PartMesh` / `AxisPivot`, `Axis` X | `Release` = Detent, `DetentAnglesDeg`, 시작 1, pinch 켬·손바닥 끔·밀기 켬(U-13) | 끝 버튼은 §8.2 |
| 기어 끝 버튼 — `BP_GearButton` | `AVBoRPokeButton` | `ButtonMesh` | `HitAreaScale` 1 | `BP_GearLever` 안 Child Actor(§8.2) |
| 방향지시 레버 — `BP_TurnSignalLever` | `AVBoRAxisPart` | 같음, 왼쪽이면 `bReverse` | Detent + `DetentNudgeDeg`(자동 복귀) | — |
| 스티어링 휠 — `BP_SteeringWheel` | `AVBoRSteeringWheel` | `WheelMesh` / `AxisPivot` | `LimitAngleDeg`, 잡는 반지름 최소·최대 | — |
| 룸미러 — `BP_RearviewMirror` | `AVBoRTwoAxisPart` | `PartMesh` / `Pivot` | `YawLimitDeg`, `PitchLimitDeg` | 반사 없음(U-12) — 거울면은 재질 그대로 |
| 콘솔 노브 — `BP_ConsoleKnob` | `AVBoRKnob` | `KnobMesh` / `Pivot`(+Z가 윗면·회전축) | `TwistGain`, `StepDeg` | — |
| 13번 도달 버튼 — `BP_ReachButton` | `AVBoRPokeButton` | `ButtonMesh` | **`HitAreaScale` 1**(13번은 판정 확대 없음) | `Car.ReachButton` — 13단계 |
| 리프트게이트 — `BP_Liftgate` | `AVBoRPoweredPanel` | `PanelMesh` / `HingeLocationCm` | `OpenAngleDeg`, `DelaySeconds`, 버튼 자리(`OpenButtonFromHinge`·`CloseButtonFromHinge`), **판정 상자 2–4개(§6.5)**, 선택: 버튼 모양(§8.3) | `Car.Liftgate` — 04·06단계. 버튼은 스스로 만듦 |
| 2열 등받이 — `BP_RearSeatBack` | `AVBoRFoldingSeat` | `SeatMesh` / `HingeLocationCm` | `FoldAngleDeg` | `Car.RearSeat`. 진행자 F·U |
| 선바이저 L·R — `BP_SunVisor_L`·`_R` | `AVBoRAxisPart` | `PartMesh` / `AxisPivot`, `Axis` Y | `Release` = Stay, palm·pinch·밀기 켬, `MaxAngleDeg` 95. **값 +가 내림**(아래). 밀기 면 상자·손끝 여유(§6.6) | 없음(자유모드 부품) |
| 시동 버튼 — `BP_StartButton` | `AVBoRPokeButton` | `ButtonMesh` | `HitAreaScale` 1 | **`Car.StartButton`** — 전원(§8.4) |
| 사이드미러 L·R — `BP_SideMirror_L`·`_R` | `AVBoRFoldingMirror` | `MirrorMesh` / **`HingeLocationCm`·`HingeRotation`**(회전한 +Z가 접는 축) | `MeshRestFromRoot` = **펼친** 자리, `FoldAngleDeg` L −65·R +65(차 뒤로 접힘), `SpeedDegPerSecond`, `Follow vehicle power`(기본 켬, §8.4) | L은 `BP_Door_FL` 밑(§8.1) |
| 박스 — `BP_Box` | `AVBoRCarryObject` | `CarryMesh`(Actor 루트) / 축 없음 | **`BoxSizeCm`**(아래) | 레벨에 놓지 않는다. 받침의 `BoxClass`(§9) |
| 키 — `BP_KeyFob_Front`·`_Rear` | `AVBoRKeyFob` | `KeyMesh` | 키 메시. 버튼 자리 `UnlockButtonCm`·`LockButtonCm`(키 루트에서 cm) | Actor Tags `Car.FrontKey`·`Car.RearKey`(§8.3) |

**문 관성 — `Door → Coast`(P5za).** `BP_Door_FL`에만 있다.
- `Coast after release`: 켜면 밀다 멈추거나 손잡이를 놓았을 때 문이 그 속도로 조금 더 가다 선다. **기본 꺼짐**(끄면 예전과 같음).
- `Coast deceleration (deg/s^2)`: 클수록 빨리 선다. 60 deg/s로 놓을 때 90 → 약 20°, **180(기본) → 약 10°**, 360 → 약 5°. 임시 값 — Quest 3에서 맞춘다. PIE 중에 바꿔도 바로 적용된다.
- 근사 조작감이다. 실차 조작력 평가로 쓰지 않는다. 평가 단계에서 켤지는 Quest 3 확인 뒤 정한다.

**선바이저 경첩 방향.** 지금 `AxisPivot`은 Yaw 180°(로컬 +X가 차 뒤)다. CAD 경첩으로 옮길 때 이 Yaw를 두고 위치·기울기만 바꾸면 쉽다. 반대 방향(Yaw 0 근처)으로 넣으면 `bReverse`를 뒤집어야 값 +가 내림이 된다. 틀리면 `vBoR.P5p.E2`가 실패한다. 얇은 판이라 메시로 잡기(§6.2)를 권장한다.

**사이드미러.** 하우징과 거울면을 merge해서 `MirrorMesh` 하나로 넣는다(안쪽 거울면은 따로 안 움직임). 경첩이 기울어 있으면 `HingeRotation`으로 기울인다.

**키 메시(P5zd).** 키 메시가 엔진 Cube일 때만 `KeySizeCm`으로 크기를 맞춘다. CAD 키 메시는 넣은 크기(BP 배율, 보통 1) 그대로다. Play 로그 `key size=… cm`가 실제 크기다.

**박스 — `BP_Box`(P5t·P5zf, 자동화는 엔진 Cube로만).**
- `CarryMesh`에 메시를 넣는다. 메시는 bounds가 `BoxSizeCm`이 되도록 축마다 따로 늘어난다. CAD 메시를 원래 크기로 쓰려면 그 크기를 그대로 넣는다(비율이 다르면 찌그러짐). 받침은 크기를 `BP_Box`에서 읽는다.
- **피벗을 박스 가운데에 둔다.** `CarryMesh`가 루트라 BP에서 옮길 수 없다. 1 cm 넘게 벗어나면 Play 때 "mesh bounds center is … cm from the Actor origin" 경고. 옮기는 방법은 §6.2 1번과 같다.
- **simple collision(상자·convex)이 있어야 한다.** 없으면 "No simple collision on carry mesh … movement disabled". 구·캡슐은 쓰지 않는다.
- 받침이 덮어쓰는 값: `LoadTarget`, `bUsePlacementAssist`(끔), `bDestroyAtFloor`(켬), `FloorHeightCm`. 높이를 크게 바꾸면 `LevelLiftMaxCm`(놓을 때 세우며 올리는 한도, 기본 15 cm)도 본다.
- 받침은 **손에서 놓은 뒤에** 다음 박스를 만든다(들고 있는 동안은 안 만듦, P5zf).
- 적재 영역(`AVBoRLoadTarget`)은 박스 크기를 따르지 않는다. 트렁크 안 공간 크기로 따로 둔다.

## 6. 판정 형상 — 잡는 곳·미는 곳 맞추기

기본은 **상자**다. 잡는 상자(`GrabBox`)는 메시 bounds + 여유(`GrabMarginCm`), 미는 면은 메시 bounds 평면이다. merge한 CAD 메시는 bounds가 실제 모양보다 커서 허공에서 잡히거나 밀린다. 아래 방법으로 좁힌다. 맞추는 동안 §3-5의 색 표시를 본다.

### 6.1 작업 전 — 메시 에셋 충돌 확인

- 사용자는 Blender에서 **simple collision(UCX)**을 만들어 넣는다(2026-11-01).
- Static Mesh 에디터 툴바의 Collision 보기: **Simple Collision**에 UCX가 보이는지, 필요하면 **Complex Collision**이 보이는 메시와 겹치는지 본다. 아무것도 안 보이면 LOD0 → Sections의 `Enable Collision`을 본다.
- Nanite 메시는 복합 충돌이 거친 대체 메시로 만들어질 수 있다(확인 안 함). 판정이 보이는 면과 다르면 알려 준다.

### 6.2 메시로 잡기 — `Grab with mesh collision`

1축 부품(손잡이·그립·레버·선바이저)·스티어링 휠·룸미러·노브의 `Input` 칸. 켜면 잡기 형상이 하늘색으로 그 메시 충돌 모양이 된다.
1. **메시 피벗을 손이 잡는 부분 가운데로 옮긴다.** ISDK는 손바닥에 닿은 후보 중 충돌 컴포넌트 **원점**이 가장 가까운 것을 고른다. CAD 메시는 피벗이 모두 차 원점이라 그대로면 가까운 부품 사이에서 무엇이 잡힐지 정해지지 않는다(피벗이 Actor 원점 1 cm 안이면 Play 때 "grab mesh pivot is at the actor origin" 경고). 회전 중심과는 상관없다(축 컴포넌트가 정함).
   - 방법(확인 안 함): 임시 레벨 원점에 CAD 메시를 놓고 Modeling 모드 → Edit Pivot으로 옮겨 Accept → 메시가 제자리에 보일 때 그 Actor의 Location(회전도 했으면 Rotation)이 옮긴 만큼 → 적고 임시 Actor는 지운다.
   - 메시 에셋 자체가 바뀐다. **CAD를 다시 import하면 사라질 수 있으니 부품마다 값을 기록한다.**
2. 부품 BP의 **`MeshRestFromRoot`에 1의 위치(와 회전)**를 넣어 메시를 원래 자리로 돌린다.
3. 메시 충돌: UCX(box·convex)가 있으면 그대로. 없으면 Collision Complexity = **Use Complex Collision As Simple**. 구·캡슐은 축마다 다른 배율에서 가장 작은 축으로 줄어드므로 쓰지 않는다. 둘 다 없으면 "grab mesh has no simple collision" 경고 + 상자로 잡음(자홍).
4. 옵션을 켠다. **BP에서 메시 Collision Presets는 바꾸지 않는다**(Play 때 코드가 정함).
- Complex As Simple은 표면만 있어서, 두께 8 cm보다 두꺼운 부품 안으로 손이 완전히 들어가면 hover가 끊긴다. 손잡이·레버·노브·휠 테는 거의 해당 없다.

### 6.3 문 밀기 면 — `Push face` 칸 (문·콘솔·글러브박스)

손바닥과 손가락 끝 4점이 면을 지나면 판이 밀린다. 셋 중 하나:
- **메시 bounds(기본)** — merge한 돌출부까지 면이 된다. 면 두께가 `Mesh thickness warning threshold`(기본 20 cm)를 넘으면 Check setup이 경고한다.
- **`Use push face box`** — `Push face box`(Details의 컴포넌트) 상자를 실제 판 부분에만 맞춘다. **상대 회전은 0**이어야 한다(아니면 경고 + 메시 bounds, 자홍). 처음 켜면 지금 면과 같은 자리에서 시작하므로 줄이면 된다.
- **`Push on mesh surface`** — 보이는 메시 삼각형을 손 점마다 trace한다(피벗은 옮기지 않음). 메시 에셋에서 `Double Sided Geometry`를 켠다. Check setup이 "DoorMesh has no traceable collision"이면 trace가 안 되는 것(자홍, bounds로 밂). 켜면 밀기 면 상자는 무시된다.
  - trace는 complex 충돌을 쓴다. UCX로 밀게 하려면 그 메시 Collision Complexity를 **Use Simple Collision As Complex**로 둔다(merge한 사이드미러를 밀면 문이 밀리는 것을 뺄 때도 이 방법).
- `Allow palm opening`을 끄면 여는 쪽 밀기는 없다(닫는 쪽만).

### 6.4 손잡이 근처 밀기 금지 — `No push near handle` (R7)

- 연결된 손잡이·그립의 잡는 상자 + 이 값(cm, 기본 **8**) 안에 손 점이 하나라도 있으면 **그 손은 판을 밀지 않는다.** 운전석에서 손잡이를 잡으러 뻗은 손가락이 안쪽 면을 먼저 밀어 버리는 것을 막는다. 다른 손은 계속 민다.
- 범위는 뷰포트에 **보라**로 보인다. 글러브박스·콘솔 같은 작은 뚜껑에서는 8 cm가 판 대부분을 덮을 수 있다 — 줄이거나 0(끔)으로.

### 6.5 리프트게이트 끼임 — `Obstruction Box` (P5zg)

- 닫히는 중에 박스가 판정에 겹치면 그 각도에서 다시 열린다(사용자 결정: 그대로 둠).
- 판정 상자가 없으면 **휜 리프트게이트 메시 전체 bounds**로 판정한다 — 트렁크 쪽으로 크게 들어와서 "박스를 조금만 가까이 실어도 다시 열림"이 된다(쓸림 표시가 자홍).
- `BP_Liftgate` → Add Component → **`Obstruction Box`**를 **`HingePivot` 밑에** 2–4개 넣어 판 아래쪽 가장자리·안쪽 트림을 덮는다. 회전·배율 허용. `HingePivot` 밖이나 크기 0인 상자는 무시되고 Play 때 경고한다.
- 뷰포트에 상자가 닫힘 → 열림 각까지 쓸고 가는 자리가 **주황**으로 보인다. 적재 영역(트렁크 안)과 겹치는 높이를 보며 맞춘다. Play 로그 한 줄이 판정 근거(`mesh bounds (approximate)` 또는 `N obstruction boxes`)를 알려 준다.

### 6.6 손가락 밀기 판 — AxisPart (선바이저·레버, R8a)

- `bAllowPush`를 켠 AxisPart는 손가락 관절 중심이 판(파랑)의 두 면을 지나면 그 방향으로 돈다.
- **`Use push face box`**(`Axis part | Push face`): 축 막대·클립이 섞인 merge 메시면 판이 실제보다 크다. `PushFaceBox`(AxisPivot 밑)를 판 부분에만 맞춘다. 꺼져 있는 동안은 메시 bounds에 자동으로 맞춰져 있어, 켜고 줄이면 된다. **상대 회전 0**(아니면 경고 + 메시 bounds, 자홍).
- **`Push contact offset`**(기본 **0.8 cm**): 판정점은 손가락 관절 중심이라 살 두께만큼 파고든 뒤 움직였다. 면을 손 쪽으로 이만큼 옮겨, 손가락 살이 면에 닿을 때 움직이기 시작한다. 표시(파랑 판)에는 여유를 넣지 않는다. 적절한 값은 Quest 3에서 본다. 0이면 예전과 같다.
- 그 밖: `PushMarginCm`(판 옆 여유), `PushMinAxisDistanceCm`(축 바로 옆 무시).

## 7. 소리와 BP 표현

### 7.1 소리 칸 (P5r·P5s)

부품마다 이벤트별 **소리 칸**이 있다. BP Class Defaults → Details에서 `…|Sound` 분류를 찾는다(전원은 `BP_Car_Core` → `VehiclePower`). 소리 파일은 `Content/Main/Audio/`에 둔다(`Development/` 소리를 쓰면 P4i 참조 시험 실패).

**칸 하나의 값:**

| 값 | 뜻 | 기본 |
|---|---|---|
| `Sound` | 소리 에셋(SoundWave·SoundCue·MetaSound). 비우면 무음 | 아래 표 |
| `StartDelaySeconds` (s) | 이벤트 뒤 소리가 나기까지(frame 시간으로 셈) | 0 |
| `VolumeMultiplier`·`PitchMultiplier` | 재생할 때 곱하는 값 | 1·1 |
| `Attenuation` | 감쇠 설정. 비우면 소리 에셋의 설정 | 비움 |
| `RepeatSeconds` (s) — 지속 칸만 | 0이면 시작 때 한 번(루프 에셋이면 계속). 0보다 크면 그 간격으로 다시(최소 0.05 s) | 0 |
| `FadeOutSeconds` (s) — 지속 칸만 | 상태가 끝날 때 줄이며 멈추는 시간 | 0 |

**칸 표**("ISDK" = 지금 들리는 ISDK poke 소리, "순간" = 이벤트마다 한 번, "지속" = 상태 동안):

| 부품 BP | 분류 | 칸 | 종류 | 언제 | 기본 |
|---|---|---|---|---|---|
| 버튼 `BP_GearButton`·`BP_ReachButton`·`BP_StartButton` | `Poke Button\|Sound` | `PressedCue` / `ReleasedCue` | 순간 | 누를 때·뗄 때 | ISDK / 무음 |
| 리프트게이트 `BP_Liftgate` | `Powered Panel\|Sound` | `ButtonPressedCue`·`ButtonReleasedCue` | 순간 | 생성 버튼 누름·뗌(버튼은 Play 때 생기므로 여기서) | ISDK·무음 |
| | | `OpenAcceptedCue`·`CloseAcceptedCue` | 순간 | 요청을 받은 다음 frame | 무음 |
| | | `WarningCue` | 지속 | 받은 다음 frame부터 도착까지 | ISDK, 0.5 s 반복 |
| | | `MotorCue` | 지속 | 움직이는 동안(끼여 다시 열려도 이어짐) | 무음 |
| | | `OpenedCue`·`ClosedCue` / `ObstructedCue` | 순간 | 도착 / 끼임 감지 | 무음 |
| 키 `BP_KeyFob_*` | `Key\|Sound` | `ButtonPressedCue`·`ButtonReleasedCue` | 순간 | 열림·잠금 버튼 | ISDK·무음 |
| 문·콘솔·글러브박스 | `Hinged panel\|Sound` | `UnlockedCue` / `LatchedCue` | 순간 | 걸쇠 풀림 / 닫혀 잠김 | ISDK |
| 손잡이·선바이저 | `Axis part\|Sound` | `OpenedCue`·`ClosedCue` | 순간 | 열림·닫힘 기준을 넘은 frame | ISDK |
| 기어·방향지시 레버 | `Axis part\|Sound` | `DetentCue` / `NudgeCue` | 순간 | 걸림 위치 바뀜 / 차선 변경 걸림 | ISDK |
| 노브 | `Knob\|Sound` | `StepCue` | 순간 | 한 칸마다 | ISDK |
| 스티어링 휠 | `Steering wheel\|Sound` | `LimitCue` | 순간 | 한계 각도 | ISDK |
| 사이드미러 | `Mirror folding\|Sound` | `UnfoldMotorCue`·`FoldMotorCue` | 지속 | 펼치는·접는 동안 | 무음 |
| | | `UnfoldedCue`·`FoldedCue` | 순간 | 도착 | 무음 |
| 전원 `BP_Car_Core` → `VehiclePower` | `Vehicle power\|Sound` | `PowerOnCue`·`PowerOffCue` | 순간 | 켜짐·꺼짐이 실제로 바뀐 순간 | 무음 |
| 박스 `BP_Box` | `Carry\|Sound` | `BlockedCue` / `PausedCue` | 순간 / 지속 | 막힘 / 든 채 추적 잃음 | ISDK / ISDK 0.25 s 반복 |

- 1축 부품은 BP마다 네 칸이 다 보이지만, 레버는 열림·닫힘 칸이, 손잡이·선바이저는 걸림 칸이 나지 않는다. 문 그립은 소리를 내지 않는다. 룸미러·2열 시트·키 쥐기·문 움직임에는 칸이 없다.
- **재생 규칙:** 같은 칸이 끝나기 전에 다시 울리면 처음부터. 진행자 `R`·부품 초기화·Play 끝에서는 예약된 소리가 지워지고 바로 멈춘다.
- **감쇠가 없으면 소리가 위치 없이 들린다**(칸과 에셋 둘 다 감쇠 없음 → 공간화 안 함). 부품 쪽에서 들리게 하려면 감쇠를 넣는다. 그러면 문·리프트게이트는 경첩 축, 1축 부품·휠은 `AxisPivot`, 노브는 `Pivot`, 버튼·미러·박스는 그 메시, 전원은 차 원점에서 들린다.
- 집 PC(`Approx` 있음)에서 BP 소리 칸을 바꾸면 `vBoR.P4i.E2`가 실패한다(§2-3과 같은 이유). 전원 칸은 해당 없음.

### 7.2 BP 표현 — 이벤트와 명령 (R3)

규칙(§1): **이벤트에 표현(조명·소리·다른 부품 움직이기)을 붙인다. 판정·시나리오 완료에 영향을 주는 로직은 붙이지 않는다.**

**부품 이벤트**(BP Event Graph에서 그 컴포넌트·Actor를 골라 `Assign`/이벤트 노드):

| 부품 | 이벤트 |
|---|---|
| 문·콘솔·글러브박스(`Hinged panel`) | `OnPanelUnlocked`, `OnPanelLocked`, `OnAngleChanged`, `OnReset` / 문만 `OnUnlocked`·`OnLocked` |
| 1축 부품(`Axis part`) | `OnPartOpened`, `OnPartClosed`, `OnPartReleased`, `OnDetentChanged`, `OnDetentNudge` |
| 리프트게이트(`Powered Panel`) | `OnStateChanged`, `OnOpened`, `OnClosed`, `OnObstructed`, `OnReset` |
| 사이드미러(`Mirror folding`) | `OnStateChanged`(Unfolding·Unfolded·Folding·Folded), `OnReset` |
| 2열 등받이(`Seat folding`) | `OnStateChanged`(Standing·Folding·Folded·Unfolding), `OnReset` |
| 전원(`Vehicle power`) | `OnPowerChanged(bOn)` |
| 버튼 / 키 / 노브 / 휠 | `OnButtonPressed`·`OnButtonReleased` / `OnUnlockPressed`·`OnLockPressed` / `OnKnobStep` / `OnLimitReached` |

- **`OnReset`은 Play 중 초기화 때만 난다**(진행자 `R` 다시 시작, BP에서 Reset 호출). 처음 시작(BeginPlay)에는 나지 않는다. 표현으로 바꾼 것(켠 조명 등)은 여기서 되돌린다. 상태가 바뀌면 상태 이벤트가 먼저, `OnReset`이 나중이다.
- 미러·등받이 상태 이벤트는 상태가 실제로 바뀔 때만 한 번 난다. 리프트게이트 `OnStateChanged`는 초기화 때 같은 상태도 낸다(예전 그대로).

**BP에서 부를 명령:** 미러 `Fold()`·`Unfold()`(움직이기 시작하면 true, 이미 그쪽 끝이거나 그쪽으로 가는 중이면 false), 등받이 `Fold()`·`Unfold()`, 각 부품의 Reset 함수. 문·콘솔에는 자동 열기 명령이 없다(모터가 없음).

**예 — 시동 대신 키 버튼으로 미러 펴기:** 미러 BP에서 `Follow vehicle power` 끔 → 키 BP(또는 레벨 BP)에서 `OnUnlockPressed` → 미러 `Unfold`. 미러 상태 판정은 그대로 C++가 한다.

## 8. 연결 규칙

### 8.1 손잡이·그립·미러 → 문

- 손잡이 BP의 **`Door side`**: 바깥 Outside, 안쪽 Inside, 문 안쪽 그립 Grip. 판과 상관없는 1축 부품(레버·선바이저)은 None.
- 레벨 Outliner에서 손잡이·그립을 **문(판) Actor 밑으로 끌어다 붙인다.** Play 때 판이 자기 밑의 부품을 `Door side`대로 바깥·안쪽·그립 칸에 연결하고, 경첩 축(`HingePivot`)에 붙여 같이 돌게 한다.
  - 판의 `Outside Axis Handle`·`Inside Axis Handle`·`Grip Axis Handle` 칸은 **비워 둔다**(에디터에서 None이 정상). 같은 쪽 부품이 둘 이상일 때만 칸에서 직접 고른다("multiple attached candidates").
  - 칸에 직접 고른 부품의 `Door side`가 칸과 다르면 경고하고 **칸이 이긴다**("… slot … has the opposite DoorHandleSide / has DoorHandleSide …; slot wins").
  - Child Actor 컴포넌트로 넣으면 그 컴포넌트를 BP 안에서 **`HingePivot` 밑에** 둔다.
  - 손잡이 하나를 두 판이 같이 쓰면 먼저 연결한 판이 갖는다(경고).
- **판에는 손잡이 상자가 없다(P5x).** 잡을 곳은 손잡이·그립 BP뿐이다. 잠금을 켠 판에 손잡이가 하나도 없으면 Play 때 "Latched panel has no opening handle".
- **어느 문에도 연결 안 된 손잡이·그립은 조용히 동작하지 않는다**(그립은 잡히지 않음). Runner `Check setup`이 "unclaimed …"로 알려 준다.
- **운전석 사이드미러도 `BP_Door_FL` 밑에 붙인다.** Play 때 문의 `HingePivot`에 붙어 문과 같이 돈다. 문 밑에 없으면 차에 남는다 — Check setup이 "not under a door"로 알려 준다. 동승석 미러는 문 BP가 없어 차 원점에 두므로 그 줄은 무시한다.
- 판마다 **Check setup**(`Hinged panel | Setup`) 버튼도 있다. Runner `Check setup`이 모든 판의 결과를 모아 보여 주므로 보통은 Runner 것만 누른다.

### 8.2 기어 끝 버튼 → 레버

- Outliner로 붙이면 레버 Actor의 루트에 붙어서 레버가 돌아도 버튼이 제자리다. 그래서 `BP_GearLever` 안에 **Child Actor 컴포넌트**(클래스 `BP_GearButton`)가 `AxisPivot` 밑에 이미 있다. 레버가 돌면 버튼이 따라온다.
- 이 버튼은 단계와 연결하지 않는다(자유모드 부품).

### 8.3 Play 때 생기는 것 — 키·생성 버튼

- **키는 키 테이블이 Play 때 만든다**(`KeyClass`). 그래서 레벨에서 키에 태그를 달 수 없고, **키 BP의 Actor Tags**(`BP_KeyFob_Front` = `Car.FrontKey`, `BP_KeyFob_Rear` = `Car.RearKey`)가 태그다. 01·02단계가 `Car.FrontKey`를 쓴다. 키 메시는 키 BP에서 바꾼다.
- **리프트게이트 열기·닫기 버튼과 키 열림·잠금 버튼도 Play 때 만든다(P5zc).** 칸이 비면 엔진 Cube 버튼이다. 모양을 바꾸려면:
  1. 부모 `VBoRPokeButton`으로 버튼 BP를 만든다(예 `Main/Car/Parts/BP_LiftgateOpenButton`).
  2. `ButtonMesh`에 메시. **Actor 원점 = 누르는 면 가운데, +Z = 누르는 면이 바라보는 쪽.** 누름 깊이 `PressDepthMm`, 판정 확대 `HitAreaScale`도 이 BP에서.
  3. `BP_Liftgate`의 `Open button class`·`Close button class`, 키 BP의 `Unlock button class`·`Lock button class`에 고른다.
  - 자리·따라 움직이기·소리(§7.1)는 부모가 맡는다. 버튼 BP의 소리 칸과 `SpectatorName`은 덮어써진다.
  - **버튼 BP의 Class Defaults에 시나리오 태그를 넣지 않는다.** 그 BP로 생기는 버튼마다 태그가 붙어 `Tag …: found 3 (need 1)`로 흐름이 막힌다(2026-10-28 실제 사례). Runner `Check setup`이 어느 칸인지 알려 준다.

### 8.4 전원 → 시동 버튼·사이드미러

- **전원은 `BP_Car_Core`의 `VehiclePower` 하나다.** 0개면 시동 버튼·진행자 `E`·미러 연동이 없고(진행자 창 `Power` 꺼짐, "No vehicle power"), 2개 이상이면 전원이 꺼진다.
- **시동 버튼**은 Actor Tags `Car.StartButton`인 **poke 버튼 Actor**다. 여러 개면 모두 묶는다. 다른 종류 Actor나 컴포넌트에 태그를 달면 무시된다(Check setup이 알려 줌). 버튼이 없어도 진행자 `E`는 된다.
- **동작:** 시작은 꺼짐·미러 접힘(에디터에서는 펼친 자리로 보임). 누르면 켜지고 미러가 펼쳐지고, 다시 누르면 꺼지고 접힌다. 움직이는 중에 누르면 그 각에서 방향을 바꾼다. 진행자 "Power on/off"·`E`도 같은 명령이다. 진행자 `R`은 전원을 끄고 미러를 바로 접는다.
- **미러 `Follow vehicle power`(기본 켬, R3):** 끄면 그 미러는 시동을 무시한다. BP에서 `Fold`·`Unfold`로 다른 조건에 붙인다(§7.2 예). 켜 둔 채 `Unfold`로 편 미러는 시동을 켜도 그대로이고, 끄면 접힌다.
- `Car.StartButton`은 시나리오 단계가 쓰지 않는다. 평가 단계 중에 눌러도 미러만 움직인다.

## 9. 레벨 배치와 시나리오 태그

### 9.1 레벨에 놓는 것 — 이미 놓여 있음, 자리만 옮김

| 무엇 | 클래스 | 놓는 법 | 태그·연결 |
|---|---|---|---|
| 바닥 경로 — 차 둘레 | `AVBoRFloorRoute` | Spline 점을 바닥에(2개 이상) | `Route.AroundCar` — 03단계 |
| 바닥 경로 — 운전석 문까지 | `AVBoRFloorRoute` | 같음. 끝 점은 문 앞 | `Route.ToDriverDoor` — 07단계 |
| 앞·뒤 키 테이블 | `AVBoRKeyTable` | 바닥에. `KeyClass` = `BP_KeyFob_Front`·`_Rear` | 키 태그는 §8.3 |
| 박스 적재 영역 | `AVBoRLoadTarget` | 트렁크 안. `LoadingRegion` 상자를 트렁크 안 공간 크기로 | — |
| 박스 받침 | `AVBoRBoxStand` | 차 뒤 동승석 쪽 바닥 | `Car.BoxStand`, **`LoadTarget` = 위 적재 영역**(레벨 인스턴스에서 스포이트로), `BoxClass` = `BP_Box` |
| 안내판 표식 5개 | `AVBoRPanelAnchor` | §9.3 | `Guide.*` |

- **경로는 `Car.Footprint`를 지나면 안 된다.** 지나면 흐름이 시작되지 않는다. 경로 점은 실제 방 크기에 맞춰 옮긴다(플레이 영역 밖이면 경로를 숨김).
- 05단계(박스 적재)는 진행자 확인으로 끝난다(U-15).

### 9.2 시나리오 태그 표 — `Main/Data/DA_Main_Scenario`

| 단계 | 판정 | 대상 태그 | 대상 종류 |
|---|---|---|---|
| 01 | 키를 쥠(표시도 키) | `Car.FrontKey` | 키 |
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

- 태그가 없거나, 둘 이상이거나, 종류가 다르면 흐름이 시작되지 않는다. Output Log에 `Start blocked:` 아래 `Tag <이름>: found N (need 1)`, `<단계>: tag <이름> type mismatch or missing`으로 보인다. **Play 전에 Runner `Check setup`으로 같은 문구를 본다**(§3-9).
- 시나리오 에셋은 하나만 쓴다.

### 9.3 안내판 자리 — `Car/Guidance`의 표식 (P5z)

- **안내판은 단계마다 정한 자리에만 뜬다.** 확인·시점 버튼은 안내판 바로 밑에 붙는다.
- 자리는 **위치 표식**(`AVBoRPanelAnchor`)이 정한다. `L_Main` Outliner `Car/Guidance`에 5개가 있다. 처음 자리는 임시 값이다 — 원하는 곳으로 옮긴다.

| 표식(Actor Tags) | 단계 |
|---|---|
| `PanelAnchor_Outside` (`Guide.Outside`) | 01·02·03·07·08·19·20 |
| `PanelAnchor_Trunk` (`Guide.Trunk`) | 04·05·06·16 |
| `PanelAnchor_Driver` (`Guide.Driver`) | 09·10·11·12·13·18 |
| `PanelAnchor_Passenger` (`Guide.Passenger`) | 14 |
| `PanelAnchor_Rear` (`Guide.Rear`) | 15 |

- **옮기는 법:** 에디터에서만 보이는 얇은 판이 안내판 크기(50×32 cm), 그 밑 작은 상자 두 개가 버튼 자리다. **노란 화살표(+X)가 참가자 쪽을 보게** 돌린다. 기울여도 된다. 버튼은 **손이 닿는 자리**에 두고, 13번 도달 버튼과 떨어뜨린다.
- **단계와 표식 연결:** `DA_Main_Scenario` → 단계 → `PanelTag`에 표식의 Actor Tag. `PanelTag`가 빈 단계는 직전 자리에 뜬다. 첫 단계는 반드시 태그가 있어야 한다. 못 찾거나 둘 이상이면 `panel tag … type mismatch or missing`.
- 안내판에는 안내문만 나온다. **자유모드에서는 안내판과 버튼이 안 뜬다**(좌석은 진행자 키 `1`·`2`·`Home`).

## 10. 손 메시 바꾸기 — `BP_vBoRPawn`

- `Main/Core/BP_vBoRPawn` → Class Defaults → 손 표시: `LeftHandMesh`·`RightHandMesh`·`LeftHandMaterial`·`RightHandMaterial`. 비우면 Meta 기본 손이다.
- **본 이름이 Meta 손(`XRHand_Palm`·`XRHand_Wrist`…)과 같아야** 손가락이 따라 움직인다. 다르면 기본 손으로 돌아가고 Warning이 한 번 뜬다.

## 11. 넣은 뒤 확인 — 순서대로

1. **Runner `Check setup`** → `No issues`(또는 일부러 둔 줄만)일 것(§3-9).
2. **Play(PIE) 한 번** — Output Log:
   - `Template Mismatch during attachment`(ensure)가 없을 것.
   - `P2e setup:` 경고가 없을 것(손잡이 연결·칸 불일치).
   - 메시로 잡기·밀기 부품에서 `grab mesh …`·`DoorMesh has no traceable collision` 경고가 없을 것.
   - 리프트게이트 판정 근거가 `N obstruction boxes`일 것(§6.5).
   - `Start blocked:`가 없을 것.
3. **진행자 창**(툴바 `Operator`):
   - **4점 정렬:** 실물 좌석 점 4개를 `P`로 짚고 → 차이(cm)·높이 차이 확인 → `S` 저장 → 다시 짚어 거리 확인 → `A`로 시작. 높이 경고가 뜨면 실물 좌석 높이를 확인한다(U-17).
   - **좌석 전환:** `1`·`2`로 동승석·2열에 가서 SgRP 차이만큼 옮겨졌는지, `Home`으로 돌아오는지.
4. **부품마다** — 뷰포트 표시(§3-4·§3-5)와 실제가 같은지 본다:
   - 열리는 방향·최대 각, 손잡이·그립·운전석 미러가 문과 같이 도는지, 기어 버튼이 레버를 따라오는지, 리프트게이트 버튼, 2열 F·U, 키 집고 버튼 누르기.
   - **문:** 운전석에서 안쪽 손잡이를 잡으러 갈 때 문이 먼저 밀려 나가지 않는지(§6.4), 바깥 면·안쪽 트림을 밀어 보이는 면에 닿을 때 밀리는지(§6.3). 그립으로 연 문을 밀고 당기는지, 잠긴 동안 그립이 반응하지 않는지.
   - **선바이저:** 잡아 내리고 놓은 자리에 남는지, 앞·뒤 면을 밀 때 손가락이 닿는 순간 움직이는지(§6.6), 잡으려다 밀려 버리지 않는지.
   - **박스:** 트렁크에 놓아 바닥에 착지하는지, 리프트게이트를 닫을 때 트렁크 안쪽 박스에는 다시 열리지 않는지(§6.5). `BP_Box` 메시를 바꿨으면 받침 위에 뜨거나 묻히지 않는지.
   - **시동·미러:** 누르면 두 미러가 펼쳐지고 다시 누르면 접히는지, 문을 연 채로도. 진행자 `E`·`R`. 미러가 접히며 차체와 겹치지 않는지.
   - **문 관성을 켰다면:** 툭 밀기, 손잡이로 당겨 놓기, 밀어 닫기(잠기는지), 밀다가 손을 시야 밖으로 빼기(서는지).
   - **소리:** 넣은 칸마다 한 번씩 들어 본다. 감쇠를 넣은 칸이 부품 쪽에서 들리는지.
   - **BP 표현을 붙였다면:** 이벤트에 맞춰 나오는지, 진행자 `R`로 되돌아가는지(§7.2).
5. **자동화 시험** `vBoR.*`(Agent-Workflow §2.4). `Approx`를 지운 뒤에는 `vBoR.P4i.E2`·`P5t.E6`이 근사 비교를 건너뛴다(Info). 나머지 P4i(BP 부모, 차 원점 배치, 정렬 점 4개, 연결, 참조 규칙)는 통과해야 한다. `vBoR.P5p.*`가 실패하면 선바이저 경첩 방향(§5), `vBoR.P5q.E13`은 전원·시동 버튼·미러 L·R과 L의 부모를 본다.
6. **VR(Quest 3 Link):** 계획서 §7 시험 ID로 기록한다. 확인 전까지 `not-tested`.

막힌 곳은 부품 이름, Output Log 줄, Check setup 결과, 진행자 창 이유를 그대로 적어 Claude에게 넘긴다. Claude가 명세로 고친다.
