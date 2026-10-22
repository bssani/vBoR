# P5v 영어 전환 — 코드·에셋에서 한글 없애기

| 항목 | 값 |
|---|---|
| 단계 | 산출물 정리(사용자 요청 2026-10-22). 시험 ID 없음 — 동작을 바꾸지 않는다 |
| 결정 근거 | **사용자(2026-10-22):** "이 프로젝트에서 한글은 존재하지 않아. 다 영어여야해. 그리고 주석도 간단하게 내 말투로 해야하고"<br>**Claude 질문 → 사용자 답:**<br>① 범위 → **코드·에셋만**. Docs(명세·리뷰·가이드)와 커밋 메시지는 한국어 그대로<br>② 주석 말투 → **짧은 조각문**(§5 예) |
| 베이스 브랜치 | `main` @ P5u 머지 뒤, 이 명세 머지 뒤 |
| 작업 브랜치 | `codex/P5v-english-text` |
| 리뷰 파일 | `Docs/Reviews/P5v-english-text.md` (첫 리뷰 때 생김) |
| 상태 | 명세 — 구현 전 |
| 에디터 작업 | **있음** — `DA_Main_Scenario` 안내문 18개, `L_Main`의 한글 문구 1개(MCP). 사용자 요청 |
| 앞 명세 | [P5u-eye-alignment-switch.md](P5u-eye-alignment-switch.md) |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **`Source/vBoR`의 한글을 모두 영어로 바꾼다** | 대상(Claude 집계 2026-10-22, P5u 브랜치 기준): 135개 파일, 한글 줄 약 900개.<br>- 주석 약 480줄<br>- 문자열 약 420줄: `UPROPERTY`/`UFUNCTION`의 `Category`·`DisplayName`·`ToolTip`, `UMETA(DisplayName=…)` 3개(`Interaction/VBoRAxisPart.h:30-32`), `UE_LOG`, 화면 문구(참가자 안내판·진행자 문구·설정 검사 결과·이유 문자열), 시험 문자열<br>- 근사 차·P0–P3 시험대(`P*Probe/`)도 포함한다 |
| D2 | **식별자는 바꾸지 않는다 — 뜻만 옮긴다** | 아래는 이름에 "English"·"Korean"이 들어 있어도 그대로 둔다. 바꾸면 레벨·BP·저장 파일의 참조가 끊긴다.<br>- C++ 클래스·함수·프로퍼티·enum 값 이름. 예: `EnglishReason()`(`VBoRVehicleAlignmentComponent.h:36`)<br>- 기본 서브오브젝트 이름. **특히 `TEXT("KoreanGuidance")`**(`Interaction/VBoRScenarioRunner.cpp:117`) — `L_Main`의 진행 Actor가 이 이름으로 컴포넌트 값을 저장한다<br>- Actor·컴포넌트 태그(`Car.*`), 자동화 시험 ID(`vBoR.*` 경로), 파일 이름, 저장 JSON의 필드 이름<br>- `Category`·`DisplayName`·`ToolTip`·`UMETA` 표시 이름은 바꿔도 저장값에 영향이 없다(표시용 메타데이터). 이것들은 D1대로 바꾼다 |
| D3 | **참가자 안내문은 아래 표대로 바꾼다** | 참가자가 보는 문장이라 Claude가 정한다. C++ 기본 시나리오(`Interaction/VBoRScenarioAsset.cpp:10-27`)와 `Content/Main/Data/DA_Main_Scenario`의 같은 한글은 같은 영어로 바꾼다. 표는 §6 |
| D4 | **한글 문자열로 분기하는 제품 코드는 만든 쪽과 읽는 쪽을 같이 바꾼다** | 구조는 바꾸지 않는다(접두어 비교 그대로).<br>- `Interaction/VBoRScenarioOperator.cpp:190` `Record.StartsWith("시점 명령")` ↔ 그 Record를 만드는 곳<br>- `Interaction/VBoRVehicleAlignmentComponent.cpp:229-230` `Reason.StartsWith("저장값 없음")`·`("재중심")` ↔ 그 `Reason`을 만드는 모든 곳. P5u의 `EyeDisabledReason`도 "저장값 없음"으로 시작한다(P5u D2) — 바꾼 접두어로 시작해야 한다<br>- 그 밖에 한글로 `Contains`·`StartsWith`·`==`를 하는 제품 코드가 있으면 같은 방식으로 짝을 맞추고 보고 ③에 적는다(Claude grep으로는 위 2곳뿐) |
| D5 | **주석은 짧은 조각문으로, 뜻은 줄이지 않는다** | 말투는 §5. 이유(괄호)·근거 ID(`U-xx`, `P2 R1-1` 등)·단위는 지키고, 그 밖의 군더더기는 줄여도 된다. 코드를 그대로 읽어 주는 주석이 되면 지운다. **"왜"가 담긴 주석은 지우지 않는다** |
| D6 | **에셋 텍스트도 바꾼다(MCP)** | Claude 바이너리 검사(직렬화된 UTF-16 문자열, 2026-10-22)에서 한글이 나온 에셋은 둘뿐이다. `Main/Car/Parts`의 BP 24개, `Main/Core`, `Development/` 레벨에는 없었다.<br>- `Content/Main/Data/DA_Main_Scenario`: 단계 안내문 19개(고유 문장 18개). §6 표대로 바꾼다. 단계 ID·규칙·태그·순서는 그대로다<br>- `Content/Main/L_Main`: 문구 "근사 형상·자리값 — 회사에서 Approx 폴더를 지우고 CAD로" 1개. 어느 Actor의 어느 값인지(TextRender 글자, Actor 라벨, 폴더 이름 등)는 MCP로 찾는다. 바꿀 문구: `Approximate geometry, placeholder values. Delete the Approx folder and use CAD`. 폴더 이름이면 바꾸지 말고 질문에 적고 멈춘다(폴더 경로는 Blueprint-Guide·P4i 시험이 쓴다)<br>- 다른 에셋은 열지 않는다 |
| D7 | **시험은 문자열만 따라 바꾸고 판정 논리는 그대로 둔다** | 제품 문자열을 비교하는 시험 문자열은 바꾼 제품 문자열과 정확히 같게 바꾼다. 시험만 쓰는 한글 문자열(예: `P3Probe/P5hSpectatorTests.cpp:157`, `P3Probe/P3FlowTests.cpp:92·404`)도 영어로 바꾼다. 시험 설명 문자열의 "Korean"(예: `P3FlowTests.cpp:406·413·736·768`)은 뜻에 맞게 "participant text" 같은 말로 바꾼다.<br>- 시험 개수·순서·판정(`TestTrue`/`TestEqual`/`TestFalse`)은 바꾸지 않는다<br>- 표에 묶인 시험(`P3FlowTests.cpp:1063`, 기본 시나리오 대조)은 §6과 같게 바꾼다 |

## 1. 왜 하는가

코드는 사용자 이름으로 나가고, 산출물을 보는 사람은 한글을 쓰지 않는다(사용자). 지금 규칙(AGENTS.md "코드 주석 — 사용자 말투로")은 한국어 개조식이었다. 이 명세와 함께 규칙을 영어로 바꾼다(이 명세 머지에 들어 있음).

## 2. 범위

### 만들 것

- `Source/vBoR/**/*.h`, `*.cpp`의 한글 → 영어(D1·D2·D4·D5·D7).
- 에셋 2개(D6).
- 에디터 표시 이름 대조표 — 커밋 본문에 넣는다(§7 보고).

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| `Docs/`, `AGENTS.md`, `CLAUDE.md`, 커밋 메시지·리뷰 파일 | 사용자 답 ①. 가이드 안의 라벨 이름은 머지 뒤 Claude가 대조표로 고친다 |
| 식별자 이름 바꾸기, 리팩터링 | D2. 영어 문구와 Reason 경로가 둘로 나뉜 곳(`EnglishReason()`·`FVBoRCommandDecision`의 `Reason`/`Record`, `Interaction/VBoROperatorPanel.h:17-22`)도 합치지 않는다. 문구만 바꾼다 |
| 동작·값·레이아웃 바꾸기 | 글꼴 크기, 안내판 크기, 줄바꿈 규칙 그대로. 영어 문장이 길어져 시험이 깨지면 고치지 말고 질문에 적고 멈춘다 |
| `Plugins/` 아래 | ISDK·MetaXR·로컬 도구는 건드리지 않는다(AGENTS.md) |
| `.gitignore` | Claude가 이 명세 머지에서 고친다 |
| D6의 두 에셋 밖의 에셋 열기·저장 | 한글이 없다(D6) |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- **동작:** 모든 판정·값·순서·시간·로그 횟수가 그대로다. 바뀌는 것은 사람이 읽는 글자뿐이다.
- **식별자·저장 이름:** D2 전부. `git diff`에서 `UPROPERTY`/`UFUNCTION` 줄은 메타데이터 문자열만 바뀌어야 한다.
- **에셋:** `DA_Main_Scenario`의 단계 수 19, ID 순서(01–16, 18–20), 규칙·태그·확인 주체는 그대로다. `L_Main`은 그 문구 하나만 바뀐다.
- **기존 시험:** `vBoR.*` 전체가 지금과 같은 개수로 모두 통과한다(P5u 머지 기준 547개 — 머지 뒤 개수를 다시 세어 보고에 적는다).
  - **문자열 대조 시험 감사(Claude, 2026-10-22):** 한글 문자열을 가진 시험 파일은 아래뿐이다. 이 파일들의 문자열은 제품 문자열과 같이 바꾼다.
    - `P3Probe/P3FlowTests.cpp` 76줄 — 진행자·참가자 문구 `Contains`, 기록 `Contains`·`Count`, 기본 시나리오 대조표(`:1063`)
    - `P3Probe/P5gFreeModeTests.cpp` 14줄
    - `P3Probe/P5iOperatorPanelTests.cpp` 6줄
    - `P3Probe/P5uEyeAlignmentTests.cpp` 4줄 — `EyeDisabledReason` 접두어(D4)
    - `P3Probe/P5aGloveboxTests.cpp` 2줄, `P2Probe/P2bAxisPartTests.cpp:399`, `P2Probe/P2ePartAssemblyTests.cpp` 1줄(설정 검사 결과 대조), `P3Probe/P5eRearviewMirrorTests.cpp` 1줄, `P3Probe/P5fKnobTests.cpp` 1줄, `P3Probe/P5hSpectatorTests.cpp:157`
    - `P3Probe/P3VehicleMock.h` 94줄·`.cpp` 1줄 — 시험이 아니라 근사 차의 `Category`·표시 문자열. 시험이 이 문자열을 읽으면 같이 바꾼다
  - **문자열을 바꿔도 결과가 같아야 하는 시험:** P5h TV 시험(안내문이 TV에 안 나옴 — 영어 안내문으로도 안 나와야 함), P5k E4 류의 높이 시험(명시적 개행 문자열을 쓰므로 영향 없음).
- **글꼴:** 참가자 안내판·진행자 문구는 `FCoreStyle::GetDefaultFontStyle`(`VBoRScenarioRunner.cpp:147·150·161`) 그대로다.

## 4. 경계 상황과 확인

| 상황 | 확인 |
|---|---|
| 접두어로 분기하는 Reason(D4)이 바뀐 뒤에도 같은 갈래로 간다 | 기존 시험(P5u E1–E3의 `EyeDisabledReason`, 진행자 창 alignment 문구)이 그대로 통과 |
| 같은 한글이 여러 곳에 있다(예: "문을 닫아보세요." 11·20) | §6 표 한 줄 = 영어 한 문장. 같은 한글은 같은 영어 |
| 한글과 영어가 섞인 문자열(예: `Approximate geometry \| … \| 손바닥 감싸 쥐기만 …`) | 한글 부분만 영어로. 구분자(`\|`, `—`)와 숫자 형식은 그대로 |
| 주석 안의 한글 식별자 설명(예: "`HingePivot` 좌표의 상자") | 식별자는 그대로, 설명만 영어 |
| 한글 자모·특수 문자(`·`, `—`, `→`) | 가운뎃점 `·`은 한글 문장 부호라 `, `나 `/`로 바꾼다. `—`·`→`는 그대로 둬도 된다 |
| 시험 설명 문자열(`TestTrue(TEXT("…"))` 첫 인자)의 한글 | 영어로. 판정 대상 문자열과 섞지 않는다 |

## 5. 주석 말투 — 짧은 조각문

- 영어 조각문으로 쓴다. 완전한 문장·관사·마침표를 고집하지 않는다.
- 무엇보다 **왜**. 이유는 괄호로 짧게.
- 근거 ID와 단위는 그대로 단다: `(U-04)`, `(P0-a §4)`, `cm`, `mm`, `deg`.
- 긴 함수의 단계 표시는 `// ── 1) Measure HMD height ──` 형식을 유지한다.

| 지금 | 바꾼 뒤 |
|---|---|
| `// 이미 진행 중이면 무시 (중복 클릭 방지)` | `// Ignore if already running (double click)` |
| `// 무효 frame 의 위치는 0 이거나 마지막 값 고정이라 못 믿음. 직전 유효 위치 기준 (P0-a §4)` | `// Bad frame pos is 0 or stale. Use last valid pos (P0-a §4)` |
| `// 면 뒤의 먼 손은 제외함 (실제 crossing·연속 밀기만 예외, P2 R1-1)` | `// Skip far hands behind face (except real crossing or ongoing push, P2 R1-1)` |
| `// 같은 밀기에는 crossing 을 다시 요구하지 않음 (속도 상한으로 면 뒤에 남는 손, P2 E8b)` | `// Same push needs no new crossing (speed cap leaves hand behind face, P2 E8b)` |
| `// 칸 순서는 위 프로퍼티 순서와 같음 (P5s D12)` | `// Slot order = property order above (P5s D12)` |

**용어(이미 코드에 있는 영어를 먼저 쓴다):** 진행자 Operator, 참가자 Participant, 정렬 Alignment, 근사 형상 Approximate geometry, 자리값 Placeholder, 진행자 확인 Operator confirm, 건너뛰기 Skip, 시점 Viewpoint, 운전석 Driver seat, 동승석 Passenger seat, 2열 Rear seat, 바닥 경로 Floor route, 손잡이 Handle, 잡는 자리 Grab area, 미는 면 Push face, 잠김·풀림 Locked·Unlocked, 설정 검사 Check setup, 재중심 Recenter, 저장값 없음 No saved value.

## 6. 참가자 안내문 (D3)

| 한글 | 영어 | 쓰는 곳 |
|---|---|---|
| 키를 가진 상태로 차량에 접근하세요. | Approach the vehicle with the key. | C++ 01 |
| 접근할 때 외장 조명을 확인하세요. | Check the exterior lights as you approach. | C++ 02 |
| 테이블의 키를 집으세요. | Pick up the key from the table. | DA |
| 키 버튼으로 외장 조명을 켜고 꺼 보세요. | Use the key button to turn the exterior lights on and off. | DA |
| 차량 주변을 둘러보세요. | Look around the vehicle. | DA |
| 리프트게이트를 열어보세요. | Open the liftgate. | DA |
| 박스를 적재해 보세요. | Load the box. | DA |
| 리프트게이트를 닫아보세요. | Close the liftgate. | DA |
| 도어 핸들을 조작해 보세요. | Operate the door handle. | C++ 07, DA |
| 문을 열어보세요. | Open the door. | C++ 08, DA |
| 차량에 탑승해 보세요. | Get in the vehicle. | C++ 09, DA |
| 좌석에 앉아보세요. | Sit in the seat. | C++ 10, DA |
| 문을 닫아보세요. | Close the door. | C++ 11·20, DA |
| 실내를 둘러보세요. | Look around the interior. | C++ 12, DA |
| 가장 먼 버튼까지 손가락을 뻗어보세요. | Reach the farthest button with your finger. | C++ 13, DA |
| 동승석 위치에서 확인하세요. | Check from the passenger seat. | C++ 14, DA |
| 2열 위치에서 확인하세요. | Check from the rear seat. | C++ 15, DA |
| 시트 폴딩 각도를 확인하세요. | Check the seat folding angle. | DA |
| 하차를 위해 문을 열어보세요. | Open the door to get out. | C++ 18, DA |
| 차량에서 내려보세요. | Get out of the vehicle. | C++ 19, DA |

- 자유모드 안내(`FreeModeInstruction`) 등 그 밖의 참가자 문구는 이 표의 말투(짧은 명령문, 마침표)로 Codex가 옮긴다. 보고 ③에 목록을 적는다.

## 7. 완료 조건

1. `LC_ALL=C.UTF-8 grep -rlP "[\x{1100}-\x{11FF}\x{3130}-\x{318F}\x{AC00}-\x{D7AF}]" Source/vBoR` 결과가 비어 있다.
2. D2·§3: `git diff main...` 에서 식별자·태그·서브오브젝트 이름·시험 ID 변경이 없다.
3. D6: MCP로 다시 읽은 `DA_Main_Scenario` 19단계 안내문이 §6과 같고, `L_Main` 문구가 바뀌어 있다. 두 에셋 외에 저장한 에셋이 없다.
4. non-unity(`-Module=vBoR`)·unity 빌드 성공, vBoR 경고 0.
5. `vBoR.*` 전체 통과, 개수는 P5u 머지 기준과 같다.
6. 커밋 본문 보고(AGENTS.md ①–⑤)에 더해:
   - **에디터 표시 이름 대조표:** 바뀐 `Category`·`DisplayName`·`UMETA` 전부(한글 → 영어). Claude가 이걸로 Blueprint-Guide·Company-PC-Guide를 고친다.
   - 진행자 창·참가자 안내판에 보이는 주요 문구 대조(10줄 안팎).
   - D4 외에 찾은 문자열 분기, D6에서 찾은 문구의 위치.

## 8. 로컬에서 확인할 것

- **빌드:** non-unity·unity, `vBoR.*` 전체 한 번(AGENTS.md 토큰 규칙).
- **에디터:** `DA_Main_Scenario`·`L_Main` MCP 저장 뒤 에디터 닫기.
- **Claude 리뷰 때:** 에셋 바이너리 한글 검사를 다시 돌린다. 진행자 창(P5i)과 VR 안내판 글자가 잘리지 않는지 PIE 화면 확인은 사용자 몫이다.
- **Quest 3:** 안내판 영어 문장이 한 화면에 읽히는지 — 다음 VR 확인 묶음에 넣는다(`not-tested`).

## 개정 이력

| 날짜 | 내용 |
|---|---|
| 2026-10-22 | 처음 작성 |
