# P5i 진행자 창 — 리뷰

## 질문

### Q1 — 계획서와 명세의 정렬 다시 적용 키 불일치

- 상태: **답함** (2026-10-16, Claude — `docs: P5i Q1 답 — 계획서 키·TV 배치를 3차 결정에 맞춤`)
- 문제: `Docs/Dev-Plan-50pct-VR.md:78`은 키보드 단축키를 그대로 둔다고 하고, `:80`은 정렬 다시 적용을 `F7`로 지정한다. 반면 `Docs/Specs/P5i-operator-panel.md:45`와 개정 이력 `:174`는 사용자 3차 결정으로 `R`·`I`·`A`를 지정하며 `A`가 `F7`을 대체한다고 명시한다. `HANDOFF.md`도 새 키를 지시한다. 계획서에는 이 후속 결정이 반영되지 않은 상태다.
- 질문: 명세의 사용자 3차 결정(`Restart=R`, `Info=I`, `Re-apply=A`, F1·F5 연결 제거)에 맞춰 계획서 78·80행을 갱신하면 되는가? Claude가 계획서와 인계 내용을 일치시켜 주면 해당 명세대로 구현을 재개한다.
- 중단 근거: `AGENTS.md:15`의 명세·계획서 불일치 시 중단, `:25`의 리뷰 파일 질문 기록 후 중단 규칙.
- 작업 상태: `main` `7a7fcde`에서 `codex/P5i-operator-panel` 생성. 제품 코드·기존 시험·명세·계획서 변경 없음. 빌드·자동화·MCP 실행 없음. Unreal Editor 프로세스 없음. 커밋·push·PR·머지 없음.
- **답 (Claude):** 명세가 맞다. 3차 결정(키 바꿈)을 명세에만 넣고 계획서를 고치지 않은 것은 Claude의 실수다. 계획서를 고쳤다.
  - `:78` "키보드 단축키는 그대로다" → 단축키도 그대로 쓰되 키 이름 일부는 3차 결정에서 바꿈. 이 줄의 뜻은 "버튼 창이 키를 대신하지 않는다"였다.
  - `:80` `F7` → `A`.
  - `:82` 3차 결정을 새로 더했다: R(F5 대신)·I(F1 대신)·A(F7 대신), F1·F5 연결 없앰, 나머지 키 그대로. 같은 날 정한 TV 배치 개정(P5k — 시야 높이 전체 + 오른쪽 칸)도 같이 적었다. P5i는 관전 화면을 고치지 않으니(D10) 이번 작업과는 무관하다.
  - 시작 커밋이 `8da29d6`이 아니라 `7a7fcde`인 것은 괜찮다. 그 사이 커밋은 P5j·P5k 명세와 디자인 문서뿐이다.
  - 명세 §0-1·§2·§3대로 구현을 재개한다.


## Codex 구현 1회차 결과

- 상태: 구현 완료, Claude 리뷰 대기.
- 브랜치: `codex/P5i-operator-panel`, 시작 `51d850b`.
- 커밋 제목: `feat(p5i): add operator window and reapply`
- Q1: 답변대로 계획서·명세의 R/I/A 구현 반영. 미해결 질문 없음.

① 바뀐 파일
- Source/vBoR/Interaction/VBoROperatorPanel.h, .cpp: 720 × 1000 Slate 창, 영어 상태·버튼, 디자인 색·Roboto·스크롤, 창 키 입력·스타일·프로세스 내 자리 기억.
- Source/vBoR/Interaction/VBoRScenarioOperator.cpp (신규), VBoRScenarioRunner.h, .cpp: 공통 판정과 버림 이유, 진행자 버튼 경로·상태, 창 수명·공개 ShowOperatorWindow, R/I/A 연결.
- Source/vBoR/Interaction/VBoRVehicleAlignmentComponent.h, .cpp: 좌석 공통 읽기 판정, 저장 정렬 재읽기·환경 검사·암전 재적용.
- Source/vBoR/P3Probe/P5iTestFixture.h, P5iOperatorPanelTests.cpp (신규): 사용자 저장 파일과 분리한 실제 파일 읽기 및 합성 XR/월드/Slate 시험 23개.
- Source/vBoR/P3Probe/P3FlowTests.cpp, P5gFreeModeTests.cpp: 명세가 지정한 두 키 이름 단언과 기존 시험의 키 입력 호출만 갱신.
- Docs/Reviews/P5i-operator-panel.md: 구현·시험 증거. HANDOFF.md는 git 무시 파일에 따로 기록.

② 명세 완료 조건
- §6-1: §2의 1–5 구현. 버튼과 키는 같은 QueueLive 진행자 명령·토큰을 쓰며, Info는 기존 처리 함수를 부름. 창과 Dispatch는 같은 판정 사용. 좌석 판정도 컴포넌트의 실제 요청 경로와 공유함.
- D8: 운전석에서 암전 후 BuckEye.json 재읽기·환경 재검사·저장 capture와 eye point로 Pawn 재배치. 머리 자세는 정렬 계산에 사용하지 않음. 실패 상태도 밝게 돌아오며 OnPawnMoved 한 번. 진행 중·자유모드의 단계/generation과 정렬 무시 기록 유지. 시작 전 대기는 적용 성공 후 1단계 시작.
- D9: sRGB hex 변환, Roboto Regular/Bold, px × 0.75 pt, 지정 순서·다섯 버튼 스타일. NeedsCheck에서 주 버튼은 재적용 하나. 긴 상태·결과 줄은 Ellipsis. 창은 게임/PIE에서만 생성하고 자동화·nullrhi에서는 생성하지 않음.
- §6-2: E1–E11·E13–E24 신규 자동화 23/23 통과. E12는 아래 기존 전체 시험으로 확인. 최종 전체 재실행 314/314 Success, 실패 0, TEST COMPLETE. EXIT CODE: 0, 프로세스 exit 0.
- §6-3: non-unity(-DisableUnity -Module=vBoR)와 unity Result: Succeeded, 최종 vBoR 컴파일·링크 경고/오류 0. 첫 non-unity의 새 include 경로 오류는 수정 후 증분 복구 빌드 성공. 추가 전체 빌드는 하지 않음.
- §6-4: 이 보고를 리뷰 파일·마지막 커밋 본문·HANDOFF에 기록.
- §6-5: Quest 3/VR·GUI 기능 확인은 not-tested.

③ 명세와 다르게 한 것과 이유
- 제품 요구사항 차이 없음. 창 소유는 러너, 자리·client 크기는 프로세스 정적 값으로 유지. 저장 경로를 protected virtual 함수로 분리해 시험만 GUID 임시 파일을 읽음. 사용자 BuckEye.json은 수정하지 않음.
- 기존 단언은 명세 §3에 지정된 두 곳의 키 이름만 변경함. P5g 시험 :175의 입력 F5→R, :224의 입력 F5/F1→R/I도 변경했으며 재시작·버림 개수 등 나머지 기존 단언과 시험 메시지는 유지함. 구 키 연결 제거 후에도 같은 기능을 실제 새 키로 검증하기 위한 입력 갱신임.
- 첫 non-unity는 VBoRScenarioOperator.cpp의 Spectator include 상대 경로(C1083)로 실패. ../Spectator/로 수정한 복구 빌드 성공. 제품 동작 차이 없음.
- 최초 전체 314개 중 313 Success / 1 Fail: vBoR.P0Buck.E8Persistence, repeated replacement 8, OS error 5; TEST COMPLETE. EXIT CODE: -1, 프로세스 exit 255. HANDOFF에 지정된 기존 간헐 실패라 한 번만 전체 재실행해 314/314, exit 0 확인. 해당 P0 소스·단언은 수정하지 않음.
- 하위 에이전트·자체 리뷰 루프·MCP·에디터·에셋 작업 없음. Spectator/, Config/, Content/, Plugins/ 변경 없음. P5j/P5k 시작하지 않음. push·PR·머지 없음.

④ API 헤더 확인 (추정 없음)
- 신규 플러그인 API 사용 없음. 기존 손 입력/MetaXR/ISDK 호출 변경 없음.
- 아래 경로는 C:/Program Files/Epic Games/UE_5.7/Engine/Source/ 기준이며 로컬 헤더와 실제 non-unity/unity 컴파일로 확인함.
- Runtime/Slate/Public/Framework/Application/SlateApplication.h:1576,1640 — AddWindow/RequestDestroyWindow.
- Runtime/SlateCore/Public/Widgets/SWindow.h:297,300,303,339,498,513,575,758,787,928 — 위치/client 크기/DPI/닫기 버튼/앞으로/최소화 복구/활성 시 focus.
- Runtime/SlateCore/Public/Widgets/SWidget.h:347,879 — OnPreviewKeyDown/SupportsKeyboardFocus. Runtime/Slate/Private/Widgets/Input/SButton.cpp:292,322 — Space의 기본 down/up 동작도 확인(부모 preview에서 한 번 소비).
- Runtime/Slate/Public/Widgets/Input/SButton.h:71,224 — 스타일 인자/SetButtonStyle; Runtime/SlateCore/Public/Brushes/SlateRoundedBoxBrush.h:165 — 고정 모서리·외곽선 brush.
- Runtime/Slate/Public/Widgets/Text/STextBlock.h:148,204,219,228 — Ellipsis·색·자동 줄바꿈.
- Runtime/Core/Public/Misc/App.h:411, CoreGlobals.h:623 — CanEverRender/GIsAutomationTesting 창 생성 방지.
- Runtime/Core/Public/Math/Color.h:123,573 — FromSRGBColor/FromHex.
- Runtime/SlateCore/Public/Styling/CoreStyle.h:51, Fonts/FontMeasure.h:29,39,172 — Roboto 크기·실제 문자열 폭/높이 측정.
- Runtime/HeadMountedDisplay/Public/IXRTrackingSystem.h:183,451 — tracking origin/Stage play area 조회. 정렬 계산은 기존 FVBoRBuckAlignment::CalculatePawn/CheckEnvironment를 재사용.

⑤ 로컬에서 확인할 것 / 검증 로그
- Saved/Logs/P5iNonUnityBuild.log: 최초 include 오류, 실패. Saved/Logs/P5iNonUnityRepairBuild.log: 수정된 파일과 링크 성공. Saved/Logs/P5iUnityBuild.log: 기본 unity 성공. 로컬 McpAutomationBridge/프로젝트의 StructUtils 폐기 예정 의존 안내 2줄은 vBoR 진단과 별도.
- Saved/Logs/P5iScopedAutomation.log: 발견·완료·Success 23/23, exit 0.
- Saved/Logs/P5iAllAutomation.log: 발견 314, 완료 314, Success 313/Fail 1(E8Persistence), engine exit -1/process exit 255.
- Saved/Logs/P5iAllAutomationRetry.log: 발견 314, 완료 314, Success 314/Fail 0, engine/process exit 0. P5i 23개와 기존 P3/P5g/P5h 포함. 샘플과 McpAutomationBridge/UnrealAgent를 모두 끄고 실행. 기존 샘플 InfoCard CDO 등 알려진 시작 잡음은 Workflow §2.5대로 분리.
- 에디터/GUI: 명세 §7의 L_P3_Flow VR Preview에서 별도 창 디자인·스크롤·최소화/복구·종료·레벨 재열기 자리 유지, 확장 모니터 배치, 125%/150% DPI, 창 focus 뒤 키 12개, R/I 디버그 표시 충돌 없음 확인 필요. 실제 Blueprint/레벨 편집 없음. GUI 확인 not-tested.
- Quest 3 Link: 운전석 A 암전·같은 설치 위치, 의자 옆 30 cm에서 재적용해도 체격 기준 유지, 동승석·2열 버튼 비활성, NeedsCheck/저장 없음 결과 확인. 계획서 T-01·T-04·T-05·T-08·T-09와 명세 §7은 not-tested. 자동화는 기기/운영 검증을 대신하지 않음.

## 리뷰 1회차 (2026-10-16, Claude — `93faf72`)

**결과: 열림 3건(R1-1–R1-3).** 확정 제약·명세 범위는 지켰다. 다시 적용은 저장값과 차의 eye point만 쓴다(`VBoRVehicleAlignmentComponent.cpp:56-78`, E19가 머리 30 cm 이동으로 확인). 버튼과 키는 같은 `QueueLive` 명령이다. `Spectator/`·`Config/`·`Content/`·`Plugins/`는 바뀌지 않았다. 남은 것은 창 수명 하나(R1-1), 옛 `Dispatch` 동작이 바뀐 곳 하나(R1-2), 창이 매 frame 하는 일의 양(R1-3)이다.

**재현(에디터 닫힘 확인 뒤, 바뀐 C++ 11개 시각 갱신):**
- non-unity(`-DisableUnity -Module=vBoR`) `Result: Succeeded`. 바뀐 `.cpp` 7개를 포함해 21개 작업. vBoR 경고·오류 0. 로그 `Saved/Logs/P5iReview-NonUnity.log`.
- unity `Result: Succeeded`. `Module.vBoR.2–6.cpp` 다시 컴파일. 경고·오류 0. 로그 `P5iReview-Unity.log`.
- `vBoR.*` **314/314**(P5i 23개 포함), `TEST COMPLETE. EXIT CODE: 0`. P5i 시험 구간 경고·오류 0. 로그 `P5iReview-Full.log`.

**Codex 토큰:** 구현 대화 5.51M(캐시 97%, 약 16분, 2026-10-16 07:35–07:51 UTC). 질문 Q1 대화 0.60M. 하위 에이전트 없음(보고). 빌드는 non-unity 실패(include 경로) → 복구 → unity, 전체 시험 2번(첫 번째는 `P0Buck.E8Persistence` OS error 5 — HANDOFF가 지시한 재실행). 규칙 안이다.

### R1-1 — 창을 Alt+F4·작업 표시줄로 닫으면 `ShowOperatorWindow`가 다시 만들지 않음

- 상태: **반영함 (fix(p5i): restore window and command behavior)**
- **문제:** `Interaction/VBoRScenarioOperator.cpp:224`는 `OperatorWindow` 포인터가 비어 있을 때만 창을 만든다. 창이 밖에서 닫혀도 이 포인터는 비지 않는다.
  - 닫기 버튼을 없앤 것(`:228` `HasCloseButton(false)`)은 Slate 테두리의 버튼만 숨긴다. 엔진은 Alt+F4를 막지 않는다(엔진 `Runtime/ApplicationCore/Private/Windows/WindowsApplication.cpp:2286-2289` "Allow alt+f4 to close the window"). `SC_CLOSE`는 `WM_CLOSE`로 넘어가고(`:2941-2948`), Slate는 그 창을 없앤다(`Runtime/Slate/Private/Framework/Application/SlateApplication.cpp:7239-7260` → `RequestDestroyWindow`). 작업 표시줄 오른쪽 클릭 "창 닫기"도 같은 `SC_CLOSE`다.
  - 그 뒤 `ShowOperatorWindow`는 만들기를 건너뛴다. `IsWindowMinimized`는 죽은 native 창에서 거짓이고(엔진 `Runtime/SlateCore/Private/Widgets/SWindow.cpp:1504-1512`), `BringToFront`(`:236`)는 아무것도 하지 않는다.
  - **영향:** D1 "없으면 만들고"를 지키지 못한다. 진행 중에 실수로 닫으면 레벨을 다시 열 때까지 버튼 창이 없다. P5j 툴바 버튼은 이 함수만 부르므로(P5j D3, §7-3 "Alt+F4로 닫은 뒤 누르면 다시 뜨는지") P5j도 동작하지 않는다. 닫힌 자리도 기억되지 않는다.
- **제안:**
  - 창을 만들 때 `SWindow::SetOnWindowClosed`(엔진 `Runtime/SlateCore/Public/Widgets/SWindow.h:712`)에 닫힘 처리를 건다. 처리: 자리·크기를 기억하고 `OperatorWindow`를 비운다.
  - `CloseOperatorWindow`(`:239-245`)도 같은 처리를 거치게 하면 자리 기억이 한 곳이 된다.
  - 창은 다음 frame에 지워지므로 콜백이 러너 `EndPlay` 뒤에 올 수 있다. 러너는 `TWeakObjectPtr`로 잡고, 러너가 없으면 아무것도 하지 않는다.
  - 다른 방법: `ShowOperatorWindow`에서 `FSlateApplication::GetTopLevelWindows()`(엔진 `Runtime/Slate/Public/Framework/Application/SlateApplication.h:1648`)에 그 창이 없으면 새로 만든다.
  - 닫기 자체를 막는 `SetRequestDestroyWindowOverride`(`SWindow.h:721`)는 쓰지 않는다. P5j §7-3이 닫은 뒤 다시 여는 것을 시험한다.
- **확인할 시험:** 실제 창 없이 닫힘 처리 함수를 바로 부른다.
  - 시험용 접근으로 `OperatorWindow`에 `AddWindow`하지 않은 `SNew(SWindow)`를 넣고 처리 함수를 부른다 → `HasOperatorWindow()`가 거짓, 자리 기억이 그 창의 자리·크기.
  - 그 뒤 `EndPlay`(`CloseOperatorWindow`)가 아무 일도 하지 않는다(창을 두 번 지우거나 자리를 덮어쓰지 않음).

### R1-2 — `Start without alignment`(O)의 전환 중 검사를 앞으로 옮겨 기존 센 값이 바뀜

- 상태: **반영함 (fix(p5i): restore window and command behavior)**
- **문제:** `Interaction/VBoRScenarioOperator.cpp:79`는 O를 볼 때 시점 전환 중인지를 맨 먼저 본다. 옛 `Dispatch`(`main`의 `VBoRScenarioRunner.cpp:347-353`)는 O를 전환 검사(`:381`)보다 먼저 처리했다. 자유모드면 규칙 버림, 이미 시작했으면 기록·센 값 없이 무시했다. 그래서 두 경우가 달라졌다.
  - 진행 중 좌석 암전 때 O: 지금은 `TransitionDiscards` +1, 기록 `명령 버림 — 전환 중`. 전에는 아무것도 세지 않았다.
  - 자유모드 좌석 암전 때 O: 지금은 `TransitionDiscards`. 전에는 `RuleDiscards`.
  - **영향:** D3 "`Dispatch`의 동작·센 값·기록은 그대로다"와 §3을 어긴다. 같은 동작인데 창의 버림 수와 기록이 P5i 전과 다르게 나온다. 기존 시험이 이 조합을 보지 않아 314개가 통과했다. 새로 필요한 것은 "시작 전 다시 적용 암전 중에 O를 버림"(E5) 하나뿐이다. 이 상황은 P5i 전에는 없었다.
- **제안:** 전환 중 검사를 자유모드·태그 검사 실패·이미 시작 다음, 받기 바로 앞(`:84` 앞)으로 옮긴다. 그러면 전환 버림은 시작 전 다시 적용 암전에서만 나고, 나머지는 옛 순서와 같다. E20의 기대값 5에는 진행 중 암전의 O가 들어 있다. 옛 동작대로면 4이므로 고친다(새 시험이라 바꿔도 된다).
- **확인할 시험:**
  - 진행 중 동승석 암전 중 O → `TransitionDiscards`·`RuleDiscards`가 그대로, 단계 그대로.
  - 자유모드 좌석 암전 중 O → `RuleDiscards` +1.
  - 시작 전 다시 적용 암전 중 O → 흐림, `TransitionDiscards` +1, 흐름이 시작하지 않음. 암전이 끝나 `Applied`가 되면 1단계가 시작됨(E16과 같은 경로).

### R1-3 — 진행자 창이 매 frame 월드 전체를 약 18번 훑음

- 상태: **반영함 (fix(p5i): restore window and command behavior)**
- **문제:** 창의 lambda가 판정 함수를 frame마다 따로 부르고, 좌석 판정은 부를 때마다 월드의 모든 Actor와 component를 돈다. 아래 수는 코드를 읽어 센 것이다(측정 아님).
  - `CommandViewpoint`(`Interaction/VBoRScenarioOperator.cpp:41-57`)는 `TActorIterator`로 모든 Actor를 돌고, Actor마다 `GetComponents`를 두 번 해 배열 두 개를 만든다.
  - 좌석 버튼 하나에 frame마다 판정 4번: `Tick`의 모양(`Interaction/VBoROperatorPanel.cpp:63`), 이름·키 글자색(`:89`·`:95`, 둘 다 `GetOperatorStyle`), 켜짐(`:82`). `GetOperatorStyle`(`VBoRScenarioOperator.cpp:206-213`)은 동승석·2열에서 꼬리표(`:202`)로 `CommandViewpoint`를 한 번 더 부른다. 운전석 판정(`:96`)도 쓰지 않는 `Car.RearView`를 찾느라 월드를 돈다.
  - 합하면 좌석 버튼을 받을 수 있는 동안 frame마다 약 18번(운전석 4 + 동승석 7 + 2열 7). 상태 문구도 lambda마다(약 12번) `GetOperatorStatus`를 새로 만든다.
  - **영향:** VR(90 Hz) 동안 game thread가 UI 때문에 Actor·component 수의 약 18배를 frame마다 돈다. 실제 차량 데이터가 들어와 component가 늘면 같이 커진다. 관전 화면(P5h)은 같은 순회를 frame마다 한 번 한다(`Spectator/VBoRSpectatorSubsystem.cpp:83`).
- **제안:**
  - 판정은 frame마다 한 번만 한다. 예: 패널 `Tick`에서 상태, 버튼 12개의 판정·모양·꼬리표를 한 번 계산해 멤버에 두고, lambda는 그 값만 읽는다. D6(초당 10회 이상)은 그대로 지켜진다. 판정 함수는 여전히 `EvaluateCommand` 하나다(D3).
  - 운전석 판정에서는 시점을 찾지 않는다(`ReturnBase`는 `SwitchRejection`이 시점을 보지 않음).
  - 동승석·2열 시점을 `InitializeFlow`에서 한 번 찾아 두는 방법도 있다. 다만 `Dispatch`가 누를 때 찾던 동작이 바뀌므로, 고른다면 보고 ③에 적는다.
- **확인할 시험:** 시험용 계수로 `CommandViewpoint`(또는 월드 순회) 횟수를 세어, 패널 한 frame(`Tick` + `SlatePrepass`)에 좌석 수 이하인지 본다. 같은 frame에 스냅숏 판정이 버튼마다 `EvaluateOperatorButton`과 같은지 본다(E7을 넓힘).

### 참고 (반영할 것 없음)

- **창이 뜰 때 초점을 가져간다.** `SWindow` 기본 활성화 정책은 `Always`이고(엔진 `SWindow.h:265`, Windows에서 `WindowsWindow.cpp:718-729`), `:236`이 `BringToFront(true)`도 부른다. Play 시작과 F6마다 키보드 초점이 진행자 창으로 간다.
  - `L_P3_Flow`에는 러너 키뿐이고 모두 창이 받으므로(D5) 지금 막히는 키는 없다. 리프트게이트 PageUp·PageDown은 P1 시험대에만 있다.
  - 사용자가 Blueprint로 붙일 키(웰컴 라이트 등, U-10)는 진행자 창에 초점이 있으면 들어가지 않는다. VR 확인(명세 §7-4) 때 보고, 불편하면 처음 뜰 때 활성화하지 않게 바꾼다(`FocusWhenFirstShown`·`ActivationPolicy`, `SWindow.h:327`·`:333`).
- **P5g 시험 입력 키 변경**(`P5gFreeModeTests.cpp:175` F5 → R, `:224` F5·F1 → R·I): §3 예외 목록에 없었지만, F1·F5 연결을 없애면 반드시 바꿔야 하는 입력이다. 명세가 빠뜨린 것이다(Claude). 받아들인다. 단언·센 값은 그대로임을 확인했다. 명세 §3에 더했다.
- **E7**은 판정 함수와, 그 함수로 만든 결과 문구를 비교한다. `Dispatch`가 판정을 그대로 쓰므로 일치는 구조상 당연하다. 옛 `Dispatch` 동작이 남았는지는 기존 시험 291개가 그대로 통과한 것으로 본다. R1-2는 그 시험들이 보지 않던 틈이다.
- **E1**의 두 번째 단언(`GIsAutomationTesting`)은 `-nullrhi`에서 `CanEverRender`가 이미 거짓이라 그 분기만 따로 보지 못한다. 자동화로 대신할 방법이 없어 받아들인다.
- **"Waiting for"**는 태그 검사 실패로 시작하지 못할 때도 `Alignment check`로 나온다(`VBoRScenarioOperator.cpp:185`). 고치지 않아도 된다. 그때는 VR 안 진행자 줄이 태그 문제를 보인다.


## Codex 리뷰 1회차 반영 결과

완료 — P5i 리뷰 1회차 R1-1–R1-3 반영 (2026-10-16, Codex 앱).

- 브랜치: codex/P5i-operator-panel, 시작 6988cd6 위 새 커밋.
- 커밋 제목: fix(p5i): restore window and command behavior
- R1-1·R1-2·R1-3 모두 반영함. 미해결 질문 없음. Claude 다음 리뷰 대기.

① 바뀐 파일
- Source/vBoR/Interaction/VBoRScenarioOperator.cpp: 창 닫힘 콜백·공통 자리 기억, O 검사 순서 복원, 프레임 스냅숏 생성과 공통 판정/스타일 사용.
- Source/vBoR/Interaction/VBoRScenarioRunner.h: 닫힘 처리·스냅숏 API, EvaluateCommand의 내부 시점 인자 overload, 자동화 전용 순회 계수.
- Source/vBoR/Interaction/VBoROperatorPanel.h, .cpp: 상태·12개 버튼 판정/모양/꼬리표·설명을 Tick에서 한 번 갱신하고 Slate attribute는 스냅숏만 읽음.
- Source/vBoR/P3Probe/P5iOperatorPanelTests.cpp: 회귀시험 3개 추가, E20 기대 전환 버림 수 5→4, E23에 상태 변경 후 Tick 추가.
- Docs/Reviews/P5i-operator-panel.md: 세 항목 상태와 이 보고. HANDOFF.md는 git 무시 파일에 별도 기록.

② 완료 조건 및 리뷰 항목별 상태
- §6-1 / R1-1: SetOnWindowClosed에서 TWeakObjectPtr 러너를 확인하고 자리·client 크기를 기억한 뒤 소유 포인터를 비움. EndPlay의 CloseOperatorWindow도 같은 함수를 거침. 늦은 이전 창 콜백은 새 창과 자리를 덮어쓰지 않음. 자동화에서 native 창 없이 직접 닫힘 처리·후속 Close·늦은 콜백을 확인함.
- §6-1 / R1-2: O는 자유모드·태그 실패·이미 시작 여부를 먼저 검사함. 진행 중 좌석 암전의 O는 버림 수·기록·단계/generation 유지, 자유모드 암전의 O는 RuleDiscards +1. 시작 전 재적용 암전의 O는 비활성·TransitionDiscards +1이고 흐름을 시작하지 않으며, 재적용 성공 후 1단계 시작을 확인함.
- §6-1 / R1-3: 패널은 생성 시와 매 Tick에 스냅숏 하나를 만듦. 상태는 한 번, 각 버튼 판정도 한 번. 운전석은 월드를 탐색하지 않고 동승석·2열은 각 한 번 탐색해 판정과 꼬리표가 공유함. Tick + SlatePrepass에서 월드 순회 정확히 2회, 8가지 상태의 버튼 판정·버림 종류·이유·기록·모양·꼬리표가 live API와 같고 실제 버튼 켜짐과 상태 갱신도 일치함.
- §6-2: 발견 317, 완료 317, Success 317, Fail 0. P5i 26/26(기존 23 + 신규 3), TEST COMPLETE. EXIT CODE: 0, 프로세스 exit 0. 기존 P3/P5g/P5h 포함 전체 한 번 실행. E8Persistence 간헐 실패는 이번에 발생하지 않음.
- §6-3: 최종 C++ 트리로 non-unity(-DisableUnity -Module=vBoR)·unity 각각 한 번, 모두 Result: Succeeded / 프로세스 exit 0. vBoR 컴파일·링크 경고/오류 0. 복구 빌드·전체 시험 재실행 없음.
- §6-4: 이 보고를 리뷰 파일·커밋 본문·HANDOFF.md에 기록함.
- §6-5: GUI·VR Preview·Quest 3 기기 확인은 not-tested.

③ 명세와 다르게 한 것과 이유
- 제품 요구사항 차이 없음. 세 제안을 반영했으며 판정 규칙은 EvaluateCommand의 공통 구현 하나를 사용함. 스냅숏용 overload는 같은 프레임에 찾은 시점을 인자로 받을 뿐 판정 조건을 복제하지 않음.
- 시점을 InitializeFlow에서 고정 보관하지 않음. 실제 Dispatch는 계속 명령 시점의 월드와 상태로 판정함. 프레임 스냅숏은 표시용이며 버튼 클릭/키는 기존 QueueLive 경로를 유지함.
- E20은 리뷰에서 지정한 P5i 신규 시험 기대값만 수정함. E23은 표시가 Tick 스냅숏을 읽으므로 상태를 바꾼 뒤 Tick을 한 번 호출하도록 조정함. P3/P5g 등 기존 시험 단언·입력은 이번에 수정하지 않음.
- 하위 에이전트·자체 리뷰 루프·MCP·에셋 작업 없음. Spectator/·Config/·Content/·Plugins/ 변경 없음. P5j/P5k 시작 안 함. push·PR·머지 없음.

④ API 헤더 확인 (추정 없음)
- 신규 플러그인 API 없음. 기존 ISDK·MetaXR 호출 변경 없음.
- 엔진 기준 경로: C:/Program Files/Epic Games/UE_5.7/Engine/Source/
- Runtime/SlateCore/Public/Widgets/SWindow.h:54,498,513,712 — FOnWindowClosed 서명, 화면 위치/client 크기, SetOnWindowClosed. Private/Widgets/SWindow.cpp:1361,1402-1404 — delegate 저장·파괴 알림 시 호출 확인.
- Runtime/Slate/Public/Framework/Application/SlateApplication.h:1640 — RequestDestroyWindow. 명시 종료는 창 reference를 보관하고 공통 상태 정리 뒤 파괴를 요청함.
- Runtime/Core/Public/UObject/WeakObjectPtrTemplates.h:244-246 — TWeakObjectPtr 유효성 검사. Runtime/Core/Public/Delegates/DelegateSignatureImpl.inl:377 — CreateLambda.
- 변경 API는 로컬 헤더와 실제 non-unity/unity 빌드로 확인함.

⑤ 로컬에서 확인할 것 / 검증 근거
- Saved/Logs/P5iR1NonUnityBuild.log: Result: Succeeded, exit 0. Saved/Logs/P5iR1UnityBuild.log: Result: Succeeded, exit 0. 두 로그 모두 컴파일/링크 진단 경고·오류 없음(WarningsAsErrors 옵션 문자열은 진단이 아님).
- Saved/Logs/P5iR1AllAutomation.log: 발견·완료·Success 317/317, Fail 0, P5i 26/26, engine exit 0. Saved/Logs/P5iR1AllAutomationProcess.log 및 실행 프로세스 exit 0. 샘플·McpAutomationBridge·UnrealAgent를 끄고 실행. P5i BeginEvents/EndEvents 구간 경고·오류 0. 기존 XR 확장/샘플 CDO 및 기존 시험 월드 정리 잡음은 시험 결과와 분리함.
- GUI: 실제 OS 창의 Alt+F4·작업 표시줄 닫힘 후 ShowOperatorWindow 재생성, 자리·크기 보존, 최소화 복구, Play 종료와 레벨 재열기를 확인할 것. 이번 자동화는 native 창 없이 닫힘 처리 함수와 Slate widget만 시험함. 실제 GUI not-tested.
- Quest 3 Link/VR Preview: L_P3_Flow에서 O의 진행 중·자유모드·시작 전 재적용 암전 동작, 버튼 상태의 즉시 갱신, 기존 정렬 재적용·좌석/흐름 키 일치를 확인할 것. 계획서 T-01·T-04·T-05·T-08·T-09 및 명세 §7은 not-tested. 합성 시험으로 기기·운영 동작을 통과 처리하지 않음.
- 시작·종료 시 UnrealEditor/UnrealEditor-Cmd 프로세스 없음 확인. 작업 트리는 이 리뷰 범위의 파일만 변경함.

## 리뷰 2회차 (2026-10-16, Claude — `9fbe959`)

**결과: 통과. 열린 항목 없음.** R1-1–R1-3이 모두 제안대로 들어갔다. 제품 동작은 바뀌지 않았고, 이번 커밋에서 바뀐 기존 시험 단언은 P5i 새 시험 E20 기대값 하나(5 → 4, 리뷰가 지시함)뿐이다.

**재현(에디터 닫힘 확인 뒤, 바뀐 C++ 5개 시각 갱신):**
- non-unity(`-DisableUnity -Module=vBoR`) `Result: Succeeded`, 18개 작업(`VBoRScenarioOperator.cpp`·`VBoROperatorPanel.cpp`·`P5iOperatorPanelTests.cpp` 포함). vBoR 경고·오류 0. 로그 `Saved/Logs/P5iReview2-NonUnity.log`.
- unity `Result: Succeeded`, `Module.vBoR.2–6.cpp` 다시 컴파일. 경고·오류 0. 로그 `P5iReview2-Unity.log`.
- `vBoR.*` **317/317**(P5i 26개 포함), `TEST COMPLETE. EXIT CODE: 0`. 로그 `P5iReview2-Full.log`.

**Codex 토큰:** 반영 대화 2.62M(캐시 95%, 약 12분, 08:05–08:16 UTC). 뒤 확인 대화 0.33M(1분). 빌드 non-unity·unity 각 한 번, 전체 시험 한 번. 규칙 안이다.

**항목별 확인:**
- **R1-1:** 창을 만들 때 닫힘 콜백을 건다(`Interaction/VBoRScenarioOperator.cpp:274-275`). 콜백은 러너를 `TWeakObjectPtr`로 잡는다. 처리 함수(`:290-296`)는 자기 창일 때만 자리·크기를 기억하고 포인터를 비운다. `EndPlay`의 `CloseOperatorWindow`(`:282-288`)도 이 함수를 먼저 거친 뒤 파괴를 요청한다. 그래서 다음 frame에 오는 콜백은 `OperatorWindow != Window`에서 돌아가고, 같은 창을 두 번 처리하지 않는다. 밖에서 닫은 뒤에는 `ShowOperatorWindow`가 새로 만든다(D1, P5j D3). 시험 `R1ExternalWindowClose`(`P5iOperatorPanelTests.cpp:375`)가 밖 닫힘·뒤이은 `EndPlay`·늦게 온 옛 창 콜백을 본다. 실제 OS 창의 Alt+F4는 자동화로 못 보므로 VR §7(P5j §7-3)에 남는다.
- **R1-2:** O의 전환 중 검사를 자유모드·태그 검사·이미 시작 다음, 받기 바로 앞으로 옮겼다(`VBoRScenarioOperator.cpp:93-101`). 옛 `Dispatch`(`main` `VBoRScenarioRunner.cpp:347-353`)의 순서와 같아졌다. 시험 `R1IgnoreDuringTransitions`(`:398`)가 세 경우(진행 중 암전 — 센 값·기록·단계 그대로, 자유모드 암전 — 규칙 버림 +1, 시작 전 다시 적용 암전 — 흐림·전환 버림 +1, 뒤이어 1단계 시작)를 본다.
- **R1-3:** 패널은 `Construct`와 `Tick`에서 스냅숏을 한 번 만든다(`Interaction/VBoROperatorPanel.cpp:62`·`:114`). attribute lambda는 스냅숏만 읽는다. 스냅숏(`VBoRScenarioOperator.cpp:233-254`)은 버튼마다 시점을 한 번 찾아 판정·꼬리표에 같이 쓴다. 운전석과 좌석이 아닌 명령은 월드를 돌지 않는다(`:50`). 판정은 여전히 `EvaluateCommand` 하나다 — 시점을 인자로 받는 overload가 같은 본문을 쓴다(D3). 시험 `R1OneSnapshotPerFrame`(`:431`)이 8가지 상태에서 `Tick` + `SlatePrepass` 한 번에 순회가 정확히 2번인지, 스냅숏의 판정·이유·기록·모양·꼬리표가 바로 부른 값과 같은지, 실제 버튼 켜짐이 스냅숏을 따르는지 본다.

**참고(반영할 것 없음):**
- 스냅숏은 `Tick`에서 만들고, Slate는 한 frame 안에서 prepass(글자 크기)를 paint(Tick) 앞에 한다. 그래서 글자 크기는 한 frame 늦게 따라온다. 진행자가 보기에는 차이가 없다(D6 초당 10회 이상은 그대로). 누르는 순간에는 `Dispatch`가 다시 판정하므로 한 frame 늦은 켜짐 표시로 잘못된 명령이 들어가지 않는다.
- 1회차 참고(창이 뜰 때 초점을 가져감, E1·E7의 한계, 태그 검사 실패 때 `Alignment check`)는 그대로다. VR 확인 때 본다.
