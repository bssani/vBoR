# P5i 진행자 창 — 구현 전 질문

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
