# P3c 집 시험용 위치 옮기기 — 리뷰

명세: [P3c-home-shift.md](../Specs/P3c-home-shift.md) · 브랜치: `codex/P3c-home-shift` · 베이스 `main` `88fc026`
상태: **1회차 리뷰 통과(2026-10-19, Claude — `2d0b466`). 열린 항목 없음.**

## 리뷰 1회차 (2026-10-19, Claude — `2d0b466`)

**결과: 통과. 열린 항목 없음.** D1–D6이 명세대로 들어갔다.
- 기존 단언·fixture는 바뀌지 않았다. 새 시험 10개만 `P3Probe/P5iOperatorPanelTests.cpp` 끝에 더했다.
- 진행자 버튼·단축키 표·`EVBoROperatorButton`·`EVBoRFlowCommand`는 그대로다.

**재현.** 에디터가 닫힌 것을 확인했고, 바뀐 C++ 6개의 시각을 갱신한 뒤 돌렸다.
- non-unity(`-DisableUnity -Module=vBoR`): `Result: Succeeded`, vBoR 경고·오류 0. 로그 `Saved/Logs/ReviewP3cNonUnity.log`.
- unity: `Result: Succeeded`, vBoR 경고·오류 0. 로그 `ReviewP3cUnity.log`.
- `vBoR.*`: 416/416(P3c 10개 포함), `TEST COMPLETE. EXIT CODE: 0`. 로그 `ReviewP3cAll.log`.
- 끝난 뒤 `UnrealEditor` 프로세스는 남지 않았다.

**Codex 토큰:** 3.80M(캐시 96%), 2026-10-19 00:38–00:45 UTC, 약 7분. 하위 에이전트는 0이다.
- 첫 non-unity 빌드가 새 시험의 `TObjectPtr` → `auto*` 오류로 실패해, 고친 뒤 한 번 더 돌렸다(보고 ③). 전체 시험은 한 번이다.

**확인한 것:**
- **D1 콘솔 변수**(`Interaction/VBoRScenarioRunner.cpp:35-36`, `:512-515`): `vBoR.HomeShift` 기본 0이고, 값이 1일 때만 켜진다. 꺼져 있으면 `HandleHomeShiftKey`가 거짓을 돌려준다. 그래서 진행자 창 키 처리에서 "처리 안 함"이고(E1), 키패드 키는 단축키 표에 없으므로 다른 명령으로도 가지 않는다.
- **D2 키**(`:174-177`, `:522-549`, `Interaction/VBoRScenarioOperator.cpp:190`):
  - 게임 입력은 `IE_Pressed`만 묶는다. 진행자 창은 `HandleOperatorKey` 맨 앞에서 같은 함수를 부르고, 반복 키는 처리만 하고 옮기지 않는다(E4가 실제 Slate `OnPreviewKeyDown`으로 확인).
  - 방향은 정렬 컴포넌트 Owner의 앞·오른쪽 벡터를 `GetSafeNormal2D`로 수평화한 것이다(`Interaction/VBoRVehicleAlignmentComponent.cpp:115-116`). E2·E3이 차 Yaw 30°에서 손으로 계산한 좌표(86.60, 50.00)·(123.21, 186.60)와 0.001 cm로 맞춘다. 나도 같은 값을 따로 계산했다.
- **D3 옮기기**(`Interaction/VBoRVehicleAlignmentComponent.cpp:111-130`):
  - Pawn 위치에 더하고, 검정에서 `FadeSeconds` 동안 밝힌 뒤 `OnPawnMoved`를 한 번 알린다. 전환 상태(`bSwitching`)는 켜지 않는다.
  - 원위치는 옮긴 양의 반대만큼 옮긴 뒤 0으로 맞춘다(실수 잔차를 남기지 않음).
  - **옮긴 양이 0이 되는 곳:** 컴포넌트에서 Pawn을 놓는 곳은 셋뿐이다. 정렬 적용(`:73-74`), 전환 이동(`:209-210`), 옮기기 자신(`:117`)이다. 앞의 둘에서 0으로 맞춘다. 재중심(`:81-85`)은 Pawn을 유지하므로 옮긴 양도 유지하는 것이 맞다. 정렬 다시 적용은 `LoadSetupAlignment` → `InitializeAlignment`를 거친다(E7).
- **D4 받지 않는 때**(`Interaction/VBoRScenarioRunner.cpp:532-536`): 레벨 다시 열기 암전과 전환·다시 적용 암전에서는 옮기지 않고 기록만 남긴다. 정렬 상태는 보지 않는다(E9: `NoSavedValue`·`NeedsCheck`에서 옮기고, 좌석 전환은 지금처럼 버림).
- **D5 R**(`:481-482`): R 처리 맨 앞에서 원위치를 부른다. 좌석 시점이면 옮긴 양이 이미 0이라 아무것도 안 한다. 기준 시점이면 원위치 뒤 01부터 시작한다(E8).
- **D6 상태**(`Interaction/VBoRScenarioOperator.cpp:223-228`): 켜져 있으면 Seat 줄 뒤에 `Home shift ON · +2.0 m front · −1.0 m side`를 붙인다. 꺼지면 원래 글이다(E10). 기록 줄은 `집 시험 옮기기 +X 100 cm → (x, y)`다.

### 참고 1 — NumLock이 꺼져 있으면 키패드 4·6이 ←·→(이전·다음 단계)로 들어간다

- Windows는 NumLock이 꺼져 있으면 키패드 8·2·4·6·5를 ↑·↓·←·→·Clear로 보낸다. UE는 Windows가 보낸 가상 키를 그대로 쓴다(`Runtime/ApplicationCore/Private/Windows/WindowsApplication.cpp`, 키패드를 따로 처리하지 않음).
- 그래서 NumLock이 꺼진 채 4·6을 누르면, 옮기기 대신 **이전 단계·다음 단계**가 실행된다. 7·1도 Home(운전석)·End(자유모드)가 된다.
- 코드 문제가 아니라 키 선택(내 명세 D2)의 함정이다. 집에서 쓸 때 **NumLock을 켠다.** 불편하면 키를 바꾸는 작은 개정을 한다.

### 참고 2 — 집에서 정렬이 "확인 필요"면 R은 버려진다

- R은 저장 정렬이 `Applied`여야 받는다(기존 규칙, `Interaction/VBoRScenarioOperator.cpp:147`). 그래서 집에서 플레이 영역이 바뀌어 "확인 필요"가 되면, R로는 원위치가 되지 않는다. 키패드 5로 원위치한다.

### 참고 3 — 시험이 콘솔 변수를 이전 값이 아닌 0으로 되돌린다

- `FP3cEnabled`는 끝에 `vBoR.HomeShift`를 0으로 둔다(`P3Probe/P5iOperatorPanelTests.cpp:519-520`). 에디터에서 1로 켜 둔 채 자동화 시험을 돌리면 꺼진다. 다시 켜면 된다. 고치지 않는다.
