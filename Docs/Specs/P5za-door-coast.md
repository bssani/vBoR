# P5za 문 관성 — 밀거나 놓은 뒤 조금 더 움직이게 (문만, 켜고 끄기)

| 항목 | 값 |
|---|---|
| 단계 | P5 자유모드·리허설 준비(사용자 요청 2026-10-25). 시험 T-03(문 손 조작) 보강 |
| 결정 근거 | **사용자(2026-10-25):** "차 문을 열 때, 살짝의 physics 느낌이 나게 할 수 있나? 지금은 문 밀면 미는 만큼만 움직이잖아. 근데 차 문 같은 경우에는 살짝 밀면 민거 보다 좀 더 밀리잖아. 모든 파츠 그럴 필요 없지만 문만"<br>**Claude 제안 → 사용자 답:** "Property로 켜고 끌 수 있게 해보자. 다른 파츠들보다는 door에만 집중해보자." |
| 베이스 브랜치 | `main` @ 이 명세 머지 뒤 |
| 작업 브랜치 | `codex/P5za-door-coast` |
| 리뷰 파일 | `Docs/Reviews/P5za-door-coast.md` (첫 리뷰 때 생김) |
| 상태 | 명세 — 구현 전 |
| 에디터 작업 | **없음** (켜는 것은 사용자가 `BP_Door_FL`에서 직접) |
| 앞 명세 | [P5z-fixed-guidance-panel.md](P5z-fixed-guidance-panel.md) |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **문에만 켜는 칸, 기본 꺼짐** | `AVBoRHingedDoor`(`Interaction/VBoRHingedDoor.h`)에 프로퍼티 둘을 둔다. 이 문서에서 부르는 이름이고, 실제 이름은 구현자가 정한다.<br>- `bCoastAfterRelease`(bool, **기본 false**). 표시 이름 예: `Coast after release`. 툴팁에 근사값임을 적는다(`Approximate feel, not real door force`).<br>- `CoastDecelerationDegPerSecond2`(float, 단위 `deg/s²`, `ClampMin` 0보다 큼, **기본 180**). 표시 이름 예: `Coast deceleration`.<br>- Category는 `Door|Coast`.<br>- **`AVBoRHingedPanel`에는 두지 않는다.** 콘솔 뚜껑·글러브박스(`BP_ConsoleLid`·`BP_Glovebox`는 `AVBoRHingedPanel` 자식, 에셋 확인)와 리프트게이트(`AVBoRPoweredPanel`)는 디테일 패널에 이 칸이 없고 동작도 그대로다.<br>- 값은 매 Tick `FVBoRDoorMotion`으로 옮긴다. 지금 판 설정은 `AVBoRHingedPanel::SyncSettings()`(`VBoRHingedPanel.cpp:51`)가 옮기고, `Motion`은 판의 private 멤버(`VBoRHingedPanel.h:128`)다. 문 자식이 값을 넣는 방법(가상 함수, protected 설정 함수 등)은 구현자가 정한다 |
| D2 | **문 속도 상태 하나 + 일정 감속** | `FVBoRDoorMotion`(`Interaction/VBoRDoorMotion.h`)에 `bCoast`(기본 false), 감속값, 문 각속도 상태 `CoastSpeed`(deg/s, 부호 있음 — +는 열림)를 둔다.<br>- **관성 이동:** 손이 몰지 않는 frame에 `CoastSpeed`로 움직이고 일정 감속으로 줄인다. 한 frame 이동은 해석식으로 낸다: 감속 `a`, dt에서 속도가 0에 닿으면 거기서 멈춘다(`Δ = v·t − ½·a·t²·sign(v)`, `t = min(dt, |v|/a)`). 그래서 frame 속도와 상관없이 같은 거리를 간다(E12).<br>- 열림 각속도 v에서 멈출 때까지 간 거리는 `v²/(2a)`다. 기본 180 deg/s²면 60 deg/s로 놓을 때 약 10° 더 간다. **임시 값이다**(Quest 3에서 사용자가 조절).<br>- 이동은 지금의 `MoveTo`(`VBoRDoorMotion.cpp:160-176`)를 거친다. 그래야 각도 범위 고정, 속도 상한(`MaxSpeedDegPerSecond`), 잠김(latch)이 같은 규칙으로 걸린다. 관성 이동은 `bPushing=false`로 넘긴다(D8) |
| D3 | **몰고 있을 때의 속도 = 문 각도의 짧은 시간 평균** | 손이 문을 실제로 움직인 frame(잡기 `:214`, 받아들여진 밀기 `:252`)마다 `CoastSpeed`를 문 각도 변화로 갱신한다.<br>- **frame 하나의 차이를 그대로 쓰지 않는다.** 손 떨림 한 frame이 큰 속도가 된다(90 Hz에서 1 cm 떨림 ≈ 65 deg/s). 약 **0.1 s 시간 창**의 평균(또는 같은 시간 상수의 지수 평활)을 쓴다. frame 수가 아니라 시간으로 정한다(E12·E13).<br>- 각도 변화는 `MoveTo` 뒤의 실제 각도로 잰다(속도 상한·범위가 이미 걸린 값). 그래서 `|CoastSpeed| ≤ MaxSpeedDegPerSecond`이다 |
| D4 | **밀기: 손은 밀 수만 있고 붙잡지 못한다** | 밀기가 받아들여진 frame에서, 손이 낸 목표 각(`FindPush`의 `OutAngle`)과 관성으로 갈 각을 비교해 **미는 방향으로 더 나간 쪽**을 고른다. 면 1(안쪽, 열림)은 큰 쪽, 면 0(바깥, 닫힘)은 작은 쪽.<br>- 손이 빨라지면 손이 이기고 속도가 갱신된다(D3).<br>- 관성 쪽이 이긴 frame은 밀기가 받아들여졌어도 관성 frame으로 친다: 속도는 감속으로만 줄고(다시 재지 않음), 이동은 `PushedDeg`에 넣지 않는다(D8).<br>- 손이 느려지거나 멈추면 문이 관성으로 손보다 앞서 나가고, 손이 면에서 떨어져 밀기가 자연히 끝난다. 그 뒤로는 관성 이동만 한다. 손을 "떼는" 판정을 따로 두지 않는다 — 열린 손바닥으로는 문을 붙잡을 수 없으니 실제와 같다.<br>- 손이 다시 따라붙으면 지금 규칙대로 새 밀기로 받아들여진다(`bFromFront`). `PushStarts`가 지금보다 늘 수 있다(D8).<br>- **반대 방향 밀기:** 문이 관성으로 움직이는 방향과 반대 면의 밀기가 받아들여지면 `CoastSpeed`를 그 frame에 0으로 두고 손 목표 각으로 간다(손이 문을 막음). 그 뒤부터는 D3대로 새 속도를 잰다. 시간 창 안에 남은 반대 방향 값이 섞이지 않게 창을 비운다 |
| D5 | **잡기: 잡는 동안 관성 없음, 놓으면 관성** | - `BeginGrab`(`:40`)에서 `CoastSpeed = 0`, 시간 창을 비운다. 관성으로 움직이던 문을 잡으면 그 자리에서 손을 따른다.<br>- 잡는 동안은 지금처럼 손이 각도를 정한다(`:207-217`). 관성 이동을 섞지 않는다. 속도는 D3대로 잰다.<br>- `ReleaseGrab(true)`(`:62-72`)로 놓이면 잰 속도를 그대로 두고 다음 `Advance`부터 관성으로 움직인다. 판 Actor에서 놓기는 `Advance` 뒤에 불린다(`VBoRHingedPanel.cpp` Tick의 손바닥 열림 처리, `EndAxisHold` 경로). 그래서 놓은 frame의 다음 frame부터 움직이면 된다 |
| D6 | **추적 손실은 관성을 주지 않는다** (AGENTS.md "손 추적 손실은 보류", 키 규칙 U-10과 같은 이유) | - **미는 손이 추적을 잃은 frame**(`:199-205`): `CoastSpeed = 0`. 문은 그 자리에 선다. 이미 손이 떨어져 관성으로 가던 중이면(미는 손 없음) 손 추적 손실은 관성과 상관없다 — 계속 간다.<br>- **잡은 손이 추적을 잃음 → 보류**(`:184-190`): `CoastSpeed = 0`, 시간 창 비움. `ReleaseGrab(false)`(무시된 놓기)도 같다.<br>- **보류에서 복구**(`:192-198`, rebase): 속도는 0에서 다시 잰다. 복구 순간의 손 이동(`RecoveryDistanceCm`)이 속도에 들어가지 않는다. 지금도 복구 frame에는 각도를 바꾸지 않으므로(`:211`의 `!bRecovering`), 그 frame을 시간 창에 넣지 않으면 된다 |
| D7 | **끝에서 멈춤 — 튕김 없음** | - 관성으로 `MaxAngleDeg`에 닿으면 거기서 `CoastSpeed = 0`.<br>- 닫히는 쪽으로 가다 0°에 닿으면 `CoastSpeed = 0`. `bUseLatch`이고 `bLatchArmed`면 지금 `MoveTo`의 규칙(`:169-174`)으로 잠긴다 — 툭 밀어 닫는 동작이다. 잠기면 `CoastSpeed = 0`.<br>- `Reset()`(`:262`)은 `CoastSpeed = 0`, 시간 창을 비운다. 바깥에서 `AngleDeg`를 직접 바꾸는 코드(시험·진행자 초기화)는 속도를 건드리지 않으므로, 그런 곳이 `Reset()`을 거치지 않으면 남은 속도로 움직일 수 있다. 구현자는 `AngleDeg`를 직접 쓰는 제품 코드를 찾아 보고 ④에 적는다(시험 코드는 그대로 둠) |
| D8 | **새 집계·새 문구 없음** | - 관성 이동은 `Metrics.PushedDeg`에 더하지 않는다(민 거리만 센다).<br>- `FVBoRDoorMetrics`, `EVBoRDoorIgnore`, 측정 문구(`GetMeasurementText`), 진행자 문구, 상태 색(`ApplyState`의 색 규칙)에 새 칸을 만들지 않는다. 관성으로 움직이는 동안은 "밀기·잡기 아님" 상태 색 그대로다.<br>- 관성으로 움직여도 `OnAngleChanged`, 잠기면 `OnLocked`·`LatchedCue`가 지금 규칙대로 난다(`ApplyState`가 각도·잠김 변화를 보므로 따로 할 일 없음).<br>- 관전 화면의 "조작 중"(`IsSpectatorInteracting`, `VBoRHingedPanel.h:38`)은 그대로 — 관성 이동은 조작이 아니다 |
| D9 | **꺼짐이면 지금과 완전히 같다** | `bCoast == false`면 `Advance`·`MoveTo`·`BeginGrab`·`ReleaseGrab`·`Reset`의 결과(각도, 잠김, 집계, 손별 상태)가 지금 코드와 같다. 속도를 재는 코드가 돌더라도 결과에 영향을 주지 않는다. 그래서 기존 시험이 모두 그대로 통과해야 한다(§3) |

## 1. 왜 하는가

- 지금 문은 손이 낸 목표 각으로만 간다(`MoveTo`). 문에 속도 상태가 없어서 손이 떨어지면 그 frame에 멈춘다. 실제 문은 툭 밀면 민 것보다 조금 더 간다.
- 사용자는 이것을 문에만, 켜고 끌 수 있게 원한다. 그래서 기본은 꺼짐이고, 꺼짐일 때 평가 단계와 기존 시험은 바뀌지 않는다(D9).
- **평가와의 관계:** 켜면 문 단계의 `DoorOpened`(`VBoRScenarioRunner.cpp:332`, `AngleDeg >= DoorOpenDeg`)가 관성으로 채워질 수 있다. 이 명세는 판정 규칙을 바꾸지 않는다. 평가 때 켤지는 Quest 3에서 느껴 본 뒤 사용자가 정한다. 관성의 세기는 근사값이므로 실차 문의 조작감·조작력 평가로 쓰지 않는다(AGENTS.md "하지 말 것").

## 2. 범위

### 만들 것

- `FVBoRDoorMotion`: D2–D7, D9.
- `AVBoRHingedDoor`: D1(프로퍼티 둘, 값 옮기기).
- 시험 `vBoR.P5za.*`(§4).

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| 콘솔 뚜껑·글러브박스·리프트게이트·레버·룸미러 등 다른 부품의 관성 | 사용자: 문만 |
| `AVBoRHingedPanel`에 칸 추가 | D1 — 다른 판에 보이지 않게 |
| 끝 각도에서 튕김, 스프링·체크 링크(중간 멈춤 위치), 자동 닫힘, 바람 | 사용자 요청은 "조금 더 밀림"까지. 필요하면 따로 명세 |
| 물리 엔진(Chaos 강체·constraint)으로 문 돌리기 | 지금 구조(각도 하나)로 충분하다. 손 추적 손실·잠김 규칙을 다시 만들어야 한다 |
| 문 단계 완료 판정(`DoorOpened` 등) 변경 | §1 — 사용자가 Quest 3 뒤 정함 |
| 측정·진행자 문구에 관성 칸 | D8 |
| `BP_Door_FL`·`L_Main`에서 켜기 | 에디터 작업 없음. 사용자가 직접 켠다(§7) |
| 보이는 손을 문 표면에 멈추기 | P5w와 같은 이유(범위 밖) |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- **꺼짐(기본)에서 기존 동작 전부**(D9): 밀기 면 선택·crossing·계속 밀기(P2 E8b), P5w 민 면 기억, 잡기가 밀기보다 먼저(P2 E16), 추적 손실 보류·복구 rebase(P2 E14), 다른 손 즉시 밀기(P2 E15b), 잠김·rearm(P2d D3), 무시 집계, 메시 표면 밀기(P2g).
- **켜짐에서도 그대로인 것:** 잠긴 문은 밀어서 움직이지 않는다. 손잡이·버튼 풀기, 잠김 규칙(`LatchAngleDeg`·`RearmAngleDeg`), 각도 범위 0–`MaxAngleDeg`, 속도 상한, 추적 손실 규칙(D6), P5w 민 면 기억.
- **문 아닌 판:** `AVBoRHingedPanel`을 바로 쓰는 판(콘솔 뚜껑·글러브박스, 시험의 `AP2cTrackedPanel`)과 `AVBoRPoweredPanel`은 칸도 동작도 그대로다.
- **기존 시험 감사:** 기본이 꺼짐이고 D9를 지키면 기존 시험은 바뀌지 않는다. `FVBoRDoorMotion`을 쓰는 시험 파일은 `P2DoorMotionTests`, `P2bAxisPartTests`, `P2cHingedPanelTests`, `P2dLatchedPanelTests`, `P2gMeshPushTests`, `P5wDoorPushSideTests`, `P3FlowTests`, `P5aGloveboxTests`, `P5gFreeModeTests`이고, `AVBoRHingedDoor`를 쓰는 시험은 `P2DoorWorldTests`, `P4iCarBlueprintTests`, `P5qPowerMirrorTests`, `P5xPanelHandlesTests`가 더 있다. **이 파일들은 고치지 않는다.** 하나라도 고쳐야 통과하면 D9가 깨진 것이므로 멈추고 리뷰 파일 "질문"에 적는다.

## 4. 시험 — `vBoR.P5za.*`

`FVBoRDoorMotion`만 쓰는 시험은 `P2Probe/P2DoorMotionTests.cpp`·`P5wDoorPushSideTests.cpp`의 `DoorHand`·`DoorPoint` 도우미와 같은 방식으로 만든다(새 파일에 같은 도우미). 기본값은 `FVBoRDoorMotion` 기본(면 Y −5/+5, 두께 10 cm, 최대 70°, 180 deg/s)이고, 따로 적지 않으면 `bUseLatch=false`, `bLocked=false`, `bCoast=true`, 감속 180 deg/s², dt 1/90 s다.

"안쪽 면으로 v deg/s로 민다"는 손 (80,−8,50) 손바닥 +Y에서 시작해, 매 frame 손을 `DoorPoint(각 + v·dt + 작은 앞섬, (80,0,50))`(손바닥은 같은 회전의 +Y)로 두는 것을 말한다. 앞섬 값은 구현자가 정하고 보고 ③에 적는다.

| ID | 상황 | 확인 |
|---|---|---|
| E1 | **꺼짐은 지금과 같음** — `bCoast=false`인 모션과, 같은 입력을 받는 비교용 모션(새 필드를 건드리지 않은 기본 상태)에 E2와 같은 순서를 준다 | 매 frame 각도·`PushHand`·`PushStarts`·`PushedDeg`가 같다. 손을 뗀 뒤 각도가 그 frame 그대로 |
| E2 | **툭 밀기** — 20°에서 안쪽 면으로 60 deg/s로 0.5 s 민 뒤, 손을 그 자리에 멈춘다(손바닥 그대로) | 손을 멈춘 뒤에도 문이 더 열린다. 멈춘 뒤 0.5 s 안에 `PushHand` −1. 최종 각 − 멈춘 순간 각 ≈ 60²/(2·180) = 10° (±1.5°). 그 뒤 2 s 동안 각 변화 없음 |
| E3 | **빠를수록 더 감** — E2를 30·90 deg/s로 | 더 간 거리가 30 < 60 < 90 순서, 각각 v²/(2a) ±1.5° |
| E4 | **감속값** — E2를 감속 360으로 | 더 간 거리 ≈ 5° (±1.5°) |
| E5 | **끝 각도에서 멈춤** — 55°에서 90 deg/s로 민 뒤 손 멈춤 | 각 = 70에서 멈춤, 그 뒤 움직임 없음. 70을 넘거나 되돌아오지 않음(튕김 없음) |
| E6 | **툭 닫아 잠금** — `bUseLatch=true`, 풀린 문 30°(먼저 5° 넘게 열어 `bLatchArmed`)에서 바깥 면으로 −90 deg/s로 0.2 s 민 뒤 손 멈춤 | 손이 멈춘 뒤 문이 관성으로 닫혀 `bLocked` true, `Latches` +1, 각 0, 그 뒤 움직임 없음 |
| E7 | **잠금 없는 판은 0에서 멈춤** — `bUseLatch=false`로 E6 | 각 0에서 멈춤, 음수 없음, `Latches` 그대로 |
| E8 | **미는 손 추적 손실** — E2처럼 밀다가 미는 도중 손 추적 끊김 | 끊긴 frame부터 각이 그대로(관성 없음). 다시 추적돼도 움직이지 않음 |
| E9 | **관성 중 손 추적 손실은 상관없음** — E2에서 손이 떨어져 관성으로 가는 중 손 추적을 끊음 | 관성이 계속돼 E2와 같은 최종 각 |
| E10 | **잡았다 놓기** — 20°에서 `BeginGrab`, 손을 0.5 s 동안 60 deg/s로 열리게 옮긴 뒤 `ReleaseGrab(true)` | 놓은 뒤 ≈10° 더 열리고 멈춤 |
| E11 | **추적 손실 놓기·복구** — E10에서 놓기 직전 손 추적 끊김 → `ReleaseGrab(false)` → 다시 추적된 손이 15 cm 옮겨진 자리에 나타남 → 그 뒤 손 멈춤 | 끊긴 동안 각 그대로, 복구 frame 각 그대로, 복구 뒤 손이 멈춰 있으면 각 그대로(복구 이동이 속도로 들어가지 않음) |
| E12 | **frame 속도** — E2·E6을 dt 1/15·1/45·1/90 s로(밀기 시간은 같은 초) | 세 경우 최종 각 차이 ≤ 1.5° |
| E13 | **손 떨림** — 30°에서 안쪽 면에 손을 대고(밀기 받아들여짐) 1 s 동안 손을 면 수직으로 ±0.3 cm 번갈아 흔든 뒤(90 Hz), 손을 면에서 −20 cm로 뺀다 | 뺀 뒤 더 간 거리 ≤ 1° |
| E14 | **관성 중 반대 면이 막음** — E2에서 문이 관성으로 열리는 중, 손을 바깥 면 앞 `DoorPoint(각,(80,8,50))` 손바닥 −Y에 두고 다음 frame 문 좌표 (80,−2,50)으로 | 밀기가 받아들여진 frame 뒤 각이 더 커지지 않음. 손을 멈추면 문도 멈춤(남은 열림 속도 없음) |
| E15 | **관성 중 다시 잡기** — E2에서 관성 중 `BeginGrab`, 손 멈춤 0.5 s | 잡은 뒤 각 그대로 |
| E16 | **Reset** — E2에서 관성 중 `Reset()`, `bLocked=false`, `AngleDeg=30`, 손 없음 1 s | 각 30 그대로 |
| E17 | **민 거리 집계** — E2 | `PushedDeg` ≈ 민 동안의 각 변화(관성으로 더 간 거리 포함 안 함) |
| E18 | **Actor: 문만 칸이 있음** — `AVBoRHingedDoor` 클래스에 D1의 두 프로퍼티가 있고 기본값 false·180. `AVBoRHingedPanel`·`AVBoRPoweredPanel` 클래스에는 그 이름의 프로퍼티가 없음(리플렉션으로 확인) | 위대로 |
| E19 | **Actor: 값 옮기기** — 시험 문(`P2bTestFixture`의 `AP2bTrackedDoor` 또는 같은 방식)을 띄워 `bCoastAfterRelease=true`, 감속 360으로 바꾼 뒤 한 Tick | `GetMotion()`의 관성 켜짐·감속 360. 다시 false로 바꾸고 Tick하면 꺼짐 |

- 수치(위치·속도·허용 오차)는 위 표를 기준으로 하되, 구현자가 같은 상황을 만드는 다른 값을 쓰면 보고 ③에 적는다. 허용 오차는 시간 창 평균 때문에 생기는 차이를 감안한 것이다. 넘으면 오차를 넓히지 말고 원인을 보고한다.

## 5. 구현자가 정할 것

- 필드·프로퍼티·함수 이름, 표시 이름 문구(영어, D1 예시 참고).
- 시간 창 방식: 고리 버퍼 평균 또는 지수 평활(시간 상수 약 0.1 s). 정확한 창 길이(0.08–0.15 s 안).
- 문 자식이 `Motion`에 값을 넣는 방법(D1).
- E18·E19를 한 시험으로 할지 나눌지.

## 6. 완료 조건

1. D1–D9 구현, `vBoR.P5za.*` E1–E19 통과.
2. `vBoR.*` 전체 통과, 기존 시험은 고치지 않고 결과 그대로(개수 = 기존 + P5za 시험 수).
3. non-unity(`-Module=vBoR`)·unity 빌드 성공, vBoR 경고 0.
4. 주석은 영어 짧은 조각문(AGENTS.md). 근사값임을 감속 프로퍼티 근처에 적는다.
5. 커밋 본문 보고 ①–⑤. ④에 D7의 `AngleDeg` 직접 쓰기 조사 결과.

## 7. 로컬에서 확인할 것

- **빌드·시험:** 끝에 한 번(AGENTS.md 토큰 규칙).
- **에디터:** 없음. 머지 뒤 사용자가 `BP_Door_FL`(또는 `L_Main`의 문 인스턴스)에서 `Coast after release`를 켜고 감속을 조절한다. Claude가 Blueprint-Guide에 칸 설명을 더한다.
- **Quest 3(T-03, `not-tested`):** 켠 상태에서 ① 안쪽 면을 툭 밀기 — 조금 더 열리는지, 너무 많이/적게 가는지(감속값 조절) ② 끝까지 세게 밀기 — 70°에서 자연스럽게 서는지 ③ 바깥에서 툭 밀어 닫기 — 잠기는지 ④ 관성으로 가는 문을 손으로 막거나 손잡이로 잡기 ⑤ 밀다가 손을 시야 밖으로 빼기(추적 손실) — 문이 서는지 ⑥ 가만히 손을 댄 채 있다 빼기 — 떨림으로 움직이지 않는지. 콘솔 뚜껑·글러브박스가 그대로인지도 본다. 평가 단계에서 켤지는 이 확인 뒤 사용자가 정한다(§1).

## 개정 이력

| 날짜 | 내용 |
|---|---|
| 2026-10-25 | 처음 작성 |
