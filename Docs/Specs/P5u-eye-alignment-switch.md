# P5u 눈 방식 정렬 스위치 — 집 시험 때만 L_Main에 적용, 끌 수 있게

| 항목 | 값 |
|---|---|
| 단계 | P5 리허설 준비 — 집 시험 도구(사용자 요청 2026-10-22). 시험 T-01 |
| 결정 근거 | **사용자(2026-10-22):** "eye level로 맞추는거 L_Main에도 가능하게 되어있나?" → "P0-c 레벨에서 맞추는거 test할때만 맞출 수 있나? 나중에 끌 수 있게 해주고?" → **개정 1:** "console에서 vbor.eyealignment 켜진 상태에서만 작동되게끔 하자. 꺼진 상태면 적용 안되게 하고 … L_Main에서 eyealignment가 켜져있는 상태면 적용시켜. 그 외는 적용시키지마"<br>**지금:** L_Main은 P0-c에서 저장한 눈 방식 값(version 1)을 **항상** 적용한다. 높이는 바닥 기준이다(`bHeightFromFloor`, U-17).<br>**정한 것:**<br>① 스위치는 집 시험 콘솔 변수다. **켜진 동안에만 적용하고, Play 중에 바꾸면 바로 반영한다**(사용자, 개정 1). 기본은 꺼짐이다(`vBoR.HomeShift`와 같은 방식, Company-PC-Guide §2 "회사 평가에서는 켜지 않는다")<br>② 켜면 높이도 저장한 눈으로 맞춘다("eye level")<br>③ 집 PC는 리포에 안 올라가는 설정 파일로 늘 켜 둔다. 회사 PC에는 그 파일이 없어 자동으로 꺼진다 |
| 베이스 브랜치 | `main` @ 이 명세 머지 뒤 |
| 작업 브랜치 | `codex/P5u-eye-alignment-switch` |
| 리뷰 파일 | `Docs/Reviews/P5u-eye-alignment-switch.md` (첫 리뷰 때 생김) |
| 상태 | 명세 — 구현 전 |
| 에디터 작업 | **없음.** BP·레벨은 바꾸지 않는다 |
| 앞 명세 | [P5t-box-blueprint.md](P5t-box-blueprint.md) |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **콘솔 변수 `vBoR.EyeAlignment`(int, 기본 0)** | 설명 문구 예: "Home testing only: 1 = L_Main applies the P0-c head alignment save (version 1), height from the saved eye. 0 = point alignment only. Changes take effect while playing."<br>- 다른 집 시험 변수(`Interaction/VBoRScenarioRunner.cpp:37-40`)와 같은 방식(`TAutoConsoleVariable`)이다. 두는 파일은 구현자가 정한다 |
| D2 | **꺼짐(0): L_Main은 파일의 눈 방식 값을 적용하지 않는다** | - 파일에서 읽는 곳(`LoadSetupAlignment`, `Interaction/VBoRVehicleAlignmentComponent.cpp:46-53`)에서 판정한다. Play 시작(`:28`)과 진행자 재적용(`:222`)이 둘 다 이곳을 지난다. Play 중에 끄는 경우는 D6이다.<br>- 읽은 값이 version 1이면 저장값이 없는 것처럼 다룬다. Pawn을 옮기지 않는다(정렬 전 자리 그대로). 흐름은 지금처럼 정렬 필요로 막히고, 진행자 `O`(무시하고 시작)는 그대로 된다.<br>- 이유 문장에 꺼져서 쓰지 않았다는 것과 켜는 방법을 쓴다. 예: "저장값 없음 — 눈 방식 저장값(집 시험용)은 vBoR.EyeAlignment 0이라 쓰지 않음. 진행자 창 P로 점 정렬". 영어 이유(`EnglishReason`, `:176`)에도 같은 뜻을 넣는다(진행자 창·TV가 씀).<br>- **version 2(4점 정렬)는 스위치와 상관없이 지금 그대로다.**<br>- 파일 값을 거치지 않고 `InitializeAlignment`에 값을 직접 넣는 경로는 바꾸지 않는다. 실제 실행에서는 파일 읽기만 이 경로를 쓴다. 기존 시험 대부분이 이 경로로 version 1 값을 넣는다. 파일로 version 1을 읽는 시험 도우미는 `FP5iWorld` 하나다(§3, 개정 2) |
| D3 | **켜짐(1): 눈 방식 값을 적용하고 높이도 눈으로 맞춘다** | - version 1이면 `bHeightFromFloor`를 무시하고 저장한 눈 높이로 Pawn을 놓는다(`FVBoRBuckAlignment::CalculatePawn`, 지금 분기 `Interaction/VBoRPointAlignment.cpp:67-69`). 그래서 P0-c 레벨과 같이 머리가 운전석 눈 점에 온다.<br>- 좌석 전환·복귀(`PawnFor`, `VBoRVehicleAlignmentComponent.cpp:34-44`)도 같은 눈 높이 기준을 쓴다. Play 중에 켜는 경우는 D6이다.<br>- **진행자에게 보이게 한다.** 눈 방식으로 적용된 동안 `Describe()`(`:274-279`)에 "눈 방식(집 시험)"을 붙인다. 진행자 창과 VR 진행자 정보(`I`)가 이 문장을 쓴다 |
| D4 | **P0-c가 4점 정렬 저장값을 덮어쓰기 전에 복사한다** | P0-c 벤치와 L_Main은 같은 파일(`Saved/Calibration/BuckEye.json`)을 쓴다(`P0Probe/P0BuckBench.cpp:100`, `VBoRVehicleAlignmentComponent.cpp:31-32`). 그래서 P0-c에서 `C`를 누르면 4점 정렬 저장값이 사라진다.<br>- 벤치가 저장하기 직전(`P0BuckBench.cpp:255`)에, 지금 파일이 version 2면 같은 폴더에 `<파일 이름>.points-backup.json`(기본 `BuckEye.points-backup.json`)으로 복사하고 로그 한 줄을 남긴다. 이미 백업이 있으면 덮어쓴다(가장 최근 4점 값).<br>- **복사가 실패하면 저장하지 않는다.** 4점 값을 그대로 두고 이유를 남긴다.<br>- 되돌리기는 백업 파일 이름을 손으로 바꾼다(코드 없음).<br>- 지금 파일이 version 1이거나 없으면 지금처럼 저장한다. P0-c 레벨 자신의 적용 규칙은 바꾸지 않는다 |
| D5 | **집 PC에서 늘 켜 두는 방법** | 리포에 안 올라가는 사용자 설정 파일 `Saved/Config/WindowsEditor/Engine.ini`에 다음을 넣는다:<br>`[ConsoleVariables]`<br>`vBoR.EyeAlignment=1`<br>- 엔진은 엔진 설정의 `[ConsoleVariables]` 절을 시작 때 적용한다(`Engine/Source/Runtime/Core/Private/Misc/ConfigCacheIni.cpp:6665`).<br>- `Config/DefaultEngine.ini`에는 넣지 않는다. 리포에 올라가 회사에서도 켜진다.<br>- 실제로 되는지는 Codex가 로컬에서 확인해 보고한다(§7). 안 되면 멈추고 질문에 적는다 |
| D6 | **Play 중에 콘솔로 바꾸면 바로 반영한다**(개정 1) | 변수 값이 바뀐 것을 다음 frame 안에 알아챈다(감지 방법은 구현자).<br>**켬(0 → 1):** 파일 값이 version 1이고 아직 눈 방식으로 적용되지 않았으면, 진행자 재적용과 같은 길(암전 → 파일 다시 읽기 → 적용, `RequestReapply` `:166-174`)로 적용한다. 재적용이 거절되는 동안(점 정렬 중·시점 전환 중, `ReapplyRejection` `:158-164`)은 기다렸다가 허용되면 한 번 한다. 파일이 version 2거나 없으면 아무것도 하지 않는다.<br>**끔(1 → 0):** 지금 정렬이 **파일의 눈 방식으로 적용된 것**이면:<br>- 그 frame에 상태를 "저장값 없음"(D2의 이유)으로 바꾼다. 그래서 좌석 전환·흐름 시작이 지금 규칙대로 거절된다. 진행 중인 시점 전환은 지금처럼 버려진다(`AdvanceSwitch` `:205-214`).<br>- 암전한 뒤 Pawn을 **정렬 전 자리**(정렬 컴포넌트 `BeginPlay`에서 첫 적용 전에 기억한 Pawn 자세)로 되돌린다. 시점은 기준(운전석)으로, `HomeShiftCm`은 0으로 돌린다. 동승석·2열에 있어도 같다.<br>- 진행 중인 흐름은 멈추지 않는다. 다음에 좌석 전환이 필요하면 지금 규칙대로 "Saved alignment required"로 거절된다.<br>지금 정렬이 version 2거나 정렬이 없으면 끄고 켜도 아무것도 바뀌지 않는다(재적용·암전 없음).<br>**`InitializeAlignment`에 직접 넣은 값**(D2 마지막 줄, 시험 경로)은 이 감지의 대상이 아니다. "파일의 눈 방식으로 적용됨"을 따로 표시해 구분한다 |

## 1. 왜 만드는가

집에는 4점 정렬을 할 실물 좌석 점이 없다. 그래서 L_Main을 시험하려면 P0-c의 눈 방식 저장이 필요하다. 하지만 회사 평가에서는 4점 정렬만 써야 한다(U-18). 참가자나 설치자의 눈에 차를 맞추면 재려는 체격 차이가 지워진다(U-07). 그래서 눈 방식은 집 시험 때만 켜고, 회사에서는 꺼져 있어야 한다. 집 의자는 차 좌석 높이가 아니므로, 집에서는 높이도 눈으로 맞추는 편이 실제 차 시점에 가깝다.

## 2. 범위

### 만들 것

- 콘솔 변수와 L_Main 정렬 규칙(D1–D3), 이유 문장·진행자 표시.
- P0-c 벤치의 4점 저장값 백업(D4).
- 시험 `vBoR.P5u.*`(§4).

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| L_Main 안에서 눈 위치 저장(`C`) | 요청은 P0-c 저장을 집 시험 때만 쓰는 것이다. 저장은 지금처럼 P0-c 레벨에서 한다 |
| P0-c 레벨의 저장·적용 규칙 바꾸기(D4 백업 제외) | 개발 시험대다. 기존 P0Buck 시험이 그대로 지킨다 |
| 4점 정렬(version 2) 규칙 바꾸기 | 회사 설치 방식이다(U-18) |
| 저장 파일을 둘로 나누기 | 기존 저장값·시험 경로가 모두 바뀐다. D4 백업으로 충분하다 |
| `bHeightFromFloor` 기본값·`BP_Car_Core` 값 바꾸기 | 회사 설치 값이다(U-17). 스위치가 켜졌을 때만 눈 높이를 쓴다 |
| 진행자 창에 스위치 버튼 | 집 시험 변수는 콘솔·설정 파일로만 켠다(U-16과 같음) |
| 문서(Company-PC-Guide 집 시험 도구 목록, Blueprint-Guide) | 머지 뒤 Claude가 고친다 |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- **version 2(4점 정렬)의 적용·전환·재적용은 그대로다**(P4h).
- **스위치가 꺼진 동안 눈 방식 높이 규칙은 지금 그대로다.** `InitializeAlignment`에 version 1 값을 직접 넣으면 지금처럼 바닥 높이로 적용된다.
- **안내 문장에서 기존 시험이 찾는 말을 지우면 안 된다:**
  - "P0-c"(`P3Probe/P3FlowTests.cpp:373`)
  - "정렬 필요"(`:505`)
  - "Saved alignment required"(`P3Probe/P5iOperatorPanelTests.cpp:211`·`:671`)
  - "저장값 없음"으로 시작하는 이유(`EnglishReason`의 `StartsWith`, `VBoRVehicleAlignmentComponent.cpp:176-182`)
  - 문장을 늘리는 것은 괜찮다.
- **P0-c 벤치:** 저장 성공 시 version 1을 쓰는 것, 저장 실패 시 앞 값을 지키는 것은 그대로다(P0Buck E8, P4h E6).
- **기존 시험 감사 — 모두 그대로 통과해야 한다:**
  - `InitializeAlignment` 직접 호출: `P3FlowTests.cpp` 4곳(도우미 `:147` 포함), `P4hPointAlignmentTests.cpp` 12곳, `P5gFreeModeTests.cpp` 2곳, `P5hSpectatorTests.cpp` 1곳, `P5iOperatorPanelTests.cpp` 2곳. 스위치 기본 꺼짐에서 동작이 같다(D2 마지막 줄).
  - **파일에서 읽는 시험 도우미는 둘이다**(개정 2 — 처음 감사가 빠뜨림, 리뷰 Q1):
    - `UP4hTestAlignment`(`P3Probe/P4hTestFixture.h:8-24`): 파일에는 version 2만 쓴다(`P4hPointAlignmentTests.cpp:80-82`, `:141-151`은 저장소만 시험). version 1은 직접 넣는다(`:212-214`). 바꾸지 않는다.
    - `UP5iStoredAlignment`를 쓰는 `FP5iWorld`(`P3Probe/P5iTestFixture.h:8-16`, `P5iOperatorPanelTests.cpp:74-115`): version 1을 임시 파일에 쓰고 Play 시작·재적용 때 파일로 읽는다(`:97-99`, `:276`, `:299`, `:349`, `:468`). 기본 꺼짐에서는 이 도우미를 쓰는 P3c·P4c·P5i·P5o 시험 22개가 정렬이 없어 실패한다.
    - **허용하는 변경 — `FP5iWorld` 도우미 본문만:** 생성자에서 `NotifyBeginPlay` 전에 `vBoR.EyeAlignment`를 1로 켜고, 소멸자에서 world를 지운 **뒤에** 원래 값으로 되돌린다(끄는 감지 D6이 정리 중에 돌지 않게). 그 파일의 시험 본문·기대값은 바꾸지 않는다.
    - **높이 기대값은 그대로다.** 켜면 눈 높이(D3)를 쓰지만, 근사 차 눈 점 높이 115 cm(`P3VehicleMock.h:41`)와 저장값 높이 115 cm(`P5iOperatorPanelTests.cpp:98`)가 같다. 그래서 눈 기준과 바닥 기준이 모두 Pawn Z = 0을 낸다(`VBoRBuckAlignment.cpp:61-68`). 차를 Yaw로만 돌리는 시험(`:548`·`:566`)도 같다. P5i E19(`:351`)는 이미 `CalculatePawn`(눈 기준)과 같기를 기대한다.
    - 안내 문장 기대는 `Contains`·같은 시험 안의 전후 비교뿐이다. D3의 "눈 방식(집 시험)" 표시가 붙어도 깨지지 않는다.
  - P4h E6 `P4hPointAlignmentTests.cpp:162-176`: version 2 파일 위에 벤치가 version 1을 쓴다. 이제 백업 파일도 생긴다. 시험 폴더(`Saved/Automation`)에 백업이 남지 않게, 새 시험이나 구현이 정리 방법을 정한다. 이 시험 파일은 바꾸지 않는다.
  - P0Buck 시험 `P0Probe/P0BuckAlignmentTests.cpp`: 저장소를 직접 쓰거나 version 1 파일만 쓴다. 그대로다.
  - 시험 시작 때 `Saved/Calibration/BuckEye.json`을 읽는 경로(정렬 컴포넌트 `BeginPlay`)는 사용자 PC 파일에 따라 결과가 달라진다. 새 시험은 그 파일에 기대지 않는다(§5).

## 4. 경계 상황과 그 시험 (`vBoR.P5u.*`)

시험 이름은 `vBoR.P5u.E<n><설명>`이다. 변수를 바꾸는 시험은 끝날 때 원래 값으로 돌린다(`P3FlowTests.cpp:757`과 같은 방식).

| ID | 상황 | 확인 |
|---|---|---|
| E1 | 기본값 | `vBoR.EyeAlignment`가 있고 기본 0이다 |
| E2 | 꺼짐 + 파일 version 1 | 적용 안 됨. Pawn이 그대로다. 상태가 "저장값 없음"이고, 이유에 `vBoR.EyeAlignment`가 들어 있다. 흐름 시작이 "정렬 필요"로 막힌다. `O`로는 시작된다 |
| E3 | 꺼짐 + 파일 version 2 | 지금처럼 적용된다(바닥 높이) |
| E4 | 켜짐 + 파일 version 1 | 적용된다. Pawn이 `CalculatePawn`(눈 높이)과 같다(±0.01 cm·0.01°). 바닥 기준 결과와 높이가 다른 값으로 시험한다. `Describe()`에 "눈 방식"이 있다 |
| E5 | 켜짐 + 파일 version 2 | E3과 같다(스위치와 상관없음) |
| E6 | Play 중에 켬(D6) | 꺼짐 + version 1 파일로 시작(적용 안 됨) → 켬 → 암전 뒤 E4와 같은 자리에 적용된다. 점 정렬 중에 켜면 점 정렬이 끝날(취소) 때까지 적용되지 않고, 끝난 뒤 한 번 적용된다. 켬 상태에서 다시 켜도(값 같음) 재적용이 없다 |
| E7 | Play 중에 끔(D6) | 켬 + version 1로 적용된 상태 → 끔 → 같은 frame에 상태가 "저장값 없음"이고 이유에 `vBoR.EyeAlignment`가 있다. 암전 뒤 Pawn이 정렬 전 자리다(±0.01 cm·0.01°). 동승석에 있을 때 꺼도 같고, 시점이 기준으로 돌아온다. 전환 중에 끄면 그 전환은 버려진다. 끈 뒤 좌석 명령은 "Saved alignment required"로 거절된다 |
| E8 | version 2는 그대로(D6) | 4점으로 적용된 상태에서 켜고 끄면 Pawn·상태·전환 수가 그대로이고 암전이 없다 |
| E9 | 직접 넣는 경로 | 꺼짐에서 `InitializeAlignment`에 version 1을 넣으면 지금처럼 바닥 높이로 적용된다. 그 뒤 켜고 꺼도 바뀌지 않는다(기존 시험 보호, D2·D6) |
| E10 | P0-c 백업(D4) | version 2 파일 위에 벤치 저장 → 백업 파일이 원래 version 2 내용과 같고, 본 파일은 version 1이다. version 1 파일 위나 파일이 없을 때는 백업이 생기지 않는다. 백업 자리를 막아 복사가 실패하면 저장하지 않고 본 파일은 version 2 그대로다 |
| E11 | 기존 시험 | §3 감사대로 통과한다. `FP5iWorld` 도우미 변경 뒤 P3c·P4c·P5i·P5o가 기대값을 바꾸지 않고 통과한다 |

## 5. 구현자가 정할 것

- 변수를 두는 파일과 읽는 함수.
- 시험이 실제 사용자 파일 대신 임시 경로를 읽게 하는 방법(예: 시험 전용 경로 지정, `P0BuckBench`의 `SavePath`와 같은 방식). 시험 전용 멤버는 `WITH_DEV_AUTOMATION_TESTS` 안에 둔다.
- 이유·로그 문장(D2·D3·D4의 내용을 담는다).
- 백업 이름을 저장 경로에서 만드는 방법.

## 6. 완료 조건

1. D1–D6을 지킨다.
2. E1–E11이 통과한다.
3. 기존 `vBoR.*` 시험이 모두 통과한다.
4. non-unity·unity 빌드에서 vBoR 경고가 없다.
5. 보고는 AGENTS.md "작업 방식" 형식으로, 커밋 본문과 HANDOFF "Codex 결과"에 쓴다.

## 7. 로컬에서 확인할 것

- **Codex(에디터만, VR 없이):** D5 파일을 넣고 에디터를 연다. 콘솔 `vBoR.EyeAlignment`가 1인지 확인한다. 에디터를 닫았다 다시 연 뒤에도 그 절이 파일에 남는지 본다. 확인한 뒤에는 그 줄을 지워 원래대로 둔다. 결과를 보고 ⑤에 쓴다.
- **사용자(VR, 끝에 한꺼번에, T-01):**
  1. 집 PC에 D5 파일을 넣는다.
  2. P0-c에서 `C`로 저장하고 L_Main을 Play한다. 머리가 운전석 눈 점에 오는지(높이 포함) 본다. 진행자 창에 "눈 방식"이 보이는지 본다.
  3. Play 중에 콘솔에서 `vBoR.EyeAlignment 0`을 친다. 암전 뒤 정렬 전 자리로 돌아가고 "정렬 필요"가 보이는지 본다. `vBoR.EyeAlignment 1`을 치면 다시 눈 방식으로 맞는지 본다.
  4. 설정 파일 줄을 지우고 다시 Play한다. 처음부터 적용되지 않는지 본다.
- **회사:** D5 파일을 만들지 않는다. 4점 정렬만 쓴다. P0-c에서 `C`를 눌렀다면 `BuckEye.points-backup.json`을 `BuckEye.json`으로 되돌린다.

## 8. 확인한 근거

| 무엇 | 근거 | 쓰는 곳 |
|---|---|---|
| L_Main 정렬 읽기·적용 | 파일 경로 `Interaction/VBoRVehicleAlignmentComponent.cpp:31-32`, 읽기 `:46-53`, BeginPlay `:28`, 재적용 `:222`, 적용 `:66-93`, 저장값 없음 문장 `:71`, `EnglishReason` `:176-182`, `Describe` `:274-279`, 좌석 전환 `PawnFor` `:34-44` | D2·D3 |
| 높이 규칙 | `BasePawn` `Interaction/VBoRPointAlignment.cpp:56-71`(version 1 분기 `:67-69`), `bHeightFromFloor` 기본 켬 `VBoRVehicleAlignmentComponent.h:67` | D3 |
| 흐름 막힘 문장 | `Interaction/VBoRScenarioRunner.cpp:303`·`:668` | D2·§3 |
| 집 시험 변수 본보기 | `Interaction/VBoRScenarioRunner.cpp:37-40`, 시험에서 바꾸고 되돌림 `P3Probe/P3FlowTests.cpp:757` | D1·§4 |
| P0-c 저장 | 경로 `P0Probe/P0BuckBench.cpp:100`, version 2 읽으면 거부 `:143-147`, 저장 `:251-256`, 저장 길이 2 s `P0BuckBench.h:48` | D4 |
| 저장소 | `FVBoRBuckAlignmentStore::Load`·`Save` `Interaction/VBoRBuckAlignment.h:87-88` | D4 |
| 설정 파일의 콘솔 변수 | 엔진 `Runtime/Core/Private/Misc/ConfigCacheIni.cpp:6665` | D5 |

## 개정 이력

| 날짜 | 내용 |
|---|---|
| 2026-10-22 | 처음 작성(Claude) |
| 2026-10-22 | 개정 1(머지 전, 사용자): Play 중 콘솔로 켜고 끄면 바로 반영(D6). 끄면 정렬 전 자리로 돌아감. 시험 E6–E9 다시 씀, E10·E11로 번호 바꿈 |
| 2026-10-22 | 개정 2(Claude, 리뷰 Q1): §3 감사에 파일에서 읽는 시험 도우미 둘을 더함. `FP5iWorld` 도우미 본문만 변수를 켜고 끄게 허용. 높이 기대값이 같은 근거. D2 마지막 줄 고침. E11 |
