# P4h 4점 정렬 — 실물 좌석 점을 손끝으로 짚어 저장, 좌석 전환은 좌석점 차이

| 항목 | 값 |
|---|---|
| 단계 | P4 50% 통합 — 산출물 레벨(L_Main)에서 Buck 설치 정렬. U-17(높이 바닥)·U-18(앞뒤·좌우·Yaw는 점 4개) |
| 결정 근거 | **사용자 결정(2026-10-20):** 볼트가 아니어도 실물 좌석의 점 4개에 맞춘다. 좌석 전환(14·15)은 운전석·동승석·2열 좌석 기준점(SgRP)으로 한다. 근거와 오차 크기는 조사 기록 2026-10-20 |
| 베이스 브랜치 | `main` @ `9d700a9`(U-18 문서) + 이 명세 커밋 |
| 작업 브랜치 | `codex/P4h-point-alignment` |
| 리뷰 파일 | `Docs/Reviews/P4h-point-alignment.md` (첫 리뷰 때 Claude가 만듦) |
| 상태 | 구현(`8d51f82`) — 리뷰 1회차 R1-1–R1-3 반영(`b25535f`) |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **정렬 점 컴포넌트** | 새 `UVBoRAlignPointComponent : USceneComponent`(`Source/vBoR/Interaction/`, `BlueprintSpawnableComponent`). 프로퍼티는 둘이다.<br>- `Order`(int32, 1부터): 짚는 순서.<br>- `Label`(FString): 진행자 창에 보일 이름. 예: `Front-left rail end`.<br>- 차 Actor에 붙은 정렬 점을 **모두** `Order` 순서로 쓴다. 2개 이상이어야 한다. `Order`가 겹치면 설정 오류다(§4 E11).<br>- 표시용으로 작은 구와 번호를 가진다. **게임에서는 기본 숨김**(D8에서만 보임), **에디터에서는 보인다**(CAD 메시 자리에 붙이게 — 리뷰 1회차 R1-2). 충돌·태그는 없다.<br>- 회사 차는 차량 Blueprint에 이 컴포넌트를 넣고, 에디터에서 CAD 메시의 그 자리에 붙인다. 숫자를 옮기지 않으므로 CAD 축 변환을 틀릴 일이 없다 |
| D2 | **손끝 기록** | 진행자가 `P`를 누르면 다음 순서의 점 하나를 기록한다.<br>- `PointCaptureSeconds`(기본 1.0 s) 동안 `PointHand`(기본 왼손 — P0-c 실측에서 반복성이 확인된 손) 검지 끝 **원본**을 읽는다. `AVBoRHandRigPawn::ReadRawIndexTip`이고, poke limiter가 옮긴 손이 아니다.<br>- 읽은 world 위치를 **Pawn 변환의 역으로 Stage 좌표**로 바꿔 모은다. 그래서 지금 정렬·집 시험 이동(HomeShift)과 상관이 없다.<br>- 창 길이는 초로 센다(15 fps와 90 fps에서 같음, 머리 저장 창과 같은 방식 `VBoRBuckAlignment.cpp:96-105`).<br>- 한 frame이라도 추적되지 않으면 **그 점만** 취소하고 같은 순서에 머문다.<br>- 흩어짐(창 안 두 표본 사이 최대 거리)이 `PointSpreadLimitMm`(기본 10 mm)를 넘으면 그 점을 거절한다.<br>- 성공하면 평균 Stage 위치를 그 점의 값으로 두고 다음 순서로 간다 |
| D3 | **풀이 — XY 강체 최소제곱, Z는 바닥** | 기록한 Stage 점 pᵢ와 차 정렬 점의 world 위치 wᵢ(XY만)로 Yaw θ와 이동 t를 구한다.<br>- 중심: p̄, w̄. 편차: p'ᵢ = pᵢ − p̄, w'ᵢ = wᵢ − w̄.<br>- θ = atan2(Σ(p'ₓw'ᵧ − p'ᵧw'ₓ), Σ(p'ₓw'ₓ + p'ᵧw'ᵧ)). UE Yaw는 위에서 볼 때 +X에서 +Y로 돌면 양수다(`FRotator(0,θ,0).RotateVector((1,0,0)) = (cosθ, sinθ, 0)`). 그래서 이 식의 θ를 그대로 Yaw로 쓴다.<br>- t = w̄ − R(θ)·p̄.<br>- Pawn = `FTransform(FRotator(0,θ,0), (tₓ, tᵧ, FloorWorldZCm))`. 높이는 U-17대로 바닥이고, 점의 Z로 정하지 않는다.<br>- 점마다 보일 값은 둘이다. **XY 차이** \|R·pᵢ + t − wᵢ\|(XY, cm)와 **높이 차이** (pᵢ.z + FloorWorldZCm) − wᵢ.z(cm)다.<br>- **저장 조건:** 모든 점을 기록했고, 가장 먼 두 점 사이가 `PointMinSpanCm`(기본 20 cm) 이상이고, XY 차이의 최댓값이 `PointFitLimitCm`(기본 2.0 cm) 이하일 것.<br>- 높이 차이가 `PointHeightWarnCm`(기본 3 cm)를 넘으면 **경고만** 한다. 실물 좌석 높이가 CAD와 다를 수 있다는 뜻이다(U-17의 전제). 손끝 관절이 닿은 면보다 조금 위라서 1 cm 안팎은 정상이다.<br>- 한계값 넷은 정렬 컴포넌트 프로퍼티다. **임시 기본값**이고 회사 실측 뒤 바꿀 수 있다. 평가 합격 수치가 아니다 |
| D4 | **저장 — 같은 파일, 새 형식** | 지금 정렬 파일(`SavedAlignmentPath()`, `Saved/Calibration/BuckEye.json`)에 **점 방식(version 2)**으로 저장한다. **마지막 저장이 이긴다.**<br>- 저장할 것: 저장 시각, 추적 원점, 플레이 영역, 점마다 `Order`·Stage 위치(cm)·흩어짐·frame 수.<br>- version 1(머리 저장) 파일은 지금처럼 읽고 지금처럼 쓴다.<br>- 모르는 version은 지금처럼 거절한다.<br>- 원자 교체(후보 파일 → 검산 → `MoveFileExW`, `VBoRBuckAlignment.cpp:208-249`)를 그대로 쓴다. 실패하면 이전 파일이 남는다.<br>- **P0-c 벤치(`AP0BuckBench`)는 version 2를 적용하지 않는다.** "저장값 없음"과 같게 두고, 이유(점 방식 저장값)를 보인다. 그 레벨에서 `C`로 저장하면 지금처럼 version 1로 덮어쓴다 |
| D5 | **적용 — 저장 점과 지금 차 점으로 다시 풂** | `InitializeAlignment`가 version 2를 받으면, 저장한 Stage 점과 **지금** 차 정렬 점의 world 위치로 D3을 다시 푼다. 그래서 레벨에서 차를 옮기거나 돌려도 정렬이 따라간다(version 1이 `BaseEye`를 읽는 것과 같은 성질).<br>- 환경 검사(추적 원점·플레이 영역 `CheckEnvironment`)는 지금 그대로다.<br>- 차의 점 개수나 `Order` 집합이 저장과 다르면 `NeedsCheck`("alignment points changed")이고 Pawn을 옮기지 않는다.<br>- 다시 푼 XY 차이 최댓값이 `PointFitLimitCm`를 넘어도(누가 Blueprint에서 점을 옮김) `NeedsCheck`("alignment points moved")이고 Pawn을 옮기지 않는다.<br>- version 2에는 `BaseEye`가 필요 없다. `bHeightFromFloor`도 쓰지 않는다(머리 높이가 없으니 늘 바닥).<br>- 참가자 머리는 읽지 않는다(체격 차이 유지, P3 D2) |
| D6 | **좌석 전환 = 기준 좌석점과의 위치 차이** | version 1·2 모두 좌석 시점 Pawn = 기준 Pawn + (그 시점 world 위치 − **기준 좌석점** world 위치)이다. 회전은 기준 Pawn 그대로이고, 시점 컴포넌트의 Yaw는 쓰지 않는다(좌석은 모두 차 앞을 봄).<br>- **기준 좌석점:** 차 Actor의 `UVBoRViewpointComponent` 중 태그 `Car.DriverView`가 있는 것이다. 없으면 `BaseEye`다.<br>- **근사 차에는 `Car.DriverView`를 넣지 않는다.** 기준이 `BaseEye`(운전석 눈)이고, 동승석·2열 시점도 눈 위치라서 결과가 지금과 같다(§3).<br>- 회사 차: 운전석·동승석·2열 시점 컴포넌트 셋을 각 좌석 SgRP에 두고, 운전석에 `Car.DriverView`를 붙인다(조사 기록 2026-10-20).<br>- `Car.DriverView` 시점으로 전환하라는 요청은 기준 복귀(`ReturnToBase`)와 같다.<br>- `Car.DriverView`가 둘 이상이면 설정 오류로 `NeedsCheck`다(이유 표시).<br>- **기준 좌석점이 없으면**(`Car.DriverView`도 `BaseEye`도 없음) 좌석 전환을 거절한다(`Driver seat reference missing`). 기준 복귀는 된다. 전환 중에 기준 Pawn을 못 구하면 Pawn을 옮기지 않고 `NeedsCheck`로 바꾼다(리뷰 1회차 R1-1) |
| D7 | **진행자 창 — 점 정렬 절** | 새 절 `Point alignment`(영어, P5i 규칙)에 버튼 셋과 상태 줄을 둔다. 버튼은 지금 명령 길(손·키 → 같은 흐름 명령 `EVBoRFlowCommand`)을 탄다.<br>- **`Capture point` — `P`**, **`Save points` — `S`**, **`Cancel points` — `X`**.<br>- `EVBoROperatorButton`은 `Count` **앞에** 이 순서로 더하고, `VBoROperator::Shortcuts()`도 같은 순서로 더한다(`Shortcut(Button)`이 enum 순서로 찾음, `VBoROperatorPanel.cpp:32`). `EVBoRFlowCommand`는 **끝에** 더한다.<br>- 허용·거절 규칙은 아래 표다. 거절 이유는 지금처럼 `LastCommand`에 보인다.<br>- **저장 성공:** 파일을 쓰고, 정렬 다시 적용과 같은 길(암전 → `LoadSetupAlignment` → 밝아짐, `VBoRVehicleAlignmentComponent.cpp:154-162`·`207-211`)로 적용한다. **흐름은 시작하지 않는다**(점 확인 때문, 지금 다시 적용은 시작 전이면 흐름을 시작함 — `VBoRScenarioOperator.cpp:204-205`). 흐름은 `A`(다시 적용)나 레벨 다시 시작으로 시작한다.<br>- **상태 줄**(새 필드 — 기존 `Alignment` 줄 문구는 바꾸지 않음):<br>  - 다음 점: `Point 2/4 — <Label>`<br>  - 마지막 기록 결과(흩어짐 mm, 또는 취소·거절 이유)<br>  - 다 기록하면 점마다 XY 차이·높이 차이(cm), 최댓값과 한계, 높이 경고<br>  - 점 표시 중이면 가장 가까운 점까지 손끝 거리(XY·높이, cm)<br>- 기록 줄(`Record`)을 남긴다: 점마다 결과, 저장, 취소 |
| D8 | **점 표시 — 확인용** | 점 정렬 중(첫 `P`부터 저장·취소까지)과 **저장 뒤 흐름이 시작되기 전까지** VR에 정렬 점 표시가 보인다.<br>- 흐름이 시작되면(`A`·`O`·다시 시작) 숨긴다. 참가자 흐름 중에는 보이지 않는다.<br>- 확인 방법: 저장 뒤 실제 점을 다시 짚으면, 가상 손끝이 표시 위에 오고 상태 줄의 거리가 작아진다 |
| D9 | **근사 차와 문구** | `AP3VehicleMock`에 정렬 점 4개를 넣는다(`자리값`에 `AlignPointsCm` 같은 프로퍼티, `OnConstruction`에서 자리 잡음). 근사 운전석 방석 판(`P3VehicleMock.cpp:94`, 중심 `DriverEyeCm + (0,0,-70)`, 크기 48×46×10) 윗면 네 모서리다. 차 로컬 cm:<br>- 1 `Mock seat front-left` (4, −60, 50)<br>- 2 `Mock seat front-right` (4, −14, 50)<br>- 3 `Mock seat rear-right` (−44, −14, 50)<br>- 4 `Mock seat rear-left` (−44, −60, 50)<br>- 이름에 `Mock`을 넣어 근사 형상임을 드러낸다(U-07).<br>- **문구:** 저장값이 없을 때의 안내에 점 정렬을 더한다. 예: `저장값 없음 — 진행자 창 P로 점 정렬 또는 P0-c 레벨에서 C로 저장`. **앞의 `저장값 없음`과 문자열 `P0-c`는 남긴다**(`EnglishReason`의 접두 비교 `VBoRVehicleAlignmentComponent.cpp:166`, 시험 `P3FlowTests.cpp:373`). 진행자 창의 영어 안내(`VBoROperatorPanel.cpp:199`, `EnglishReason`)도 점 정렬을 언급하게 고쳐도 된다 |

**D7 허용·거절 규칙** (위에서부터 먼저 걸리는 이유를 보인다):

| 명령 | 허용 조건 | 거절 이유(영어) |
|---|---|---|
| `Capture point` (P) | 흐름 시작 전(`!bRunning && !bFreeMode`) | `Before the flow starts only` |
| | 시점 전환·다시 적용 중 아님 | `View transition in progress` |
| | 기준 시점 | `Return to the driver seat first` |
| | 손끝을 읽을 수 있는 Pawn(손 Pawn) | `Hand tracking pawn required` |
| | 차 정렬 점 2개 이상, `Order` 겹침 없음 | `Alignment points missing on the car` / `Alignment point order duplicated` |
| | 지금 기록 창이 돌고 있지 않음 | `Capturing a point` |
| | 아직 기록할 점이 남음 | `All points captured — save or cancel` |
| `Save points` (S) | 점 정렬 중이고 모든 점 기록 | `Capture all points first` |
| | 점 퍼짐 ≥ `PointMinSpanCm` | `Points too close` |
| | XY 차이 최댓값 ≤ `PointFitLimitCm` | `Fit <최댓값> cm over limit <한계> cm` |
| | 시점 전환 중 아님 | `View transition in progress` |
| `Cancel points` (X) | 점 정렬 중(기록한 점이 있거나 기록 창이 돎) | `No point alignment in progress` |
| `Re-apply saved alignment` (A) | (지금 규칙에 더해) 점 정렬 중이 아님 | `Point alignment in progress` |

- `O`(정렬 없이 시작)는 점 정렬 중에도 지금처럼 받는다. 받으면 점 정렬을 끝내고(기록 버림) 표시를 숨긴다.
- 재중심(`VRHeadsetRecenter`)이 오면 기록한 점을 모두 버린다(Stage가 바뀌었을 수 있음). 이유는 `Recenter — points cleared`다. 정렬 상태 처리(`NeedsCheck`, `VBoRVehicleAlignmentComponent.cpp:88-92`)는 지금 그대로다.

## 1. 왜 만드는가

- 지금 정렬은 사람이 헤드셋을 쓴 채 앉아서 눈 위치를 저장한다. 저장하는 사람의 자세에 따라 앞뒤가 몇 cm씩 달라지고, Yaw는 그 순간의 고개 방향이다. 실물 좌석의 단단한 점은 사람 몸과 상관이 없다(조사 기록 2026-10-20).
- 회사에서는 L_Main의 진행자 창에서 설치 정렬을 해야 한다. 지금은 P0-c 레벨에서만 저장된다(사용자 요청 2026-10-19).
- 회사 차는 설계 눈 위치 대신 좌석 SgRP로 좌석을 바꾼다(사용자 2026-10-20).

## 2. 범위

### 만들 것

1. D1 정렬 점 컴포넌트
2. D2–D5 기록·풀이·저장·적용(version 2), P0-c 벤치의 version 2 처리
3. D6 좌석 전환을 기준 좌석점 차이로 바꿈(version 1·2 공통)
4. D7–D8 진행자 창 절·명령·키·점 표시
5. D9 근사 차 점 4개, 문구
6. §4 자동화 시험

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| L_Main 레벨·차량 Blueprint 만들기, 레벨 수정 | L_Main은 사용자가 `L_P3_Flow`를 복제해 만든다(2026-10-20). 이 명세는 C++만이다. 에디터 MCP 작업 없음 |
| 근사 차에 `Car.DriverView` 넣기, 근사 차의 눈·시점 자리 바꾸기 | D6 — 기존 시험 결과를 그대로 두기 위해 |
| 참가자 머리·손으로 정렬을 고치기, 참가자마다 맞추기 | U-07·P3 D2 |
| P0-c 벤치의 머리 저장 방식 바꾸기 | D4 — 집 시험용으로 남김(U-18) |
| 패스스루·컨트롤러로 점 찾기 | AGENTS.md 확정 제약 |
| 점 짚는 순서 안내를 VR에 띄우기(글자·화살표), 소리 | 진행자가 말로 안내한다. 필요해지면 따로 정한다 |
| 회사 차의 실제 점·SgRP 값 | 회사 PC에서(Company-PC-Guide §4) |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- **version 1(머리 저장) 결과는 그대로다.** 기준 Pawn은 지금 식(`CalculatePawnOnFloor`·`CalculatePawn`, `VBoRBuckAlignment.cpp:21-35`)이다.
  - D6의 차이 식은 근사 차에서 지금과 같은 값을 낸다. 시점 Yaw가 `BaseEye` Yaw와 같기 때문이다(`P3VehicleMock.cpp:77`은 위치만 둔다).
  - `bHeightFromFloor` 끔도 같다. 기준 Z + (시점 Z − 기준 Z) = 시점 Z − 저장 머리 Z이기 때문이다.
- **U-17:** version 2의 Pawn Z는 늘 `FloorWorldZCm`다. 좌석 시점은 기준 좌석점과의 높이 차이만 더한다.
- 진행자 창의 기존 버튼·키·허용 규칙·문구는 그대로다. 예외는 D7 표의 `A` 한 줄(점 정렬 중 거절)뿐이다.
  - `FVBoROperatorStatus::Alignment` 문구(`No saved alignment` 등)는 바꾸지 않는다.
- 정렬 점·표시는 충돌과 태그가 없고 기본은 숨김이다. 근사 차의 다른 부품 자리·태그·개수는 그대로다.
- 재중심·환경 검사·HomeShift·다시 적용의 지금 동작은 그대로다(D7에서 더한 것만 다름).
- P0-c 벤치의 version 1 저장·적용·`T` 손끝 대조는 그대로다.
- **기존 단언은 바꾸지 않는다. 예외 없음.** Claude가 정렬·좌석 전환·진행자 창을 쓰는 시험을 모두 대조했다. 아래는 그대로 통과해야 한다.
  - `vBoR.P0Buck.*`(`P0Probe/P0BuckAlignmentTests.cpp`): 저장·읽기·원자 교체, `version 99` 거절(`:199-202`), 벤치. → version 1 길이 그대로이고, version 2는 새 길이다.
  - `vBoR.P3Flow.E10E11ViewpointFadeAndExactTransform`(`P3Probe/P3FlowTests.cpp:303-324`): 시점마다 `CalculatePawn(저장, 시점)`과 0.1 mm·0.01°로 같다. → 근사 차 시점 Yaw = 기준 Yaw라서 차이 식과 같은 값이다.
  - `vBoR.P3Flow.E28HeightFromFloor`(`:326-352`): 2열 Z = 2열 시점 Z − 운전석 눈 Z, 기준 복귀 0, 끄면 지그 방식. → 같다.
  - `vBoR.P3Flow.E13E14AlignmentGateAndRecenter`(`:368-`): 진행자 글에 `P0-c`(`:373`), 다른 원점은 Pawn 유지, 재중심은 `NeedsCheck`. → D9가 `P0-c`를 남긴다.
  - `GetCurrentEye`(`:843`·`:857`): 지금 시점 또는 `BaseEye`. → 바꾸지 않는다.
  - P3b 근사 차 태그 개수(`:805-820`): 태그로 센다. → 정렬 점에 태그가 없다.
  - `vBoR.P5i.*`(`P3Probe/P5iOperatorPanelTests.cpp`):
    - E2(`:134-150`)는 모든 단축키에서 버튼 = 키 = 같은 흐름 명령 하나인지 본다. → 새 버튼 셋도 흐름 명령을 큐에 넣어야 한다(`Info`처럼 따로 빼지 않음).
    - E3·E5·E6(`:152`·`:171`·`:174`)은 상태마다 켜진 버튼 집합을 정확히 본다. → P5i 시험 Pawn은 **손 Pawn이 아닌 `APawn`**이고(`:87`), 정렬 컴포넌트는 `UP5iStoredAlignment`(`P5iTestFixture.h`, 저장 경로만 바꿈)다. 그래서 `Capture point`는 `Hand tracking pawn required`로 꺼지고 집합이 그대로다. **이 판정이 P5i 시험 Pawn에서 거짓이 되게 만들지 않는다**(예: 손끝 읽기를 기본으로 허용하지 않는다).
    - E7(`:177`)은 모든 상태 × 모든 버튼에서 받음·거절 표시가 결정과 같은지 본다. → 새 버튼도 같은 길이다.
    - E8(`:200-`)은 "주 버튼은 하나". → 새 버튼은 그 상태들에서 `Primary`가 되지 않는다.
    - E23(`:386-`)은 글자가 버튼 폭에 들어가는지 본다. 새 버튼은 316 px 규칙이다. → 영어 이름을 짧게 둔다(위 이름 그대로).
    - `:205` `S.Alignment == "No saved alignment"`. → 바꾸지 않는다.
    - `:349`·`:546`·`:641`: version 1 적용·차 30° 회전. → 같다.
  - `vBoR.P3c.*`(같은 파일 `:524-`): HomeShift 이동·좌석 절대 자세(E6)·다시 적용 초기화(E7). → 시점 Pawn은 매번 저장값에서 새로 계산하므로(누적 없음) 같다.
  - `vBoR.P5g.*`·`vBoR.P5h.*`: 같은 정렬 API를 쓰고 근사 차 결과가 같다.
  - 이 목록 밖에서 실패하는 단언이 있으면 고치지 말고 리뷰 파일 "질문"에 적고 멈춘다.

## 4. 경계 상황과 그 시험

| # | 상황 | 기대 동작 | 자동화 시험 |
|---|---|---|---|
| E1 | 풀이 — 정확한 점 | 알려진 Yaw(30°, −120°, 179°)·이동으로 만든 4점에서 θ는 0.01° 안, t는 0.01 cm 안이다. Pawn Z = `FloorWorldZCm`. **부호 확인:** 30°와 −30°가 서로 바뀌지 않는다 | 순수 함수 |
| E2 | 풀이 — 흔들린 점, 최소 점 | 점마다 ±1 cm를 더하면 XY 차이가 보고되고 1 cm 수준이다. 2점으로도 풀린다. 퍼짐 < `PointMinSpanCm`이면 저장 거절 | 순수 함수 |
| E3 | Stage 좌표 | Pawn이 옮겨지고 돌고 HomeShift된 상태에서 기록해도 같은 Stage 값이 나온다 | 정렬 컴포넌트(시험용 손끝 읽기) |
| E4 | 기록 창 | 15 fps와 90 fps에서 같은 초 길이다. 한 frame 추적 끊김이면 그 점만 취소되고 순서는 그대로다. 흩어짐 > 한계면 거절, 이유 표시 | 정렬 컴포넌트 |
| E5 | 저장·읽기 | version 2를 저장하고 읽으면 값이 같다. version 1은 지금처럼 읽힌다. 모르는 version·깨진 값은 거절. 마지막 저장이 이긴다(1→2, 2→1) | 파일(시험 경로) |
| E6 | P0-c 벤치와 version 2 | 벤치는 version 2를 적용하지 않고 Pawn을 옮기지 않으며 이유를 보인다. 벤치에서 저장하면 version 1로 덮어쓴다 | P0 벤치 |
| E7 | 적용 | 저장 Stage 점이 차 점에 0.01 cm 안으로 겹친다. 차를 옮기고 30° 돌린 뒤 다시 적용하면 따라간다. 점 개수·`Order`가 다르거나 점을 옮겨 차이가 한계를 넘으면 `NeedsCheck`, Pawn 그대로. 환경 검사(원점·영역)는 version 1과 같다 | 정렬 컴포넌트 |
| E8 | 저장 조건·경고 | XY 차이가 한계를 넘으면 `Save points`가 거절되고 이유에 값이 보인다. 높이 차이 > 경고값이면 경고 문구가 나오되 저장은 된다 | 진행자 명령 |
| E9 | 좌석 전환(D6) | `Car.DriverView`가 있으면 시점 Pawn = 기준 + (시점 − 운전석 좌석점), Yaw는 기준 그대로(시점을 20° 돌려도). 없으면 `BaseEye` 기준(근사 차 — 지금 결과와 같음). `Car.DriverView` 대상 전환 = 기준 복귀. 둘이면 `NeedsCheck`. 둘 다 없으면 전환 거절·Pawn 그대로. 전환 중 기준 Pawn 실패면 Pawn 그대로·`NeedsCheck`. version 1·2 모두 | 정렬 컴포넌트 |
| E10 | 진행자 명령 규칙 | D7 표의 행마다 허용·거절과 이유. `P`·`S`·`X` 키 = 버튼. 점 정렬 중 `A` 거절, `O`는 받고 점 정렬을 끝냄. 재중심은 기록을 버림 | 진행자(손 Pawn 대신 시험용 손끝 읽기) |
| E11 | 설정 오류 | 정렬 점 0·1개, `Order` 겹침이면 `Capture point` 거절과 이유 | 진행자 |
| E12 | 저장 뒤 | 저장하면 암전 → 적용(`Applied`) → 밝아짐이고, **흐름은 시작하지 않는다.** 그 뒤 `A`를 누르면 흐름이 시작된다. 파일 쓰기 실패면 점 정렬이 그대로 남고 이유를 보인다 | 진행자 + 파일 |
| E13 | 점 표시 | 점 정렬 중·저장 뒤 시작 전에는 보이고, 흐름이 시작되면 숨는다. 가장 가까운 점 거리 줄이 맞다 | 정렬 컴포넌트·진행자 |
| E14 | 근사 차 | 점 4개가 D9 자리·`Order`·`Label`이다. 충돌·태그 없음, 게임에서 기본 숨김·에디터에서 보임 | 근사 차 |
| E15 | 기존 시험 | §3 목록 모두 그대로 통과. `vBoR.*` 전체 통과 | 전체 시험 |

## 5. 구현자가 정할 것

- 저장 구조체(기존 `FVBoRBuckSavedAlignment`에 점 배열을 더할지, 새 구조체를 둘지)와 JSON 필드 이름. D4의 내용과 version 규칙만 지킨다.
- 기록 상태(점 정렬 중·기록 창·기록한 점)를 둘 곳. 정렬 컴포넌트가 자연스럽다(Pawn·파일 경로·다시 적용을 이미 가짐).
- 손끝 읽기를 시험에서 바꿔 넣는 방법. 예: 정렬 컴포넌트의 가상 함수를 시험 자식(`UP5iStoredAlignment`처럼)이 덮어씀. §3의 P5i 조건(시험 Pawn에서는 꺼짐)을 지킨다.
- 점 표시의 모양(작은 구 지름·색, 번호 글자).
- 진행자 창 절의 배치(기존 `Pair` 배치처럼 한 줄 둘)와 상태 줄 줄바꿈.
- 순수 함수의 이름과 위치(예: `FVBoRBuckAlignment`에 2D 강체 풀이).

## 6. 완료 조건

1. §2의 1–6이 들어 있다.
2. §4 시험이 자동화로 통과한다. 기존 `vBoR.*`도 통과한다(§3 — 예외 없음).
3. non-unity(`-DisableUnity -Module=vBoR`)·unity 빌드에서 vBoR 경고·오류 0. 전체 시험은 최종 트리로 한 번 돌린다.
4. AGENTS.md 보고 ①–⑤를 커밋 본문과 `HANDOFF.md` "Codex 결과"에 남긴다. **리뷰 파일은 만들지 않는다**(P4f 리뷰 참고 1).
5. 시험 상태는 `not-tested`다.

## 7. 로컬에서 확인할 것 — 사용자

1. **집(근사 차, `L_P3_Flow` 또는 `L_Main`):** PIE+VR로 띄우고 진행자 창을 연다. 집 의자 둘레의 단단한 점 넷(의자 다리 위 모서리 등)을 정해 `P`로 차례로 짚는다.
   - 근사 차 점과 실물이 맞지 않으므로 XY 차이가 커서 `Save points`가 거절되는 것이 맞다. 이유에 값이 보이는지 본다.
   - 흐름까지 보려면 정렬 컴포넌트 Details에서 `PointFitLimitCm`을 크게 바꿔(레벨은 저장하지 않음) 저장한다.
   - 그다음 같은 점을 다시 짚어 상태 줄 거리가 작은지(같은 점을 다시 짚는 반복성) 본다. 표시가 보이고 `A`로 흐름이 시작되면 숨는지도 본다.
2. **집:** 점 저장 뒤 P0-c 레벨을 열면 적용되지 않고 이유가 보이는지, `C`로 다시 저장하면 머리 방식으로 돌아오는지 본다.
3. **회사(실제 차):** 차량 Blueprint에 정렬 점 4개와 좌석 시점 셋(SgRP, 운전석 `Car.DriverView`)을 넣는다. 실물 좌석을 SgRP 기준 상태로 두고 정렬한 뒤, 점을 다시 짚어 차이를 본다. 높이 경고가 뜨면 실물 좌석 높이를 확인한다. 이때의 값으로 한계값 넷을 정한다.
4. Quest 3 시험 ID: 정렬 관련 T-ID(계획서 §7)는 회사에서 `not-tested` → 확인.

## 8. 확인한 API 근거

경로: `Source/vBoR/` 아래(`main` @ `9d700a9`).

| 쓸 것 | 위치 | 쓰임 |
|---|---|---|
| 지금 정렬 적용·전환·다시 적용 | `Interaction/VBoRVehicleAlignmentComponent.cpp:34-39`(`PawnFor`), `:63-86`(`InitializeAlignment`), `:88-92`(재중심), `:139-145`(`SwitchRejection`), `:154-162`(`RequestReapply`), `:194-234`(`AdvanceSwitch`, 다시 적용 `:207-211`, 시점 `:214-218`), `:236-239`(`GetCurrentEye`) | D5·D6·D7 |
| 계산·기록 창·파일 | `Interaction/VBoRBuckAlignment.cpp:21-35`(Pawn 식), `:71-140`(머리 기록 창 — 초 단위·취소·흩어짐), `:142-206`(검사·읽기), `:208-249`(원자 저장) | D2–D5 |
| 손끝 원본 | `Interaction/VBoRHandRigPawn.cpp:120-131`(`ReadRawIndexTip`, `UnrealWorldSpace`, Tracked만) | D2 |
| P0-c 벤치 | `P0Probe/P0BuckBench.cpp:100`(같은 파일 경로), `:142-150`(읽고 적용), `:253-254`(저장) | D4 |
| 진행자 창 | `Interaction/VBoROperatorPanel.h:12-13`(버튼 enum), `Interaction/VBoROperatorPanel.cpp:12-32`(단축키·순서 조회), `:194-199`(정렬 절) | D7 |
| 흐름 명령·시작 | `Interaction/VBoRScenarioRunner.h:23`(`EVBoRFlowCommand`), `Interaction/VBoRScenarioOperator.cpp:51-60`(시점 태그로 찾음), `:90-95`(다시 적용 판정), `:198-206`(`FinishReapply` — 시작 전이면 흐름 시작), `Interaction/VBoRScenarioRunner.cpp:290-291`·`:638`(`P0-c` 안내) | D7·D9 |
| 근사 차 | `P3Probe/P3VehicleMock.cpp:39-44`(눈·시점·정렬), `:77`(자리), `:94`(운전석 방석 판) | D6·D9 |
| 시험 고정물 | `P3Probe/P5iOperatorPanelTests.cpp:83-104`(`APawn`, `UP5iStoredAlignment`), `P3Probe/P5iTestFixture.h:9-16` | §3·E10 |

## 개정 이력

| 날짜 | 개정 | 내용 |
|---|---|---|
| 2026-10-20 | 처음 | 작성(사용자 결정 U-18: 실물 좌석 점 4개, 좌석 전환은 SgRP). 정렬·좌석 전환·진행자 창을 쓰는 기존 시험을 모두 대조함 — 바뀌는 단언 없음 |
| 2026-10-20 | 리뷰 1회차 | D1: "기본 숨김"을 "게임에서만 숨김, 에디터에서는 보임"으로 고침(R1-2 — 명세 문구가 에디터에서도 숨기게 읽혔다). D6·E9: 기준 좌석점이 없을 때와 전환 중 기준 실패를 정함(R1-1 — 명세에 없던 경우). 반영은 Claude가 직접 함 |
