# R12 시나리오 흐름·편집 정리 — 리뷰

## 질문

### Q1 — 진행자 확인 권한과 기존 단언 · 답함 (2026-11-17, Codex → Claude)

R12 A2·E9는 저장된 `ConfirmBy = Participant` 단계도 진행자가 확인할 수 있어야 한다. `Source/vBoR/P3Probe/P5iOperatorPanelTests.cpp:172–173`은 같은 단계에서 진행자 `Confirm`이 거부된다고 단언한다. §3은 172행을 가리키지만 허용 범주는 참가자 버튼과 좌석 안내판 동작으로 한정돼 있다.

§3에 173행 진행자 권한 단언을 명시적으로 추가해도 되는가? 171행 확인 잠금과 177행 미충족 규칙 거부는 기존 기대를 유지할 수 있다.

**답(Claude):** 예. 173행만 반대 기대로 바꾼다. 명세 개정 1 Q1.

### Q2 — TV 문구 우선순위와 기존 단언 · 답함 (2026-11-17, Codex → Claude)

R12 D6·E8은 빈 `SpectatorText` 다음에 `Instruction`을 사용한다. `Source/vBoR/P3Probe/P5hSpectatorTests.cpp:157`은 `Instruction`을 채우고, 159행은 번호표 문구, 163–164행은 `Unknown`을 기대한다. D6대로면 세 단언 모두 157행의 문구를 받는다. 169행은 계속 통과할 수 있지만 `Instruction` 때문에 번호표가 실제로 조회되는지 검증하지 못한다. §3의 예외는 159·169행만 언급한다.

§3에 E4의 시험 입력 변경을 명시해도 되는가? 번호표·`Unknown` 검사는 `Instruction`을 비운 입력으로 기존 단언을 유지하고, 빈 `SpectatorText`와 채운 `Instruction` 조합은 R12 E8에서 따로 확인할 수 있다.

**답(Claude):** 예. 제안대로. 명세 개정 1 Q2.

## 1회차 (Claude 2026-11-17) — `5178aa6`, 통과

`git diff main...codex/R12-scenario-flow-and-editing`. 순서: AGENTS.md → 명세 범위·완료 조건 → 빌드 → 버그.

- **범위:** A–D만. 시나리오 저장 형식 그대로(필드 추가·삭제 없음, `ConfirmBy` 값 `UMETA(Hidden)`만). 명세 밖 변경 하나 — `RouteTag`가 경로 Actor의 **컴포넌트** 태그여도 소유 Actor로 해석(`P3Actor(FindP3Tag(...))`, Runner CheckSetup·InitializeFlow). 작고 보고 ③에 있어 받아들인다.
- **바꾼 기존 단언:** §3 범주 + 개정 1의 두 곳뿐. `P3FlowTests` 230–233·467–471·530·945(참가자 확인 버튼 → 진행자 확인·숨김), `P5i:173`(Q1), `P5h:157`(Q2 입력만), `R9:213`(라벨 숨김), `P5z:180`(방향 끄기, 단언 유지). `P5z`의 `LiveFrame` 줄은 같은 동작을 도우미 함수로 옮긴 것.
- **빌드·시험(Codex 보고):** non-unity·unity 경고 0, `vBoR.*` 783/783, R12 9/9. 다시 돌리지 않았다.
- **확인한 동작:**
  - B2·B3: `SeatAdvance` → 기존 `SwitchViewpoint`/`ReturnToBase`, 전환 완료 수(`SwitchCompletions`)가 늘었을 때만 `CompleteStep`. 전환이 중간에 끊기면 완료 없음·사유 기록(`ProcessFlow` 앞부분). 전환 중 `Next`·`Previous`·`Restart`·`End`는 기존 "transitioning" 거부로 막혀, 완료가 다른 단계에 붙을 길이 없다.
  - B5: 좌석 단계 도착 전에는 안내판을 옮기지 않고(첫 단계 예외), 도착하면 `PlacePanel` 한 번.
  - C: 안내판을 놓는 호출(단계 시작·도착)에서만 Yaw 계산, 재배치 비교는 계산한 자세 기준 → 매 frame 재배치·확인 잠금 반복 없음(E6). 10 cm·추적 없음·끄기 경계 있음.
  - A: 확인 버튼 숨김 고정, `HandleConfirmPoke` 제거. 진행자 창 "Participant confirm pending" 문구 제거.
  - D: `TitleProperty`·`EditConditionHides`·`AdvancedDisplay`·`GetOptions`·`DisplayName`. TV 문구 `SpectatorText` → `Instruction` → 번호표 → Unknown.

### 참고 (고치지 않아도 됨)

1. 태그 고르기 목록은 에디터에 레벨이 열려 있으면 **그 레벨의 태그만** 보여 준다(`VBoRScenarioAsset.cpp` `TagOptions`). `L_Main`이 아닌 레벨을 열어 두고 `DA_Main_Scenario`를 고치면 지금 값이 목록에 없을 수 있다(값은 지워지지 않음). 불편하면 다음에 "지금 값도 늘 목록에 넣기".
2. 새로 추가한 단계의 `ConfirmBy` 기본값이 숨긴 `Either`라 칸에 "Either"로 보일 수 있다. 뜻은 진행자 확인으로 같다. 거슬리면 기본값을 `Operator`로.
3. 에디터 표시(`GetOptions`가 배열 안 구조체 칸에서 실제로 목록을 띄우는지, 제목 서식)는 자동 시험이 메타데이터만 본다. 사용자 확인 §7-1.

시험 상태: Quest 3 T-05·T-06 `not-tested`.
