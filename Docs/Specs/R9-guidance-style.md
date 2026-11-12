# R9 안내판·버튼 이름표 디자인 — WBP 하나 + 스타일 3가지

| 항목 | 값 |
|---|---|
| 단계 | 구조 보수 R9(디자인). 시험 계획 T-09(실제 운영 — 이해·피로) 보조, Quest 확인 전 `not-tested` |
| 결정 근거 | **사용자(2026-11-12):** 시나리오 3D 위젯이 흰 바탕·검정 글씨로 너무 기본이다. 전문적이고 진중한 분위기로, **Blueprint에서 디자인을 고칠 수 있게.** 시안 3개(Graphite·Navy Instrument·Soft Light, 캔버스 `https://claude.ai/artifact/B7WhSY53RbsXC9HW5tgPXG`) 중 **"WBP 하나 + 스타일 3가지, Runner Details 드롭다운으로 고름"**(사용자 답 ③).<br>**Meta 디자인 가이드(Color, 2026-04-19):** VR에서 순수 흰색·검정을 피한다 — 밝은 바탕 #DADADA 이하, 어두운 바탕 #1A1A1A 이상, 글자 대비 4.5:1 이상(큰 글 3:1). 지금 판은 바탕 `FLinearColor(0.95,…)`(≈ sRGB #F9F9F9)에 순검정 글씨다(`VBoRScenarioRunner.cpp:111-115`, `:153`) |
| 베이스 브랜치 | `main` |
| 작업 브랜치 | `codex/R9-guidance-style` |
| 리뷰 파일 | `Docs/Reviews/R9-guidance-style.md` (첫 리뷰 때 생김) |
| 상태 | 명세 작성 |
| 에디터 작업 | **있음** — WBP 2개 만들기(§6, MCP). **`L_Main`은 고치지 않는다**(사용자가 Runner 칸 지정) |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **C++ 부모 위젯 두 개** | `Source/vBoR/Interaction/`에 `UVBoRGuidanceWidget`(안내판)과 `UVBoRButtonLabelWidget`(버튼 이름표), 둘 다 `UUserWidget` 자식(`Abstract`, `Blueprintable`). 모듈은 이미 `UMG`에 의존한다(`vBoR.Build.cs:13`).<br>- 안내판 칸(`meta=(BindWidget)` 필수): `Plate`(`UBorder`), `MainText`(`UTextBlock`). 선택(`BindWidgetOptional`): `AccentBar`(`UBorder` 또는 `UImage`, 구현자), `Header`(`UTextBlock`).<br>- 이름표 칸(필수): `Plate`(`UBorder`), `LabelText`(`UTextBlock`).<br>- 함수: 안내판 `SetInstruction(FText)`, 이름표 `SetLabel(FText, bool bEmphasis)`, 둘 다 `ApplyStyle(const FVBoRPanelStyle&)`. `BlueprintNativeEvent`로 둬서 WBP가 덮어쓸 수 있게 한다(기본 C++ 구현이 칸에 색·글을 넣음) |
| D2 | **스타일 묶음 `FVBoRPanelStyle`**(`USTRUCT(BlueprintType)`) | 칸(전부 `EditAnywhere`·`BlueprintReadWrite`): 판 색, 판 테두리 색·두께(px), 모서리 반경(px), 글 색, 글 크기(기본 40 — 지금 판과 같은 약 2 cm), 강조색, 강조 막대 표시·너비(px, 0 = 판 전체 폭), 머리글 표시·머리글 글(기본 `GUIDANCE`)·머리글 색, 이름표: 강조(Confirm) 채움 여부·강조 글 색, 보통 이름표 판 색·테두리 색·글 색. 색은 `FLinearColor`, **에디터에서 sRGB 값으로 보이게**(구현자 — `FLinearColor::FromSRGBColor`로 프리셋을 정의). 둥근 판은 `UBorder` 브러시 `DrawAs = RoundedBox` + 외곽선 설정(`FSlateBrushOutlineSettings`) |
| D3 | **프리셋 3개 + 직접 조정** | `enum class EVBoRPanelStylePreset { Graphite, NavyInstrument, SoftLight, Custom }`. 값은 캔버스 시안 그대로(아래 표). Runner에 `PanelStylePreset`(기본 `Graphite`)와 `PanelStyle`(기본 = Graphite 값).<br>- 에디터에서 `PanelStylePreset`을 바꾸면(`PostEditChangeProperty`) `PanelStyle`을 그 프리셋 값으로 덮어쓴다.<br>- 사용자가 `PanelStyle` 칸을 직접 고치면 `PanelStylePreset`이 `Custom`이 된다(고친 값은 그대로).<br>- `Custom`을 고르면 `PanelStyle`을 건드리지 않는다.<br>- **Play 때는 `PanelStyle`만 쓴다**(프리셋 이름이 아니라 값) |
| D4 | **Runner 칸 — 비면 지금 판 그대로** | Runner(`AVBoRScenarioRunner`)에 `GuidanceWidgetClass`(`TSubclassOf<UVBoRGuidanceWidget>`)와 `ButtonLabelWidgetClass`(`TSubclassOf<UVBoRButtonLabelWidget>`), Category `Guidance|Style`.<br>- **비어 있으면 지금 Slate 판 경로를 한 글자도 바꾸지 않는다**(`P3TextPlate`, `PanelText`·`PanelBorder`, 흰 바탕·검정 40) — 기존 시험이 그대로 통과한다.<br>- 들어 있으면 그 위젯을 만들어 `UWidgetComponent`에 넣는다(`Panel`·`ButtonLabels` 컴포넌트, 그리기 크기 1000×640·500×120과 월드 배율은 그대로). 안내판 칸과 이름표 칸은 따로 판단한다(하나만 지정해도 됨) |
| D5 | **내용은 지금과 같다** | 안내문은 `GetPanelText()` 그대로(P5z D5 — 안내문만, 빈 글이면 판 숨김). 이름표 글은 지금 문구 그대로(`Confirm`, `ActionLabel()` — `VBoRScenarioRunner.cpp:910`). Confirm 이름표가 `bEmphasis = true`, 시점 버튼 이름표가 `false`. 숨김·자리·버튼 동작은 바꾸지 않는다. **머리글(`GUIDANCE`)은 단계 정보가 아닌 고정 꾸밈 글**이라 P5z D5와 충돌하지 않는다(Navy 프리셋만 기본 켬, 사용자가 끌 수 있음) |
| D6 | **에디터 미리보기** | 위젯 자신도 `PreviewStyle`(기본 Graphite) 칸을 갖고 `NativePreConstruct`에서 적용한다 — UMG 디자이너에서 WBP를 열면 Graphite 모양이 보인다. Play 때는 Runner의 `PanelStyle`이 덮는다 |
| D7 | **월드 위젯 투명 모서리** | 둥근 모서리 바깥이 비치도록 `Panel`·`ButtonLabels`의 `UWidgetComponent` 블렌드를 확인한다(기본 Masked면 그대로, 모서리가 사각으로 보이면 구현자가 `SetBlendMode`). **위젯 클래스가 지정된 경우에만** 바꾼다(D4 — 빈 칸 경로 불변) |

**프리셋 값 (sRGB, 캔버스 시안 그대로):**

| 칸 | Graphite (기본) | NavyInstrument | SoftLight |
|---|---|---|---|
| 판 색 | #1E2226 | #18212B | #D3D1CC |
| 판 테두리 | 없음 | #2A3644, 2 px | 없음 |
| 모서리 반경 | 28 | 20 | 24 |
| 글 색 | #E3E6E9 | #DCE3EA | #22262A |
| 강조색 | #6F8FAF | #C99A4B | #3B4A5A |
| 강조 막대 | 켬, 판 전체 폭, 8 px 높이 | 끔 | 켬, 96 px 폭, 6 px 높이 |
| 머리글 | 끔 | 켬 `GUIDANCE`, #8E9BAA | 끔 |
| Confirm 이름표 | 판 색 바탕 + 강조색 테두리 3 px, 글 #E3E6E9 | 강조색 채움, 글 #1A1D21 | 강조색 채움, 글 #E4E6E8 |
| 보통 이름표 | 판 색 바탕 + #3E454C 테두리, 글 #C3C9CF | #18212B + #3A4757 테두리, 글 #C4CDD6 | #D3D1CC, 글 #2C3136 |

## 1. 왜 하는가

지금 판은 C++ Slate로 직접 그려 에디터에서 손댈 수 없고, 거의 순백 바탕이라 VR에서 눈이 피곤하다(Meta 가이드). 디자인을 WBP와 스타일 칸으로 빼서 사용자가 에디터에서 고르고 고친다.

## 2. 범위

### 만들 것

- D1–D7, 프리셋 표. 시험 `vBoR.R9.*`. WBP 2개(§6).

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| `L_Main` 수정(Runner 칸 지정 포함) | AGENTS.md — 사용자 레벨은 사용자가 고친다. §8 |
| 빈 칸일 때 지금 판의 색·글꼴·크기 변경 | 기존 시험(§3) |
| 진행자 판(`OperatorPanel` — 늘 숨김 `VBoRScenarioRunner.cpp:899`), TV 관전 창, 진행자 창 디자인 | 참가자 안내판·이름표만 |
| 단계 번호·상태 줄 표시 | P5z D5 |
| 새 글꼴 에셋 가져오기 | 엔진 기본(Roboto). 필요하면 사용자가 WBP에서 바꾼다 |
| 판 크기·자리·숨김·버튼 위치 변경 | 이번은 모양만 |
| C++ 헤더에 에셋 경로 박기(`ConstructorHelpers`로 WBP 찾기) | 빈 칸 = 지금 판 규칙(D4)이 깨지고 시험 월드가 에셋에 묶인다 |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- 위젯 클래스 칸이 비면 Runner 동작·Slate 판·로그가 지금과 같다.
- `GetPanelText`, 판 숨김 조건(`:898`), 판·버튼 자리 계산, 이름표 숨김(버튼과 같이), 확인 잠금.
- **기존 시험 감사(Claude 2026-11-12):** 안내판·이름표의 모양을 보는 시험은 `P3FlowTests.cpp` `R1EyeLevelAndReadableStyle`(`:510-539` — 검정 글씨·40·`WhiteBrush`·밝은 불투명 바탕)과 `FP3FlowTestAccess`의 `Slate`/`Text`/`Border`/`Label`/`LabelText`(`:54-61`)를 쓰는 시험(`E16E18E19` `:407,414`, `R1ParticipantAndOperatorText`, `R1EyeLevelAndReadableStyle` `:525-531`)이다. 모두 Runner를 C++로 스폰하므로 새 칸이 비어 있다 → **D4 덕에 그대로 통과해야 한다. 고쳐도 되는 단언: 없음.** 깨지면 멈추고 리뷰 파일 "질문"에 적는다.
- `P5zFixedGuidanceTests`·`P5gFreeModeTests`의 `Panel->bHiddenInGame`·`GetPanelText` 단언도 그대로.

## 4. 시험 — `vBoR.R9.*`

시험용 위젯은 시험 파일 안의 C++ 하위 클래스(필수 칸을 `WidgetTree`로 직접 만들어 연결)로 한다 — 시험이 WBP 에셋에 묶이지 않게.

| ID | 상황 | 확인 |
|---|---|---|
| E1 | 두 칸 비움 | `PanelText`·`PanelBorder`가 있고 Slate 판 경로(검정·40·흰 바탕) — 지금과 같음, 새 위젯 없음 |
| E2 | 안내판 칸 = 시험 위젯, 단계 진행 | `Panel`의 위젯이 시험 위젯, `MainText` 글 = `GetPanelText()`. 다음 단계로 넘어가면 글이 바뀜. 끝·자유모드에서 판 숨김(지금과 같은 조건) |
| E3 | 이름표 칸 = 시험 이름표 위젯 | Confirm 이름표 `LabelText` = `Confirm`·강조, 시점 버튼 이름표 = `Move to passenger seat`·보통(E16 단계 구성 재사용). 버튼과 같이 숨김 |
| E4 | `PanelStyle` = NavyInstrument 값 | 안내판 `Plate` 브러시 색·반경·외곽선, `MainText` 색, `Header` 보임·글 `GUIDANCE`, `AccentBar` 숨김. Confirm 이름표 판 = 강조색 채움 |
| E5 | 프리셋 바꾸기(에디터, `PostEditChangeProperty`) Graphite → SoftLight | `PanelStyle`이 SoftLight 값. 그 뒤 `PanelStyle` 글 색 하나 바꾸면 프리셋 = `Custom`, 바꾼 값 유지. `Custom` 선택은 값 그대로 |
| E6 | 세 프리셋의 대비(자동 계산, WCAG 상대 휘도) | 판 글 / 판 바탕 ≥ 4.5, 머리글 / 판 바탕 ≥ 3.0, 이름표 글 / 그 판 바탕 ≥ 4.5(둘 다). 어두운 판 바탕 휘도 ≥ #1A1A1A의 휘도, 밝은 판 바탕 휘도 ≤ #DADADA의 휘도(Meta 가이드 — **채널별이 아니라 상대 휘도로 비교**: Navy #18212B는 R 채널이 0x18이지만 휘도 0.0146 > 0.0103). Claude 계산(2026-11-12): 최저 대비는 Navy 머리글 5.75, 나머지 6.6–12.8 |
| E7 | 필수 칸 없는 위젯(`Plate` 없음)을 지정 | 크래시 없음, 경고 한 줄(어느 칸이 없는지), 그 판은 지금 Slate 판으로 대체 |
| E8 | EndPlay·다시 시작(R) | 위젯 해제(`Panel` 위젯 null), 다시 시작해도 위젯이 하나뿐 |

## 5. 구현자가 정할 것

- 위젯을 만드는 방법(`CreateWidget` + `UWidgetComponent::SetWidget` 권장 — `SetWidgetClass`는 컴포넌트가 스스로 만들 때까지 칸 접근 시점이 늦다).
- `AccentBar`의 위젯 종류, 머리글 왼쪽 점(Navy 시안) 표현 여부(넣으면 `BindWidgetOptional`).
- `FLinearColor` sRGB 표시 방법, 프로퍼티 이름·Category·DisplayName(영어).
- `PostEditChangeProperty`에서 "직접 고침" 판단 방법(구조체 안 칸 변경 감지 — `MemberProperty`).
- 블렌드 모드(D7).

## 6. 에디터 작업 (Codex, MCP — Agent-Workflow §2.10 순서)

C++ 빌드 → 에디터 열기 → 아래 → 저장 → 에디터 닫기 → 전체 시험.

1. `Content/Main/UI/` 폴더(없으면 만든다).
2. `WBP_GuidancePanel`(부모 `UVBoRGuidanceWidget`): 루트 `Plate`(Border) 안에 세로 상자 — `AccentBar`, `Header`, `MainText`(자동 줄바꿈, 왼쪽 정렬, 세로 가운데). 여백은 시안 기준(좌우 72 px, 위아래 56 px). 그리기 크기 1000×640 기준.
3. `WBP_ButtonLabel`(부모 `UVBoRButtonLabelWidget`): 루트 `Plate`(Border) 안 `LabelText`(가운데). 500×120 기준.
4. 두 WBP 컴파일·저장. 디자이너 미리보기가 Graphite인지 스크린샷으로 확인.
5. **`L_Main`은 열어서 읽기만 한다**(Runner가 어디 있는지 이름 보고). 칸 지정은 하지 않는다.
6. MCP로 위젯 트리(자식 위젯 추가·이름·슬롯)를 만들 수 없으면 **멈추고 리뷰 파일 "질문"에 적는다**(우회 구조 만들지 않음).
7. 보고: 에셋 경로, 트리 구조, 스크린샷 경로, `L_Main`의 Runner 이름.

## 7. 완료 조건

1. D1–D7·프리셋 표 구현, §3 조건 지킴.
2. `vBoR.R9.E1`–`E8` 통과, 기존 시험 변경 없이 통과.
3. non-unity(`-Module=vBoR`)·unity 빌드 성공, `vBoR.*` 전체 통과(작업 끝에 한 번).
4. WBP 2개 저장, 맵·에셋 로드 경고 없음.
5. 새 주석·표시 이름·로그·에셋 글 영어(AGENTS.md).

## 8. 로컬에서 확인할 것

- 빌드·시험: 위 2–3.
- 에디터(사용자): `L_Main`의 Runner 선택 → Details `Guidance|Style`에서 `Guidance Widget Class` = `WBP_GuidancePanel`, `Button Label Widget Class` = `WBP_ButtonLabel`, `Panel Style Preset`으로 세 가지 바꿔 보기 → 저장. 색 하나 고치면 `Custom`이 되는지.
- WBP를 UMG 디자이너에서 열어 여백·글꼴을 고쳐 보고 Play에 반영되는지.
- Quest 3(T-09 보조, `not-tested`): 세 프리셋을 VR에서 바꿔 보며 ① 70 cm에서 글이 편하게 읽히는지 ② 눈부심·번짐 ③ Confirm 이름표가 눈에 먼저 들어오는지. 고른 프리셋을 알려 주면 기본값으로 둔다.
