# R12 시나리오 흐름·편집 정리 — 리뷰

## 질문

### Q1 — 진행자 확인 권한과 기존 단언 · 열림 (2026-11-17, Codex)

R12 A2·E9는 저장된 `ConfirmBy = Participant` 단계도 진행자가 확인할 수 있어야 한다. `Source/vBoR/P3Probe/P5iOperatorPanelTests.cpp:172–173`은 같은 단계에서 진행자 `Confirm`이 거부된다고 단언한다. §3은 172행을 가리키지만 허용 범주는 참가자 버튼과 좌석 안내판 동작으로 한정돼 있다.

§3에 173행 진행자 권한 단언을 명시적으로 추가해도 되는가? 171행 확인 잠금과 177행 미충족 규칙 거부는 기존 기대를 유지할 수 있다.

### Q2 — TV 문구 우선순위와 기존 단언 · 열림 (2026-11-17, Codex)

R12 D6·E8은 빈 `SpectatorText` 다음에 `Instruction`을 사용한다. `Source/vBoR/P3Probe/P5hSpectatorTests.cpp:157`은 `Instruction`을 채우고, 159행은 번호표 문구, 163–164행은 `Unknown`을 기대한다. D6대로면 세 단언 모두 157행의 문구를 받는다. 169행은 계속 통과할 수 있지만 `Instruction` 때문에 번호표가 실제로 조회되는지 검증하지 못한다. §3의 예외는 159·169행만 언급한다.

§3에 E4의 시험 입력 변경을 명시해도 되는가? 번호표·`Unknown` 검사는 `Instruction`을 비운 입력으로 기존 단언을 유지하고, 빈 `SpectatorText`와 채운 `Instruction` 조합은 R12 E8에서 따로 확인할 수 있다.
