# R2a 움직임 방향 표시 + 미리보기 — 질문

첫 리뷰 결과가 아니라 구현 전 질문만 기록한다(2026-11-01, Codex).

## 질문

### Q1 — D9·E10의 Play 시작 값 0과 기존 부품 시작 상태 충돌

- 상태: **열림**.
- 문제:
  - `Docs/Specs/R2a-motion-guide.md:26`(D9)은 게임 월드에서 "지금처럼 0에서 시작", `:86`(E10)은 BeginPlay 뒤 "각·값 0, 닫힘 상태"를 요구한다. 그러나 `:60`(§3)은 부품 동작·이벤트 등을 그대로 유지하고, `:69`는 기존 시험 단언 변경을 허용하지 않는다.
  - 미러는 `Source/vBoR/Interaction/VBoRFoldingMirror.cpp:44`에서 `ResetMirror()`를 호출하고, `:70-71`에서 `FoldAngleDeg`와 `Folded`로 시작한다(기본 각도는 `VBoRFoldingMirror.h:45`의 -65 deg). 기존 `Source/vBoR/P3Probe/P5qPowerMirrorTests.cpp:138-139`도 Play 시작의 `Folded`·`FoldAngleDeg`를 단언한다. 따라서 Preview를 무시하더라도 기존 시작 각은 0이 아니다.
  - 리프트게이트도 `Source/vBoR/Interaction/VBoRPoweredPanel.cpp:86` → `:127-128`에서 `bStartOpen`에 따라 열린 상태·`OpenAngleDeg`로 시작할 수 있다.
  - AxisPart detent는 `Source/vBoR/Interaction/VBoRAxisPart.cpp:219-222`에서 `DetentStartIndex`의 각도로 시작하므로 설정에 따라 값이 0이 아니다.
- 제안: D9·E10을 "Preview를 무시하고 기존 부품별 초기 상태와 설정을 그대로 사용"으로 개정한다. 미러의 접힘, 리프트게이트의 `bStartOpen`, detent의 `DetentStartIndex`를 유지하고 Preview=0/1의 게임 시작 결과가 같음을 시험한다. 이는 제안이며 Codex가 명세를 임의로 고치거나 시작 동작을 변경하지 않는다.
- 질문: 위와 같이 기존 부품별 초기 상태를 유지하는 것으로 D9·E10을 개정하는가? Claude가 명세를 직접 개정하면 해당 개정을 확인한 뒤 같은 브랜치에서 재개한다.
- 확인할 시험: E10에서 미러 기본 접힘(-65 deg), `bStartOpen=true` 리프트게이트, 시작 각이 0이 아닌 detent를 포함하여 Preview=0/1의 초기 상태·자세·이벤트가 동일한지 확인한다. 게임 중 Preview 변경도 상태·자세·이벤트에 영향을 주지 않아야 한다. 기존 시험 단언은 그대로 둔다.
- 확인 상태: `main@98886ad`에서 `codex/R2a-motion-guide` 생성. 제품 코드·시험·에셋·명세 변경 없음. 빌드·자동화 미실행. 에디터/MCP 작업 없음. 에디터 시각 확인은 `not-tested`, Quest 3 T-ID는 본 명세에 없음.
