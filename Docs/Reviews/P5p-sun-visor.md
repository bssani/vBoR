# P5p 선바이저 — 질문

명세: [P5p-sun-visor.md](../Specs/P5p-sun-visor.md) · 브랜치: `codex/P5p-sun-visor` · 베이스: `main@641a673`

구현 전 질문만 기록한다. Claude의 리뷰 회차·결과는 작성하지 않는다.

## 질문

### Q1 — 실제 BP 인스턴스의 합성 손 입력을 어떤 방식으로 시험하는가?

- 상태: **열림** (2026-10-21, Codex 앱). 시험 작성·에셋 수정·빌드 전에 중단함.

**문제:**
- 명세 §4는 BP 클래스를 `LoadClass`로 읽어 시험 world의 차 원점에 놓고, E2·E3·E7은 실제 SDK grabber, E4–E8은 P5m 방식의 합성 손 점으로 시험하도록 요구한다. D1은 제품 C++ 변경을 금지한다.
- 실제 BP의 부모 `AVBoRAxisPart`는 SDK Select에서 `IsHandTracked()`가 false이면 소유권을 즉시 버린다(`Source/vBoR/Interaction/VBoRAxisPart.cpp:461-462`). 이 함수는 MetaXR의 실제 위치 유효성·High 신뢰도를 읽는다(`:303-307`). SDK 변환과 Tick도 같은 검사를 한다(`:110`, `:524`). grabber 위치·PalmGrab/PinchGrab만 넣어서는 headless 합성 선택이 성립하지 않는다.
- 밀기도 `ReadPushHands()`가 실제 Pawn·MetaXR·손끝을 읽는다(`:332-345`, `:549`). 해당 함수와 추적 함수는 C++ virtual이며 Blueprint override 이벤트가 아니다(`VBoRAxisPart.h:139-142`).
- 본보기는 실제 BP와 다른 네이티브 시험 자식이다. `AP2bTrackedPart`는 `IsHandTracked()`를 재정의한다(`Source/vBoR/P2Probe/P2bTestFixture.h:10-17`). `AP5mTrackedPart`는 이를 상속하고 손 점·선택 grabber·잡기 인식기 입력을 재정의한다(`Source/vBoR/P3Probe/P5mTestFixture.h:9-20`). 실제 BP를 이 시험 클래스로 cast하거나 spawn class만 바꿔 같은 인스턴스로 취급할 수 없다.
- 따라서 기존 도우미를 그대로 쓰는 것으로는 **실제 BP 인스턴스 + 합성 입력 + 제품 C++ 무변경**을 동시에 만족하지 못한다. BP 기본값을 시험 자식에 복사하는 방식으로 임의 대체하지 않았다.

**제안 (Claude 명세 개정 필요):**
- 제품 C++ 무변경을 유지하고, E1·E9·E10 등 에셋·배치 검사는 실제 BP로 한다. 동작 시험은 각 BP의 편집 가능한 설정·축·메시를 기존 `AP5mTrackedPart`에 옮긴 뒤 실제 SDK grabber와 합성 손 점을 사용하도록 명시한다. 복사 누락을 막는 비교 검사도 포함한다.
- 또는 실제 BP 인스턴스에 입력을 넣어야 한다면, 허용할 최소 시험용 주입 지점과 D1의 예외를 명세에 정한다. 선택·소유권·변환을 시험 코드가 직접 확정해서 SDK 검사를 우회하는 방법은 동작 시험의 대체로 쓰지 않는다.

**확인할 시험:**
- 개정한 입력 방법으로 두 BP 설정의 palm·pinch 선택, Stay 유지, 양면 밀기·상하한·잡기 우선·축 근처 거부를 E2–E8에서 검증한다.
- E1의 실제 BP 부모·기본값·Event Graph 및 E10의 실제 맵 배치 검사는 유지한다.
- 합성 입력 통과와 Quest 3 T-08 `not-tested`를 구분한다.

**정적 확인한 플러그인 헤더:**
- `Plugins/MetaXR/Source/OculusXRInput/Public/OculusXRInputFunctionLibrary.h:216,258`: 실제 추적 신뢰도·위치 유효성 조회 선언.
- `Plugins/MetaXRInteraction/Source/OculusInteraction/Public/Interaction/IsdkGrabberComponent.h:69,75`: `PinchGrab()`·`PalmGrab()` 선언. 이 호출이 제품의 추적 검사까지 대체하지는 않는다.
