# P3b 바깥 안내 — 바닥 경로(화살표 + 도착 고리), 도착·좌석 때 안내판 방향, P3 보류 3건

| 항목 | 값 |
|---|---|
| 단계 | P3 안내 흐름 보강(바깥 이동 구간). 계획서 §3 "03. 외관을 둘러보세요를 안내하고 경로나 관찰 위치로 유도" |
| 결정 근거 | **사용자 결정(2026-10-17):** 바닥 안내 형태는 **A안**(예시 페이지 https://claude.ai/artifact/VPcQgK2hVUQBxqLU6tAcay — 진행 방향 V 화살표가 앞으로 차례로 밝아지고 끝에 노란 도착 고리, 지금 단계의 경로만 보임). **사용자 결정(2026-10-13):** 시나리오·3D 위젯·안내 세부는 "바깥 안내" 명세에 묶는다 — P3 보류 3건(진행자 판이 버튼을 덮음 R2-1, "시작 위치"가 헤드셋을 쓰기 전 위치일 수 있음, 앉았을 때 안내판이 그 순간 머리 Yaw를 씀). **U-10:** 시작은 차 앞, 차를 바라봄. **AGENTS.md:** 안내는 시각·청각으로만, 머리를 따라다니는 패널 없음(P3 R1-7) |
| 베이스 브랜치 | `main` @ `1ad2547`(P5j 머지 `e9df71c` + U-12 문서) 뒤 이 명세 커밋 |
| 작업 브랜치 | `codex/P3b-outdoor-guidance` |
| 리뷰 파일 | `Docs/Reviews/P3b-outdoor-guidance.md` (첫 리뷰 때 생김) |
| 상태 | 명세 — 구현 전 |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **바닥 경로 액터** | 새 클래스 `AVBoRFloorRoute`(`Interaction/`). 루트는 `USplineComponent`(엔진 `Runtime/Engine/Classes/Components/SplineComponent.h:222`)이고, 레벨에 놓고 점을 끌어 모양을 고친다. 찾는 이름표는 Actor 태그다(예 `Route.ToDriverDoor`). 경로가 자기 화살표와 도착 고리를 그린다(D4). 보이기·숨기기 함수만 밖에 연다. 색 재질은 키 버튼과 같은 방법(ISDK `GrabbableDefault` + `ColorParam`, `Interaction/VBoRKeyFob.cpp:87-91`)이다. 새 에셋은 만들지 않는다. 메시는 엔진 기본 큐브다 |
| D2 | **단계가 경로를 가리킴** | `FVBoRScenarioStep`에 `RouteTag`(FName, 생략 가능)를 더한다(`Interaction/VBoRScenarioAsset.h:25`의 `MarkerTag` 옆). 흐름 시작 검사(`InitializeFlow`, `Interaction/VBoRScenarioRunner.cpp:192-194`의 `MarkerTag` 검사와 같은 곳)에서 태그가 `AVBoRFloorRoute` 하나에만 맞는지, 점이 둘 이상인지 본다. 아니면 `ValidationProblems`에 단계 ID와 함께 적는다(지금 태그 규칙과 같음 — 흐름이 시작하지 않음). **생성자 기본 단계는 바꾸지 않는다**(`Docs/Reviews/P3-guided-flow.md:62-63`, P4a D8). 07에 경로를 다는 것은 사용자가 `DA_P3_Scenario`에서 한다(§7-1) |
| D3 | **보이는 때 — 지금 단계 하나만** | 흐름 진행 중(`bRunning`), 자유모드 아님, 지금 단계에 `RouteTag`가 있음, 아직 도착 안 함(D5), 플레이 영역 검사 통과(D6-b)일 때만 그 경로가 보인다. 다른 경로는 모두 숨긴다. 경로가 보이는 동안에는 그 단계의 `MarkerTag` 표시(고리 네 조각·대상 위 화살표, `:556-571` `UpdateMarker`)를 숨긴다. 대신 지금 화살표(`MarkerArrow`)를 **도착 고리 위 160 cm**에 아래로 향해 둔다(멀리서 보이는 표시 — 예시 페이지 추천의 "빛기둥" 대신 지금 있는 것을 씀). `RouteTag`가 없는 단계의 표시는 지금 그대로다 |
| D4 | **모양(1080·cm 자리값, 모두 프로퍼티)** | 예시 페이지 A안 값. **화살표:** V 모양(폭 32 cm, 앞뒤 길이 22 cm, 띠 두께 8 cm), 경로 방향을 향함, 간격 38 cm, 첫 화살표는 경로 시작에서 25 cm, 마지막은 고리 중심에서 40 cm 앞까지. 바닥(스플라인 점의 Z)에서 1 cm 위(같은 평면이면 깜빡임 — P0-c 격자 경험). 색 `#0fa7c4`. **흐름 표시:** 앞으로 차례로 밝아진다 — 주기 1.6 s, 화살표마다 0.14 s 늦게, 밝을 때 100%·어두울 때 35% 밝기. **도착 고리:** 스플라인 끝점 중심, 반지름 30 cm(지름 60 cm), 띠 두께 4 cm, 둥글게 보이게(조각 수는 구현자), 1 cm 위, 색 `#e7a91f`. 색은 sRGB hex → `FLinearColor::FromSRGBColor(FColor::FromHex(...))`(엔진 `Runtime/Core/Public/Math/Color.h:123`·`:573`). 화면에 "근사 형상·자리값"임을 드러낸다(주석·로그). 계산에 쓸 스플라인 함수는 `GetSplineLength`(`SplineComponent.h:706`), `GetLocationAtDistanceAlongSpline`(`:737`), `GetDirectionAtDistanceAlongSpline`(`:745`), 점 수·점 위치 `:619`·`:635` |
| D5 | **도착** | 추적된 머리의 XY가 고리 중심에서 반지름 안에 **누적 0.5 s**(자리값) 있으면 도착이다. 추적이 끊긴 frame은 세지 않고(보류), 추적된 채 고리 밖에 나가면 0으로 돌린다. 도착하면 ① 경로(화살표·고리)를 숨기고 ② 그 단계의 `MarkerTag` 표시를 지금처럼 보이고 ③ 안내판·버튼을 한 번 다시 놓는다(D7, 확인 잠금 `ConfirmLockSeconds` 그대로). 도착은 단계에 들어갈 때마다 처음부터다(이전·처음부터·다음 단계로 다시 들어오면 경로가 다시 보임). 도착 뒤 고리를 나갔다 와도 경로를 다시 켜지 않는다(깜빡임 방지). 도착은 단계 완료가 아니다 — 완료는 지금 규칙이 정한다 |
| D6 | **안전 검사 둘** | **(a) 차체 영역 — 흐름 시작 검사.** 근사 차·실제 레벨에 `Car.Footprint` 태그 `UBoxComponent` 하나(차체 + 여유). `RouteTag`가 있는 단계가 하나라도 있으면 이 상자가 정확히 하나여야 한다. 경로를 10 cm 이하 간격으로 샘플링한 점, 고리 둘레 점, **도착 때 안내판 자리**(고리 중심 + 도착 방향 × `PanelDistanceCm`)가 상자 안(상자 변환 기준 XY)에 있으면 `ValidationProblems`(예 `07: 바닥 경로 Route.ToDriverDoor가 차체 영역을 지남`). 이유: 참가자는 가상 차를 통과해 걷고, 차 자리에는 실물 Buck이 있다. **(b) 플레이 영역 — 단계 시작 때.** 경로를 보이기 전에 같은 샘플 점과 고리 둘레 점을 Stage 좌표로 바꿔(Stage 원점 = Pawn 변환 — 머리 = HMD 자세 × Pawn, `Interaction/VBoRVehicleAlignmentComponent.cpp:206-215`) 플레이 영역 안쪽 여유 30 cm(자리값) 안인지 본다. 플레이 영역은 `UHeadMountedDisplayFunctionLibrary::GetPlayAreaBounds(Stage)`(엔진 `Plugins/Runtime/XRBase/Source/XRBase/Public/HeadMountedDisplayFunctionLibrary.h:403`)로 읽는다. OpenXR에서 이 값은 Stage 원점 가운데의 **전체 크기**(cm)이고, UE 축(X 앞·Y 오른쪽)에 맞춰 너비·깊이를 바꿔 준다(엔진 `Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.cpp:477-486`) — 안쪽 = \|x\| ≤ X/2 − 여유, \|y\| ≤ Y/2 − 여유. **밖이거나 영역 값이 0이면(모름) 그 단계의 경로를 그리지 않고** 진행자에게 알린다(D10). 그 단계는 경로가 없는 단계처럼 진행한다(`MarkerTag` 표시, 도착 다시 놓기 없음). 벽으로 걷게 하는 안내는 위험하므로 그리지 않는 쪽이다 |
| D7 | **안내판 방향 — 그 자리의 정면** | 안내판·버튼을 놓을 때(`PlacePanelAtHead`, `:418-433`) **방향(Yaw)만** 아래로 정한다. 위치는 지금처럼 머리 + 방향 × 거리다(R1-3 거리 70·45 cm, 높이 그대로). ① **도착 때:** 경로 끝 방향(스플라인 끝 접선의 XY). ② **앉아 있거나 다른 좌석 시점일 때**(좌석 영역 안 `Seat->IsHeadInside()`, `Interaction/VBoRParticipantZones.h:39`, 또는 기준 시점이 아님): 지금 시점 eye의 정면(기준이면 `BaseEye`, 동승석·2열이면 그 시점 컴포넌트). 앉음 다시 놓기(`:445-447`), 시점 전환 뒤 다시 놓기(`:413-416`), 앉은 채 단계 시작이 모두 여기에 든다. 지금 시점 eye를 읽는 함수를 정렬 컴포넌트에 하나 더한다(`CurrentViewpoint`는 private, `Interaction/VBoRVehicleAlignmentComponent.h:52`). ③ **그 밖(서 있음):** 지금처럼 그 순간 머리 Yaw. **안내판 위치를 레벨 점(앵커)으로 따로 두지 않는다** — 도착 때 다시 놓기로 대신한다(레벨에 점을 더 두지 않고, 높이가 참가자 눈높이를 따름) |
| D8 | **진행자 정보 판(I 켬)은 문구 패널 옆 (P3 R2-1)** | I를 켜면 진행자 판(50 × 35 cm)이 지금은 버튼과 같은 45 cm 평면에 놓여 버튼·이름을 덮는다(`:517-521`, `Docs/Reviews/P3-guided-flow.md:256-280`). I 켬 판은 **문구 패널 옆**(같은 거리·같은 높이, 판 사이 2 cm 이상)에 둔다. 옆은 앉아 있거나 다른 좌석 시점이면 차 가운데 쪽, 서 있으면 오른쪽이다(구현자가 시험으로 고름). I 끔 한 줄은 지금 자리 그대로다. 기준은 P3 E22(`Docs/Specs/P3-guided-flow.md:224`) — eye에서 버튼·버튼 이름·문구 패널 중심까지의 선분이 진행자 판 사각형과 만나지 않고, 진행자 판은 차 판에 가리지 않는다 |
| D9 | **"시작 위치"는 시작 표시 위에서 (P3 보류)** | 지금은 흐름이 시작하고 처음 추적되는 frame에 기록한다(`:253`, `:450`). 그래서 헤드셋을 쓰기 전(책상 위·손에 든 채)의 위치일 수 있다. 바꾼다: `Car.Start` 태그 컴포넌트가 있으면, 첫 단계 동안 **추적된 머리의 XY가 그 표시 중심에서 50 cm 안이고, 머리가 표시보다 100 cm 이상 높을 때**(자리값 둘 — 서서 쓰고 있음) 처음 한 번 기록한다. 첫 단계가 끝날 때까지 한 번도 안 되면 그 순간에 `시작 위치 — 시작 표시를 거치지 않음 | <좌표>`로 기록한다. `Car.Start`가 없는 레벨은 지금처럼 첫 추적 frame이다. OpenXR 착용 상태는 쓰지 않는다 — `GetHMDWornState`는 세션 준비 여부만 돌려주고(엔진 `OpenXRHMD.h:402`), 착용 이벤트는 런타임이 `XR_EXT_user_presence`를 줄 때만 온다(`OpenXRHMD.cpp:4042-4055`, Quest Link에서 확인 안 함) |
| D10 | **진행자 알림** | D6-b로 경로를 그리지 않은 단계에서는 진행자 창 상태 패널에 한 줄 `Floor route hidden — outside the play area`(영역 값을 모르면 `— play area unknown`), VR 안 진행자 줄(I)에 `바닥 경로 숨김 — 플레이 영역 밖`(또는 `— 영역 모름`)을 보이고 한 번 기록한다. 진행자 창은 상태 구조체에 알림 칸 하나와 그 줄만 더한다(`Interaction/VBoRScenarioOperator.cpp:192`, 표시 `Interaction/VBoROperatorPanel.cpp:159-167` 근처, 색 `#f2b544`). P5i의 다른 줄·버튼·판정은 바꾸지 않는다 |
| D11 | **근사 차** | ① `Car.Footprint` 상자: 차체(`BodySizeCm`, `P3Probe/P3VehicleMock.h:30`) + 여유 20 cm(자리값, 높이는 차 높이), 충돌 없음. ② 시작 표시(`StartMarker`, `P3Probe/P3VehicleMock.cpp:44`·`:83`)에 `Car.Start` 태그. ③ 경로 하나 `Route.ToDriverDoor`를 `BeginPlay`에서 만든다(키 테이블과 같은 곳, `:92-93`). 점은 차 기준 cm 배열 프로퍼티(편집 가능), 기본 (360, −20, 0) → (330, −150, 0) → (150, −165, 0) → (−65, −165, 0) — 차 앞에서 운전석 쪽으로 돌아 바깥 손잡이(`OutsideHandlePivotCm` (−65, −101.5, 97), `.h:47`) 옆 바깥에서 끝남. 끝 방향은 −X(차 옆을 따라 뒤쪽)라 도착 안내판이 차체 밖이다. 러너는 첫 `Tick`에서 태그를 검사하므로(`:308`) `BeginPlay`에서 만든 경로를 찾는다. 단계에는 코드로 연결하지 않는다(D2) |

## 1. 왜 만드는가

- 바깥 단계(01–07, 하차 뒤 19–20)에서 참가자는 차 둘레를 걷는다. 지금은 대상 위 화살표와 바닥 네 조각 고리뿐이라, 차를 돌아가는 길이 보이지 않고 가상 차를 가로질러 걸을 수 있다.
- 안내판은 단계 시작 때 그 자리에 놓이고 따라오지 않는다(R1-7). 걸어간 끝에서 확인 버튼이 뒤에 남는다. 도착 때 한 번 다시 놓는다.
- P3 보류 3건은 같은 "안내 배치" 문제다. 진행자 판이 버튼을 덮는 것(R2-1), 앉을 때 고개 방향으로 안내판이 옆에 놓이는 것, 시작 위치 기록이 헤드셋 착용 전일 수 있는 것.

## 2. 범위

### 만들 것

**1. 바닥 경로** — D1–D6.
**2. 안내판 방향** — D7.
**3. 진행자 정보 판 자리** — D8.
**4. 시작 위치 기록** — D9.
**5. 진행자 알림** — D10.
**6. 근사 차** — D11.
**7. 자동화 시험** — §4 전부.

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| 01·03–06 경로 연결, 01 규칙 바꾸기 | 다음 명세 P4 골격(U-10의 키·리프트게이트·박스 단계를 데이터에 연결). 이 명세는 장치와 근사 차 경로 하나 |
| 생성자 기본 단계 바꾸기, 에셋·레벨 저장 | D2. 07 경로 태그는 사용자가 에셋에서 |
| 다음 단계 경로 미리 보이기, 자동 길찾기 | A안: 지금 단계만. 경로는 사람이 그린다 |
| 시야 밖일 때만 빛기둥 | D3 — 지금 화살표를 고리 위에 둠 |
| 단계별 안내판 앵커 태그 | D7 — 도착 때 다시 놓기로 대신 |
| OpenXR 착용 감지 | D9 — Link에서 확인 안 됨 |
| 소리, 차 안 바닥 안내 | 차 안은 안내판으로 충분(예시 페이지 규칙) |
| 진행자 창의 다른 부분, 관전 화면 | P5i·P5k |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- 기존 `vBoR.*`가 모두 통과한다. 기존 단언은 바꾸지 않는다. **예외 둘(사용자 결정으로 동작이 바뀜):**
  - `P3Probe/P3FlowTests.cpp:461` — "시작 위치" 기록 시점(D9). 머리를 시작 표시 위에 두는 식으로 고친다.
  - `P3Probe/P3FlowTests.cpp:512` — 좌석에 다시 들어올 때 고개 90°이면 지금은 고개 방향에 놓는다. 좌석 정면(D7)으로 고친다.
- `RouteTag`가 없는 단계의 표시·안내판 동작은 지금 그대로다(P3 E21·E22, R1-7 사건 다시 놓기). 머리를 따라다니는 배치를 만들지 않는다. 새 사건은 "도착" 하나다.
- 안내판·버튼 크기·거리·높이(R1-3: 70 cm·45 cm·눈 아래 30 cm), 확인 잠금 0.6 s, 13번 버튼과 판정은 그대로다.
- 경로는 플레이 영역 밖으로 그리지 않고, 차체 영역을 지나지 않는다(D6).
- 생성자 기본 단계는 그대로다(D2).
- 진행자 창은 알림 한 줄만 더한다(D10). P5i 시험·P5k 시험은 그대로 통과한다.

## 4. 경계 상황과 그 시험

| # | 상황 | 기대 동작 | 자동화 시험 |
|---|---|---|---|
| E1 | 지금 단계에 경로 있음 / 없음 / 자유모드 / 시작 전 | 그 경로만 보임 / 경로 없음, `MarkerTag` 표시 지금대로 / 모두 숨김 / 모두 숨김 | 보임 상태(합성 경로 둘) |
| E2 | 화살표 배치 | 첫 25 cm, 간격 38 cm, 고리 중심 40 cm 앞까지. 각 화살표 방향 = 그 자리 스플라인 방향(1° 안). Z = 점 Z + 1 cm. 폭 32·길이 22 cm | 컴포넌트 위치·회전·bounds |
| E3 | 도착 고리 | 끝점 중심, 반지름 30 cm, 1 cm 위. 지금 화살표가 고리 위 160 cm에 아래로. 경로가 보이는 동안 `MarkerTag` 고리 숨김 | 위치·보임 |
| E4 | 흐름 표시 | 시각 t에서 화살표 i가 밝으면 t + 0.14 s에 i+1이 밝음(앞으로 진행). 밝기 100%·35% 범위 | 색 값(시각 두 개) |
| E5 | 머리가 고리 안 0.4 s / 0.5 s, 중간 추적 끊김, 도착 뒤 나갔다 들어옴 | 0.4 s 아직 / 0.5 s 도착 → 경로 숨김·`MarkerTag` 표시·다시 놓기 한 번 / 끊긴 frame은 안 셈 / 다시 켜지지 않음 | 합성 머리 |
| E6 | 도착 다시 놓기 | 머리 Yaw 90°여도 안내판 = 머리 + 경로 끝 방향 × 70 cm, 버튼도 같은 방향. 0.3 s 안 참가자 확인 버림, 0.7 s 받음 | 위치·확인 |
| E7 | 다음 단계·이전·처음부터 | 새 단계 경로만 보임. 같은 단계로 다시 들어오면 도착 초기화(경로 다시 보임) | 보임·도착 |
| E8 | 플레이 영역: 경로 점 하나가 여유 밖 / 영역 0 / 모두 안 | 그 경로 숨김 + 진행자 창 알림 줄(영어)·VR 진행자 줄(한국어)·기록 한 번, `MarkerTag` 표시 / 숨김 + `play area unknown` / 보임, 알림 없음 | 합성 XR 영역 |
| E9 | 차체 영역: 경로가 지남 / 고리가 안 / 도착 안내판 자리가 안 / `Car.Footprint` 없음·둘 / 경로 태그 없음·둘·점 하나 | 모두 `ValidationProblems`, 흐름 시작 안 함(단계 ID·태그 이름이 문구에 있음) | 검사 문구 |
| E10 | 근사 차 기본값 | `Car.Footprint`·`Car.Start`·`Route.ToDriverDoor` 하나씩. 경로가 D6 두 검사(영역은 충분히 큰 합성 영역)를 통과. 끝점이 바깥 손잡이에서 XY 70 cm 안, 차체 영역 밖 | 태그·검사 |
| E11 | 안내판 방향 | 앉아 고개 90° → 좌석 정면(`P3FlowTests.cpp:512` 고침). 동승석·2열 전환 뒤 고개 30° → 그 시점 정면. 서서 단계 시작 고개 30° → 머리 방향(지금 그대로) | 위치·회전 |
| E12 | I 켬·끔, 운전석·동승석·2열 | eye → 두 버튼 중심·두 이름 중심·문구 패널 중심 선분이 진행자 판 사각형(DrawSize × scale)과 안 만남. 진행자 판 중심까지 선분이 차 판과 안 만남 | 선분–상자(`FMath::LineBoxIntersection`, 엔진 `Runtime/Core/Public/Math/UnrealMathUtility.h:1919`) |
| E13 | 시작 위치 | 추적됐지만 표시 밖 또는 높이 부족 → 기록 없음 / 표시 위 50 cm 안·100 cm 이상 → 한 번 / `Car.Start` 없는 레벨 → 첫 추적 frame(지금대로) / 첫 단계가 표시 없이 끝남 → `시작 표시를 거치지 않음` 기록 | 기록 문구(`P3FlowTests.cpp:461` 고침) |
| E14 | 기존 시험 | §3의 두 곳 말고 모두 그대로 통과 | 전체 시험 |

## 5. 구현자가 정할 것

- 경로 액터의 화살표·고리 컴포넌트 구성(조각 수, 컴포넌트 풀, 밝기를 바꾸는 방법). 화살표가 30개쯤이면 화살표마다 동적 재질을 둬도 된다.
- 도착 판정·플레이 영역 검사를 러너와 경로 중 어디에 둘지(러너가 머리·Pawn을 가짐).
- I 켬 판의 옆 방향 고르는 규칙(D8의 가림 시험을 통과하면 됨).
- 프로퍼티 이름, 진행자 창 알림 줄의 정확한 자리.

## 6. 완료 조건

1. §2의 1–7이 들어 있다.
2. §4 시험이 모두 자동화로 통과한다. 기존 `vBoR.*`도 통과한다(§3 두 곳은 새 기대값으로).
3. non-unity(`-Module=vBoR`)·unity 빌드에서 vBoR 경고·오류가 0이다. 전체 시험은 최종 트리로 한 번 돌린다.
4. AGENTS.md 보고 ①–⑤를 커밋 본문과 `HANDOFF.md`에 남긴다.
5. 시험 상태는 `not-tested`다.

## 7. 로컬에서 확인할 것 — 사용자

1. **에디터:** `DA_P3_Scenario`의 07 단계 `RouteTag`에 `Route.ToDriverDoor`를 넣고 저장한다(생성자 기본값은 바꾸지 않음 — D2).
2. **작은 방(약 2.0 × 2.5 m):** 근사 차 경로는 방보다 크다. 07에서 진행자 창·VR 진행자 줄에 "경로 숨김 — 플레이 영역 밖"이 뜨는지 본다. 걸어 보려면 근사 차의 경로 점 프로퍼티를 방 안에 맞게 줄인다(차체 영역 검사는 그대로 받는다).
3. **회사 방(차 둘레를 걸을 수 있음):** 07 경로를 걸어 본다. 화살표가 1–3 m 앞에서 잘 읽히는지, 밝아지는 방향이 진행 방향인지, 고리와 그 위 화살표가 멀리서 보이는지, 도착하면 꺼지는지, 도착 안내판이 손잡이를 가리지 않는지 본다.
4. **앉아서:** 13에서 옆으로 기댔다 돌아오고, 좌석에서 나갔다 다시 앉아 본다. 안내판이 좌석 정면에 놓이는지 본다.
5. **I 켬:** 운전석·동승석·2열에서 I를 켜고, 진행자 판이 확인·이동 버튼과 이름을 가리지 않는지 본다.
6. **시작 위치:** 헤드셋을 손에 든 채 레벨을 열고, 시작 표시에 서서 쓴다. 기록의 "시작 위치"가 표시 위에서 쓴 뒤에 나오는지 본다.

## 8. 확인한 API 근거

경로: `Source/vBoR/` 아래(`main` @ `1ad2547`), 엔진은 `C:/Program Files/Epic Games/UE_5.7/Engine/` 아래.

| 쓸 것 | 위치 | 쓰임 |
|---|---|---|
| 스플라인 | 엔진 `Source/Runtime/Engine/Classes/Components/SplineComponent.h:222`(클래스), `:570`(`SetSplinePoints`), `:619`·`:635`(점 수·점 위치), `:706`(`GetSplineLength`), `:737`·`:745`(거리별 위치·방향) | D1·D4·D11 |
| 플레이 영역 | 엔진 `Plugins/Runtime/XRBase/Source/XRBase/Public/HeadMountedDisplayFunctionLibrary.h:403`, `Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.cpp:444-487`(전체 크기, `:485` 축 바꿈), `Source/Runtime/HeadMountedDisplay/Public/IXRTrackingSystem.h:451`(기본 0) | D6-b |
| 착용 상태(쓰지 않는 이유) | 엔진 `Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.h:402`, `OpenXRHMD.cpp:4042-4055` | D9 |
| 선분–상자 | 엔진 `Source/Runtime/Core/Public/Math/UnrealMathUtility.h:1919` | E12 |
| 색 변환 | 엔진 `Source/Runtime/Core/Public/Math/Color.h:123`·`:573` | D4 |
| 색 재질 | `Interaction/VBoRKeyFob.cpp:87-91`(`GrabbableDefault` + `ColorParam`) | D1 |
| 단계 칸·태그 검사 | `Interaction/VBoRScenarioAsset.h:25`(`MarkerTag`), `Interaction/VBoRScenarioRunner.cpp:26-59`(`FindP3Tag`·`P3Actor`·`P3Scene`), `:192-194`(표시 태그 검사), `:308`(첫 Tick에 검사) | D2·D11 |
| 표시·안내판 | `Interaction/VBoRScenarioRunner.cpp:92-100`(화살표·고리 조각), `:405-433`(놓기), `:442-459`(머리 표본·앉음·시작 기록), `:512-554`(`RefreshPanel`, 진행자 판 `:517-521`), `:556-571`(`UpdateMarker`), `:480-502`(진행자 줄), `.h:94-97` | D3·D5·D7·D8·D10 |
| 시작 기록 | `Interaction/VBoRScenarioRunner.cpp:253`, `:450` | D9 |
| 좌석·시점 | `Interaction/VBoRParticipantZones.h:32-41`, `Interaction/VBoRVehicleAlignmentComponent.h:25`·`:38`·`:52`, `.cpp:206-215`(`ReadWorldHead`) | D6-b·D7 |
| 진행자 창 | `Interaction/VBoRScenarioOperator.cpp:192`(`GetOperatorStatus`), `Interaction/VBoROperatorPanel.cpp:159-173` | D10 |
| 근사 차 | `P3Probe/P3VehicleMock.h:30`·`:47`·`:100-101`, `P3Probe/P3VehicleMock.cpp:44`·`:83`·`:92-93` | D11 |
| 고칠 기존 단언 | `P3Probe/P3FlowTests.cpp:461`, `:512` | §3 |
| 생성자 기본 단계 고정 | `Docs/Reviews/P3-guided-flow.md:62-63`, `Docs/Specs/P4a-key-table.md:23` | D2 |

## 개정 이력

| 날짜 | 개정 | 내용 |
|---|---|---|
| 2026-10-17 | 처음 | 작성(사용자 결정: 바닥 안내 A안. 2026-10-13 묶음: P3 보류 3건). 단계별 안내판 앵커 대신 도착 때 다시 놓기(Claude 판단 — 사용자에게 알림) |
