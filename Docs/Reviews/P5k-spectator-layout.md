# P5k 리뷰 기록

명세: [P5k-spectator-layout.md](../Specs/P5k-spectator-layout.md) · 브랜치: `codex/P5k-spectator-layout`

## 구현 착수 확인 (2026-10-16, Codex 앱)

- 베이스 `main` `0ab9b5a`, 시작 시 작업 트리 깨끗함. Unreal Editor 프로세스 없음.
- 명세와 디자인의 간격 지시가 충돌하여 C++ 구현 전에 중단함. AGENTS.md:15,25에 따라 질문을 남김.
- 제품 코드·시험·설정·에셋 변경 없음. 빌드·자동화 실행 없음. 커밋·push·PR·머지 없음.

## 질문

### Q1 — 꼬리표 아래 미션 간격: 12 px 또는 24 px

**상태: 반영함 (feat(p5k): revise spectator side panel).** Claude 답변 `7f21863`의 24 px·D3 환산 규칙 반영.

- **문제:** `Docs/Specs/P5k-spectator-layout.md:19` D3은 1080 기준으로 “② 12 px 아래 미션 문구”를 지정함. 같은 명세 `:10,19` 및 `HANDOFF.md:10,15`는 960 × 540 디자인 값을 모두 두 배로 읽도록 지정함. `Docs/Design/P5i-operator-panel/TvBand.dc.html:26`의 미션 `margin-top: 12px`는 두 배로 읽으면 1080 기준 24 px임. 두 지시를 동시에 충족할 수 없음.
- **제안:** 디자인의 두 배 값을 유지한다면 D3의 간격을 24 px로 정정. 12 px가 의도라면 디자인 간격을 6 px로 정정하거나 D3에 배율 예외를 명시. 구현자는 값을 임의로 선택하지 않음(§5 재량의 1–2 px 세부 범위를 초과).
- **확인할 시험:** 확정된 간격을 P5h E17의 실제 Slate 배치에서 확인하고, P5k E4의 꼬리표·미션 비겹침 및 문구 높이 검증에 포함.
- **답 (Claude):** **24 px가 맞다.** D3에 디자인 값을 두 배로 바꾸지 않고 그대로 적은 것은 Claude의 실수다. D3을 고쳤다.
  - 규칙을 맨 앞에 한 줄로 적었다. 길이는 디자인 × 2 px. 글꼴은 디자인 px × 1.5를 pt로 넣고 소수점 아래는 버린다(글꼴 프로퍼티가 `int32`). D3에 없는 값도 이 규칙으로 읽는다.
  - 같은 대조에서 나온 것도 같이 고쳤다. 진행 문구 20 → **19 pt**(13 px × 1.5 = 19.5). 꼬리표 40(40.5)·작은 제목 16(16.5)은 버림 규칙대로 그대로다.
  - 적지 않았던 값도 적었다: 진행 묶음 줄 사이 12 px, 막대 모서리 4 px, 구분선과 조작 중 묶음 위 각 32 px, 조작 중 줄 사이 8 px, 작은 제목 자간 80(`LetterSpacing`은 1/1000 em — 엔진 `SlateTextShaper.cpp:432`).
  - 디자인과 다르게 하는 것 둘을 밝혔다. 꼬리표는 넘치면 줄바꿈한다(디자인 `nowrap`은 240 px 견본에서만 맞음). 줄 높이는 Slate 글꼴 기본을 쓴다(디자인 `line-height`는 맞추지 않음).
  - 확인할 시험은 Codex 제안대로다: E4에서 꼬리표 아래와 미션 위 사이가 24 px인지 실제 배치로 본다.
  - 명세 §0 D3과 개정 이력대로 구현을 재개한다.

① 바뀐 파일: 이 질문 기록. `HANDOFF.md`의 Codex 결과는 git 무시 파일에 별도 기록.

② 완료 조건: §6의 1–3 미완료(구현 전 중단), 4 중단 사유를 파일에 기록(구현 커밋 없음), 5 Quest 3·TV 시험 상태 `not-tested` 유지.

③ 명세와 다르게 한 것: 구현 없음. 충돌 해결 권한은 Claude 명세 개정에 있어 임의 해석하지 않음.

④ API 확인: 신규 엔진·플러그인 API 사용 없음. 헤더 대조·컴파일 검증 단계 미진행.

⑤ 로컬에서 확인할 것: Q1 해소 뒤 구현을 재개하고 최종 트리 non-unity·unity 및 `vBoR.*` 전체 시험 필요. Quest 3 Link + TV의 명세 §7, 계획서 T-08 및 리허설은 `not-tested`.

## Codex 구현 결과 (2026-10-16)

완료 — P5k 관전 화면 배치 개정 구현 1회차 (2026-10-16, Codex 앱). Claude 리뷰 대기.

브랜치 codex/P5k-spectator-layout, 시작 7f21863 (베이스 main 0ab9b5a).
Q1 반영함 (feat(p5k): revise spectator side panel): 꼬리표→미션 24 px, 길이 ×2 px, 글꼴 ×1.5 pt 버림, 진행 문구 19 pt.
최종 재실행 로그: Saved/Logs/P5kFinalAllAutomationRetry.log (직전 P0 간헐 실패는 P5kFinalAllAutomation.log에 보존).
신규 질문 없음. 하위 에이전트·에디터 MCP·에셋 편집·push·PR·머지 없음.

① 바뀐 파일
- Source/vBoR/Spectator/VBoRSpectatorModel.h, .cpp: 왼쪽 4:3 시야·오른쪽 칸 배치, 칸 픽셀 크기·1080 기준 배율, 최근 이름 최대 두 개 목록. Line과 MissionText의 구현·문구는 유지.
- Source/vBoR/Spectator/VBoRSpectatorSubsystem.h, .cpp: BandHeight 제거, Config 프로퍼티·sRGB 색·글꼴 기본값, 칸 크기의 렌더 타깃 생성·ResizeTarget, 배율 DrawWidget, 오른쪽 Slate 패널.
- Source/vBoR/P3Probe/P5hSpectatorTests.cpp: 허용된 E15–E18 갱신, P5k 시험 4개 추가. E1–E13 시험 본문은 HEAD와 문자 단위 비교하여 동일함을 확인.
- Docs/Reviews/P5k-spectator-layout.md: Q1 반영 상태와 이 보고. HANDOFF.md 결과는 git 무시 파일에 별도 기록.

② 명세 §6 완료 조건
1. 충족: D1–D5 및 Q1 반영. 16:9는 시야 1440×1080·칸 480×1080, 좁은 창에서 시야 세로 가운데. UI 텍스처 영역은 칸에만 있고 불투명. 최소화는 무시하고 같은 크기는 재제출·ResizeTarget 없음.
2. 충족: 321개 발견·321개 완료, 321/321 Success, 실패 0 (P5h 15/15·P5k 4/4), engine/process exit 0. P5k E1–E4/E8은 P5h E15–E18과 크기·배율 시험, E5 이름 목록·실제 위젯 표시, E6 실제 자유모드 배너의 진행 제목·막대·문구 표시, E7 sRGB 팔레트로 확인. E9 기존 무HMD/nullrhi 시험 유지. 기존 다른 시험의 단언 변경 없음.
3. 충족: 최종 non-unity(-DisableUnity -Module=vBoR)·unity Result: Succeeded, process exit 0. vBoR 경고·오류 0. 최종 트리 전체 시험은 기존 P0 간헐 실패 때문에 HANDOFF 지시대로 1회 재실행(두 결과는 ③).
4. 충족: 보고 ①–⑤를 이 파일·커밋 본문·HANDOFF.md에 기록.
5. 충족: Quest 3 Link/TV·VR Preview의 시험 상태는 not-tested 유지.

③ 명세와 다르게 한 것 / 구현자 재량·실패 기록
- 기능 명세 이탈 없음. Q1은 Claude가 개정한 값 그대로 반영.
- 새 Config 이름은 MinPanelWidth, SecondaryTextColor, ProgressBackgroundColor, LineColor, HeadingFontSize, ProgressFontSize. target 재생성 대신 ResizeTarget 선택.
- PartLine 중복 캐시는 제거. E10의 시험 도우미만 기존 Line("Interacting: ") 읽기로 바꿔 원래 단언을 유지. 제품 위젯은 RecentNames 목록과 InteractingPrefix 제목을 사용. E1–E13 시험 본문·다른 시험의 단언은 바꾸지 않음.
- 명시적인 WrapTextAt으로 첫 prepass부터 줄바꿈 높이를 계산하고, 미션은 남은 FillHeight 안에서 Ellipsis + ClipToBounds. 줄 높이는 Slate 기본. 진행 막대는 흰색 rounded brush를 색으로 tint하며 높이 12 px·모서리 4 px.
- 창 폭이 최소 칸보다 작아지는 극단 입력은 유효 영역이 뒤집히지 않도록 칸 폭을 창 비율의 0.99 이하로 제한. 명세의 네 창 비율에서는 계산식 그대로임.
- 최초 전체: 321개 발견·321개 완료, 319 Success/2 Fail, engine exit -1/process 255 (P5kAllAutomation.log). E17의 공백 없는 W 400자는 기본 Slate에서 한 줄이라 높이 초과 fixture가 아니었음. 명시적 100줄로 바꿔 높이 초과·기본 22개 문구 높이·Q1 간격을 확인. E2E3 배율은 float 비교 기본 허용치 1e-8이 너무 작아 1e-6으로 정함. 최종 로그의 1200 px 배율 actual 1.111111045 / expected 1.111111164 / delta -0.000000119로 float 반올림 차이를 확인.
- 시험 보강 중 막대 높이 TestEqual에 float와 double을 섞어 C2666 발생(P5kTestCorrectionNonUnityFailed.log). 12.f로 수정한 뒤 최종 두 빌드 통과. 제품 코드 변경 없이 시험만 정정.
- 최종 코드 첫 전체 실행: 321개 발견·321개 완료, 320 Success/1 Fail, engine exit -1/process 255 (P5kFinalAllAutomation.log). P5h 15/15·P5k 4/4 통과. 기존 P0Buck.E8Persistence의 replacement 17에서 OS error 5로 실패. HANDOFF에 허용된 1회 재실행을 코드 변경 없이 수행하고 그 결과를 최종으로 함께 기록.
- 수정 후 E17 단독 1/1 Success, exit 0 (P5kTargetedAutomation.log). 같은 ExecCmds의 두 번째 RunTests는 실행되지 않아 P5k 단독 실행 증거로 세지 않음. P5k 4개는 최종 전체 실행에서 확인.
- 빌드·전체 시험 1회 원칙의 예외: 실제 실패를 수정해 검증하기 위해 위 재실행이 필요했음. 의도적 red·반복/스트레스·자체 리뷰 루프 없음.

④ 엔진·플러그인 API 근거
경로는 C:/Program Files/Epic Games/UE_5.7/Engine/Source/ 기준.
- Runtime/HeadMountedDisplay/Public/HeadMountedDisplayTypes.h:158–215: FSpectatorScreenModeTexturePlusEyeLayout 영역·bUseAlpha·bClearBlack·IsValid 확인.
- Runtime/HeadMountedDisplay/Public/ISpectatorScreenController.h:34,42,58: 기존 mode·texture·layout 설정 경로 확인.
- Runtime/UMG/Public/Slate/WidgetRenderer.h:49,75 및 Runtime/UMG/Private/Slate/WidgetRenderer.cpp:157: CreateTargetFor와 scale DrawWidget, 배치 크기 = 픽셀 크기/scale 확인.
- Runtime/Engine/Classes/Engine/TextureRenderTarget2D.h:189: ResizeTarget 확인.
- Runtime/Core/Public/Math/Color.h:123,573: FromSRGBColor·FromHex 확인.
- Runtime/Slate/Public/Widgets/Text/STextBlock.h:110,118,148 및 Runtime/SlateCore/Public/Fonts/SlateFontInfo.h:179: WrapTextAt·WrappingPolicy·OverflowPolicy·LetterSpacing 확인.
- Runtime/SlateCore/Public/Brushes/SlateRoundedBoxBrush.h:75 및 Runtime/SlateCore/Public/Styling/SlateTypes.h:1182,1187: 모서리 반경과 progress brush 설정 확인.
- Runtime/SlateCore/Public/Widgets/SBoxPanel.h:333,344: AutoHeight·FillHeight 확인.
- Runtime/Slate/Private/Framework/Text/TextLayout.cpp:626: 기본 정책은 공백 없는 단어의 강제 글자별 줄바꿈을 하지 않음(시험 fixture 정정 근거).
신규 ISDK·MetaXR 호출 없음. 사용한 엔진 API는 로컬 헤더·구현 및 실제 빌드로 확인함.

⑤ 로컬에서 확인할 것 / 검증 기록
- 최종 빌드: Saved/Logs/P5kFinalNonUnityBuild.log, P5kFinalUnityBuild.log. vBoR 진단 0. 로컬 도구 StructUtils 폐기 예정 의존 안내 2줄은 별도 잡음이며 도구 소스 변경 없음.
- 최종 전체 재실행: Saved/Logs/P5kFinalAllAutomationRetry.log — 321개 발견·321개 완료, 321/321 Success, 실패 0 (P5h 15/15·P5k 4/4), engine/process exit 0. nullrhi, OculusInteractionSamples·McpAutomationBridge·UnrealAgent 비활성. 워크플로 §2.5의 샘플 InfoCard CDO·XR 확장 잡음은 시험 실패와 구분.
- 시작·종료 시 UnrealEditor/UnrealEditor-Cmd 프로세스 0개. GUI 에디터·MCP·Blueprint·레벨·설정 수정 없음.
- Quest 3 Link/VR Preview L_P3_Flow + TV 명세 §7-1–5, 계획서 T-08 및 리허설: not-tested. 실제 RHI의 target resize·TV 화면 비율·몇 m 거리 가독성·헤드셋에 UI가 안 보이는지 사용자 확인 필요. Slate 배치·계산·headless 자동화가 기기/화면 동작 증거를 대신하지 않음.

## 리뷰 1회차 (2026-10-16, Claude — `d7a59ad`)

**결과: 통과. 열린 항목 없음.** 명세 D1–D5와 Q1 답(D3 개정)대로 들어갔다. 헤드셋 경로·문구 규칙·부품 수집은 그대로다(D6). 고친 기존 시험은 §3이 허용한 P5h E15–E18뿐이다. E10은 시험 도우미가 지운 `PartLine` 대신 `Names.Line`을 읽게 바꿨고 단언은 그대로다. 진행자 창(P5i)과 `MissionText`는 건드리지 않았다.

**재현(에디터 닫힘 확인 뒤, 바뀐 C++ 5개 시각 갱신):**
- non-unity(`-DisableUnity -Module=vBoR`) `Result: Succeeded`. vBoR 경고·오류 0. 로그 `Saved/Logs/P5kReview-NonUnity.log`.
- unity `Result: Succeeded`. 경고·오류 0. 로그 `P5kReview-Unity.log`.
- `vBoR.*` **321/321**(P5h 15, P5k 4, P5i 26), `TEST COMPLETE. EXIT CODE: 0`. P5h·P5k 시험 구간 경고·오류 0. 로그 `P5kReview-Full.log`.

**Codex 토큰:** 6.09M(캐시 94%, 13:40–14:06 UTC 약 25분. 질문 Q1으로 멈춘 것과 재개를 한 대화에서 함). 빌드·전체 시험을 두 번 넘게 했다. 시험 실패 2건(E17 fixture, E2E3 float 허용치), 시험 컴파일 오류(C2666), `P0Buck.E8Persistence` 재실행 때문이고, 보고 ③에 이유와 로그가 있다. 실제 실패를 고친 재실행이라 규칙 안이다.

**확인한 것:**
- **D1 배치:** `VBoRSpectatorModel.cpp:80-98`의 식을 네 창 비율에 넣어 E1 기대값과 같은지 손으로 계산했다(16:9 시야 x 0–0.75, 16:10 0.76875·y 0.03875, 4:3 0.7225·y 0.13875, 21:9 0.571429). `bUseAlpha` 거짓(칸 불투명), `bClearBlack` 참. 극단적으로 좁은 창에서 칸 폭을 창 비율의 0.99로 막는 것은 명세 네 비율에 영향이 없다.
- **D2 텍스처:** 텍스처 영역 = 칸. 렌더 타깃은 칸 픽셀 크기로 만든다. 크기가 바뀔 때만 `ResizeTarget`(`VBoRSpectatorSubsystem.cpp:119-124`), 최소화(0)는 그 앞(`:118`)에서 돌아간다. `DrawWidget`의 배율은 창 높이 / 1080(`:126`). 그래서 Slate 배치는 늘 1080 높이 기준이다(`DesignSize`).
- **D3 칸 배치:** 여백 40·32, 꼬리표 → 미션 24, 진행 묶음 12, 막대 12·모서리 4, 구분선·조작 중 묶음 위 각 32, 이름 사이 8, 왼쪽 선 2, 글꼴 40·36·16·19·27 pt, 자간 80. 꼬리표·미션의 줄바꿈 폭은 칸 폭 − 66(좌우 여백 64 + 선 2). E17이 가장 좁은 칸(4:3, 400 px)에서 24 px·8 px 간격과 칸 높이 안에 들어가는지 잰다.
- **D4 색:** 6색 모두 sRGB hex → `FromSRGBColor`, 새 프로퍼티 셋은 `Config`(E18).
- **D5 이름:** `RecentNames`가 최근 것부터 최대 둘. `Line`은 남김(P5h E7–E10).

### 참고 (반영할 것 없음 — TV 확인 때 볼 것)

- **넘치는 미션의 말줄임 모양.** 미션은 `ETextOverflowPolicy::Ellipsis`다(`VBoRSpectatorSubsystem.cpp:187`). 여러 줄 글이 세로로 넘치면, Slate는 반쯤 보이는 마지막 줄까지 그리고 그 줄에 말줄임을 붙인다(엔진 `Runtime/Slate/Private/Framework/Text/SlateTextLayout.cpp:114-124`·`:156-159`). 그래서 맨 아래 줄이 글자 중간에서 잘린 채 보인다. `MultilineEllipsis`(엔진 `Runtime/SlateCore/Public/Styling/SlateTypes.h:86-87`, 처리 `SlateTextLayout.cpp:103-113`)는 잘린 줄을 통째로 빼고 앞 줄에 말줄임을 붙인다.
  - 4:3 창의 400 px 칸에서 기본 36 pt면 약 11줄이 들어간다(코드로 어림잡은 것). 원본 19개 기본 문구는 E17에서 들어감을 확인했다. 그래서 이 경우는 사용자가 아주 긴 영어 문구를 에셋에 넣었을 때만 생긴다.
  - TV 확인(명세 §7-3)에서 글꼴을 키우다 이 모양이 보이면, 그 한 단어를 바꾼다.
- **TV 칸 색이 진행자 창과 같은지 나란히 본다.** 그리는 경로는 P5h부터 그대로다(`FWidgetRenderer(true)` + `CreateTargetFor(..., false)` — 엔진 `Runtime/UMG/Private/Slate/WidgetRenderer.cpp:85-93`에서 `SRGB`·선형 감마 설정). 자동화(`-nullrhi`)로는 실제 화면 색을 볼 수 없다. 칸이 진행자 창보다 눈에 띄게 밝거나 어두우면, 이 감마 조합을 먼저 본다.
