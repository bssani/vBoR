# P5t 박스 BP — 메시·크기·소리를 BP에서 바꾸기

| 항목 | 값 |
|---|---|
| 단계 | P5 자유모드·리허설 준비(사용자 요청 2026-10-21). 시험 T-02 |
| 결정 근거 | **사용자(2026-10-21):** "박스도 블루프린트로 만들 수 있어야 해. mesh를 변경할 수 있거든."<br>**Claude 질문 → 사용자 답(모두 권장안):**<br>① 크기는 → **박스 BP의 크기 값**(`BoxSizeCm`). 메시는 그 크기로 늘린다(지금 방식, U-10 "박스 크기는 프로퍼티로 조절"). 받침은 덮어쓰지 않는다<br>② 받침도 BP로 → **박스만** |
| 베이스 브랜치 | `main` @ 이 명세 머지 뒤 |
| 작업 브랜치 | `codex/P5t-box-blueprint` |
| 리뷰 파일 | `Docs/Reviews/P5t-box-blueprint.md` (첫 리뷰 때 생김) |
| 상태 | 명세 — 구현 전 |
| 에디터 작업 | **있음** — `BP_Box` 만들기, `L_Main` 받침의 `BoxClass` 바꾸기(MCP). 사용자 요청 |
| 앞 명세 | [P5s-part-sounds.md](P5s-part-sounds.md) |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **`BP_Box`를 만든다** | `Content/Main/Car/Parts/BP_Box`. 부모는 정확히 `AVBoRCarryObject`이다(`Interaction/VBoRCarryObject.h:69`, 이미 `Blueprintable`).<br>- Event Graph는 비운다(Blueprint-Guide §4.1-7).<br>- **값은 바꾸지 않는다.** 메시는 C++ 기본(엔진 Cube), `BoxSizeCm`은 45×35×30, 소리 칸도 기본이다. 그래서 지금 보이는 박스와 동작이 그대로다.<br>- 팀은 BP에서 `CarryMesh` 컴포넌트의 Static Mesh와 `BoxSizeCm`, 소리 칸을 바꾼다 |
| D2 | **`L_Main` 받침이 `BP_Box`를 만든다** | `L_Main`의 받침(`AVBoRBoxStand`, 태그 `Car.BoxStand`) 인스턴스에서 `BoxClass`를 `BP_Box`로 고른다. 받침 자체는 C++ 그대로다(사용자 답 ②). |
| D3 | **박스 크기는 박스 클래스가 정한다 — 받침의 `BoxSizeCm`을 지운다** | **지금:** 받침이 자기 `BoxSizeCm`을 만든 박스에 덮어쓴다(`Interaction/VBoRBoxStand.cpp:47`). 그래서 BP에서 크기를 바꿔도 받침 값으로 돌아간다.<br>**바꿀 것:**<br>- 받침의 `BoxSizeCm` 프로퍼티(`VBoRBoxStand.h:22`)와 그 정리(`.cpp:24`)를 지운다.<br>- 받침은 `BoxClass`의 기본 객체(CDO)에서 `BoxSizeCm`을 읽는다. 절댓값을 쓰고 축마다 최소 1 cm로 막는다(박스 `ApplyBoxSize`와 같은 정리, `VBoRCarryObject.cpp:106`). 이 값을 시작 자리(`StartPose`, `.cpp:38`)와 보충 칸(`.cpp:62`)에 쓴다.<br>- 박스를 만들 때 크기를 덮어쓰지 않는다. **나머지 덮어쓰기는 그대로다:** `LoadTarget`, `bUsePlacementAssist` 끔, `bDestroyAtFloor` 켬, `FloorHeightCm`(`.cpp:47-49`).<br>- 메시는 지금처럼 `BoxSizeCm`으로 늘린다(`ApplyBoxSize`, `.cpp:101-108`, P1b D2). CAD 메시를 원래 크기로 쓰려면 그 크기를 `BoxSizeCm`에 넣는다.<br>**지워도 잃는 값이 없다:** `L_Main.umap`의 이름표에 `BoxSizeCm`이 없다. 받침 인스턴스가 기본값(45×35×30)이라는 뜻이다(Claude 확인 2026-10-21). C++ 기본 박스도 45×35×30이라 크기가 같다(`VBoRCarryObject.h:101`). |
| D4 | **메시 피벗이 가운데가 아니면 경고한다** | `CarryMesh`가 Actor 루트라서(`VBoRCarryObject.cpp:58-59`) BP에서 메시를 옮길 수 없다. 피벗이 박스 가운데가 아니면 박스가 받침 위에 뜨거나 묻힌다(시작 자리는 Actor 원점 = 박스 가운데로 계산함, `VBoRBoxStand.cpp:38`). 돌릴 때도 모서리를 축으로 돈다.<br>- 박스 `BeginPlay`에서 메시 bounds 중심(늘린 뒤 cm, `GetMeshLocalBounds()` × Actor scale)이 Actor 원점에서 **1 cm 넘게** 떨어져 있으면 Warning을 한 번 남긴다. 문장에 박스 이름, 떨어진 거리(cm), "메시 피벗을 박스 가운데로 옮기라"를 넣는다.<br>- 자동으로 고치지 않는다(보정하면 회전 중심·착지 판정을 모두 다시 봐야 함).<br>- 지금 simple collision 경고(`VBoRCarryObject.cpp:133-138`)는 그대로 둔다 |
| D5 | **`BoxClass`가 비면 경고한다** | 지금은 조용히 박스를 만들지 않는다(`VBoRBoxStand.cpp:43`). 받침 `BeginPlay`에서 Warning을 한 번 남긴다. 그 밖의 동작(만들지 않음, `ResetStand`·`Tick`에서도 안 만듦)은 그대로다 |

## 1. 왜 만드는가

05 박스 적재의 박스는 지금 C++ 기본 클래스다. 받침이 Play 때 만들기 때문에 레벨에서 고를 BP가 없고, 팀이 메시·소리를 바꿀 곳이 없다. 크기도 받침이 정한다. 팀은 실제 박스 모양의 메시를 넣으려 하고(사용자), 크기는 메시와 한곳에서 보는 것이 맞다(사용자 답 ①).

## 2. 범위

### 만들 것

- 받침: `BoxSizeCm` 삭제, `BoxClass` CDO의 크기 사용, 크기 덮어쓰기 삭제, `BoxClass` 빔 경고(D3·D5).
- 박스: 피벗 경고(D4).
- 에셋: `BP_Box`(D1), `L_Main` 받침 `BoxClass`(D2).
- 시험 `vBoR.P5t.*`(§4)와 시험용 박스 클래스(§5).

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| 받침 BP, 받침 메시 바꾸기 | 사용자 답 ② |
| 메시 원래 크기 모드(늘리지 않기) | 사용자 답 ① |
| 피벗 자동 보정, 메시 위치 오프셋 칸 | D4 |
| 적재 영역 크기(`AVBoRLoadTarget::GoalSizeCm`)를 박스 크기에 맞추기 | 따로 움직인다(P4d 리뷰 참고 2). 안착 보조가 꺼져 있고 05는 진행자 확인이다 |
| 박스 여러 종류·받침 여러 개 | 요청 없음 |
| 근사 차(`AP3VehicleMock`)의 받침·박스 | 근사 차는 C++ 기본 클래스 그대로다(`P3VehicleMock.cpp:366`) |
| `LevelLiftMaxCm` 기본값을 박스 높이에 묶기 | 지금 15 cm(기본 높이의 절반)이고 따로 정한 값이다 |
| `P4iBlueprints()` 목록에 `BP_Box` 넣기 | 그 목록은 레벨에 놓는 BP다. 넣으면 P4i E1–E4의 개수가 바뀐다. P5q처럼 따로 시험한다(E1·E2) |
| Blueprint-Guide 고치기 | 머지 뒤 Claude가 한다 |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- **C++ 기본 박스:** `BoxSizeCm` 45×35×30, 메시 엔진 Cube, 메시를 크기로 늘리는 규칙(`ApplyBoxSize`)은 그대로다.
- **받침:** 시작 자리는 박스 바닥이 받침 윗면에서 0.01 cm 위다(P1b D5·P4d D1). 보충 칸은 박스 크기 + 1 cm 여유다(`VBoRBoxStand.cpp:62`). 손에 든 채 보류 중이면 보충하지 않는다(`:59`). `StandSizeCm`·`StandTopHeightCm`·받침 메시는 그대로다.
- **진행자 R:** 모든 받침을 `ResetStand`한다(`VBoRScenarioRunner.cpp:515`). 그대로다.
- **`BP_Box`:** Event Graph 비움. `Development/` 에셋을 참조하지 않는다(P4i E5).
- **기존 시험 감사 — 모두 그대로 통과해야 한다.** 받침의 `BoxSizeCm`을 쓰는 시험은 없다(`grep BoxSizeCm`: 시험은 박스나 `AP1LoadBench`의 같은 이름 칸만 씀).
  - P4d E1 `P3Probe/P3FlowTests.cpp:1132-1133`: 받침 덮어쓰기(목표·보조 끔·바닥 소멸), 박스 바닥 75.01 cm.
  - P4d E4 `:1186`: 받침 박스로 게이트 끼임.
  - P4d E6·E7 `:1211-1225`: +31 cm까지 차 있음(높이 30 + 1), 보충, 원래 자리, `ResetStand`, `EndPlay`.
  - P4d E8·E9 `:1235-1250`: R에서 옛 박스 정리, 새 박스 하나.
  - P4i E4 `P4iCarBlueprintTests.cpp:256-263`: `L_Main` 받침 하나, `LoadTarget` 연결. E5 `:314-320`: 의존성(이제 `BP_Box` 포함).
  - P1Box `P1Probe/P1BoxLoadTests.cpp:36-78`: 크기·메시·충돌. 박스 클래스를 바꾸지 않는다.
  - P1c·P5n·P5s 박스 시험: C++ 기본 박스를 직접 만든다. 그대로다.

## 4. 경계 상황과 그 시험 (`vBoR.P5t.*`)

시험 이름은 `vBoR.P5t.E<n><설명>`이다.

| ID | 상황 | 확인 |
|---|---|---|
| E1 | `BP_Box` 에셋 | `/Game/Main/Car/Parts/BP_Box`가 있다. 부모가 정확히 `AVBoRCarryObject`이고 Event Graph 노드가 0개다 |
| E2 | `L_Main` 받침 | 받침이 하나이고 `BoxClass`가 `BP_Box_C`다 |
| E3 | 크기는 박스 클래스에서(D3) | 시험용 박스 클래스(기본 `BoxSizeCm` 20×30×40, `bUsePlacementAssist` 켬)로 받침을 만든다.<br>- 만든 박스의 충돌 bounds 크기가 20×30×40이다(±0.02 cm).<br>- 바닥이 받침 윗면 + 0.01 cm다.<br>- 나머지 덮어쓰기는 그대로다: 목표 = 받침의 `LoadTarget`, 보조 꺼짐, 바닥 소멸 켬, `FloorHeightCm` = 받침 Z |
| E4 | 보충 칸도 박스 크기(D3) | E3의 받침에서 원래 박스를 잡고 +41 cm(높이 40 + 1)로 올리면 아직 차 있다(보충 0). +50 cm면 하나 보충되고 원래 자리에 놓인다. P4d E6과 같은 순서다 |
| E5 | `BP_Box`로 받침 | `BoxClass` = `BP_Box`로 받침을 만든다. 박스 클래스가 `BP_Box_C`다. 크기가 `BP_Box` CDO의 `BoxSizeCm`과 같다. 바닥이 받침 윗면 + 0.01 cm다. 값을 읽어 비교하므로 팀이 크기를 바꿔도 통과한다 |
| E6 | 지금과 같음 | `L_Main`에 `Approx` 폴더가 있으면 `BP_Box` CDO가 C++ 기본과 같다: `CarryMesh`의 Static Mesh, 그리고 `BoxSizeCm`·소리 칸을 포함한 vBoR 쪽 Edit 프로퍼티(오브젝트 참조 프로퍼티는 빼는 P4i E2 규칙, `P4iCarBlueprintTests.cpp:93-103`). 없으면 Info 한 줄 남기고 건너뛴다(회사 CAD 상태) |
| E7 | 피벗 경고(D4) | 늘린 뒤 bounds 중심이 원점에서 0.5 cm면 경고 0. 2 cm면 경고 1(이름·거리 포함). 메시 단위로 1이 떨어져도 scale 3이면 3 cm라 경고한다. Cube 기본 박스는 경고 0 |
| E8 | `BoxClass` 빔(D5) | 받침을 `BoxClass` 없이 만든다. 경고 1, 박스 0. `ResetStand`와 `Tick` 뒤에도 박스 0, 충돌 없음 |
| E9 | 기존 시험 | §3 감사대로 통과한다 |

## 5. 구현자가 정할 것

- CDO를 읽는 방법(예: `TSubclassOf::GetDefaultObject()`, 엔진 `CoreUObject/Public/Templates/SubclassOf.h:141`).
- 경고 문장(D4·D5의 내용을 담는다).
- 시험용 박스 클래스(예: 시험 폴더 fixture 헤더의 `UCLASS` 자식, `P3Probe/P4aTestFixture.h:8-9`와 같은 방식).
- E7을 시험하는 방법. 엔진 기본 메시는 모두 가운데 피벗이다. 판정을 bounds·scale을 받는 함수로 떼어 시험해도 된다.
- MCP 호출 방식(MCP Python 일괄 권장). 스크립트는 `Saved/`에만 둔다.

## 6. 완료 조건

1. D1–D5를 지킨다.
2. E1–E9가 통과한다.
3. 기존 `vBoR.*` 시험이 모두 통과한다.
4. non-unity·unity 빌드에서 vBoR 경고가 없다.
5. 순서: C++ 빌드 → 에디터를 열어 MCP 작업(`BP_Box` 만들기, `L_Main` 받침 `BoxClass`) → 저장 → 에디터 닫기 → 전체 시험 한 번.
6. 보고는 AGENTS.md "작업 방식" 형식으로, 커밋 본문과 HANDOFF "Codex 결과"에 쓴다. 만든·바꾼 에셋 목록을 ①에 넣는다.

## 7. 로컬에서 확인할 것 — 사용자(VR, 끝에 한꺼번에)

1. `L_Main` Play: 받침 위 박스가 지금과 같은 크기·자리다. 들면 새 박스가 생긴다. R에서 정리된다.
2. `BP_Box`의 메시를 다른 메시로 바꿔 Play해 본다.
   - 그 메시에 simple collision(상자·convex)이 있어야 한다. 없으면 경고가 뜨고 박스가 움직이지 않는다.
   - 피벗이 가운데가 아니면 경고가 뜬다.
   - `Approx`가 있는 집 PC에서는 `vBoR.P5t.E6`이 실패하는 것이 맞다(근사 값과 다름).
3. 시험 상태는 VR 전까지 `not-tested`(T-02).

## 8. 확인한 근거

| 무엇 | 근거 | 쓰는 곳 |
|---|---|---|
| 박스 클래스 | `Blueprintable` `Interaction/VBoRCarryObject.h:69`, `BoxSizeCm` `:101`, 메시가 루트 `VBoRCarryObject.cpp:58-59`, Cube `:68-69`, `ApplyBoxSize` `:101-108`, simple collision 경고 `:133-138`, `GetMeshLocalBounds` `:148-151` | D1·D3·D4 |
| 받침 | `BoxSizeCm` `Interaction/VBoRBoxStand.h:22`, `BoxClass` `:23`, 기본 클래스 `VBoRBoxStand.cpp:17`, 정리 `:24`, 시작 자리 `:38`, 만들기·덮어쓰기 `:43-49`, 보충 `:59-62` | D2·D3·D5 |
| 지금 L_Main 값 | `Content/Main/L_Main.umap`에 이름 `BoxSizeCm` 없음, `VBoRBoxStand`는 있음 | D3 |
| 근사 차 받침 | `P3Probe/P3VehicleMock.cpp:365-368` | §2 |
| 적재 목표 크기 | `Interaction/VBoRLoadTarget.cpp:20`, P4d 리뷰 참고 2 | §2 |
| L_Main 시험 본보기 | P5q E13 `P3Probe/P5qPowerMirrorTests.cpp:306-331`, P4i `Approx` 판정 `P4iCarBlueprintTests.cpp:61-67`, 설정 비교 `:93-103` | E2·E6 |
| R | `Interaction/VBoRScenarioRunner.cpp:515` | §3 |

## 개정 이력

| 날짜 | 내용 |
|---|---|
| 2026-10-21 | 처음 작성(Claude) |
