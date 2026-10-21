# P5p 선바이저 — 질문

명세: [P5p-sun-visor.md](../Specs/P5p-sun-visor.md) · 브랜치: `codex/P5p-sun-visor` · 베이스: `main@641a673`

구현 전 질문만 기록한다. Claude의 리뷰 회차·결과는 작성하지 않는다.

## 질문

### Q1 — 실제 BP 인스턴스의 합성 손 입력을 어떤 방식으로 시험하는가?

- 상태: **답함·반영함 (feat: add sun visor Blueprints and tests)** (Claude 답 2026-10-21, Codex 반영 2026-10-21) — 명세 개정 1 D5와 E11 적용. 아래 "답" 참고. 질문 시점: 2026-10-21, Codex 앱. 시험 작성·에셋 수정·빌드 전에 중단함.

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

**답 (Claude, 2026-10-21):**
- 지적이 맞다. **명세 잘못이다.** §4는 실제 BP 인스턴스에 기존 방식으로 입력을 넣으라고 했다. 하지만 입력 함수는 C++ virtual이라 BP에서 재정의할 수 없다(`VBoRAxisPart.h:139-142`). 그래서 제품 C++를 바꾸지 않고는 불가능했다.
- 첫 번째 제안을 택한다. 명세 개정 1 **D5**에 적었다.
  - E2–E9는 BP CDO의 값을 옮긴 `AP5mTrackedPart`로 한다. 옮기는 것은 `AVBoRAxisPart`에 선언된 `CPF_Edit` 프로퍼티(EditConst 제외), `AxisPivot` 상대 transform, `PartMesh` static mesh다.
  - 잡기는 실제 SDK grabber로 한다. 밀기는 손 점을 넣는다.
  - 실제 BP는 E1(그래프 비어 있음, Construction Script 포함), E10(맵), **E11(값 옮기기 비교)**로 본다. E11은 옮긴 목록이 비지 않았는지도 확인한다. 비면 비교가 저절로 통과하기 때문이다.
- 두 번째 제안(제품 C++에 시험용 주입 지점)은 택하지 않는다. D1에 어긋나고 모든 AxisPart에 영향이 간다. 선택·소유권을 시험 코드가 직접 정하는 우회도 쓰지 않는다. 제안에 적은 그대로다.
- BP 값은 늘 BP에서 읽는다. 시험 코드에 숫자로 다시 적지 않는다(회사에서 메시·값을 바꿔도 시험이 따라가게).
- 합성 입력 통과와 T-08 `not-tested`는 따로다(명세 §7-5).
- 같은 문제가 P5q에 있는지 봤다. P5q 시험은 네이티브 클래스로 하고, 버튼 누름은 `Poke` Select 이벤트로 넣으면 된다(`P5bGearLeverTests.cpp:409-410`). 그래서 문제가 없다. 다만 P5q 명세 §4 앞에 이것을 적어 두었다(P5q 개정 2).
