# R2c 판정 경계 표시

| 항목 | 값 |
|---|---|
| 단계 | 구조 보수 R2c(`Docs/Refactor-Plan.md` §4, 요구 6·9). 시험 계획 T-ID 없음 — 에디터 작업 도구 |
| 결정 근거 | **사용자(2026-11-01):** "작업할 때 경계 같은 걸 보면서 작업하면 좀 더 정확히 작업할 수 있으니까, 이런 것들도 볼 수 있으면 좋겠어." 앞서: 문을 "mesh로 밀기" 했을 때 충돌이 안 보임, 박스를 리프트게이트 가까이 실으면 닫히다 다시 열림. 충돌은 Blender로 만든 simple collision(UCX)을 넣는다.<br>**지금 보이지 않는 것(조사 2026-11-01, 계획 §2.5):**<br>- 손잡이 잡기가 메시 충돌을 쓰는지 GrabBox를 쓰는지는 Play 때 정해진다(`ConfigureVBoRGrabCollider` `VBoRGrabCollider.cpp:10-39`). 메시 simple collision은 에디터에서 프록시가 NoCollision으로 만들어져 Show Collision에 안 나온다.<br>- 문 "Push on mesh surface"는 **complex 추적**(`VBoRHingedPanel.cpp` `QueryMeshFace`, `bTraceComplex = true`)이라 UCX가 아니라 보이는 삼각형에 맞춘다(메시가 Use Simple Collision As Complex면 UCX).<br>- AxisPart 밀기 면은 메시 경계에서 만든 판(`PushGeometry`, R2a에서 뽑음)이다.<br>- 리프트게이트 끼임은 장애물 상자(없으면 판 메시 경계 상자)를 닫히는 각마다 돌려 겹침을 본다(`FindObstruction` `VBoRPoweredPanel.cpp`). 닫힐 때 쓸고 지나가는 자리는 어디에도 안 보인다 |
| 베이스 브랜치 | `main` (R2a 머지 뒤, `3185712` 이후) |
| 작업 브랜치 | `codex/R2c-judgment-guide` |
| 리뷰 파일 | `Docs/Reviews/R2c-judgment-guide.md` (첫 리뷰 때 생김) |
| 상태 | 명세 개정 1 |
| 개정 | **1 (2026-11-01, Claude):** Codex Q1 — R2a 시험 E7(`R2aMotionGuideTests.cpp:217` 그립 "전체 모양 0")·E11(`:323-324` `Motion guide` Off·미선택에서 전체 모양 0, Off에서 `BatchedLines` 0)이 **종류별이 아니라 전체 수**를 단언한다. 내 §3 감사가 "종류 수"만 봤다. §3에 허용 수정 한 가지를 넣음: 그 두 시험 준비에서 `Judgment guide = Off`. 제품 기본값(Always)·독립성은 그대로 |
| 에디터 작업 | **없음** |
| 앞 명세 | [R2a-motion-guide.md](R2a-motion-guide.md) — 이 명세는 R2a의 `UVBoRMotionGuideComponent`에 모양을 더한다 |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **잡기 형상 (AxisPart)** | Play 때 실제로 쓸 잡기 충돌을 에디터에서 그린다(하늘색).<br>- 메시를 쓸 조건(`bGrabWithMesh` + 메시 simple collision 있음 또는 `CTF_UseComplexAsSimple`, `VBoRGrabCollider.cpp:16-17`)이면 **메시 simple collision 요소**(박스·구·캡슐·볼록)를 선으로. `CTF_UseComplexAsSimple`이면 메시 경계 상자 + "complex" 표시 색(아래 색 정리)으로 대신해도 된다.<br>- 아니면 **GrabBox**를 그린다. `bGrabWithMesh`인데 메시 충돌이 없어 GrabBox로 떨어지는 경우는 경고 색으로.<br>- 문 그립(`IsDoorGrip()`)도 잡기는 하므로 그린다(R2a 방향 표시는 없음) |
| D2 | **밀기 면 (HingedPanel)** | 손바닥 밀기 판정에 실제로 쓰는 면을 그린다.<br>- **상자 모드**(지금 `SyncSettings`가 쓰는 면 — 메시 경계 또는 `PushFaceBox` `VBoRHingedPanel.cpp:63-73`): 바깥 면(face 0)·안쪽 면(face 1) 사각형.<br>- **메시 면 모드**(`bPushOnMeshSurface` 그리고 추적 가능): 면 범위 안 격자 점마다 `QueryMeshFace`로 실제 맞는 점을 구해 **두 면 각각 격자망**으로 그린다. 판정과 같은 함수를 부르므로 그려진 망 = 실제로 손이 닿는 면이다. 맞지 않는 점은 빼고, 하나도 안 맞으면 상자 모드 면을 경고 색으로.<br>- `bAllowPalmOpening = false`로 꺼진 면은 흐리게(또는 생략 — 구현자) |
| D3 | **밀기 판 (AxisPart)** | `bAllowPush`이고 Rotate일 때 `PushGeometry`의 판(`Bounds`를 `Radial`·`Normal`·축 좌표로)을 상자 선으로 그린다. 밀기가 꺼져 있으면 없음 |
| D4 | **리프트게이트 쓸림 영역** | 판정 형상을 **닫힘 → 열림 각까지 일정 간격(10° 이하, 끝 각 포함)**으로 돌린 상자 선들을 그린다.<br>- 장애물 상자가 하나라도 유효하면 그 상자들(주황), 아니면 판 메시 경계 상자(경고 색 — "근사"임을 보이기).<br>- 상자 자세는 `FindObstruction`이 쓰는 것과 **같은 함수**로 낸다(지금 `FindObstruction` 안의 자세 계산을 함수로 뽑아 둘 다 부름). 여유(margin)는 그리지 않는다.<br>- 장애물 상자 자체(UBoxComponent)는 지금처럼 엔진이 그린다 — 이 명세는 각마다 돌린 모습만 더한다 |
| D5 | **켜고 끄기** | R2a의 `Motion guide`와 따로 `Judgment guide`(Always / Selected only / Off, 기본 Always). R2a와 같은 방식 |
| D6 | **판정 함수를 표시 때문에 바꾸지 않는다** | 표시와 판정이 같은 함수를 쓰도록 뽑아내는 것(R2a처럼)은 된다. 뽑아낸 함수는 **로그를 남기지 않는다** — 지금 경고(`grab mesh has no simple collision`, `grab mesh pivot is at the actor origin`)는 지금처럼 `ConfigureVBoRGrabCollider`를 부를 때만 같은 횟수로 나온다 |
| D7 | **게임에서는 없음** | R2a와 같다 — 게임 월드에서는 모양을 만들지 않고, 판정·충돌 설정·이벤트를 바꾸지 않는다 |

**색 정리:** 잡기 = 하늘색, 밀기 면·판 = 파랑, 쓸림 영역(장애물 상자) = 주황, **경고·근사**(GrabBox로 떨어짐, 메시 면 추적 실패, 장애물 상자 없음) = 자홍. R2a 색(초록·빨강·노랑·흰색)과 겹치지 않게.

## 1. 왜 하는가

- 잡기·밀기·끼임이 **실제로 어떤 형상으로** 판정되는지 Play 없이 본다. 지금은 Show Collision이 실제 판정과 다르게 보여(프록시 캐시, complex/simple 차이) 잘못 맞추기 쉽다.
- 리프트게이트가 "조금만 가까이 실어도 다시 열리는" 원인이 판정 상자 크기인지 박스 위치인지를 뷰포트에서 바로 판단한다(사용자 2026-11-01). 끼임 동작 자체는 그대로 둔다(Q7).

## 2. 범위

### 만들 것

- R2a 표시 컴포넌트에 D1–D4 모양, `Judgment guide`(D5).
- 판정 자세·조건을 표시와 같이 쓰도록 뽑은 함수(D6) — 잡기 조건, 리프트게이트 각별 판정 상자 자세.
- 시험 `vBoR.R2c.*`.

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| 적재 영역(LoadTarget)·쓸림 영역 겹침 경고 | 적재 영역 상자는 `GoalSizeCm`(기본 높이 120 cm, `VBoRLoadTarget.h:22`·`VBoRLoadTarget.cpp:20`) 크기라 닫힌 리프트게이트와 겹치기 쉽다 — 경고가 잡음이 된다. 눈으로 본다. 적재 영역 상자는 지금처럼 엔진이 그린다 |
| 끼임 동작 변경(닫기 전 막기, 진행자 표시 등) | 사용자 Q7 — 지금 그대로 |
| 노브·휠·룸미러 잡기 형상 | R2a와 같은 대상 범위. 필요하면 따로 |
| complex 삼각형 전체를 선으로 | CAD 메시는 삼각형 수십만 개. D2 격자 추적으로 대신한다 |
| GrabBox·PushFaceBox·장애물 상자의 기존 표시 속성(`ShapeColor`·숨김) 변경 | 기존 시험·BP가 기대하는 값 |
| 손잡이 근처 밀기 금지 범위 | R7 |
| `L_Main`·근사 차·자식 BP 수정 | 에디터 작업 없음 |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- 판정 결과·충돌 설정·이벤트·로그(D6)는 그대로. 표시는 읽기만 한다.
- R2a의 표시(`Motion guide`)·`Preview`·D8(저장 값 보존)은 그대로. `Preview` 중에는 판정 형상도 보이는 자세를 따라간다(움직이는 부품에 붙은 것).
- **기존 시험 감사(Claude 2026-11-01).**
  - `P2Probe/P2fMeshGrabTests.cpp` 전체(`E1`–`E10`) — 특히 경고 횟수 단언 `:210`(`Contains`, 4번), `:252`, `:274-277`. 표시가 잡기 조건을 계산해도 **경고가 늘면 안 된다**(D6).
  - `P1Probe/P4bPoweredPanelTests.cpp`, `P1Probe/P5zgObstructionBoxTests.cpp`, `P1Probe/P5rSoundCueTests.cpp`, `P3Probe/P3FlowTests.cpp`의 끼임 관련 단언 — `FindObstruction` 자세 계산을 함수로 뽑아도 결과(멈춤 각 ≤ 1.5° 차, 겹침 대상) 그대로.
  - `P3Probe/R2aMotionGuideTests.cpp` — R2a 모양 수·종류 단언이 있다. R2c 모양은 **새 종류**로 넣어 기존 종류(`Axis`·`Arc`·`Rest`·`Travel`·`Detent`·`Push`) 수가 바뀌지 않게 한다. `Judgment guide = Off`면 R2a 모양만 남는다.
  - **고쳐도 되는 것(개정 1, 유일한 허용):** 같은 파일의 `E7GripAndMissingMesh`(`:216-217` 그립 전체 모양 0)과 `E11DisplayFlags`(`:323-324` 전체 모양·`BatchedLines` 0)는 **R2a 표시만** 시험하려는 것이다. 그 시험 준비에서 부품의 `Judgment guide = Off`로 두어 원래 의도를 지킨다. 단언 자체는 바꾸지 않는다. 그립 잡기 형상(D1)과 `Judgment guide`·`Motion guide`의 독립성은 R2c 시험(E1·E12)에서 확인한다.
  - `P5xPanelHandlesTests.cpp:84-88`(HingedPanel CDO의 `UBoxComponent` 1개) — 새 컴포넌트를 더하지 않는다(R2a 컴포넌트에 모양만).
  - **그 밖에 고쳐도 되는 단언: 없음.** 기존 시험을 더 고쳐야 통과하면 멈추고 리뷰 파일 "질문"에 적는다.

## 4. 시험 — `vBoR.R2c.*`

R2a처럼 모양 목록을 만드는 순수 함수와 실제 판정 함수를 비교한다.

| ID | 상황 | 확인 |
|---|---|---|
| E1 | AxisPart `bGrabWithMesh = false`, 그리고 같은 설정의 문 그립(`DoorHandleSide = Grip`) | 둘 다 잡기 모양 = GrabBox(월드 자세·크기 0.1 cm), 경고 색 아님. 그립은 R2a 모양 0개 |
| E2 | `bGrabWithMesh = true`, 메시 simple collision 있음(박스 요소 1개 이상인 시험 메시) | 잡기 모양 = 메시 simple 요소(위치·크기가 요소 월드 자세와 0.1 cm), GrabBox 모양 없음 |
| E3 | `bGrabWithMesh = true`, simple collision 없음 | GrabBox를 경고 색으로. **경고 로그는 지금과 같은 횟수**(표시 때문에 늘지 않음 — `AddExpectedMessage` 횟수 또는 로그 수집으로) |
| E4 | `CTF_UseComplexAsSimple` 메시 | 메시 쪽(D1 대체 표시 포함), GrabBox 없음 |
| E5 | 잡기 판단 일치: E1–E4 각각에서 Play 시작 후 `Grabbable`의 잡기 충돌 | E1·E3 → GrabBox, E2·E4 → 메시. 표시가 고른 쪽과 같다 |
| E6 | HingedPanel 상자 모드(메시 경계 / `PushFaceBox` 사용) | 두 면 사각형이 `Motion.FaceMinCm`·`FaceMaxCm`의 바깥(Max.Y)·안쪽(Min.Y) 면과 같다 |
| E7 | HingedPanel 메시 면 모드, 휜 시험 메시(예: 원통 일부 또는 기울인 판) | 격자 점 각각이 같은 점에서 `QueryMeshFace` 결과와 같다(0.1 cm). 점 수 > 0. 메시 밖으로 나간 점은 없음 |
| E8 | 메시 면 모드인데 추적이 하나도 안 맞음(충돌 없는 메시) | 상자 모드 면을 경고 색으로, 오류 없음 |
| E9 | AxisPart 밀기 판(`bAllowPush`, Rotate, `Axis` X·Y·Z) | 상자 꼭짓점 = `PushGeometry` 판의 꼭짓점을 피벗 좌표에서 월드로 바꾼 것. `bAllowPush = false`나 Translate면 없음 |
| E10 | 리프트게이트 장애물 상자 2개, `OpenAngleDeg` 85 | 각 단계(≤ 10°, 0과 85 포함)마다 상자 2개. 각 상자 자세 = 같은 각에서 `FindObstruction`이 쓰는 자세(여유 제외, 0.1 cm) |
| E11 | 장애물 상자 없음 | 판 메시 경계 상자를 경고 색으로, 각마다 1개. 자세 = 메시 경계 경로의 자세 |
| E12 | `Judgment guide` Off / Selected only(선택 안 됨) / 게임 월드. 그리고 `Motion guide = Off` + `Judgment guide = Always` | 앞의 셋: R2c 모양 0개, R2a 모양은 `Motion guide` 설정대로. 마지막: R2a 모양 0개, R2c 모양은 있음(서로 독립) |
| E13 | `Preview` 1(R2a) | 움직이는 부품의 잡기·밀기 모양이 미리보기 자세를 따른다. 리프트게이트 쓸림 영역은 `Preview`와 상관없이 같다(전 구간을 그림) |

## 5. 구현자가 정할 것

- 볼록 요소를 그리는 방법(요소의 꼭짓점·면 데이터 `FKConvexElem`, 엔진 헤더 경로를 보고 ④에 적기). 박스·구·캡슐은 선 몇 개로 근사.
- D2 격자 크기(예: 8 × 8)와 다시 계산하는 때(값이 바뀔 때만 등 — R2a는 매 Tick 다시 만든다. 메시 면 추적이 무거우면 바뀐 경우만).
- D4 각 간격(10° 이하), 판정 상자 자세 함수의 형태.
- 꺼진 면(D2)을 흐리게 할지 생략할지.
- `Judgment guide` 프로퍼티를 각 부품에 둘지 R2a처럼 부품마다 따로 둘지(R2a와 같은 방식 권장).

## 6. 완료 조건

1. D1–D7 구현, §3 조건 지킴.
2. `vBoR.R2c.E1`–`E13` 통과, 기존 시험 변경 없이 통과.
3. non-unity(`-Module=vBoR`)·unity 빌드 성공, `vBoR.*` 전체 통과(작업 끝에 한 번).
4. 새 주석·표시 이름 영어(AGENTS.md). Category는 R2a와 같은 `|Guide`.

## 7. 로컬에서 확인할 것

- 빌드: 위 3.
- 에디터(사용자):
  ① 손잡이 `Grab with mesh collision`을 켜고 끄면 하늘색 형상이 UCX ↔ GrabBox로 바뀐다. UCX 없는 메시면 자홍색 GrabBox.
  ② 문 `Push on mesh surface`를 켜면 문 면을 따라 파란 격자망이 보인다. UCX로 밀게 하려면 메시 Collision Complexity = Use Simple Collision As Complex — 그러면 망이 UCX 모양으로 바뀐다.
  ③ 리프트게이트에 주황 상자들이 닫힘~열림을 따라 보인다. 트렁크에 박스를 놓고 그 상자들과 겹치는지 본다. 자홍이면 장애물 상자가 없는 것(경계 상자 근사).
  ④ 회사 차: 박스가 "조금만 가까이 실으면 다시 열리던" 자리가 주황 상자 안인지.
- Quest 3: 없음.
