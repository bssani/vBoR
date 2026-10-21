# P5p 선바이저 — 리뷰

명세: [P5p-sun-visor.md](../Specs/P5p-sun-visor.md) · 브랜치: `codex/P5p-sun-visor` · 베이스: `main@641a673`

상태: **1회차 리뷰 — 문제 1개(R1-1: 밀기 시험이 지금 경첩 방향을 가정함), Claude가 직접 반영함(사용자 "작은 작업은 너가"). 열린 항목 없음. 머지 승인 대기.** 질문 Q1은 답했고 반영됐다.

## 리뷰 1회차 (2026-10-21, Claude — `127f28a`)

**결과: 문제 1개.** R1-1은 회사에서 경첩을 다른 방향으로 넣으면, BP가 맞아도 밀기 시험이 실패하는 문제다. 원인 일부는 내 명세 D1의 "면 0은 값 +"다. 나머지는 명세대로다.

**BP 2개:**
- 부모는 `AVBoRAxisPart`이고 그래프는 비어 있다.
- 설정: Stay, palm·pinch·push 켬, Axis Y, 한계 95°, `SpectatorName` "Sun Visor".

**자리:** Codex MCP 조회 결과 `Saved/P5pRecovery/Result.json`에서 확인했다.
- 경첩은 L (18, −37, 142), R (18, 37, 142)이고, `AxisPivot` Yaw는 180°다.
- 판은 16 × 35 × 1.5 cm, 메시 중심은 (10, ∓37, 142)다.
- L이 운전석(−Y) 쪽이다.
- 바이저는 Y 19.5–54.5 cm에 있어 룸미러(Y ±12.5)와 겹치지 않는다.

**`L_Main`:**
- Actor가 45개에서 47개가 됐다. 기존 Actor는 그대로다(Codex가 바꾸기 전 기록과 비교).
- 두 BP는 `Car` 폴더에 있고, 루트는 단위 변환이다.

**그 밖:** 제품 C++·기존 시험은 바뀌지 않았다. 스크립트·백업은 `Saved/`에만 있다.

**재현.** 에디터가 닫힌 것을 확인했고, 단계마다 에디터가 다시 열렸는지 보며 돌렸다.
- `127f28a` 그대로: non-unity·unity `Result: Succeeded`, vBoR 경고·오류 0. `vBoR.*` **489/489**, `TEST COMPLETE. EXIT CODE: 0`. 로그 `Saved/Logs/P5pReview*.log`.
- R1-1 반영 뒤: non-unity·unity 성공, vBoR 경고·오류 0(deprecated 2건은 로컬 도구 플러그인). `vBoR.*` **489/489**(P5p 11개 포함), EXIT CODE 0. 경첩 반대 방향 경우도 E4·E5에서 통과했다. 로그 `Saved/Logs/P5pR1*.log`.

**Codex 토큰:** Q1과 구현이 한 세션이었고 **7.74M**이다. 하위 에이전트는 0이다. 멈춘 한 번(Q1)은 내 명세 탓이다.

**확인한 것:**
- **D5 값 옮기기.**
  - `AVBoRAxisPart`에 선언된 프로퍼티 중 Edit이면서 EditConst가 아닌 것 전부를 옮긴다. `AxisPivot`과 static mesh도 옮긴다.
  - 옮기는 때는 `FinishSpawning` 전이다. 시험 코드에 BP 숫자를 다시 적지 않았다.
- **E11:**
  - 실제 BP 인스턴스와 프로퍼티별로 비교하고, 이름을 실패 메시지에 쓴다.
  - construction 뒤 메시 world transform과 `GrabBox`를 비교한다.
  - 옮긴 목록이 비지 않았는지, 필수 이름 8개가 들어 있는지 본다.
- **E1:** 함수 그래프에 진입 노드만 있는지 본다(D5의 전제).
- **밀기 손 점의 좌표계가 맞다.** fixture의 `ReadPushHands`는 경첩 쉬는 좌표계로 넣는다. 제품도 같은 좌표계로 바꿔서 쓴다(`VBoRAxisPart.cpp:335-342`).
- **E2는 내림 방향을 실제로 본다.** 값이 +이면서 메시 아래 끝 world Z가 줄어드는지 본다.
- **E8의 점은 축 거리 규칙을 실제로 시험한다.** 규칙이 없으면 면을 넘는 접촉이 성립하는 점(이전 점은 바깥, 접촉 깊이 0.05 cm)을 골랐다.
- **E7**은 같은 손과 다른 손 둘 다 본다. 잡은 뒤 다른 손 접촉도 막히는지 본다.
- 주석 말투는 규칙대로다.

### R1-1 밀기 시험이 지금 경첩 방향을 가정해 회사에서 깨질 수 있음 — 반영함 (Claude, 이 커밋)

**문제:**
- 위치: `P3Probe/P5pSunVisorTests.cpp`(`127f28a`). `FP5pSurface`(:100-115)와 E4–E8의 면 번호 0·1이다.
- 도우미가 다음 셋을 고정으로 둔다. 셋 다 지금 BP(`AxisPivot` Yaw 180°, `bReverse` 꺼짐)에서만 맞다.
  - 판은 경첩 로컬 +X 쪽에 있다.
  - 면 0은 윗면(+Z)이다.
  - 면 0을 밀면 값이 + 된다.
- **회사에서 깨진다.** 회사에서는 Blueprint-Guide §4.1-5대로 CAD 경첩 방향을 넣는다.
  - 이때 Yaw 0에 `bReverse`로 맞추면 BP는 올바르다(내림 = 값 +).
  - 하지만 판이 로컬 −X 쪽에 오고, 값을 키우는 면이 면 1이 된다.
  - 그러면 E4·E5는 `face=0`을 기대해 실패한다. 손 점도 반대 면에 놓인다.
- 명세 D5는 "회사에서 값이 바뀌어도 시험이 따라감"이라고 했다. P4i R1-1과 같은 종류의 문제다.
- **원인 일부는 내 명세 D1이다.** "면 0은 값 +"라고 적었는데, `bReverse`가 꺼졌을 때만 맞다. 명세 개정 2로 고쳤다.

**반영한 것:**
- **면 위치:** `FP5pSurface`가 반지름 방향과 법선을 bounds에서 구한다. 규칙은 제품 `ConfigurePush`(`VBoRAxisPart.cpp:347-367`)와 같다. 면 0은 법선이 가장 작은 쪽, 면 1은 가장 큰 쪽이다.
- **내림 면:** 값 +(내림) 면은 `bReverse`면 1, 아니면 0이다(`LowerFace`).
  - 근거: 면 0은 원래 각을 키운다(`AdvancePush`).
  - 값 = 원래 각 × (`bReverse`면 −1)이다(`VBoRAxisPart.cpp:282`, `:288-293`).
- **E4·E5:** 두 경우를 다 돈다.
  - BP 그대로인 경우.
  - 경첩 반대 방향인 경우: `AxisPivot`을 로컬 Z로 180° 돌리고 `bReverse`를 뒤집는다. 메시 자리는 그대로다.
  - E4는 밀었을 때 메시 아래 끝이 실제로 내려가는지도 본다. 이것으로 반대 방향 설정도 올바른 BP라는 것을 확인한다.
- **E6–E8:** 내림 면을 `LowerFace`로 고른다.
- 그대로 둔 것: 기대값·허용 오차, 제품 코드·BP·레벨.

**확인할 시험:**
- 위 재현을 본다.
- 회사에서 CAD 경첩으로 바꾼 뒤의 E4–E8은 그때 본다.

### 참고

1. **밀기 손 점의 world → 경첩 좌표 변환은 자동화로 안 본다.**
   - fixture가 `ReadPushHands`를 통째로 바꾸므로, `RestPivot * GetActorTransform()` 역변환(`VBoRAxisPart.cpp:335-342`)을 거치지 않는다.
   - P5m은 경첩이 단위 변환이라 차이가 없었다. 바이저는 경첩이 Yaw 180°이고 자리도 옮겨진 첫 밀기 부품이다.
   - 코드는 읽어서 맞다고 봤다. VR(명세 §7-2)에서 앞·뒤 밀기가 맞는 쪽으로 움직이는지 본다.
2. **올린 상태에서 밀어 내리려면 손이 지붕을 통과해야 한다.**
   - 판 윗면이 Z 142.75, 지붕 아랫면이 143.5라 틈이 0.75 cm뿐이다.
   - 근사 차에서는 내리는 일을 사실상 잡기로 한다. VR에서 볼 것은 "잡으려다 밀려 버림"과 "밀어서는 안 내려감" 두 가지다.
3. **경첩 Z는 판 두께의 가운데**(앞 모서리 가운데)다. D3 "Z 약 142"를 지킨다.
4. **Blueprint-Guide에 바이저 줄이 아직 없다.** P5q 머지 뒤 사이드미러와 함께 적는다. 경첩 방향은 Yaw 180° 그대로 두거나, 반대로 넣으면 `bReverse`를 뒤집는다는 것도 적는다.

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
