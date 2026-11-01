# R2c 판정 경계 표시 — 질문

## 질문

### Q1 — 독립 판정 표시와 R2a 전체 모양 수 단언 충돌

- 상태: **열림** (2026-11-01, Codex). 제품 코드·시험 수정 전에 중단. 베이스 `main@c46d3fd`, 브랜치 `codex/R2c-judgment-guide`.
- **문제:**
  - 명세 `Docs/Specs/R2c-judgment-guide.md:18` D1은 문 그립도 잡기 형상을 표시하도록 요구한다. 그러나 `Source/vBoR/P3Probe/R2aMotionGuideTests.cpp:216-217`의 E7은 그립에서 `MotionDisplay->BuildShapes().Num() == 0`을 단언한다. 새 종류로 구분해도 전체 수 단언에는 잡기 형상이 포함된다.
  - 명세 `:22` D5는 `Judgment guide`를 `Motion guide`와 독립적으로 두고 기본 Always를 요구한다. E12(`:81`)도 두 표시의 독립성을 요구한다. 그러나 같은 기존 시험 `:323` E11은 `MotionGuide = Off` 뒤 `BuildShapes().Num() == 0`과 **`BatchedLines.Num() == 0`**을 단언한다. `:324`는 MotionGuide SelectedOnly / 미선택에서도 전체 모양 수 0을 요구한다. 판정 표시를 별도 목록으로 만들더라도 같은 컴포넌트에 렌더 선이 남으면 `:323`의 렌더 선 단언과 충돌한다.
  - 명세 `:62`는 기존 단언 변경을 전부 금지한다. 따라서 기본값을 Off로 바꾸거나 Motion guide에 종속시키거나 그립 표시를 생략하지 않고는 위 시험을 그대로 유지할 수 없다. 모두 D1/D5의 요구를 바꾸는 것이므로 임의 구현하지 않았다.
- **제안:** 명세 §3의 허용 변경에 R2a E7·E11의 시험 준비만 명시한다. 해당 시험에서는 R2c 대상 부품의 `Judgment guide = Off`를 명시해 기존 R2a 전체 수·렌더 선 단언을 유지한다. 제품의 Judgment guide 기본 Always와 독립 제어는 유지한다. 별도 R2c 시험으로 기본 Always, 그립 잡기 표시, 두 스위치의 독립성을 확인한다. 이는 권장안이며 명세·기존 시험을 Codex가 먼저 수정하지 않는다.
- **확인할 시험:**
  - R2a E7: Judgment guide를 끈 그립에는 기존대로 방향 모양이 없다.
  - R2a E11: Judgment guide를 끈 상태에서 기존 Motion guide Off / SelectedOnly / Always 단언을 그대로 유지한다.
  - R2c E1/E12: Motion guide가 Off이거나 미선택 SelectedOnly여도 Judgment guide Always면 잡기 형상이 남는다. Judgment guide Off / 미선택 SelectedOnly는 R2c 모양만 없앤다. 그립도 기본 Always에서 잡기 형상이 있고 기존 방향 종류는 없다.
  - 게임 월드는 두 표시 모두 모양·렌더 선이 없다. 기존 R2a E10·R2c E12로 확인한다.
- **질문:** 위 E7·E11 시험 준비 변경을 §3에서 허용하도록 명세를 개정할지 확인해 주세요. 다른 계약을 원하면 명세에서 명시해 주세요.
