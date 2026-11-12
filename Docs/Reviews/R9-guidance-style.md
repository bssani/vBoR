# R9 안내판·버튼 이름표 디자인 — 리뷰

## 1회차 (2026-11-12, `025c9cd`) — WBP 두 곳만 고치면 통과

**판정:** C++는 명세 D1–D7과 프리셋 표를 충족한다. 빈 칸 경로는 그대로이고, 기존 시험 단언도 바뀌지 않았다. 고칠 것은 에셋(WBP) 두 곳뿐이다(문제 1·2). C++는 고치지 않는다.

**확인한 것:**
- 빌드·시험 로그: `R9-Automation.log` `vBoR.R9.E1`–`E8` 성공, `TEST COMPLETE. EXIT CODE: 0`. 커밋 본문은 전체 724/724라고 보고했다.
- D4: 클래스 칸이 비면 `BeginPlay`의 Slate 생성 줄은 글자 그대로다. 안내판과 이름표를 따로 판단한다(E2·E3이 서로 반대쪽 대체를 확인한다).
- D7: 블렌드는 `Opaque`일 때만 `Masked`로 바꾸고, 클래스가 지정된 경우에만 그렇게 한다. 엔진 기본값도 `Masked`다(`Engine/Source/Runtime/UMG/Private/Components/WidgetComponent.cpp:635`).
- E8과 EndPlay: 기존 `Panel->SetSlateWidget(nullptr)`가 UMG 위젯도 해제한다. `SetSlateWidget`은 `Widget != nullptr`이면 `SetWidget(nullptr)`를 부른다(`WidgetComponent.cpp:1843-1846`). 그래서 EndPlay를 고치지 않아도 해제된다.
- D3: `GetMemberPropertyName()`으로 구조체 안 칸의 변경을 `PanelStyle`로 받는다. `Custom`을 고르면 값을 그대로 둔다(E5).
- 프리셋 값과 명세 표를 칸마다 대조했다. 모두 일치한다.
- E6은 상대 휘도로 비교한다(명세대로).

### 문제 1 — Graphite 강조 막대가 판 위 가장자리에서 떨어져 판을 가로지른다 — 열림

- **문제:** 미리보기 `Saved/Screenshots/R9/WBP_GuidancePanel.png`를 픽셀로 쟀다. 강조 막대는 y=33–37에 있고 x=0–999로 판 끝에서 끝까지 이어진다. 본문은 x=45부터 시작한다. 막대 위로 판 색 띠가 남고, 양 끝은 둥근 모서리 판의 좌우 가장자리까지 닿는다. 그래서 장식 막대가 아니라 판을 가로지르는 선처럼 보인다. 시안(위 가장자리에 붙은 띠)과도 다르고, 본문 왼쪽 선과도 맞지 않는다.
- **제안(Claude 결정):** 막대를 본문과 같은 여백 안에 둔다. `GuidanceColumn` 안에서 `AccentBar` 슬롯의 좌우 여백을 본문과 같게 맞추고(음수 여백·판 전체 폭 확장을 쓰지 않음), 막대를 본문 영역의 맨 위에 둔다. 판 위 가장자리에 딱 붙이는 방식은 쓰지 않는다. `UBorder`는 자식을 둥근 모양으로 자르지 않으므로, 8 px 막대의 모서리가 28 px 둥근 모서리 밖으로 삐져나온다.
  - Graphite(`AccentBarWidth = 0`)는 본문 폭만큼 채운다. SoftLight(96 px)는 본문 왼쪽 선에서 시작한다. C++(`HAlign_Fill`/`HAlign_Left`)는 그대로 둔다.
  - 고칠 곳은 WBP 슬롯 여백뿐이다.
- **확인할 시험:** 없음(에셋 배치). 고친 뒤 Graphite 미리보기를 다시 찍어 막대의 왼쪽 끝이 본문 왼쪽 끝과 같은 x인지 보고한다.

### 문제 2 — 이름표 글 크기를 지금 판과 맞춘다 — 열림

- **문제:** 지금 Slate 이름표는 `Bold 32`다(`VBoRScenarioRunner.cpp` BeginPlay의 이름표 `STextBlock`). `FVBoRPanelStyle`에는 이름표 글 크기 칸이 없어서 WBP `LabelText`의 기본 글꼴이 그대로 쓰인다. 미리보기에서 `Confirm`은 폭 66 px, 높이 13 px로 작게 보인다. 미리보기는 UMG DPI 배율이 걸려 실제 크기를 단정할 수 없다. 하지만 `LabelText` 글꼴이 UMG 기본(24)이면, 위젯을 지정하는 순간 버튼 이름이 지금보다 25% 작아진다. 명세는 "이번은 모양만"(§2)이다. 13번 버튼 근처라 70 cm에서 읽혀야 한다.
- **제안:** `WBP_ButtonLabel`의 `LabelText` 글꼴을 Roboto `Bold`, 크기 32로 둔다. 이미 32면 그대로 두고 보고만 한다. 안내판 `MainText`의 굵기도 같이 확인한다. 크기는 `ApplyStyle`이 40으로 넣지만 굵기는 WBP 값을 쓰므로 `Bold`인지 본다. 스타일 구조체에 칸을 더하지 않는다(범위 밖).
- **확인할 시험:** 없음. 보고에 두 TextBlock의 글꼴(typeface·size)을 적는다.

### 참고 1 — P5i 시험 상수 이름 변경

- `P5iOperatorPanelTests.cpp`의 내부 `Flags`를 `P5iFlags`로 바꿨다. unity 묶음에서 Chaos 헤더의 같은 이름과 충돌했기 때문이다. diff를 확인했다. 이름 말고는 바뀐 줄이 없고 단언도 그대로다. 명세 §3 "기존 시험 변경 없음"의 취지(단언 불변)에 맞으므로 **그대로 둔다.**

### 참고 2 — Quest에서 볼 것 (사용자, `not-tested`)

- `Masked` 블렌드는 반투명 가장자리를 잘라낸다. 그래서 둥근 모서리의 안티앨리어싱이 사라져 계단처럼 보일 수 있다. 거슬리면 `Transparent`로 바꿀 수 있다. 다만 반투명 정렬 문제가 따라오므로 Quest에서 보고 정한다. 지금은 고치지 않는다.
- 색은 sRGB 값 그대로 들어간다. 다만 VR 화면에서 보이는 밝기는 헤드셋에서 확인해야 한다(명세 §8 ①–③).

### 참고 3 — 구조(고치지 않음)

- `UVBoRButtonLabelWidget`의 구현이 `VBoRGuidanceWidget.cpp`에 같이 들어 있다. 동작에는 문제가 없다. 나중에 찾기 쉽도록 다음에 이 파일들을 고칠 일이 있으면 `VBoRButtonLabelWidget.cpp`로 나눈다. 이번 작업에서는 하지 않는다.

### 다음

- Codex: 문제 1·2를 반영한다. 에디터 MCP로 WBP 두 개만 고치고 저장한 뒤 에디터를 닫는다. C++를 바꾸지 않았다면 전체 빌드·시험은 다시 하지 않아도 된다. 다만 `vBoR.R9.*`와 맵 로드는 다시 확인한다. 새 미리보기 스크린샷을 남긴다.
- 그다음 Claude 2회차 리뷰 → `main` 머지(사용자 승인).
