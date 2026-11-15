# M2 Buck 프로필 — 마커 정렬 저장과 Buck 전환

| 항목 | 값 |
|---|---|
| 단계 | 50% 범위 밖 실험의 다음 단계(M1 → M2). 시험 계획 T-01(데이터·Buck 정렬)의 보조 |
| 결정 근거 | **사용자 결정(2026-11-16):** 같은 차(운전석)를 운전석 하나짜리 실물 Buck 여러 대에서 시험한다. PC 하나로 Buck을 바꿔 가며 쓴다 → Buck마다 정렬값을 따로 저장하고 전환한다. 마커 정렬 결과를 `L_Main`에서 쓴다. M1 결과: 집 바닥 60 mm 마커 4장, 점마다 XY 1.5–1.8 cm, Air Link에서도 동작(사용자 2026-11-16) |
| 베이스 브랜치 | `main` (`0d7b30f` 이후) |
| 작업 브랜치 | `codex/M2-buck-profiles` |
| 리뷰 파일 | `Docs/Reviews/M2-buck-profiles.md` |
| 에디터 작업 | **없음.** 프로필 값은 사용자가 `BP_Car_Core`에 넣는다. 레벨·에셋을 고치지 않는다 |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **Buck 프로필 = 정렬 컴포넌트의 배열** | `UVBoRVehicleAlignmentComponent`에 `Bucks`(TArray, 기본 빈 배열). 항목 `FVBoRBuckProfile`: `Name`(FString, 파일 이름·표시용), `FirstMarkerId`(int32). 이 Buck의 마커 ID = `FirstMarkerId + (Order − 1)`(정렬점 `Order`마다).<br>- **정렬점은 모든 Buck이 같다**(모두 같은 운전석, 사용자 2026-11-16). Buck마다 다른 것은 실물 Buck의 방 안 위치·방향 → 저장값만 Buck별.<br>- **`Bucks`가 비어 있으면 지금과 완전히 같다**(파일 `BuckEye.json` 하나, 진행자 새 버튼 비활성). 기존 시험·집 시험이 이 경로를 쓴다 |
| D2 | **설정 검사** | `Bucks`가 비어 있지 않을 때: 이름이 비었거나 겹침, 파일 이름에 못 쓰는 문자(`FPaths::ValidatePath` 또는 같은 수준의 검사), 마커 ID 범위가 0–49(`DICT_4X4_50`)를 벗어나거나 Buck끼리 겹침 → 설정 오류. 정렬 상태 `NeedsCheck`, 이유 `Buck profiles invalid — <첫 문제>`, Pawn을 옮기지 않는다. 마커 시험대 `M`도 같은 이유로 거부 |
| D3 | **저장 파일 = Buck마다 하나** | `Saved/Calibration/Bucks/<Name>.json`. 형식은 지금 version 2(점 방식) 그대로 — `FVBoRBuckAlignmentStore::Save/Load`, 원자 교체·재시도 그대로. 손 4점(`P`·`S`)도 활성 Buck 파일에 저장한다(`SavedAlignmentPath()`가 활성 Buck 파일을 돌려줌) |
| D4 | **활성 Buck** | `Saved/Calibration/ActiveBuck.txt`에 이름 한 줄. 없거나 `Bucks`에 없는 이름이면 첫 번째 Buck(파일은 쓰지 않음, 이유에 적음 `Active buck file missing — using <Name>`). 쓰기 실패는 전환 실패(D6) |
| D5 | **`L_Main` 적용** | `LoadSetupAlignment`: `Bucks`가 있으면 활성 Buck 파일을 읽어 지금 version 2 규칙으로 적용(`CheckEnvironment`, `NeedsCheck` 규칙 그대로). 파일이 없으면 `NoSavedValue`, 이유 `No saved value for buck <Name> — ...`(접두 `No saved value`는 유지, `EnglishReason` 접두 비교 `VBoRVehicleAlignmentComponent.cpp:166` 참고).<br>- **`vBoR.EyeAlignment` 1(집 시험)이면 Buck과 상관없이 지금처럼 `BuckEye.json`을 읽는다**(P5u). 0이면 위 규칙 |
| D6 | **진행자 Buck 전환 — `B` "Next buck"** | 진행자 버튼·키 하나(`EVBoROperatorButton` `Count` 앞, `Shortcuts()` 같은 순서, `EVBoRFlowCommand` 끝 — P4h D7과 같은 방식). 누르면 `Bucks` 순서로 다음 Buck(끝이면 처음)을 활성으로 쓰고, **지금 다시 적용과 같은 길**(암전 → 파일 다시 읽기 → 적용, `RequestReapply`)로 적용한다. 흐름은 시작하지 않는다.<br>- 거부: `Bucks` 0·1개(`One buck or none`), 흐름 시작 뒤·자유모드(`Before the flow starts only`), 점 정렬 중·시점 전환 중(지금 `ReapplyRejection` 규칙), `ActiveBuck.txt` 쓰기 실패(이유와 함께, 활성 Buck 그대로).<br>- 표시 스타일은 Primary가 아니다(진행자 창 Primary는 하나 규칙, P5i). |
| D7 | **진행자 표시** | 진행자 창 정렬 절에 새 줄 `Buck: <Name> (<i>/<n>)`(프로필 없으면 줄 없음). 기존 `Alignment` 줄 문구는 바꾸지 않는다. 기록 줄(`Record`): 전환 결과 |
| D8 | **마커 시험대 — Buck 자동 판단과 저장** | `AM1MarkerAlignProbe`:<br>- `Bucks`가 있으면 기록 중 처음 받은(재투영 등 통과한) 마커 ID가 속한 Buck으로 세션 Buck을 정한다. 그 Buck의 ID만 모으고, 다른 Buck 범위 ID는 세고 버린 뒤 결과에 한 줄(`Ignored markers from buck <Name>: ID 5 ×12`). 화면 시작 줄에 판단한 Buck 이름.<br>- `Bucks`가 비면 지금처럼 ID = `Order`.<br>- **새 키 `S`: 저장.** 마지막 결과(`Fit`)가 있고 화살표 검사를 통과했을 때만. 정렬 컴포넌트의 새 공개 함수(예: `SaveMarkerPoints(BuckName, Samples, Failure)`)를 부른다 — 점 `Order`·Stage 중심·흩어짐·프레임 수로 `FVBoRAlignPointSample`을 채우고, 손 4점 저장과 같은 검사(`PointMinSpanCm`, `PointFitLimitCm`)·환경(추적 원점·플레이 영역)·원자 저장, 그 Buck을 활성으로. 결과 `Saved for buck <Name> — open L_Main` 또는 거부 이유. 프로필이 없으면 `BuckEye.json`에 저장<br>- 시험대의 `G`/`H`/`T`·다른 검사는 그대로 |

## 1. 왜 하는가

- 마커 정렬(M1)이 집 바닥 시험에서 20 s, 점마다 2 cm 안으로 됐다. 결과를 `L_Main`에서 쓰려면 저장이 필요하다.
- 한 PC로 실물 Buck 여러 대를 번갈아 쓴다. Buck마다 다시 정렬하거나 값을 손으로 옮기지 않고, 정렬해 둔 값을 골라 쓴다. Buck마다 마커 번호대를 달리 붙이면 어느 Buck인지 카메라가 알아서 정한다.
- **동승석·2열 전환은 바꾸지 않는다.** 지금도 운전석 정렬 + 좌석 기준점 차이만큼 이동이다(`VBoRVehicleAlignmentComponent.cpp:44-52`). Buck이 바뀌어도 그대로 따라온다.

## 2. 범위

### 만들 것

- D1–D8, 시험 `vBoR.M2.*`(§4).

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| `L_Main`에서 카메라 켜기·마커 읽기 | 카메라는 설치 정렬 시험(시험대 레벨)에서만(AGENTS.md 실험 예외). `L_Main`은 저장값만 읽는다 |
| Buck별 정렬점(Buck마다 다른 점 좌표) | 모든 Buck이 같은 운전석(사용자). 필요해지면 따로 |
| 저장 형식 version 바꾸기, `BuckEye.json` 옮기기·지우기 | 프로필이 없을 때 지금과 같아야 한다 |
| 레벨·`BP_Car_Core`·에셋 변경 | 프로필 값은 사용자가 넣는다 |
| 진행자 창 배치 큰 변경 | 버튼 하나·상태 줄 하나만 |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- **`Bucks`가 비면 동작·문구·파일 경로가 지금과 같다.** P4h·P5u·P5i·R0 등 기존 시험이 이 경로다.
- 기존 진행자 버튼의 키·문구·순서·활성 규칙, `Alignment` 줄 문구, `EnglishReason` 접두(`No saved value`, `P0-c`)는 그대로.
- 좌석 전환 계산(`PawnFor`), `CheckEnvironment`, 원자 저장(`FVBoRBuckAlignmentStore`)은 그대로 쓴다.
- 손 4점 `PointFitLimitCm`(2 cm)·`PointMinSpanCm` 기본값 그대로. 마커 저장도 같은 값을 쓴다(값은 사용자가 컴포넌트에서 조정).
- 마커 시험대의 M1 결정(D1–D17, 개정 1·1a·1b·2)은 D8에서 바꾼 것 말고 그대로.

**기존 시험 감사(Claude 2026-11-16):**
- 진행자 버튼 목록을 도는 시험: `P5iOperatorPanelTests.cpp` E2(버튼 = 키, `:145-160`), E3·E5·E6(`Enabled` — 기대 목록 밖 버튼은 비활성이어야 함, `:125-131`·`:162-187`), E7(스냅샷 = 실시간), E23(라벨 폭 ≤ 316 px, `:399-405`), "Primary 하나"(`:381-382`). `FP5iWorld`에는 `Bucks`가 없으므로 새 버튼은 항상 비활성 → **이 단언들은 그대로 통과해야 한다.** 새 키 `B`는 Runner에 바인딩(E2). 라벨 `Next buck`은 폭 안.
- `SavedAlignmentPath()`를 덮어쓰는 시험 도우미(`P3Probe/P4hTestFixture.h`, `P5iTestFixture.h`, `P1Probe/P1LoadBench.h`, `P2Probe/P2DoorBench.cpp`)는 `Bucks`가 비어 있어 영향 없음.
- **고쳐도 되는 단언: 없음.** 기존 시험이 깨지면 멈추고 리뷰 파일 "질문"에 적는다.

## 4. 시험 — `vBoR.M2.*`

파일 경로는 시험용 임시 폴더(덮어쓰는 가상 함수 또는 같은 방식). 실제 `Saved/Calibration`을 건드리지 않는다.

| ID | 상황 | 확인 |
|---|---|---|
| E1 | `Bucks` 빔 | 저장·읽기 경로가 `BuckEye.json`(지금과 같음), 새 버튼 비활성(`One buck or none`), 상태 줄 없음 |
| E2 | `Bucks` = A(1), B(5), `ActiveBuck.txt` 없음 | 활성 A, 이유에 `Active buck file missing — using A`, A 파일 없으면 `NoSavedValue`·이유에 `buck A` |
| E3 | 설정 오류: 이름 겹침 / 빈 이름 / `A/..` / ID 범위 겹침(A 1, B 3) / 범위 밖(B 48 → 48–51) | 각각 `NeedsCheck`, `Buck profiles invalid — …`, Pawn 그대로 |
| E4 | A·B 파일에 서로 다른 version 2 점(B는 A를 X +100 cm, Yaw 20° 옮긴 자리) | 활성 A → A 자리, `B` 버튼 → 암전 → B 자리, `ActiveBuck.txt` = B, 다시 `B` → A. 각 전환 뒤 `Passenger`로 바꾸면 그 Buck 기준 + 좌석 차이(지금 규칙) |
| E5 | 전환 거부 | 흐름 시작 뒤, 자유모드, 점 정렬 중, 시점 전환 중, Buck 1개 → 거부와 이유, 활성 그대로. `ActiveBuck.txt` 쓰기 실패(읽기 전용) → 거부, 활성·Pawn 그대로 |
| E6 | 손 4점 `P`·`S`(활성 B) | B 파일에만 저장, A 파일 바이트 그대로 |
| E7 | `vBoR.EyeAlignment` 1 + `Bucks` 있음 | `BuckEye.json`(version 1)을 지금처럼 적용, Buck 파일 안 읽음 |
| E8 | 시험대 Buck 판단: B 범위 ID(5–8)로 기록, 화면에 A 범위 ID 2도 보임 | 세션 Buck = B, ID 2는 무시되고 결과 줄에 개수. 결과는 B ID로 정렬점 `Order` 1–4 |
| E9 | 시험대 `S` | 결과 있음·화살표 통과 → B 파일 저장(점 4개 Stage 중심 = 시험대 중앙값), 활성 = B, 문구. 결과 없음 / 화살표 거부 / `PointFitLimitCm` 넘음 → 거부, 파일 그대로 |
| E10 | 시험대 `S` 후 같은 world에서 정렬 컴포넌트가 그 파일을 읽어 적용 | Pawn = 시험대 `Fit.Pawn`(±0.01 cm, ±0.01°) |
| E11 | 진행자 상태 | `Buck: B (2/2)` 줄, 기존 `Alignment` 줄 문구 그대로 |

## 5. 구현자가 정할 것

- `FVBoRBuckProfile`·새 함수 이름, 파일 이름 검사 방법, `ActiveBuck.txt` 쓰기 방식(원자 교체까지는 필요 없음, 실패만 잡으면 됨).
- 시험 경로를 바꾸는 방법(가상 함수 하나 더 등).
- 진행자 창에서 새 버튼 자리(정렬 절 안, 레이아웃 크게 바꾸지 않음).

## 6. 완료 조건

1. D1–D8 구현, §3 조건, §2 범위 밖 없음.
2. `vBoR.M2.E1`–`E11` 통과, 기존 시험 변경 없이 `vBoR.*` 전체 통과(작업 끝에 한 번).
3. non-unity(`-Module=vBoR`)·unity 빌드 성공.
4. 새 주석·로그·표시 영어(AGENTS.md).

## 7. 로컬에서 확인할 것 — 사용자

1. **집:** `BP_Car_Core` 정렬 컴포넌트에 `Bucks` = `HomeA`(1), `HomeB`(5). 마커 ID 5–8 인쇄물은 Claude가 만든다(지금 PDF는 1–4).
   - `L_M1_MarkerAlign`: 1–4 직사각형 → `M` → `S`(HomeA 저장). 5–8로 바꿔 붙이고 → `M` → `S`(HomeB).
   - `L_Main`: 시작 Buck 확인 → 진행자 `B`로 오가며 차 자리가 바뀌는지, 동승석 `1`·2열 `2`가 따라오는지.
2. **회사:** 실물 Buck마다 마커 번호대를 정해 붙이고 같은 순서. `L_Main` 정렬 상태 `Applied`, 확인점(`Align.Check`)으로 차이.
3. Quest T-01 보조 `not-tested`.

## 8. 확인한 근거

| 쓸 것 | 위치 |
|---|---|
| 저장 경로·읽기·적용 | `Interaction/VBoRVehicleAlignmentComponent.cpp:41-42`(`SavedAlignmentPath`), `:55-64`(`LoadSetupAlignment`, `vBoR.EyeAlignment`) |
| 손 4점 저장 | `Interaction/VBoRPointAlignment.cpp:136-146`(`SavePointsRejection`), `:148-156`(`SavePoints`), `:80-84`(`ReadPointEnvironment`) |
| 저장소 | `Interaction/VBoRBuckAlignment.h:85-92`(`FVBoRBuckAlignmentStore::Load/Save`), `:24-30`(`FVBoRAlignPointSample`) |
| 좌석 전환 | `Interaction/VBoRVehicleAlignmentComponent.cpp:44-52`(`PawnFor`) |
| 진행자 버튼·키 | `Interaction/VBoROperatorPanel.h:12-13`, `Interaction/VBoROperatorPanel.cpp:12-34`, `Interaction/VBoRScenarioRunner.cpp:235-256`(키 바인딩), `Interaction/VBoRScenarioOperator.cpp:100-109`(점 정렬 명령 거부 규칙) |
| 마커 시험대 | `M1Probe/M1MarkerAlignProbe.h/.cpp`(`StartCapture`, `ProcessFrame`, `FinishCapture`) |

## 개정 이력

| 날짜 | 개정 | 내용 |
|---|---|---|
| 2026-11-16 | 처음 | 작성(사용자 결정: 운전석 Buck 여러 대, PC 하나, 전환 기능 포함) |

## 개정 1 (Claude 2026-11-16) — 정렬점도 Buck마다 다르다

**사용자 정정(2026-11-16):** "Buck이 다르면 alignment point도 달라진다." 실물 Buck마다 마커를 붙이는 자리(차 CAD 기준 좌표)가 다르다. 처음 명세 D1의 "정렬점은 모든 Buck이 같다"는 틀렸다. **이 개정이 위 본문보다 우선한다.**

### 바뀌는 결정

| # | 바뀜 | 내용 |
|---|---|---|
| D1 | **정렬점에 Buck 이름** | `UVBoRAlignPointComponent`에 `Buck`(FString, 기본 빈 값). `Bucks`가 비어 있으면 이 값은 무시하고 지금처럼 모든 정렬점을 쓴다. `Bucks`가 있으면 **Buck마다 `Buck` = 그 Buck `Name`인 정렬점만** 그 Buck의 점이다. 같은 차 Blueprint에 Buck A 점 4개, Buck B 점 4개를 함께 둔다 |
| D2 | **설정 검사 추가** | `Bucks`가 있을 때: Buck마다 점이 2개 이상(마커 시험대는 정확히 4개 — M1 `StartCapture` 규칙), 그 Buck 안에서 `Order` 겹침 없음, `Buck` 값이 `Bucks`에 없는 정렬점이 있으면 오류(`Alignment point <이름> has unknown buck <값>`). 빈 `Buck` 값 정렬점이 있어도 오류(어느 Buck인지 모름) |
| D3·D5 | **저장·적용은 그 Buck의 점으로** | Buck 파일의 점(`Order`)과 **그 Buck의 정렬점**으로 맞춘다(`FitCaptured`·`FindAlignPoints`가 활성 Buck의 점만 돌려줌). `alignment points changed`·`moved` 판정도 그 Buck의 점 기준 |
| D6 | 그대로 | 전환하면 새 Buck의 파일과 새 Buck의 점으로 다시 적용 |
| P·S | **손 4점은 활성 Buck의 점** | `P`가 짚을 순서·라벨·표시 구가 활성 Buck의 점. 다른 Buck의 점은 숨김 그대로 |
| D8 | **시험대: 세션 Buck의 점** | 마커 ID로 Buck을 정한 뒤 그 Buck의 점 4개로 기록·맞춤·화살표 검사·저장. `G` 때 보이는 정렬점도 그 Buck의 것 |

- 좌석 시점(운전석 기준점·동승석·2열)은 차에 하나다. Buck과 상관없다(지금 그대로).

### 시험 바뀜·추가

| ID | 바뀜 | 확인 |
|---|---|---|
| E3 | 추가 | Buck A 점 3개 + B 점 4개(시험대 `M` 거부, 손 4점은 됨), 한 Buck 안 `Order` 겹침, `Buck` = `C`(없는 이름), `Buck` 빈 값 → 각각 설정 오류·이유 |
| E4 | 바꿈 | A 점과 B 점을 **다른 자리**에 둔다(B 점 = A 점을 차 기준 X −30 cm, Y +20 cm 옮긴 자리). A·B 파일은 각자 점으로 저장된 값. 전환하면 각 Buck 점 기준으로 맞은 Pawn(±0.01 cm). B 파일을 A 점으로 맞추면 다른 값이 나온다는 것도 확인(점 집합이 실제로 Buck별로 쓰임) |
| E6 | 바꿈 | 활성 B에서 `P` 순서·라벨이 B 점, 표시 구는 B 점만 |
| E8 | 바꿈 | 세션 Buck B → B 점 4개로 맞춤(A 점 아님) |
| E12 | 새 | `Bucks` 빈 상태에서 정렬점 `Buck` 값이 아무거나 있어도 지금과 같은 결과(무시) |

**기존 시험 감사 추가:** `Bucks`가 비면 `Buck` 값을 무시하므로 P4h·P5i·M1 시험의 정렬점 결과는 그대로다. 정렬점 컴포넌트에 프로퍼티 하나가 늘 뿐 기존 단언 변경 없음.

### 사용자 확인 바뀜

- **집:** `BP_Car_Core`(커밋하지 않음)에 HomeA 점 4개(지금 근사 좌석 점, `Buck` = HomeA)와 HomeB 점 4개(다른 크기 직사각형, 예: 60 × 40 cm, `Buck` = HomeB)를 둔다. 바닥 테이프도 두 직사각형. 1–4 → HomeA, 5–8 → HomeB.
- **회사:** Buck마다 볼트 윗면 가운데에 정렬점 4개(`Buck` = 그 Buck 이름), 마커 번호대.

### 개정 1의 D6 바꿈 — `B` = 활성 Buck을 바꾸고 레벨 다시 열기 (리뷰 파일 질문 답)

`L_Main`은 저장 정렬이 적용되면 첫 Tick에 01번을 자동 시작한다(`VBoRScenarioRunner.cpp:547`, `:658`). 그래서 "흐름 시작 전에만"으로는 전환할 틈이 없다(Codex 질문). 흐름 쪽 규칙을 바꾸지 않고 해결한다.

| # | 바뀜 | 내용 |
|---|---|---|
| D6 | **`B` "Next buck" = 다음 Buck을 활성으로 쓰고 지금 레벨을 다시 연다** | `ActiveBuck.txt`에 다음 Buck(끝이면 처음)을 쓴 뒤, **`F6`(Reload level)과 같은 레벨 다시 열기**를 한다. 다시 열린 `L_Main`은 지금 규칙 그대로 새 활성 Buck 파일을 적용하고, 적용되면 01번을 자동 시작한다. 정렬값이 없으면 지금처럼 시작이 막힌다.<br>- **흐름 상태와 상관없이 받는다**(시작 전·진행 중·자유모드). 진행 중이면 진행이 사라지므로 버튼 스타일은 `Danger`(Primary 아님).<br>- 거부: Buck 0·1개(`One buck or none`), 시점 전환 중·점 정렬 중(지금 `ReapplyRejection`과 같은 이유), `ActiveBuck.txt` 쓰기 실패(이유, 활성·레벨 그대로).<br>- `F6`·`A`·좌석 버튼·자동 시작 규칙은 바꾸지 않는다(P5g D8 그대로) |

- 처음 본문 D6의 "다시 적용과 같은 길", "흐름 시작 뒤·자유모드 거부"는 없앤다.
- **E4 바꿈:** `B` → `ActiveBuck.txt` = B, 레벨 다시 열기 요청 1회(실제 `OpenLevel`은 시험용 대체로 확인 — F6 시험과 같은 방식). 다시 연 상태(새 world 또는 `LoadSetupAlignment` 재호출)에서 B 점·B 파일로 맞은 Pawn, 01번 자동 시작. 거기서 `Passenger`/`Rear`는 지금 규칙대로 B 기준 + 좌석 차이. 다시 `B` → A.
- **E5 바꿈:** 거부는 Buck 1개, 시점 전환 중, 점 정렬 중, 쓰기 실패만. 진행 중·자유모드에서는 **받는다**(레벨 다시 열기 요청).
- **기존 시험 감사:** P5i E3·E5·E6의 기대 목록은 `FP5iWorld`(프로필 없음)에서 `B`가 비활성이라 그대로. `Danger` 스타일은 프로필이 있을 때만.
