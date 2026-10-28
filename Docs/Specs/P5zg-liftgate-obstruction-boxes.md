# P5zg 리프트게이트 끼임 판정 상자 — 메시 경계 대신 사용자가 맞춘 상자

| 항목 | 값 |
|---|---|
| 단계 | P5 리허설 준비(사용자 요청 2026-10-28, 회사 PC 시험). 시험 T-02(박스 적재 — "입구 및 게이트 간섭 확인") |
| 결정 근거 | **사용자(2026-10-28):** "트렁크에 box를 실으면 liftgate이 닫히다가 열리던데 box에 부딪히면 다시 열리는 건가?" → Claude 설명 → "명세서 작성해".<br>**지금:** 닫히는 동안 1° 단위로 판 메시의 **경계 상자**(메시 축 OBB, 중간 각도 자세 + 여유)와 박스가 겹치는지 본다(`AVBoRPoweredPanel::FindObstruction` `VBoRPoweredPanel.cpp:147-170`, P4b D5). 근사 판은 얇은 판이라 경계 = 판이다. **회사 CAD 리프트게이트는 굽어 있어(유리 + 아래 판, ㄱ자) 경계 상자가 트렁크 안쪽 공간까지 덮는다.** 그래서 제대로 실린 박스도 끼임으로 잡혀 다시 열린다(Claude 추정 — 사용자가 `obstructions=` 로그 확인 전). 메시 모양 그대로 보는 방법은 §1 |
| 베이스 브랜치 | `main` @ 이 명세 머지 뒤 |
| 작업 브랜치 | `codex/P5zg-liftgate-obstruction-boxes` |
| 리뷰 파일 | `Docs/Reviews/P5zg-liftgate-obstruction-boxes.md` (첫 리뷰 때 생김) |
| 상태 | 명세 개정 2 |
| 개정 2 | **2 (2026-10-28, Claude):** E3 각도 차 기준 **≤ 1° → ≤ 1.5°**. Codex Q1: 작은 Tick(1/60 s, 검사 간격 약 0.35°)과 큰 Tick(4 s, 1°)은 간격과 여유(margin = 경첩 거리 × 반 간격, `VBoRPoweredPanel.cpp:155-157`)가 달라 멈춘 각도가 54.90° / 56.00°(차 1.10°)였다. **기존 메시 경계 경로도 같은 값**(P4b E6). 두 각도는 실제 닿는 각 θ에 대해 각각 [θ, θ + 간격 + 반 간격] 안에 있으므로 차의 상한은 1° + 0.5° = 1.5°다. 1°는 근거 없이 정한 Claude 실수. **알고리즘(D3·D6)은 바꾸지 않는다** |
| 개정 | **1 (2026-10-28, Claude):** 시험 ID T-04·T-06 → **T-02**(계획서 §7 `Dev-Plan-50pct-VR.md:255` "입구 및 게이트 간섭 확인". T-04는 손·키보드 입력 일치, T-06은 순서·완료 판정). P5zf Q1 때 함께 발견. 내용 변경 없음 |
| 에디터 작업 | **없음** (`BP_Liftgate`에 상자를 넣는 것은 사용자가 회사 CAD로 한다, §7) |
| 앞 명세 | [P5zf-box-refill-after-release.md](P5zf-box-refill-after-release.md) — 순서는 P5zf 뒤 |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **판정 상자 컴포넌트 하나 만들기** | `UVBoRObstructionBoxComponent`(부모 `UBoxComponent`, `Source/vBoR/Interaction/`). BP Add Component 목록에 `Obstruction Box`로 보인다(`BlueprintSpawnableComponent`). 에디터에서 선(상자)으로 보이고, **게임에서는 숨김, 충돌 없음**(손·박스·다른 질의에 걸리지 않음). 직접 겹침을 묻지 않고 리프트게이트가 모양만 읽는다 |
| D2 | **상자가 있으면 상자로, 없으면 지금 그대로** | 리프트게이트의 `HingePivot` **밑에(자손)** 붙은 판정 상자가 1개 이상이면 `FindObstruction`은 **그 상자들만** 본다(하나라도 겹치면 끼임). 판 메시 경계는 쓰지 않는다. 유효한 상자가 0개면 지금과 **완전히 같다**(메시 경계) |
| D3 | **상자 자세 = 문과 같이 돌린 자세** | 상자마다 `HingePivot` 기준 상대 변환을 중간 각도(`(From+To)/2`) 경첩 회전과 Actor 변환에 곱한다(`MeshPoseAt` `:141-145`와 같은 방식). **회전·배율 허용:** 크기는 `GetUnscaledBoxExtent() × |배율|`. 여유(margin)는 지금 식(`:155-157`)을 **상자마다** 그 상자의 경첩 거리로 계산한다 — 큰 Tick에도 관통 없음(P4b E9 의도) |
| D4 | **잘못 붙은 상자는 무시하고 한 번 경고** | `HingePivot` 밑이 아닌 판정 상자(예: 루트에 붙음 — 문과 같이 안 돎)와 크기 0인 상자는 판정에 넣지 않고 BeginPlay에 한 번 경고한다: 이름, 이유. 무시하고 남은 상자가 0개면 D2대로 메시 경계 |
| D5 | **로그에 판정 근거** | BeginPlay에 한 줄: 판정이 `mesh bounds (approximate)`인지 `N obstruction boxes`인지. 기존 상태 로그(`:218`)는 그대로 |
| D6 | **나머지 그대로** | 닫힐 때만 검사, 박스(`AVBoRCarryObject`의 `Mesh`)만 끼임 대상, 걸리면 그 각도에서 바로 열기로 바뀜, 소리·이벤트·`ObstructionCount`·대기·속도, 1° 단위 나눔(`:255-258`) |

## 1. 왜 이렇게 하는가 — 택하지 않은 방법

| 방법 | 택하지 않은 이유 |
|---|---|
| 판 메시의 충돌 모양(복합·단순)으로 겹침 | 합친 CAD 메시는 단순 충돌이 없거나 Auto Convex면 다시 볼록 덩어리가 되어 굽은 안쪽을 덮는다. 복합(`Use Complex As Simple`)은 Chaos에서 삼각형 메시를 질의 모양으로 못 쓰고 박스 쪽에서 메시를 묻는 식으로 뒤집어야 해 구조가 커진다(AGENTS.md "가장 단순한 방법"). 결과도 CAD 충돌 설정에 따라 달라진다 |
| 경계 상자 축소 비율 프로퍼티 | 굽은 모양에는 어떤 비율도 맞지 않는다 |
| 상자를 숫자(cm)로 입력 | 사용자 2026-10-28 "숫자로 자리·크기 정하는 구조는 공사가 필요"(HANDOFF). 뷰포트에서 상자를 보며 맞추는 쪽이 메시 우선 방향과 같다. 문 `PushFaceBox`(P2g)와 같은 생각 |

## 2. 범위

### 만들 것

- `UVBoRObstructionBoxComponent`, `AVBoRPoweredPanel`의 상자 찾기·판정·경고·로그(D1–D6).
- 시험 `vBoR.P5zg.*`(§4).

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| `BP_Liftgate`·`L_Main`·근사 차에 상자 추가 | 사용자가 CAD로 한다(§7). 근사 차는 지금 판이 경계와 같아 필요 없다 |
| 문(`AVBoRHingedPanel`)·다른 부품에 같은 상자 | 이 명세는 리프트게이트 끼임만 |
| 끼임 대상을 박스 밖(손·몸)으로 넓힘 | 그대로(P4b) |
| 게임 중 상자 보이기 옵션 | 필요하면 따로 |
| Check setup 버튼 | Runner Check setup과 함께 mid review 뒤(HANDOFF) |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- **판정 상자가 없는 리프트게이트는 지금과 완전히 같다**(멈추는 각도·이벤트·소리·횟수).
- 판정 상자는 다른 어떤 질의에도 걸리지 않는다(충돌 없음) — 박스 착지·막힘(P2f D2), 손 잡기, 시나리오 태그 검사(컴포넌트 태그를 쓰지 않음).
- **기존 시험 감사 — 고치지 않는다**(Claude 2026-10-28, `Obstruct`·`AVBoRPoweredPanel` 검색): `P1Probe/P4bPoweredPanelTests.cpp`(E6 `MultipleBoxesReverseWithoutPenetration`, E7 `InsideBoxesAllowClosing`, E9 `LargeTickMatchesSmallTicks` 등 전부), `P1Probe/P5rSoundCueTests.cpp`, `P3Probe/P3FlowTests.cpp` `vBoR.P4d.E4ClosedAndObstruction`, `P5zcSpawnedButtonClassTests`, `P4iCarBlueprintTests`는 판정 상자를 쓰지 않는다. **하나라도 고쳐야 통과하면 멈추고 리뷰 파일 "질문"에 적는다.**

## 4. 시험 — `vBoR.P5zg.*`

리프트게이트·박스는 `P4bPoweredPanelTests.cpp`의 `FPoweredWorld`·`Box()` 방식으로 만든다. 판정 상자는 시험 world에서 `HingePivot`에 붙여 등록한다. **굽은 CAD 흉내:** `PanelMesh`를 크게(경계가 트렁크 안쪽을 덮게) 하고, 판정 상자는 얇게 판 자리만 덮는다.

| ID | 상황 | 확인 |
|---|---|---|
| E1 | **문제 재현:** 큰 메시 경계 안·판정 상자 밖에 박스, 판정 상자 있음, 닫기 | 끝까지 닫힘, 끼임 0, 박스 안 움직임 |
| E2 | E1과 같은 박스, 판정 상자 **없음** | 지금처럼 끼임 1(메시 경계) — 상자 유무로만 바뀜을 확인 |
| E3 | 판정 상자가 지나는 길에 박스, 작은 Tick(1/60 s)과 큰 Tick(4 s) | 둘 다 끼임 1, 이벤트의 박스 일치, 바로 열기, 멈춘 각도에서 판정 상자가 박스와 겹치지 않음, 두 각도 차 ≤ 1.5°(개정 2 — 1° 검사 간격 + 반 간격 여유) |
| E4 | 판정 상자 2개(ㄱ자), 두 번째 상자 길에만 박스 | 끼임 1 |
| E5 | 회전(예: Pitch 30°)·배율(예: 2, 1, 0.5) 상자: ① 회전 안 한 상자라면 겹칠 자리지만 회전한 상자 밖인 박스 ② 그 반대 | ① 닫힘 ② 끼임 — 회전·배율 반영 |
| E6 | 판정 상자를 `HingePivot`이 아닌 루트에 붙임(유효 상자 0) | 경고 1번(이름·이유), 판정은 E2와 같음(메시 경계) |
| E7 | 크기 0 상자 1개 + 정상 상자 1개 | 경고 1번, 정상 상자로 판정 |
| E8 | 게임에서 판정 상자 | 숨김, 충돌 없음(`NoCollision`), 박스를 들고 상자를 지나가도 막힘(Blocked) 없음 |
| E9 | BeginPlay 로그 | 상자 없음 → `mesh bounds (approximate)`, 상자 2개 → `2 obstruction boxes` |

## 5. 구현자가 정할 것

- 상자를 찾는 시점(BeginPlay에 한 번 모아 둠 권장 — Play 중 컴포넌트는 늘지 않음, P5o와 같은 이유)과 저장 방식.
- 경고 문구(영어, 이름·이유 포함).
- 판정 상자 선 색.

## 6. 완료 조건

1. D1–D6 구현, §3 조건 지킴.
2. `vBoR.P5zg.E1`–`E9` 통과.
3. non-unity(`-Module=vBoR`)·unity 빌드 성공, `vBoR.*` 전체 통과(작업 끝에 한 번).
4. 새 주석·에디터 표시·로그 영어(AGENTS.md).

## 7. 로컬에서 확인할 것

- 빌드: 위 3.
- **에디터(사용자, 회사 PC):** `BP_Liftgate` → Add Component → `Obstruction Box`를 `HingePivot` 밑에 넣는다. 닫힌 상태에서 리프트게이트 판(유리 아래 테두리·아래 판) 모양을 따라 상자 1~3개로 덮는다(회전 가능). 트렁크 안쪽 빈 공간은 덮지 않는다. Play → Output Log에 `N obstruction boxes`.
- Quest 3(T-02, `not-tested`): ① 트렁크 안쪽 깊이 실은 박스로는 끝까지 닫힘 ② 입구에 걸친 박스에는 다시 열림 ③ 소리·버튼 그대로.
