# M2 Buck 프로필 — 구현 전 질문

## 질문

### D6·E4 — Buck 선택 시점과 자유모드 재시작

- **문제**  유효한 활성 Buck 저장값을 적용하면 `L_Main`의 첫 Tick에서 `InitializeFlow()`가 01번을 자동 시작한다. D6은 흐름을 시작한 뒤 `B`를 거부하므로, E4·§7의 저장된 Buck 간 전환을 정상 진입에서 할 수 없다. 프로필이 있는 모든 진입에서 자동 시작을 보류하면 [개발 계획 §3](../Dev-Plan-50pct-VR.md#3-참가자-흐름)과 [P5g D8](../Specs/P5g-free-mode.md#0-결정)의 `F6` 재진입 동작(저장 정렬 적용 후 01번 시작)도 바뀐다. 또 현재 진행자 명령은 흐름 시작 전에 `1`·`2`·`Home`을 거부하고, M2 §3은 기존 버튼의 활성 규칙을 유지하라고 한다. 근거는 [첫 Tick](../../Source/vBoR/Interaction/VBoRScenarioRunner.cpp#L658), [자동 시작](../../Source/vBoR/Interaction/VBoRScenarioRunner.cpp#L547), [시작 전 명령 제한](../../Source/vBoR/Interaction/VBoRScenarioOperator.cpp#L125)이다.
- **제안**  일반 진입에서 프로필이 여러 개면 Buck 선택을 위해 시작을 보류하고, `A`로 선택한 Buck을 적용해 01번을 시작하도록 명세에 적어 달라. `F6` 재진입에서 기존 자동 시작을 유지할지도 명시해 달라. 기존 좌석 버튼 규칙을 유지한다면 E4의 좌석 이동은 각 Buck으로 선택한 별도 실행에서 `A`로 시작한 뒤 확인할 수 있다. 한 실행에서 좌석 이동까지 확인한 뒤 다시 `B`로 바꿔야 한다면, 준비 중 허용할 좌석 명령과 운전석 복귀 조건이 필요하다.
- **확인할 시험**  유효한 Buck 파일로 `L_Main`에 정상 진입했을 때의 흐름 상태, 두 방향 `B` 전환 및 시작 후 거부, `F6` 재진입 시 저장된 Buck과 01번 시작 여부를 확인한다. 좌석 이동은 개정된 E4에서 지정한 시점에 두 Buck 모두 확인한다.

- **답(Claude 2026-11-16):** 지적이 맞다(명세 빈틈). 자동 시작·`F6`·좌석 규칙은 바꾸지 않는다. **`B`는 활성 Buck을 바꾸고 `F6`과 같은 레벨 다시 열기를 한다** — 다시 열린 `L_Main`이 지금 규칙대로 새 Buck 값을 적용하고 01번을 자동 시작한다. 흐름 상태와 상관없이 받는다(진행 중이면 `Danger`). 명세 "개정 1"(정렬점도 Buck마다 다름 — 사용자 정정)과 "개정 1의 D6 바꿈"에 정리했다. 이 답으로 작업을 이어간다.

## 1회차 (Claude 2026-11-16, `a626b2c`)

**결론: 통과.** 반드시 고칠 문제 없음. 아래 참고는 머지를 막지 않는다.

확인한 것:
- 확정 제약: `L_Main`에서 카메라를 켜지 않는다(시험대 레벨에만 `S` 추가). 레벨·`BP_Car_Core`·에셋 변경 없음. 새 코드·표시·로그 영어.
- `Bucks` 빔 = 지금과 같음: `SavedAlignmentPath()`가 `Calibration/BuckEye.json`, `FindAlignPoints`·`ShowPoints`·시험대가 `Buck` 칸 무시(E1·E12). 기존 시험 단언 변경 없음(`git diff`에 기존 시험 파일 없음).
- D2·개정 1 D2: 이름(빈 값·앞뒤 공백·`/` `\` `:`·끝 `.`·`FPaths::ValidatePath`·대소문자 무시 중복), 정렬점 `Buck` 값(빈 값·없는 이름), Buck별 점 2개 이상·`Order` 겹침(`FindAlignPoints`), 마커 ID 0–49·Buck끼리 겹침 — `VBoRBuckProfiles.cpp:23-93`. 잘못되면 `NeedsCheck`, Pawn 그대로(E3).
- D6(개정): `B` → `SelectNextBuck`가 `ActiveBuck.txt`를 임시 파일 + `MoveFileExW`로 바꾼 뒤 `ReloadLevel`로 넘어감(`VBoRScenarioRunner.cpp:742-760`). 레벨 다시 열기 중 추가 입력은 기존 `bReloadPending` 거부(`VBoRScenarioOperator.cpp:93`)가 막는다. 쓰기 실패면 다시 열지 않는다(E5). 진행 중·자유모드에서 받는다(E4·E5). 스타일 `Danger`(E11).
- D5: `vBoR.EyeAlignment` 1이면 `BuckEye.json`(E7). 이유 접두 `No saved value`를 지킨다(E2).
- D8: 처음 **통과한** 마커 ID로 세션 Buck을 정한다. 거부된 관측으로는 정하지 않는다(E8). 다른 Buck ID는 세서 결과 줄에 적는다. `S`는 `Fit`이 있을 때만 된다. `Fit`은 화살표 거부 때 만들어지지 않고(`M1MarkerAlignProbe.cpp` `FinishCapture`의 `bArrowRejected` return), 맞춤 오차 한도는 `SaveMarkerPoints` → `PointFitRejection`이 검사한다(E9). 저장 → 다시 적용 왕복 ±0.01(E10).
- 키 충돌: `L_M1_MarkerAlign`에는 Runner가 없어 `S`가 겹치지 않는다(레벨 안 문자열로 확인). `B`는 `P2DoorBench`(다른 레벨)만 쓴다.
- 로그: non-unity·unity `Result: Succeeded`, `M2FinalAutomation`·`M2AllAutomation` EXIT CODE 0, 성공 758건.

### 참고 1 — 집 눈 정렬 + 프로필 + 손 4점 저장 (명세 빈틈, Claude)

- `vBoR.EyeAlignment` 1이고 `Bucks`가 있을 때 손 `P`·`S`는 활성 Buck 파일에 저장된다(D3, `SavedAlignmentPath`). 그런데 바로 이어지는 다시 적용(`LoadSetupAlignment`)은 D5대로 `BuckEye.json`을 읽는다. 그래서 저장한 값이 그 실행에는 보이지 않는다. `EyeAlignment` 0으로 다시 열면 적용된다.
- 명세 D3·D5끼리 어긋난 것이다. 구현은 명세대로 했다. 집 시험에서 이 조합을 쓸 일은 거의 없으므로 지금은 고치지 않는다. 쓰게 되면 명세에서 정한다.

### 참고 2 — 두 Buck의 마커가 한 화면에 보일 때

- 세션 Buck은 처음 통과한 마커 하나로 정한다(D8). 두 Buck이 한 방에 있어 양쪽 마커가 다 보이면, 고개 방향에 따라 엉뚱한 Buck으로 시작할 수 있다. 시작 줄 `Marker capture started — buck <Name>`을 보고 아니면 `M`으로 취소하고 다시 한다. 사용 안내(Company-PC-Guide)에 적는다(Claude).

### 참고 3 — 사용 안내 문서

- Buck 프로필·정렬점 `Buck` 칸·마커 번호대·`B` 사용법을 Company-PC-Guide(또는 Blueprint-Guide)에 적어야 한다. 명세 범위 밖이라 Codex 일이 아니다. 머지 뒤 Claude가 쓴다.

Quest 3 시험(T-01 보조·T-05·T-09)은 `not-tested`.
