# R2a 움직임 방향 표시 + 미리보기

| 항목 | 값 |
|---|---|
| 단계 | 구조 보수 R2a(`Docs/Refactor-Plan.md` §4, 요구 5). 시험 계획 T-ID 없음 — 에디터 작업 도구(참가자 화면에 나오지 않음) |
| 결정 근거 | **사용자(2026-11-01):** "interaction 하면 방향을 설정하잖아. 예를 들어 문을 밀 때, console box를 열 때 등등... scene graph에서 어느 방향으로 밀리거나 도는지 화살표나 이런 걸로 표시하면 좋겠어." "x, y, z로 그리고 reverse로 선택하는 것도 있지만, glove box, door 이런 것들은 pivot을 맞춰야 하잖아."<br>**지금:** 방향은 부품마다 규칙이 다르다 — 문·콘솔 `HingePivot` +Z 축·+X 끝 쪽·+Y 바깥(`VBoRDoorMotion.h:5`), 리프트게이트·등받이 +Y 축(`VBoRPoweredPanel.cpp:139`, `VBoRFoldingSeat.cpp:53`), 미러 `HingeRotation` 뒤 +Z(`VBoRFoldingMirror.cpp:63`), AxisPart `Axis`(X/Y/Z) + `bReverse` + `Movement`(`VBoRAxisPart.cpp:290-294`). 맞았는지는 Play해서 손으로 밀어 봐야 안다 |
| 베이스 브랜치 | `main` (R0 머지 뒤) |
| 작업 브랜치 | `codex/R2a-motion-guide` |
| 리뷰 파일 | `Docs/Reviews/R2a-motion-guide.md` (첫 리뷰 때 생김) |
| 상태 | 명세 작성 |
| 에디터 작업 | **없음**(C++ 부모에 컴포넌트를 더하므로 자식 BP는 다시 열면 자동으로 받는다) |
| 다음 명세 | R2c 판정 경계 표시 — 이 명세의 표시 컴포넌트에 모양을 더한다 |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **대상 부품 5종** | `AVBoRHingedPanel`(자식 `AVBoRHingedDoor` 포함 — 문·콘솔·글러브박스), `AVBoRPoweredPanel`(리프트게이트), `AVBoRFoldingMirror`, `AVBoRFoldingSeat`, `AVBoRAxisPart`(손잡이·레버·선바이저·서랍. 문 그립 `IsDoorGrip()`은 움직이지 않으므로 표시 없음) |
| D2 | **에디터 전용 표시 컴포넌트** | 대상 부품 C++ 부모에 표시 컴포넌트 하나를 더한다. **BP 에디터 뷰포트와 레벨 뷰포트에서 보이고, Play(PIE)·패키지에서는 보이지 않는다.** 충돌 없음. 그리는 방법은 §5(권장 `UDebugDrawComponent`) |
| D3 | **회전 표시** | ① **축 선**(노랑): 실제 회전축 위, 길이는 부품 메시 경계가 축 방향으로 걸친 범위(메시 없으면 20 cm). ② **열림 호**(초록): 닫힘(0) → 최대 각을 축 둘레로 그린 호, 끝에 화살촉(열리는 방향). ③ **닫힘 표시**(빨강): 0 위치의 짧은 반지름 선. ④ AxisPart `Release = Detent`면 **detent 각마다 눈금**(흰색), 시작 detent는 굵게. 호 반지름은 메시 경계에서 축까지 가장 먼 점의 거리(최소 5 cm) |
| D4 | **이동 표시** | AxisPart `Movement = Translate`: 시작점(쉬는 자리)에서 실제 이동 방향으로 **직선 화살표**(초록), 길이 = `MaxTravelCm`. 쉬는 자리에 빨간 짧은 가로선 |
| D5 | **손바닥 밀기 표시** | 밀기가 가능한 면마다 그 면 가운데에 **"이 면을 밀면" 화살표**(면 안쪽으로 향함). 색은 그 밀기의 결과로 정한다 — 열림 쪽이면 초록, 닫힘 쪽이면 빨강. 밀기가 꺼진 면은 그리지 않는다. HingedPanel: 바깥 면(face 0)·안쪽 면(face 1), `bAllowPalmOpening = false`면 여는 쪽 면 생략(`VBoRDoorMotion.cpp:149`). AxisPart: `bAllowPush = true`일 때만, 두 면 |
| D6 | **방향은 판정 코드와 같은 식에서 낸다** | 표시용으로 축·부호를 따로 다시 적지 않는다. 각 부품이 실제로 쓰는 자세 함수(`ValueTransform`·`ApplyAngle`의 회전 식·`MeshPoseAt` 등)와 밀기 판정 규칙에서 방향을 얻는다. 그래야 `bReverse`·`HingeRotation`을 바꾸면 표시가 같이 바뀌고, 표시와 실제가 어긋나지 않는다(§4 E1–E6이 이것을 시험) |
| D7 | **`Preview (0–1)` 슬라이더** | 부품마다 `Preview` 프로퍼티(0~1, 기본 0, `Transient` — 저장하지 않음). 0 = 닫힘·쉬는 자리, 1 = 최대(D3·D4의 끝). 에디터 뷰포트에서 **보이는 움직이는 부분(메시와 같이 도는 자식들)을 그 자세로** 보인다. Detent 부품은 첫 detent → 마지막 detent 사이를 0~1로 |
| D8 | **미리보기는 저장된 값을 바꾸지 않는다** | 사용자가 맞춘 `HingePivot`·`AxisPivot`의 상대 변환, `MeshRestFromRoot`, `PushFaceBox`, 장애물 상자 등 **저장되는 값은 미리보기 전후로 같다.** 저장된 쉬는 자세는 Play 시작(`PreInitializeComponents` 등)에서 읽히므로(예: `ClosedHingeRotation` `VBoRHingedPanel.cpp:331`) 미리보기가 그 값을 건드리면 Play가 열린 채 시작한다. 방법은 §5 |
| D9 | **Play에서는 미리보기 무시** | 게임 월드에서는 `Preview` 값과 상관없이 지금처럼 0에서 시작하고, Play 중에 바꿔도 아무 일도 없다. 미리보기는 이벤트·소리·끼임 검사·시나리오 판정을 일으키지 않는다 |
| D10 | **표시 켜고 끄기** | 부품마다 `Motion guide` 선택: `Always`(기본) / `Selected only` / `Off`. `Selected only`가 단순하게 안 되면(§5) `Always`/`Off` 두 값만 두고 보고 ③에 적는다 |

**색 정리:** 열림·열리는 밀기 = 초록, 닫힘 자리·닫히는 밀기 = 빨강, 축 = 노랑, detent = 흰색.

## 1. 왜 하는가

- 방향 규칙이 부품마다 달라서, 회사 차에 merge 메시를 붙일 때 피벗을 맞추고 Play해서 손으로 밀어 봐야 맞았는지 안다. 뷰포트에서 바로 보이면 그 왕복이 사라진다.
- 미리보기로 열린 상태를 보면 열린 문·리프트게이트가 차체·다른 부품을 뚫는지 Play 없이 본다.
- R2c(판정 경계)와 R7(손잡이 근처 밀기 금지 범위)이 같은 표시 컴포넌트에 모양을 더한다.

## 2. 범위

### 만들 것

- 표시 컴포넌트(D2)와 5종 부품의 표시(D3–D6, D10).
- `Preview`(D7–D9).
- 표시 모양을 계산하는 **순수 함수**(월드 좌표 선·호·화살표 목록을 돌려줌) — 그리기와 나눠서 시험할 수 있게(§4).
- 시험 `vBoR.R2a.*`.

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| 판정 경계(GrabBox·밀기 면 상자·메시 면·장애물 상자·쓸림 영역·적재 영역) 그리기 | R2c |
| 노브·스티어링 휠·룸미러(2축) | 자유모드 부품, 이번 요구 밖. 필요하면 따로 |
| 회전 규칙 통일(모든 부품 +Z 축 등) | B — 기존 BP 값이 바뀐다(계획 §3) |
| 에디터 모듈·Component Visualizer·뷰포트 위젯(끌어서 축 바꾸기) | 큰 구조(AGENTS.md "일을 키우지 않는다"). 런타임 모듈 안 에디터 전용 컴포넌트로 한다 |
| 글자(각도 숫자) 표시 | 선택. 하려면 `FDebugDrawDelegateHelper` 등 추가 구조가 필요하다. 넣지 않아도 완료 조건 충족 |
| 부품 동작·판정 변경 | 이 명세는 보이기만 한다 |
| `L_Main`·근사 차·자식 BP 수정 | 에디터 작업 없음 |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- 부품의 동작·판정·이벤트·소리·로그는 그대로.
- D8: 저장되는 변환·값은 미리보기와 상관없이 같다. 레벨을 저장했다 다시 열어도 `Preview`는 0이다.
- 표시 컴포넌트는 충돌 없음, 그림자 없음, 게임에서 숨김, 패키지에서 빠짐(`bIsEditorOnly` 등).
- 새 컴포넌트 이름에 `Pivot`을 넣지 않는다 — `P4iComponents`가 이름에 `Pivot`이 든 컴포넌트를 부모·BP 사이에서 짝지어 비교한다(`P3Probe/P4iCarBlueprintTests.cpp:84`).
- **기존 시험 감사(Claude 2026-11-01, 대상 클래스의 `GetComponents` 검색).**
  - `P4iCarBlueprintTests.cpp:77-91`(`P4iComponents`): 스태틱 메시·Box·이름에 `Pivot`이 든 것만 비교 → 새 컴포넌트가 이 셋에 해당하지 않으면 영향 없음.
  - `P5xPanelHandlesTests.cpp:84-88`: HingedPanel CDO의 `UBoxComponent` 1개(`PushFaceBox`)·grabbable 0개 → 새 컴포넌트가 `UBoxComponent`·grabbable이 아니면 영향 없음.
  - `P5zFixedGuidanceTests.cpp:249-250`: `AVBoRPanelAnchor`의 primitive 4개 — 대상 아님.
  - `P3FlowTests.cpp:815`·`:892`, `P4iCarBlueprintTests.cpp:183`·`:244`·`:265`: 태그 검색·차 Core 비교 — 대상 부품에 태그 없는 컴포넌트를 더해도 영향 없음.
  - **고쳐도 되는 단언: 없음.** 기존 시험을 고쳐야 통과하면 멈추고 리뷰 파일 "질문"에 적는다.

## 4. 시험 — `vBoR.R2a.*`

렌더링은 시험하지 않는다. §2의 순수 함수가 돌려준 모양과 **실제 자세 함수**를 비교한다. 각 시험은 축·부호를 바꾼 경우를 포함한다(D6).

| ID | 상황 | 확인 |
|---|---|---|
| E1 | HingedPanel, `HingePivot`을 임의 회전(예: Yaw 30°, Roll 166° — 회사 CAD 손잡이 노드처럼) | 축 선 방향 = `HingePivot` 월드 +Z(±). 호의 시작점·끝점 = 메시 기준점이 각 0과 `MaxAngleDeg`에서 실제 있는 월드 위치(허용 0.1 cm). 화살촉이 각이 커지는 쪽 |
| E2 | PoweredPanel·FoldingSeat(+Y 축), FoldingMirror(`HingeRotation` 임의, `FoldAngleDeg` 음수) | E1과 같은 비교(끝 각 = `OpenAngleDeg`·`FoldAngleDeg`). 미러 음수 각에서도 화살촉이 실제 접히는 쪽 |
| E3 | AxisPart Rotate, `Axis` X·Y·Z × `bReverse` 2가지 = 6가지 | 축 선 = 실제 축. 호 끝 = `Value = MaxAngleDeg` 자세의 실제 위치. `bReverse`를 바꾸면 호가 반대쪽 |
| E4 | AxisPart Translate, 6가지 | 화살표 방향 = `Value` 증가 때 실제 이동 방향, 길이 = `MaxTravelCm`(0.1 cm) |
| E5 | AxisPart Detent(`DetentAnglesDeg = {-20,0,20}`, 시작 1) | 눈금 3개가 각 detent 자세 위치, 시작 눈금 표시. 호 범위 = 첫 → 마지막 detent |
| E6 | **밀기 방향 일관성:** HingedPanel 바깥·안쪽 면, AxisPart(`bAllowPush`) 두 면 | 각 화살표 색(열림/닫힘)이 **실제 밀기 판정으로 그 면을 밀었을 때 값이 변하는 방향**과 같다(기존 밀기 시험의 손 입력 방식으로 한 프레임 밀어 비교). `bAllowPalmOpening = false`면 여는 쪽 화살표 없음, `bAllowPush = false`면 AxisPart 화살표 없음 |
| E7 | 그립(`IsDoorGrip()`)·메시 없는 부품 | 그립은 표시 없음. 메시 없으면 축 선 20 cm, 호 반지름 5 cm, 오류 없음 |
| E8 | **미리보기 자세:** 5종 각각 `Preview` = 0, 0.5, 1 | 보이는 메시의 월드 변환 = 같은 각·이동을 Play 코드 경로로 줬을 때의 메시 월드 변환(0.1 cm·0.1°). Detent는 첫~마지막 detent 보간 |
| E9 | **D8:** `Preview` 1로 바꿨다가 0으로 | 저장 대상 값(`HingePivot`/`AxisPivot` 상대 변환, `MeshRestFromRoot`, `PushFaceBox` 변환·크기)이 처음과 같다. `Preview` 1인 채 위치·`MaxAngleDeg`를 바꿔도 미리보기가 새 값으로 다시 계산되고 저장 값은 사용자가 바꾼 것만 바뀜 |
| E10 | **D9:** `Preview` 1인 채 게임 월드에서 시작(BeginPlay) | 각·값 0, 닫힘 상태, 문 잠금(`bUseLatch`) 지금과 같음. 게임 중 `Preview` 바꿔도 각 그대로, 이벤트 없음 |
| E11 | 표시 컴포넌트 속성 | 게임 월드에서 숨김, 충돌 없음, editor-only. `Motion guide = Off`면 모양 0개 |

## 5. 구현자가 정할 것

- **그리는 방법.** 권장: `UDebugDrawComponent` 상속(`Engine/Classes/Debug/DebugDrawComponent.h:49`, `CreateDebugSceneProxy`만 재정의 `:58`) + `FDebugRenderSceneProxy`의 선·화살표·원(`Engine/Public/DebugRenderSceneProxy.h:138`·`:248`·`:156`). 이 프록시의 기본 보기 플래그는 `Game`(`Engine/Private/DebugRenderSceneProxy.cpp:64-66`) — 에디터 뷰포트에서 보이고 Play에서 숨기는 설정(플래그·`bHiddenInGame`·`bIsEditorOnly`)은 구현자가 확인해 보고 ④에 적는다. 자체 렌더 프록시·머티리얼을 새로 만들지 않는다.
- 값이 바뀔 때 다시 그리는 방법(`OnConstruction`에서 `MarkRenderStateDirty` 등).
- **D8을 지키는 미리보기 방법.** 예: 저장되는 피벗은 그대로 두고 메시(와 같이 도는 자식) 쪽 상대 변환만 바꾸기, 또는 피벗이 원래 프로퍼티에서 다시 계산되는 부품(리프트게이트·미러·등받이 — `HingeLocationCm` 등)은 그 계산에 미리보기 각을 넣기. 부품마다 달라도 된다. 문 밑에 Outliner로 붙인 손잡이 Actor까지 미리보기에서 따라오게 할 필요는 없다(되면 좋음, 보고 ③).
- `Selected only` 구현 여부(D10). 프록시의 `IsSelected()`(`Engine/Public/PrimitiveSceneProxy.h:778`)로 단순하게 되면 넣는다.
- 순수 함수의 형태와 위치, 공통 코드를 어디에 둘지(`Interaction/`).
- 호를 몇 개 선분으로 나눌지, 화살촉 크기.

## 6. 완료 조건

1. D1–D10 구현, §3 조건 지킴.
2. `vBoR.R2a.E1`–`E11` 통과, 기존 시험 변경 없이 통과.
3. non-unity(`-Module=vBoR`)·unity 빌드 성공, `vBoR.*` 전체 통과(작업 끝에 한 번).
4. 새 주석·표시 이름·툴팁 영어(AGENTS.md). 프로퍼티 이름: `Preview`, `Motion guide`(Category는 부품마다 기존 것 아래 `|Guide`).

## 7. 로컬에서 확인할 것

- 빌드: 위 3.
- 에디터(사용자):
  ① 근사 차 레벨에서 문·리프트게이트·미러·등받이·선바이저에 화살표가 보이고 Play하면 사라진다.
  ② 문 BP 에디터 뷰포트에서도 보인다.
  ③ `Preview`를 끌면 메시가 열리고, 0으로 돌리고 저장 → 다시 열기 → Play 결과가 전과 같다.
  ④ AxisPart `bReverse`를 바꾸면 호가 반대로 간다.
  ⑤ 회사 차: merge 메시를 붙인 부품의 화살표가 실제 여는 방향과 맞는지, 열린 상태가 차체를 뚫는지.
- Quest 3: 없음(참가자 화면에 나오지 않음 — Play에서 숨김만 확인).
