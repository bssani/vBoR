# P5p 선바이저 — 잡기·pinch·양쪽 밀기, 놓은 자리에 머묾

| 항목 | 값 |
|---|---|
| 단계 | P5 자유모드 실내 조작 추가(사용자 요청 2026-10-21). 시험 T-08 |
| 결정 근거 | **사용자(2026-10-21):** "선바이저도 만들 수 있나? 잡기, pinch 및 양쪽 밀기까지. 이건 열고 닫고가 없어." Claude 질문 "위아래 1축만?" → 사용자: **위아래 1축만**(옆 창문 쪽 둘째 축 없음).<br>허용 조작(AGENTS.md 확정 제약): 1축 회전, 손바닥·pinch 잡기, 손가락·손으로 밀기 |
| 베이스 브랜치 | `main` @ 이 명세 머지 뒤 |
| 작업 브랜치 | `codex/P5p-sun-visor` |
| 리뷰 파일 | `Docs/Reviews/P5p-sun-visor.md` (첫 리뷰 때 생김) |
| 상태 | 명세 — 구현 전 |
| 에디터 작업 | **있음** — BP 2개 만들기와 `L_Main` 배치(MCP). 사용자 요청 |
| 다음 명세 | [P5q-power-mirrors.md](P5q-power-mirrors.md). 이 명세가 머지된 뒤 시작한다 |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **새 부모 없이 `AVBoRAxisPart`로** | 1축 부품이 이미 다 가진다.<br>- 놓으면 그 자리: `Release = Stay`. 값은 `[0, MaxAngleDeg]`로 자른다(`Interaction/VBoRAxisPart.cpp` `AcceptTransform`).<br>- 손바닥·pinch: `bAllowPalmGrab`·`bAllowPinchGrab`(`VBoRAxisPart.h:96-97`).<br>- 양쪽 면 밀기: `bAllowPush`(`:98`). 면 0은 값 +, 면 1은 값 −로 민다(`AdvancePush`).<br>- **잡기가 먼저다.** 잡고 있으면 밀기를 끄고, 잡으면 밀기를 지운다(`AdvancePush` 첫 줄·잡기 시작 `ClearPush`).<br>- **걸림·잠김·열림/닫힘 상태가 없다**("열고 닫고가 없어"). `Release = Detent`를 쓰지 않는다.<br>**제품 C++는 바꾸지 않는다.** 이 조합으로 안 되는 것이 나오면 멈추고 질문한다 |
| D2 | **BP 2개** | `Content/Main/Car/Parts/BP_SunVisor_L`·`BP_SunVisor_R`(부모 `AVBoRAxisPart`). Event Graph는 비운다.<br>- `Axis` = 차 좌우(Y). `AxisPivot` = 바이저 경첩(앞 윗변).<br>- **값 +는 바이저가 아래(앉은 사람 앞)로 내려오는 쪽**이다. 방향은 `bReverse`로 맞춘다.<br>- `MaxAngleDeg` 약 95°(근사). `Release = Stay`. palm·pinch·push 모두 켬. `DoorHandleSide` = 없음.<br>- `SpectatorName` "Sun Visor". 메시로 잡기(`bGrabWithMesh`)는 끔(엔진 Cube). |
| D3 | **근사 자리·크기 — 근사 차에 바이저가 없으므로 이 명세 값** | 근사 차 좌표(cm, `P3Probe/P3VehicleMock.h` 기본값 기준):<br>- 운전석 눈 `(-20, -37, 115)`, 지붕 판 아랫면 Z 약 143.5(`P3VehicleMock.cpp:93`).<br>- **접힌(값 0) 바이저:** 지붕 바로 밑에 수평으로, 눈 앞 위다. 판 약 X 16 × Y 35 × Z 1.5. 경첩은 앞 윗변 X 약 18, Y = ∓37, Z 약 142.<br>- 내리면 경첩에서 아래로 매달려 눈 앞을 가린다. 아래 끝 Z 약 126.<br>- 룸미러(`Y` 0, 폭 25)와 겹치지 않는다.<br>- 정확한 값은 구현자가 정한다(§5). 근사임을 보고에 적는다 |
| D4 | **`L_Main` 배치** | 두 BP를 **차 원점**에 놓는다. 자리는 BP 기본값의 축·`MeshRestFromRoot`로만 정한다(Blueprint-Guide §4.1-2, P4i D3). `Approx` 폴더가 아니다 — 실제 부품 BP다 |

## 1. 왜 만드는가

자유모드에서 참가자가 바이저를 내리고 올려 본다. 사용자 요청이다.

## 2. 범위

### 만들 것

1. BP 2개(MCP)와 `L_Main` 배치(MCP), 저장.
2. 자동화 시험 §4(`vBoR.P5p.*`).

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| 옆 창문 쪽 둘째 축 | 사용자: 위아래 1축만 |
| 제품 C++, 새 부모 클래스 | D1 |
| 시나리오 단계·태그 | 자유모드 부품이다. 단계와 잇지 않는다 |
| 근사 차(`AP3VehicleMock`)·`L_P3_Flow`에 바이저 넣기 | 근사 차는 시험대로 그대로 둔다 |
| 화장 거울·조명 | 요청 없음 |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- `AVBoRAxisPart`의 동작·기본값은 그대로다. 기존 시험(`vBoR.P2b.*`·`vBoR.P5b.*`·`vBoR.P5c.*`·`vBoR.P5m.*`·`vBoR.P2f.*`·`vBoR.P4i.*`)이 그대로 통과한다.
- **P4i 시험 감사:**
  - `P4iCarBlueprintTests.cpp`의 BP 목록 `P4iBlueprints()`는 18개로 고정이고, E1·E4는 그 목록만 센다. 그래서 바이저 BP가 생겨도 단언은 그대로다.
  - E5(참조 규칙)는 `/Game/Main` 아래 모든 에셋을 본다. 바이저 BP도 `Development/`를 참조하면 안 된다.
- `L_Main`의 다른 Actor·BP·`Approx`는 바꾸지 않는다.

## 4. 경계 상황과 그 시험 (`vBoR.P5p.*`)

BP 클래스를 `LoadClass`로 읽어 시험 world에 차 원점으로 놓는다. 잡기는 `P2Probe/P2bAxisPartTests.cpp`의 실제 grabber 방식, 밀기는 `P3Probe/P5mLeverTests.cpp`의 손 점 넣기 방식을 따른다.

| ID | 상황 | 확인 |
|---|---|---|
| E1 | BP 기본값 | 두 BP의 부모가 `AVBoRAxisPart`, Event Graph 노드 0. `Release = Stay`, palm·pinch·push 켬, `Axis` = Y, `DoorHandleSide` = 없음, `SpectatorName` "Sun Visor", `MaxAngleDeg` > 0 |
| E2 | 손바닥으로 내림 | 손바닥 잡기로 약 40° 내린 뒤 놓음 → 값이 그대로 남는다(되돌아가지 않음). 내리는 방향이 값 +이고, 메시 아래 끝이 내려간다(world Z 감소) |
| E3 | pinch로 올림 | pinch로 잡아 값을 줄인 뒤 놓음 → 그대로 남는다 |
| E4 | 앞면 밀기 | 면 0 쪽에서 손 점으로 밀면 값이 늘어난다. `MaxAngleDeg`에서 멈춘다 |
| E5 | 뒷면 밀기 | 내려온 상태에서 면 1 쪽에서 밀면 값이 줄어든다. 0에서 멈춘다 |
| E6 | 밀고 뗌 | 민 손을 떼면 그 자리에 남는다(`Stay`) |
| E7 | 잡기가 먼저 | 밀던 중에 같은 손이나 다른 손으로 잡으면 밀기가 끝나고 잡은 손을 따른다 |
| E8 | 축 가까이 | 경첩 바로 옆(`PushMinAxisDistanceCm` 안)에서 밀면 움직이지 않는다 |
| E9 | 두 바이저 독립 | 왼쪽을 내려도 오른쪽 값은 0 그대로다 |
| E10 | `L_Main` 정적 검사 | 맵 패키지를 Play 없이 읽는다. 두 BP가 하나씩 있고 루트 상대 transform이 단위 변환(차 원점)이다. `Approx` 폴더가 아니다 |

## 5. 구현자가 정할 것

- 근사 자리·크기·`MaxAngleDeg`·`PushMarginCm`(D3 범위 안). 왼쪽과 오른쪽은 Y만 대칭이다.
- MCP 호출 방식(P4i처럼 MCP Python 일괄을 권장 — 승인 수를 줄인다). 스크립트는 `Saved/`에만 둔다.
- 시험에서 잡기·밀기를 넣는 방법(기존 시험의 도우미를 그대로 쓰거나 같은 방식으로).

## 6. 완료 조건

1. D1–D4를 지킨다.
2. E1–E10이 통과한다.
3. 기존 `vBoR.*` 시험이 모두 통과한다.
4. non-unity·unity 빌드에서 vBoR 경고가 없다.
5. 순서: C++(시험) 빌드 → 에디터를 열어 MCP 작업 → 저장 → 에디터 닫기 → 전체 시험 한 번.
6. 보고는 AGENTS.md "작업 방식" 형식으로, 커밋 본문과 HANDOFF "Codex 결과"에 쓴다. 만든·바꾼 에셋 목록을 ①에 넣는다.

## 7. 로컬에서 확인할 것 — 사용자(VR, 끝에 한꺼번에)

1. 앉아서 바이저를 손바닥·pinch로 내리고 올린다. 놓은 자리에 머무는지 본다.
2. 손등이나 손가락으로 앞·뒤에서 밀어 본다. 잡으려다 밀려 버리는 일이 잦은지 본다(촉감이 없으므로).
3. 회사에서는 CAD 바이저 메시와 경첩 자리로 바꾼다(Blueprint-Guide §4). 얇은 판이라 메시로 잡기(§4.3)를 권장한다.
4. 시험 상태는 VR 전까지 `not-tested`(T-08).

## 8. 확인한 근거

| 무엇 | 근거 | 쓰는 곳 |
|---|---|---|
| 놓기 방식 | `EVBoRAxisRelease { Return, Stay, Detent }` `Interaction/VBoRAxisPart.h:25`. Detent가 아니면 값 `[0, Limit()]` `VBoRAxisPart.cpp:285-296`(`AcceptTransform`), `MaxAngleDeg` `VBoRAxisPart.h:113` | D1 |
| 입력 허용 | `bAllowPalmGrab`·`bAllowPinchGrab`·`bAllowPush`·`PushMarginCm` `VBoRAxisPart.h:96-100` | D1·D2 |
| 밀기 | `ConfigurePush` `VBoRAxisPart.cpp:347-367`(Rotate만, 메시 bounds 면), `FindPush` `:376-`, 면 0 → 값 +·면 1 → 값 −와 잡는 중 밀기 끔 `AdvancePush` `:410-446`, 축 가까움 `PushMinAxisDistanceCm` `VBoRAxisPart.h:99` | D1·E4–E8 |
| 시험 본보기 | 실제 grabber `P2Probe/P2bAxisPartTests.cpp`, 밀기 `P3Probe/P5mLeverTests.cpp:114-` | §4 |
| 근사 차 자리 | 눈 `P3VehicleMock.h:41`, 지붕 판 `P3VehicleMock.cpp:93`, 룸미러 `P3VehicleMock.h:113-115` | D3 |

## 개정 이력

| 날짜 | 내용 |
|---|---|
| 2026-10-21 | 처음 작성(Claude) |
