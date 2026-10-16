# P5j 에디터 툴바 아이콘 — Play 옆 버튼으로 진행자 창을 앞으로

| 항목 | 값 |
|---|---|
| 단계 | P5 자유모드·리허설(진행자 지원) |
| 결정 근거 | **사용자 요청(2026-10-16):** 진행자 창을 편하게 열 수 있게 툴바에 아이콘을 둔다. **사용자 문답(2026-10-16):** 평가는 **에디터 VR Preview**로 진행한다(툴바는 에디터 안에서만 동작). 창은 **Play 때 자동으로 뜨고**(P5i D1), 툴바 아이콘은 **최소화·가려진·닫힌 창을 다시 앞으로** 가져온다. 아이콘 디자인은 P5i와 같은 CX Toolkit 톤(`Docs/Design/P5i-operator-panel/Toolbar.dc.html`) |
| 베이스 브랜치 | P5k 머지 뒤 `main`(순서: P5i → P5k → P5j. P5i D1의 "창을 보이게 하는 공개 함수"를 부름) |
| 작업 브랜치 | `codex/P5j-operator-toolbar` |
| 리뷰 파일 | `Docs/Reviews/P5j-operator-toolbar.md` (첫 리뷰 때 생김) |
| 상태 | 명세 — P5i·P5k 머지 대기 |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **에디터 전용 모듈을 새로 둔다** | 툴바는 에디터 API(`UnrealEd`·`ToolMenus`·`LevelEditor`)가 필요하므로 런타임 모듈 `vBoR`에 넣지 않는다. 새 모듈(예: `vBoREditorTools` — 에디터 타깃 이름 `vBoREditor`와 헷갈리지 않게)을 `vBoR.uproject`의 `Modules`에 `"Type": "Editor"`로 더하고 `Source/<모듈>/`에 Build.cs·모듈 클래스를 둔다. 이 모듈은 `vBoR`을 참조하고, **`vBoR`은 이 모듈을 참조하지 않는다**(게임·패키지 빌드에 들어가지 않게). 타깃 파일(`Source/vBoREditor.Target.cs:13`)에 모듈 이름을 더해야 하는지는 빌드로 확인한다 |
| D2 | **자리 — Play 툴바** | `LevelEditor.LevelEditorToolBar.PlayToolBar`(엔진 `Editor/LevelEditor/Private/LevelEditorToolBar.cpp:1510`)에 버튼 하나를 더한다. `UToolMenus::RegisterStartupCallback`(엔진 `Developer/ToolMenus/Public/ToolMenus.h:145`)에서 `ExtendMenu`(`:169`)로 넣고, `FToolMenuEntry::InitToolBarButton`(`Developer/ToolMenus/Public/ToolMenuEntry.h:189`)을 쓴다. 이름표 `Operator`, 툴팁 `Show the operator window. Restores it if minimized, reopens it if closed.` 모듈을 내릴 때 등록을 푼다(`ToolMenus.h:148` `UnRegisterStartupCallback`, 메뉴 항목·스타일 해제) |
| D3 | **누르면 — P5i 함수만 부른다** | Play 중인 월드(`GEditor->PlayWorld`, 엔진 `Editor/UnrealEd/Classes/Editor/EditorEngine.h:518`)에서 러너를 찾아 P5i D1의 "창을 보이게 하는 공개 함수"를 부른다(없으면 만들고, 최소화면 풀고, 앞으로). 툴바 쪽에 창을 만드는 코드를 따로 두지 않는다 — 창은 P5i 것 하나다 |
| D4 | **누를 수 없을 때는 흐리게** | Play 중이 아니면 흐리고 툴팁은 `Start Play first.` Play 중인데 러너가 없는 레벨이면 흐리고 툴팁은 `This level has no scenario runner.` 판정은 월드를 받아 돌려주는 함수 하나로 해서 시험한다 |
| D5 | **아이콘** | `Docs/Design/P5i-operator-panel/Toolbar.dc.html`의 20 × 20 SVG(둥근 사각형 안 세 줄 슬라이더, 1.5 px 선) 그대로를 `Resources/Editor/OperatorPanel.svg`로 둔다. 선 색은 흰색(`#ffffff`)으로 저장하고 브러시 색을 `#5b8cff`(sRGB → `FLinearColor::FromSRGBColor`, P5i D9)로 준다. 흐릴 때는 툴바가 알아서 흐리게 한다. `FSlateStyleSet`에 `FSlateVectorImageBrush`(엔진 `Runtime/SlateCore/Public/Brushes/SlateImageBrush.h:85`)로 등록한다(`Runtime/SlateCore/Public/Styling/SlateStyleRegistry.h:22`·`:29`). 스타일 루트는 프로젝트 폴더 기준 경로다 |

## 1. 왜 만드는가

- 진행자 창은 PC 모니터에 따로 뜬다. 에디터·VR Preview 창 뒤에 가려지거나 최소화되면 찾기 어렵다. Play 버튼 옆 아이콘 하나로 바로 앞으로 가져온다.

## 2. 범위

### 만들 것

**1. 에디터 전용 모듈과 툴바 버튼** — D1–D5.
**2. 자동화 시험** — §4 전부.

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| Standalone·패키지에서 창을 여는 단축키 | 사용자 결정 — 에디터 VR Preview로 진행 |
| 진행자 창 자체를 바꾸기 | P5i |
| 툴바에서 창을 따로 만들기 | D3 — 창은 P5i 것 하나 |
| 런타임 모듈 `vBoR`에서 에디터 모듈 참조 | D1 |
| 프로젝트 설정·다른 툴바 바꾸기 | 버튼 하나만 더한다 |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- 기존 `vBoR.*`가 모두 그대로 통과한다. 기존 단언은 바꾸지 않는다.
- 게임 타깃(`vBoR.Target.cs`) 빌드에 에디터 모듈이 들어가지 않는다.
- 자동화(`-nullrhi`)에서 경고·오류 없이 지나간다.

## 4. 경계 상황과 그 시험

| # | 상황 | 기대 동작 | 자동화 시험 |
|---|---|---|---|
| E1 | Play 안 함 | 흐림, 툴팁 `Start Play first.` | 판정 함수(월드 없음) |
| E2 | Play 중, 러너 있는 레벨 | 켬. 누르면 P5i 함수를 한 번 부름 | 판정 함수·호출 수(시험용 월드) |
| E3 | Play 중, 러너 없는 레벨 | 흐림, 툴팁 `This level has no scenario runner.` | 판정 함수 |
| E4 | 아이콘 | SVG 파일이 있고 스타일이 그 브러시를 찾음 | 스타일 조회 |
| E5 | 툴바 등록 | Play 툴바에 항목이 한 번만 있음(모듈을 다시 시작해도 두 번 아님) | `UToolMenus`에서 항목 찾기 |
| E6 | 기존 시험 | 모두 통과 | 전체 시험 |

자동화에서 실제 창은 만들어지지 않는다(P5i D1). 누른 뒤의 창 동작(최소화 풀기·앞으로)은 VR 확인 §7에서 본다.

## 5. 구현자가 정할 것

- 모듈 이름, 스타일 세트 이름, 판정 함수 모양.
- 툴바 안 정확한 위치(Play 묶음 오른쪽 끝 권장).

## 6. 완료 조건

1. §2의 1–2가 들어 있다.
2. §4 시험이 모두 자동화로 통과한다. 기존 `vBoR.*`도 통과한다.
3. non-unity(`-Module=vBoR`, 그리고 새 모듈)·unity 빌드에서 vBoR·새 모듈 경고·오류가 0이다. 전체 시험은 최종 트리로 한 번 돌린다.
4. AGENTS.md 보고 ①–⑤를 커밋 본문과 `HANDOFF.md`에 남긴다.
5. 시험 상태는 `not-tested`다.

## 7. 로컬에서 확인할 것 — 사용자

1. 에디터를 열면 Play 버튼 옆에 `Operator` 아이콘이 흐리게 있는지 본다.
2. `L_P3_Flow`에서 VR Preview를 시작하면 켜지는지, 진행자 창을 최소화하거나 다른 창 뒤로 보낸 뒤 누르면 앞으로 오는지 본다.
3. 진행자 창을 Alt+F4로 닫은 뒤 누르면 다시 뜨는지 본다.
4. Play를 멈추면 다시 흐려지는지 본다.

## 8. 확인한 API 근거

엔진은 `C:/Program Files/Epic Games/UE_5.7/Engine/Source/` 아래.

| 쓸 것 | 위치 | 쓰임 |
|---|---|---|
| Play 툴바 이름 | `Editor/LevelEditor/Private/LevelEditorToolBar.cpp:1510` | D2 |
| 메뉴 확장 | `Developer/ToolMenus/Public/ToolMenus.h:109`(`Get`), `:145`·`:148`(시작 콜백 등록·해제), `:169`(`ExtendMenu`) | D2 |
| 툴바 버튼 | `Developer/ToolMenus/Public/ToolMenuEntry.h:189`(`InitToolBarButton`) | D2 |
| Play 중인 월드 | `Editor/UnrealEd/Classes/Editor/EditorEngine.h:518`(`PlayWorld`) | D3·D4 |
| 아이콘 브러시·스타일 | `Runtime/SlateCore/Public/Brushes/SlateImageBrush.h:85`(`FSlateVectorImageBrush`), `Runtime/SlateCore/Public/Styling/SlateStyleRegistry.h:22`·`:29` | D5 |
| 프로젝트 모듈 | `vBoR.uproject`(`Modules`), `Source/vBoREditor.Target.cs:13` | D1 |

## 개정 이력

| 날짜 | 개정 | 내용 |
|---|---|---|
| 2026-10-16 | 처음 | 작성(사용자 요청·문답: 툴바 아이콘, 에디터 VR Preview, Play 때 자동 + 툴바로 다시). P5i에서 나눔(새 에디터 모듈이라 따로 확인) |
