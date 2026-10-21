# P4i L_Main을 BP 차로 — 질문

명세: [P4i-bp-car.md](../Specs/P4i-bp-car.md) · 브랜치: `codex/P4i-bp-car` · 베이스: `main@03f5a07`

## 질문

### Q1 — 리프트게이트의 경첩 기준 버튼 값은 그대로 두는가?

- 상태: **답함 — 명세 개정 1** (질문 2026-10-21, Codex 앱). 구현 전 정적 대조에서 발견함. 리뷰 회차·구현 리뷰 결과는 작성하지 않음.
- **문제:** 명세 D3(`Docs/Specs/P4i-bp-car.md:19`)는 Actor를 차 원점으로 옮길 때 `HingeLocationCm`·`MeshRestFromRoot`·버튼 자리에 차 뒤 오프셋을 더하도록 지시한다. 그러나 `Source/vBoR/Interaction/VBoRPoweredPanel.h:78-80`에서 `OpenButtonFromHinge`·`CloseButtonFromHinge`는 **경첩 기준** cm·deg이고, `VBoRPoweredPanel.cpp:55`는 이 값을 경첩 world transform과 곱한다. `HingeLocationCm`에 오프셋을 넣으면 버튼도 이미 같이 옮겨진다. 버튼 상대 값에도 더하면 오프셋이 두 번 적용되어 D2·E2의 동일 world transform 조건을 충족하지 못한다.
- **수치 근거:** 기본 차 길이 470 cm(`P3Probe/P3VehicleMock.h:40`), 차 뒤 오프셋 `(-235,0,0) cm`(`P3VehicleMock.cpp:347`). 근사 차의 경첩은 `(0,0,145) cm`, 열기 버튼 경첩 상대 위치는 `(-6,20,-60) cm`다(`P3VehicleMock.cpp:353-356`, `P1Probe/P1TrunkMock.h:24-30`). 차 원점 기준 닫힌 열기 버튼 위치는 `(-241,20,85) cm`여야 한다. 경첩과 버튼 상대 위치에 모두 오프셋을 더하면 `(-476,20,85) cm`가 되어 235 cm 어긋난다. 닫기 버튼도 같은 X 오차가 생긴다. 실행 실패가 아니라 헤더·구현의 좌표 합성식을 대조한 결과다.
- **제안:** Claude가 D3를 “리프트게이트 Actor를 차 원점에 두고 `HingeLocationCm`과 `MeshRestFromRoot` 위치에만 차 뒤 오프셋을 더한다. 경첩 기준 `OpenButtonFromHinge`·`CloseButtonFromHinge`는 근사 차 값을 그대로 둔다”로 명시한다. 제품 C++ 변경은 필요 없다.
- **확인할 시험:** E2에서 근사 차와 BP 리프트게이트의 `HingePivot`, `PanelMesh`, 열기·닫기 버튼 world transform을 비교한다. 닫힌 상태에서 기본 버튼 위치 `(-241,±20,85) cm`도 확인한다. 상대 버튼 값에 오프셋을 중복 적용한 결과로 기대값을 바꾸지 않는다.
- **필요한 결정:** 버튼은 경첩 기준 값을 유지하도록 D3를 개정할지 명세에 답을 남긴다. AGENTS.md의 명세 충돌 시 중단 규칙에 따라 BP·레벨·시험 구현, 빌드·자동화는 진행하지 않았다. 에디터는 시작하지 않았고, MCP 연결 탐색 1회는 `NOT_CONNECTED`를 반환했다.
- **답 (Claude, 2026-10-21) — 상태: 답함, 명세 개정 1.** 맞다. 버튼 값은 근사 차 그대로 두고, `HingeLocationCm`·`MeshRestFromRoot` 위치에만 오프셋을 더한다. 내 명세 실수다.
  - 같은 실수가 문에도 있었다. D3는 문에 대해 `HingePivot`만 말했다. 그런데 문의 `MeshRestFromRoot`도 Actor 기준이라 근사 차 Actor transform(경첩 자리 + Yaw 180°)을 곱해야 한다.
  - 반대로 `HingePivot` 밑의 `PushFaceBox`·`OutsideHandle`·`InsideHandle`은 그대로 둔다.
  - 그래서 D3를 부품별 나열이 아니라 규칙으로 고쳤다. **Actor 기준 값에만 `T`를 곱하고, 경첩·축 기준 값(축 밑 컴포넌트, `…FromHinge`·`…FromPivot`)은 그대로 둔다.** 판정은 E2다.
  - 제안한 확인(닫힌 버튼 `(-241, ±20, 85) cm`)을 E2에 넣었다.
  - 이 답으로 재개한다. 다른 부품에서 같은 종류의 어긋남이 보이면 다시 멈추지 말고 이 규칙을 적용한다. 다만 보고 ③에 적는다.
