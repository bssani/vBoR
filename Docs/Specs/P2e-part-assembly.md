# P2e 부품 조립 — 문 밑에 넣은 손잡이를 문이 찾아 붙임, 미는 면 상자, 설정 검사 버튼

| 항목 | 값 |
|---|---|
| 단계 | P2 보강 — 실제 차 데이터를 Blueprint 자식으로 넣기 전 준비(U-14) |
| 결정 근거 | **사용자 설명(2026-10-18, U-14):** 실제 차 CAD는 메시가 수만 개이고, 문 하나에도 부품이 따로따로 나뉘어 있다. 움직이거나 상호작용하는 부품만 Blueprint로 만들고, 나머지 차량 메시는 월드에 그대로 둔다(필요하면 merge). 문 BP에는 문에 붙어 같이 도는 것이 다 들어가야 하고, 바깥·안쪽 손잡이 BP도 문에 실린다. **사용자 문답(같은 날):** Claude 추천안으로 간다 — ① 손잡이를 문 밑에 넣기만 하면 문이 Play 때 찾아 연결하고 경첩 축에 붙인다. Outliner에서 끌어 넣기와 BP 안 Child Actor 둘 다 된다. ② 미는 면 상자 옵션 ③ 설정 검사 버튼 |
| 베이스 브랜치 | `main` @ P1c 머지 뒤(`7b86817` + 이 명세 커밋) |
| 작업 브랜치 | `codex/P2e-part-assembly` |
| 리뷰 파일 | `Docs/Reviews/P2e-part-assembly.md` (첫 리뷰 때 생김) |
| 상태 | 명세 — 구현 전 |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **손잡이에 "문의 어느 쪽" 값을 둔다** | `AVBoRAxisPart`에 `DoorHandleSide`(enum: 없음·바깥·안쪽, 기본 없음)를 더한다. 손잡이 BP의 기본값으로 한 번 정한다. 예: `BP_DoorHandle_FL_Ext` = 바깥. 기어 레버처럼 문과 상관없는 1축 부품은 "없음" 그대로다. 값만 더하고, 1축 부품의 동작은 바꾸지 않는다 |
| D2 | **문이 Play 때 자기 밑의 손잡이를 찾는다** | `AVBoRHingedPanel`(문·글러브박스·콘솔 뚜껑 공통)의 `BeginPlay`에서, 지금의 손잡이 연결(`Interaction/VBoRHingedPanel.cpp:210`) 전에 손잡이를 정한다. **후보:** 이 판의 어느 컴포넌트에든 붙은 `AVBoRAxisPart` 중 `DoorHandleSide`가 "없음"이 아닌 것이다. 엔진 `AActor::ForEachAttachedActors`(`Runtime/Engine/Classes/GameFramework/Actor.h:3554`, 구현 `Private/Actor.cpp:2832` — root부터 모든 자식 컴포넌트를 훑음)로 찾는다. 그래서 두 가지가 다 된다: **(가)** Outliner에서 손잡이를 문 위로 끌어 놓은 것(문 root에 붙음) **(나)** 문 BP 안의 Child Actor 컴포넌트가 만든 손잡이. **정하는 순서:** 칸(`OutsideAxisHandle`·`InsideAxisHandle`, `.h:78-79`)에 직접 지정한 손잡이가 먼저다. 칸이 비었으면 그 쪽 후보가 하나일 때만 쓴다. 후보가 둘 이상이면 그 쪽은 연결하지 않는다(D5 문제). 칸에 지정한 손잡이의 `DoorHandleSide`가 반대쪽이어도 칸대로 연결한다(D5 문제로 알림) |
| D3 | **연결하면 경첩 축에 붙인다** | 손잡이를 연결하는 모든 길에서 붙인다. 길은 둘이다: D2의 `BeginPlay`, 그리고 코드에서 부르는 `SetAxisHandle`(`:121-126`). 손잡이 root가 이미 `HingePivot` 아래(그 자손 컴포넌트 포함, `USceneComponent::IsAttachedTo`, `Components/SceneComponent.h:1302`)면 그대로 둔다. 근사 차(`P3Probe/P3VehicleMock.cpp:118`, `:127`)와, `HingePivot` 밑에 둔 Child Actor가 그 경우다. 아니면 `AttachToComponent(HingePivot, KeepWorldTransform)`로 붙인다. 월드 자리는 변하지 않는다. 붙였다는 로그를 1번 남긴다. **Child Actor인데 `HingePivot` 아래가 아니면**(`AActor::IsChildActor`, `Actor.h:3184`) 떼어 옮기지 않는다. Child Actor 컴포넌트가 다시 만들 때 원래 자리로 돌려놓기 때문이다. 그 손잡이는 연결하지 않고 D5 문제로 알린다("Child Actor를 HingePivot 밑으로"). 붙이는 순간 판이 닫혀 있지 않으면(각도 > 0) 그 자세 기준으로 붙고 D5 문제로 알린다. 보통 Play 시작 때는 닫혀 있다 |
| D4 | **미는 면 상자** | 지금 미는 면은 `DoorMesh` 정적 메시 bounds를 경첩 좌표로 옮긴 상자다(`SyncSettings` `:101-106`, 두께 = Y 크기). 실제 문 메시에 튀어나온 부품(사이드 미러 등)이 merge되거나 곡면이 크면 상자가 두꺼워진다. 그러면 문 겉면보다 한참 앞에서 밀린다. **바꾼다:** `HingePivot` 밑에 `PushFaceBox`(`UBoxComponent`, 충돌 없음, 게임에서 숨김, 에디터에서 보임)를 두고 `bUsePushFaceBox`(기본 끔)를 더한다. **끔:** construction 때 상자를 지금 메시 bounds에 맞춘다. 그래서 에디터에서 지금 미는 면이 보인다. 면은 지금 그대로다. **켬:** 상자를 사용자가 놓은 대로 두고, 면 = 상자의 경첩 좌표 범위(상대 위치 ± 크기 반 × 상대 scale), 두께 = Y 크기다. 상자의 상대 회전이 0이 아니면 경고하고 메시 bounds를 쓴다(면은 경첩 축에 나란해야 함). 끔 → 켬으로 바꾸면 메시 bounds에 맞춰진 상자에서 손보기 시작한다 |
| D5 | **설정 검사 — 버튼과 BeginPlay가 같은 판정** | 판정 함수 하나(예: 문제 글 목록을 돌려주는 const 함수)를 둘이 쓴다. 규칙을 두 벌 두지 않는다(P5i 원칙). **버튼:** `UFUNCTION(CallInEditor)` "설정 검사"(`Runtime/CoreUObject/Public/UObject/ObjectMacros.h:1005`). 아무것도 연결·이동하지 않고, 결과를 Output Log와 Details의 읽기 전용 글(`SetupReport`, Transient)에 남긴다. 문제가 없으면 "문제 없음"이다. **BeginPlay:** 같은 목록을 문제마다 Warning 1번으로 남긴다. **문제 목록:** ① 그 쪽 후보가 둘 이상(칸 비었을 때) ② 칸에 지정한 손잡이와 밑에 있는 후보가 다름(칸이 이김) ③ 칸 손잡이의 `DoorHandleSide`가 반대쪽 ④ Child Actor 손잡이가 `HingePivot` 밖 ⑤ 같은 손잡이를 다른 판도 가짐(칸 지정 또는 그 판 밑) — 두 판 모두 그 손잡이를 연결하지 않는다 ⑥ `DoorMesh`가 엔진 큐브가 아닌데 `MeshRestFromRoot`가 근사 기본값 그대로(`.h:77`) — 실제 CAD(피벗 = 차 원점)면 보통 identity ⑦ 미는 면 상자를 끈 채 두께가 `PushFaceWarnThicknessCm`(프로퍼티, 자리값 20 cm)보다 큼 — 튀어나온 부품 merge 확인 또는 D4 상자 ⑧ 상자를 켰는데 회전됨 ⑨ D3의 "열린 판에 붙임". ⑤는 같은 월드의 판들을 훑어서 정한다. Play 순서와 상관없이 결과가 같아야 한다 |

## 1. 왜 만드는가

- 실제 차는 부품이 많다. 문마다 손잡이 칸을 손으로 채우면 빠뜨리기 쉽고, Outliner에서 관계가 안 보인다.
- 지금은 레벨에 따로 놓은 손잡이가 문이 열려도 따라오지 않는다. 근사 차만 코드로 붙인다(`P3VehicleMock.cpp:118`, `:127`).
- 실제 문 메시는 튀어나온 부품·곡면 때문에 bounds 상자가 문 두께보다 크다.
- Play 해 보기 전에 실수를 잡아야 부품 수십 개를 관리할 수 있다.

## 2. 범위

### 만들 것

**1. 1축 부품** — `AVBoRAxisPart`
- D1 `DoorHandleSide` 프로퍼티. 동작 변경 없음.

**2. 경첩 판** — `AVBoRHingedPanel`
- D2 찾기, D3 붙이기(`SetAxisHandle` 포함), D4 미는 면 상자, D5 판정 함수·버튼·BeginPlay 경고.
- 새 프로퍼티(`bUsePushFaceBox`, `PushFaceWarnThicknessCm`)와 컴포넌트(`PushFaceBox`)는 편집 가능하게 둔다. 실행 때 설정 검사는 `PreInitializeComponents`·`BeginPlay`에서 한다(construction은 경고를 내지 않음 — P5e E15).

**3. 자동화 시험** — §4 전부

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| 손잡이를 컴포넌트로 바꾸기 | 1축 부품 부모 전체를 고치는 큰 수술. 사용자 문답에서 고르지 않음 |
| 에디터에서 미리 붙이기(construction에서 다른 액터를 붙이거나 칸을 채움) | 레벨 저장·되돌리기와 꼬임. Play 때 한 번만 |
| 거울·레버·박스의 미는 면 | 거울은 머리 메시뿐(사용자 2026-10-18), 레버 bounds는 레버 그대로 |
| 근사 차 바꾸기 | 근사 차는 지금처럼 코드로 붙이고 칸을 채운다. 결과가 같아야 한다(§3) |
| 에디터 전용 모듈·Slate UI·메시지 창 | `CallInEditor` 버튼 + 로그 + Details 글이면 충분 |
| 사용자용 "BP 만드는 법" 문서 | Claude가 리뷰 뒤 최종 이름으로 쓴다 |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- 칸에 손잡이를 지정하는 지금 방식은 그대로 된다. 칸이 언제나 먼저다.
- 근사 차의 문·글러브박스·콘솔 동작과 시험(P2b·P2c·P2d·P5a, `vBoR.P3*`)은 그대로다. 근사 차 손잡이가 두 번 붙거나 자리가 바뀌지 않는다.
- `bUsePushFaceBox`를 끈 판의 미는 면·두께는 지금과 같다(같은 bounds 식).
- 손잡이 → 문 넘김(80%), 잠김, 잡는 자리 끄기(`UpdateGrabAreas` `:133-145`)는 그대로다. 연결이 안 된 쪽은 지금처럼 잡는 자리를 쓴다.
- 1축 부품의 잡기·밀기·걸림(P5b·P5c·P5m)은 그대로다.
- 기존 단언은 바꾸지 않는다.

## 4. 경계 상황과 그 시험

| # | 상황 | 기대 동작 | 자동화 시험 |
|---|---|---|---|
| E1 | (가) 문과 손잡이 둘(바깥·안쪽)을 따로 놓고 손잡이를 문 root에 붙임 → Play | 두 칸 연결, 둘 다 `HingePivot` 밑으로 옮김, 월드 자리 그대로(±0.01 cm·0.01°), 로그 각 1번 | 연결·자세 |
| E2 | (나) 문의 `HingePivot` 밑 Child Actor 컴포넌트 둘이 손잡이를 만듦 → Play | 두 칸 연결, 다시 붙이지 않음(부모 그대로) | 연결·부모 |
| E3 | (나) Child Actor 컴포넌트가 문 root 밑(경첩 밖) | 그 쪽 연결 안 함, 문제 ④ 1번, 그 쪽 잡는 자리 그대로 | 연결·문제 |
| E4 | 지금 방식: 손잡이를 문 밖에 따로 두고 칸에 지정 | 연결, `HingePivot` 밑으로 옮김, 월드 자리 그대로 | 연결·자세 |
| E5 | E1·E2·E4 뒤 문을 30° 엶 | 손잡이가 경첩 축(실제 `HingePivot` 축) 둘레로 30° 같이 돎 | 자세 |
| E6 | 열린 문(30°)의 바깥 손잡이를 잡아 당김 | 손잡이 각도가 늘고, 80%에서 지금처럼 문으로 넘어감 | 상태 |
| E7 | 칸에 A, 문 밑에 같은 쪽 B | A 연결, B 무시, 문제 ② | 연결·문제 |
| E8 | 칸 비고 같은 쪽 후보 둘 | 그 쪽 연결 없음, 문제 ①, 반대쪽은 정상 | 연결·문제 |
| E9 | 칸 손잡이의 쪽 값이 반대 | 칸대로 연결, 문제 ③ | 연결·문제 |
| E10 | 손잡이 하나를 판 둘이 가짐(한 판 칸 + 다른 판 밑) | 두 판 모두 연결 안 함, 두 판 각각 문제 ⑤. 두 판의 BeginPlay 순서를 바꿔도 결과 같음 | 연결·문제 |
| E11 | 쪽 값 "없음"인 1축 부품이 문 밑에 있음 | 무시, 문제 없음, 그 부품 동작 그대로 | 연결 |
| E12 | 근사 차 | 지금과 같음 — 연결·자세·로그(새 경고 0) | 근사 차 |
| E13 | 열린 판에 `SetAxisHandle` | 붙고 문제 ⑨ 1번 | 문제 |
| E14 | 미는 면 상자 끔 | construction 뒤 상자 = 메시 bounds, 면·두께 = 지금 | 면 |
| E15 | 상자 켬(메시 bounds보다 얇게) | 면·두께 = 상자. 상자 밖이지만 메시 bounds 안의 손 점은 안 밂, 상자 면을 넘은 점은 밂 | 면·밀기 |
| E16 | 상자 켬 + 상대 회전 | 경고 1번, 메시 bounds 사용, 문제 ⑧ | 면·문제 |
| E17 | `DoorMesh`가 엔진 큐브가 아닌데 자리값이 근사 기본값 / 엔진 큐브 | 문제 ⑥ / 문제 없음 | 문제 |
| E18 | 상자 끔 + bounds 두께 25 cm / 상자 켬 | 문제 ⑦ / 없음 | 문제 |
| E19 | 버튼 "설정 검사"를 부름(Play 전) | E7–E18의 같은 배치에서 BeginPlay와 같은 문제 목록. 연결·부모·칸·자세는 바뀌지 않음. `SetupReport`에 표시, 문제 없으면 "문제 없음" | 판정·불변 |
| E20 | BeginPlay 경고 | 문제마다 Warning 1번(같은 문제를 두 번 내지 않음) | 로그 |
| E21 | 기존 시험 | 모두 그대로 통과(§3) | 전체 시험 |

## 5. 구현자가 정할 것

- 판정 함수의 꼴(문제 글 배열, 구조체 등)과 `SetupReport`의 글 모양.
- ⑤를 위해 같은 월드의 판을 훑는 방법(`TActorIterator` 등).
- `DoorHandleSide` enum 이름과 DisplayName(한국어 권장, 기존 판 프로퍼티처럼).
- 붙였다는 로그의 꼴.

## 6. 완료 조건

1. §2의 1–3이 들어 있다.
2. §4 시험이 모두 자동화로 통과한다. 기존 `vBoR.*`도 통과한다.
3. non-unity(`-Module=vBoR`)·unity 빌드에서 vBoR 경고·오류가 0이다. 전체 시험은 최종 트리로 한 번 돌린다.
4. AGENTS.md 보고 ①–⑤를 커밋 본문과 `HANDOFF.md`에 남긴다.
5. 시험 상태는 `not-tested`다.

## 7. 로컬에서 확인할 것 — 사용자(에디터·VR, 끝에 한꺼번에)

1. 레벨에 문 BP와 손잡이 BP 둘을 놓는다. 손잡이를 Outliner에서 문 위로 끌어 놓는다. Play → 문을 열면 손잡이가 따라오는지 본다.
2. 문 BP 안에 Child Actor로 손잡이를 넣는 방법도 해 본다(`HingePivot` 밑). 어느 쪽이 관리하기 편한지 알려 준다.
3. "설정 검사" 버튼을 눌러 결과가 Details에 보이는지 본다. 일부러 손잡이 쪽 값을 비우거나 둘을 같은 쪽으로 해 본다.
4. 문 BP에서 "미는 면 상자"를 켜고 상자를 문 겉면·안쪽 면에 맞춘다. VR에서 겉면에 닿을 때 밀리는지 본다.

## 8. 확인한 API 근거

경로: `Source/vBoR/` 아래(`main` @ P1c 머지 뒤 줄 번호), 엔진은 `C:/Program Files/Epic Games/UE_5.7/Engine/Source/` 기준.

| 쓸 것 | 위치 | 쓰임 |
|---|---|---|
| 판 손잡이 칸·메시 자리값 | `Interaction/VBoRHingedPanel.h:77-79` | D2·D5⑥ |
| 손잡이 연결 | `Interaction/VBoRHingedPanel.cpp:210`(`BeginPlay`), `:121-126`(`SetAxisHandle`) | D2·D3 |
| 잡는 자리 끄기 | `Interaction/VBoRHingedPanel.cpp:128-145` | §3 |
| 미는 면 = 메시 bounds | `Interaction/VBoRHingedPanel.cpp:101-106`(`SyncSettings`), `Interaction/VBoRDoorMotion.h:28-29` | D4 |
| 근사 차가 손잡이를 붙이는 곳 | `P3Probe/P3VehicleMock.cpp:112-128`(문), 글러브박스 `:147-164` | D3·E12 |
| 1축 부품이 판을 아는 곳 | `Interaction/VBoRAxisPart.h:136`(`friend class AVBoRHingedPanel`), `:145`(`LinkedDoor`), `Interaction/VBoRAxisPart.cpp:486-489`(`ConnectDoor`) | D2·D5⑤ |
| 붙은 액터 훑기 | `Runtime/Engine/Classes/GameFramework/Actor.h:3554`(`ForEachAttachedActors`), 구현 `Runtime/Engine/Private/Actor.cpp:2832` | D2 |
| Child Actor 여부 | `Actor.h:3184`(`IsChildActor`), `:3221`(`GetParentComponent`), `Runtime/Engine/Classes/Components/ChildActorComponent.h:208`(`GetChildActor`) | D3·E2·E3 |
| 경첩 밑인지 | `Runtime/Engine/Classes/Components/SceneComponent.h:1302`(`IsAttachedTo`) | D3 |
| 붙이기 | `Actor.h:2018`(`AttachToComponent`) | D3 |
| 에디터 버튼 | `Runtime/CoreUObject/Public/UObject/ObjectMacros.h:1005`(`CallInEditor`) | D5 |

## 개정 이력

| 날짜 | 개정 | 내용 |
|---|---|---|
| 2026-10-18 | 처음 | 작성(사용자 설명: 실제 차 CAD 구성, 문답: Claude 추천안 — 문 밑에 넣으면 찾기(Outliner·Child Actor 둘 다), 미는 면 상자, 설정 검사 버튼) |
