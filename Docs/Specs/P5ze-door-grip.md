# P5ze 문 안쪽 손잡이(그립) — 잡고 밀고 당기기, 잠금은 풀지 않음

| 항목 | 값 |
|---|---|
| 단계 | P5 리허설 준비(사용자 요청 2026-10-28). 시험 T-03(문 손 조작) |
| 결정 근거 | **사용자(2026-10-28):** "문 손잡이가 필요해. 안쪽에서 문 열 때 지금 손잡이를 잡고 여는데, 그거 말고 손잡이 뒤에 있는 문 여는 용도가 아닌 진짜 손잡는 거 있잖아. 그거로 밀고 당기기 가능해야 해."<br>**지금:** 문에 연결되는 손잡이는 바깥·안쪽 **잠금 레버** 둘뿐이다(`AVBoRHingedPanel` `OutsideAxisHandle`·`InsideAxisHandle`, `VBoRHingedPanel.h:60-61`). 레버를 끝까지 당기면 잠금이 풀리고 그 손이 문을 잡는다(`VBoRAxisPart.cpp:579-582` → `BeginAxisGrab` `VBoRHingedPanel.cpp:307-322` → `FVBoRDoorMotion::BeginGrab`, 그 안에서 `Unlock` `VBoRDoorMotion.cpp:40-51`) |
| 베이스 브랜치 | `main` @ 이 명세 머지 뒤 |
| 작업 브랜치 | `codex/P5ze-door-grip` |
| 리뷰 파일 | `Docs/Reviews/P5ze-door-grip.md` (첫 리뷰 때 생김) |
| 상태 | 명세 — 구현 전 |
| 에디터 작업 | **없음** (그립 BP·배치는 사용자가 한다, §7) |
| 앞 명세 | [P5zd-key-mesh-size.md](P5zd-key-mesh-size.md) |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **그립은 손잡이와 같은 부품(`AVBoRAxisPart`)에 새 종류 하나** | `EVBoRDoorHandleSide`(`VBoRAxisPart.h:28-33`)에 **`Grip`**을 **맨 끝에** 더한다(저장된 None·Outside·Inside 값이 바뀌지 않게). 표시 이름 `Grip`.<br>- 그래서 잡기 설정(손바닥·pinch 허용, 메시로 잡기 P2f, 잡는 상자, 추적 손실 보류 P2b D7, hover 색)을 그대로 쓴다. 사용자는 손잡이 BP처럼 `BP_DoorGrip_FL_Int`(부모 `AVBoRAxisPart`)를 만들어 문 밑에 붙인다 |
| D2 | **문 연결: 세 번째 칸 + 자동 후보** | `AVBoRHingedPanel`에 `GripAxisHandle`(EditInstanceOnly, 손잡이 칸 옆)을 둔다. 칸이 비면 Outliner에서 문 밑에 붙은 `DoorHandleSide = Grip` 부품을 자동 후보로 쓴다(손잡이와 같은 규칙: 후보 둘 이상이면 Check setup 문제, Child Actor면 `HingePivot` 밑이어야 함, 다른 판과 공유 금지 — `EvaluateSetup` `VBoRHingedPanel.cpp:206-261`). 연결되면 `HingePivot`에 붙어 문과 같이 돈다(`ApplyAxisHandle` `:271-284`와 같음).<br>- **지금 후보 분류는 Outside가 아니면 모두 안쪽(1)으로 넣는다**(`:157-159`). `Grip`이 안쪽 레버 후보로 섞이지 않게 고친다 |
| D3 | **그립은 잠금을 풀지 않는다** | 그립으로 잡을 때 `Unlock`을 부르지 않는다. 잠긴(닫힌) 문에서 그립을 잡고 당겨도 문은 움직이지 않는다. 실제 차와 같다 — 열려면 레버를 쓴다.<br>- `FVBoRDoorMotion::BeginGrab`에 "잠금 풀지 않음" 길을 둔다(인자, 별도 함수 등 구현자). 잠겨 있으면 그 길은 실패한다(false) |
| D4 | **잡으면 바로 문을 잡는다 — 레버와 다른 점** | 레버는 끝까지 당겨야(`OpenThreshold`) 문을 잡는다. 그립은 **잡는 순간**(그 손이 부품을 잡았고 문이 풀려 있고 문을 잡은 손이 없을 때) 문 잡기를 시작한다. 그 뒤 움직임은 지금 문 잡기 그대로: 잡은 손 위치의 경첩 둘레 각으로 문이 따라간다(`Advance` 잡기 `VBoRDoorMotion.cpp:207-217`). 당기면 닫히고 밀면 열린다, 속도 상한·범위·잠김(latch) 그대로 |
| D5 | **잡은 동안 조건이 생기면 그때 시작** | 그립을 잡았지만 문 잡기를 시작하지 못한 경우(잠김, 다른 손이 레버로 문을 잡고 있음)에는 매 Tick 다시 본다. 조건이 되면(예: 다른 손이 레버로 풀고 놓음) 그 frame에 시작한다. 시작할 때 잡은 점을 기준으로 삼아(`RebaseGrab`) **그동안 움직인 손이 한 번에 적용되지 않는다**(AGENTS.md "추적 손실은 보류"와 같은 이유) |
| D6 | **그립 자체는 움직이지 않는다** | 그립 부품의 축 값은 0에 고정한다(문과 같이 돌 뿐 문 위에서 회전·이동하지 않음). `OnPartOpened`·`OnPartClosed`·걸림 이벤트·소리(`OpenedCue` 등)를 내지 않는다. 놓기(손바닥 펴기)·추적 손실·잠김으로 끝나는 규칙은 레버의 문 잡기와 같다(`EndAxisHold`, `bNeedsPalmOpen`) |
| D7 | **집계는 따로, 문구는 그대로** | 그립 잡기는 레버 집계(`Grabs[손][손잡이]`·`Attempts`, 측정 문구 `VBoRHingedPanel.cpp:479-482`)에 섞지 않는다. `FVBoRDoorMetrics`에 손별 그립 잡기 수 하나를 더하고 `Since()`에도 넣는다. **측정·진행자 문구는 바꾸지 않는다** |
| D8 | **다른 동작은 그대로** | 레버 두 개, 손바닥 밀기(P5w 면 기억 포함), 관성(P5za — 그립을 놓을 때도 레버 놓기와 같이 적용), 잡기가 밀기보다 먼저(P2 E16) |

## 1. 왜 하는가

안쪽에서 문을 닫을 때 사람은 잠금 레버가 아니라 문 안쪽의 손잡이(풀 핸들)를 잡고 당긴다. 지금은 레버만 있어서, 열린 문을 안쪽에서 끌어 닫으려면 레버를 다시 당기거나 손바닥으로 바깥에서 밀어야 한다.

## 2. 범위

### 만들 것

- `EVBoRDoorHandleSide::Grip`, `AVBoRHingedPanel` 그립 칸·연결·Check setup, `AVBoRAxisPart`의 그립 동작, `FVBoRDoorMotion`의 잠금 안 푸는 잡기와 집계(D1–D8).
- 시험 `vBoR.P5ze.*`(§4).

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| 그립 BP 에셋, `L_Main` 배치, 근사 차(`AP3VehicleMock`)에 그립 추가 | 사용자가 회사 CAD로 한다(§7). 근사 차 변경은 P4i 비교 시험을 흔든다 |
| 레버로 잡는 규칙(끝까지 당김, 잠금 해제) 변경 | 그대로 |
| 두 손 잡기(레버 + 그립 동시) | 문 잡기는 한 손뿐이다(`GrabHand` 하나). D5처럼 순서대로 넘긴다 |
| 그립으로 잠금 풀기, 그립 쥐면 잠금 해제(스마트 도어) | D3 |
| 측정·진행자 문구에 그립 칸 | D7 |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- **그립이 없는 문은 지금과 완전히 같다.** 레버 연결·자동 후보(Outside/Inside), 잠김·풀림, 밀기, 집계, 문구.
- `FVBoRDoorMotion::BeginGrab`의 지금 호출(레버)은 결과가 같다(잠금 풀림 포함).
- `EVBoRDoorHandleSide`의 기존 값 순서는 그대로(D1).
- **기존 시험 감사 — 고치지 않는다.** 손잡이 연결을 보는 `P2Probe/P2ePartAssemblyTests.cpp`·`P2bAxisPartTests.cpp`·`P2fMeshGrabTests.cpp`·`P2DoorWorldTests.cpp`·`P2gMeshPushTests.cpp`, 문 동작 `P2DoorMotionTests.cpp`·`P5wDoorPushSideTests.cpp`·`P5zaDoorCoastTests.cpp`, 판 `P2cHingedPanelTests.cpp`·`P2dLatchedPanelTests.cpp`, 근사 차·BP `P3Probe/P4iCarBlueprintTests.cpp`·`P5xPanelHandlesTests.cpp`·`P5pSunVisorTests.cpp`·`P5qPowerMirrorTests.cpp`는 `Grip`을 쓰지 않는다(Claude 2026-10-28, `DoorHandleSide`·`BeginGrab`·`Grabs[` 검색). `FVBoRDoorMetrics`에 필드를 더해도 기존 필드 값은 같다. **하나라도 고쳐야 통과하면 멈추고 리뷰 파일 "질문"에 적는다.**

## 4. 시험 — `vBoR.P5ze.*`

판 Actor 시험은 `P2Probe/P2bTestFixture.h`(`AP2bTrackedDoor`)·`P2ePartAssemblyTests.cpp`의 손잡이 연결 방식, 손은 시험 손(추적 점·손바닥 열림 값 주입)으로 한다. 그립은 시험 world에서 `AVBoRAxisPart`를 `DoorHandleSide = Grip`으로 만들어 문 밑에 붙인다. 문 각도 기준은 `vBoR.P2*`와 같다(최대 70°, 180 deg/s).

| ID | 상황 | 확인 |
|---|---|---|
| E1 | **열린 문 당겨 닫기** — 풀린 문 40°, 안쪽 면 가까이의 그립을 손바닥으로 잡고 손을 닫히는 쪽으로 옮김 | 잡는 순간 문 잡기 시작(끝까지 당김 없이). 각이 손을 따라 줄어듦. `Unlocks` 그대로 |
| E2 | **밀어 열기** — E1에서 손을 열리는 쪽으로 | 각이 손을 따라 늘어남(최대 70) |
| E3 | **잠긴 문** — 닫히고 잠긴 문(0°), 그립 잡고 당기고 밈 | 각 0, `bLocked` true, `Unlocks` 0, 문 잡기 없음(`GrabHand` −1) |
| E4 | **레버 뒤 넘겨받기** — E3 상태에서 다른 손으로 안쪽 레버를 끝까지 당겨 열고(문 잡음) 문을 10° 연 뒤 레버를 놓음. 그 사이 그립 손은 5 cm 움직여 있음 | 레버 손이 잡는 동안 그립 손은 문을 못 잡음. 레버를 놓은 frame 다음에 그립 손이 문을 잡음. 넘겨받는 frame의 각 변화 0(쌓인 손 이동 미적용) |
| E5 | **놓기** — E1 도중 손바닥을 폄 | 문 잡기 끝, 각 그 자리. 관성을 켠 문(P5za)이면 놓은 속도로 조금 더 감 |
| E6 | **추적 손실** — E1 도중 그립 손 추적 끊김 3 frame → 다른 자리에서 다시 잡힘 | 끊긴 동안 각 그대로(보류), 복구 frame 각 변화 0 |
| E7 | **당겨서 잠김** — 6°에서 그립으로 당겨 닫음 | 래치 각에서 잠김(`Latches` +1), 문 잡기 끝, 그립 손은 손바닥을 폈다 다시 쥐어야 다시 잡음(레버와 같음) |
| E8 | **그립은 문 위에서 안 움직임** — E1·E2 동안 | 그립의 축 값 0, 문 경첩 기준 자리 그대로. 그립 `OnPartOpened`·`OnPartClosed` 0번 |
| E9 | **집계** — E1 뒤 | 그립 잡기 수 1(그 손), 레버 `Grabs`·`Attempts` 그대로, 측정 문구 문자열이 그립 없는 문과 같은 형식 |
| E10 | **연결 규칙** — ⓐ 문 밑에 Grip 하나 → 자동 연결 ⓑ 둘 → Check setup 문제 한 줄, 연결 안 함 ⓒ `GripAxisHandle` 칸 지정 → 칸 우선 ⓓ Grip이 안쪽 레버 후보로 섞이지 않음(안쪽 레버 하나 + Grip 하나 → 둘 다 각자 연결) | 위대로 |

- 수치는 위를 기준으로 하되, 구현자가 같은 상황을 만드는 다른 값을 쓰면 보고 ③에 적는다.

## 5. 구현자가 정할 것

- 잠금 안 푸는 잡기를 `BeginGrab` 인자로 할지 별도 함수로 할지, `GrabHandle`에 그립을 어떤 값으로 둘지(레버 0·1과 구분, 기존 0·1 쓰는 곳이 그립을 레버로 오인하지 않게).
- 그립 부품에서 축 값을 0에 묶는 방법(움직임 계산을 건너뜀 등).
- 칸·집계 필드 이름, Check setup 문구(영어).

## 6. 완료 조건

1. D1–D8 구현, `vBoR.P5ze.*` E1–E10 통과.
2. `vBoR.*` 전체 통과, 기존 시험은 고치지 않고 결과 그대로(개수 = 기존 + P5ze 시험 수).
3. non-unity(`-Module=vBoR`)·unity 빌드 성공, vBoR 경고 0.
4. 주석·표시 이름·로그는 영어(AGENTS.md).
5. 커밋 본문 보고 ①–⑤.

## 7. 로컬에서 확인할 것

- **빌드·시험:** 끝에 한 번(AGENTS.md 토큰 규칙).
- **에디터(사용자, 머지 뒤):** 부모 `AVBoRAxisPart`로 `BP_DoorGrip_FL_Int`를 만들고 `Door side` = Grip, 그립 메시(문 트림의 손잡이 부분은 문 메시 Merge에서 빼서 이 BP로), 잡는 방식(손바닥·pinch, 메시로 잡기 §4.3). Outliner에서 `BP_Door_FL` 밑에 붙인다. 문 "Check setup"으로 연결 확인. Claude가 Blueprint-Guide에 적는다.
- **Quest 3(T-03, `not-tested`):** 안쪽에서 레버로 열고 → 그립으로 바꿔 잡아 닫기, 열린 문 그립으로 밀어 더 열기, 닫힌 문에서 그립만 당기면 안 열리는지, 그립과 레버가 가까워 엉뚱한 것이 잡히지 않는지(메시로 잡기면 피벗을 각자 가운데로, §4.3).

## 개정 이력

| 날짜 | 내용 |
|---|---|
| 2026-10-28 | 처음 작성 |
