# R10 물체 생성 지점 — Box Stand를 메시 없는 생성 지점으로

| 항목 | 값 |
|---|---|
| 단계 | 구조 보수(메시 우선, 사용자가 맞추는 구조). 시나리오 05(짐 싣기) 준비물 |
| 결정 근거 | **사용자 결정(2026-11-16):** "box stand는 필요없어. 그냥 box가 생성되는 지점만 필요해." 받침대 메시를 바꾸면 박스가 겹친다. C++가 받침대를 100 cm 큐브·가운데 피벗으로 보고 위치·크기를 덮어쓰기 때문이다(`VBoRBoxStand.cpp:20-26`, BP Construction Script 뒤에 실행 — 엔진 `ActorConstruction.cpp:997`(UCS) → `:1005`(`OnConstruction`)). 이름은 박스가 아닌 물체도 받게 일반 이름으로. 받침대는 사용자가 따로 마음대로 놓는다 |
| 베이스 브랜치 | `main` (`7140d9e` 이후) |
| 작업 브랜치 | `codex/R10-object-spawn-point` |
| 리뷰 파일 | `Docs/Reviews/R10-object-spawn-point.md` |
| 에디터 작업 | **`L_Main`의 Box Stand 한 개를 새 클래스로 옮기고 위치를 올린다(MCP, 사용자 승인 2026-11-16 — §2 D6).** 그 밖의 레벨·에셋은 고치지 않는다 |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **새 클래스 `AVBoRObjectSpawnPoint`가 `AVBoRBoxStand`를 대신한다** | `Source/vBoR/Interaction/VBoRObjectSpawnPoint.h/.cpp`. `AVBoRBoxStand`(.h/.cpp)는 없앤다. `Blueprintable`. Category `Object Spawn` |
| D2 | **메시·크기·높이 칸 없음 — Actor 위치 = 물체 바닥 가운데** | `Stand`(메시 컴포넌트)·`StandSizeCm`·`StandTopHeightCm`·`OnConstruction`의 위치·크기 덮어쓰기를 없앤다. 물체는 **Actor 위치에 바닥 가운데**, Actor 회전으로 생긴다. 받침대 윗면에 이 Actor를 놓으면 박스가 그 위에 얹혀 나온다.<br>- `AVBoRCarryObject`이면 바닥 = `BoxSizeCm.Z/2` 아래(지금 `StartPose`와 같은 계산, 0.01 cm 띄움 그대로 — P1b D5/P4d D1).<br>- 그 밖의 Actor는 생성한 뒤 충돌 켜진 컴포넌트 경계 상자의 바닥을 Actor 위치에 맞춘다(구현자가 정할 것 §5). |
| D3 | **생성할 클래스 = `SpawnClass` (`TSubclassOf<AActor>`)** | 기본값 `AVBoRCarryObject::StaticClass()`(지금 `BoxClass` 기본과 같음). 비었으면 지금처럼 경고 한 번(`SpawnClass is empty; nothing will be spawned`)·생성 안 함.<br>- **`AVBoRCarryObject`일 때만** 지금 박스 동작 그대로: `LoadTarget` 넘김, `bUsePlacementAssist=false`, `bDestroyAtFloor=true`, 집어 들고 놓으면 다시 채움(P5zf D1 규칙·`Tick` 빈자리 검사 그대로), 처음 생긴 물체 추적(`SourceBox` 역할).<br>- 그 밖의 클래스: BeginPlay·`Restart` 때 한 개 생성만(다시 채움 없음). 지금 쓰는 곳은 없다 — 이름만 넓힌다. 다른 물체용 규칙은 필요해질 때 명세로 |
| D4 | **바닥 높이 = 정렬 컴포넌트 `FloorWorldZCm`** | 지금은 받침대 Actor Z를 바닥으로 썼다(`VBoRBoxStand.cpp:61`). 이제 Actor가 받침대 위에 있으므로 쓸 수 없다. 월드의 `UVBoRVehicleAlignmentComponent` **하나**의 `FloorWorldZCm`(사용자가 레벨 바닥 높이로 맞추는 값, U-17)을 `FloorHeightCm`으로 넘긴다. 없거나 둘 이상이면 0 + 경고 한 번(`Floor height from alignment unavailable — using 0 cm`) |
| D5 | **에디터 표시 — 게임에서는 안 보임** | 메시 대신 에디터 전용 표시: 방향 화살표(`UArrowComponent`, Actor +X)와 `AVBoRCarryObject`이면 그 `BoxSizeCm` 크기의 선 상자(바닥이 Actor 위치, 충돌 없음, `bHiddenInGame`). `SpawnClass`를 바꾸면 선 상자 크기가 따라온다(`OnConstruction`에서 크기만 갱신 — 물체를 생성하지 않음). 충돌 없음 |
| D6 | **이전 레벨 옮기기** | `Config/DefaultEngine.ini` `[CoreRedirects]`(설정 파일을 텍스트로 고침 — 사용자 승인 2026-11-16): 클래스 `/Script/vBoR.VBoRBoxStand` → `/Script/vBoR.VBoRObjectSpawnPoint`, 프로퍼티 `VBoRBoxStand.BoxClass` → `VBoRObjectSpawnPoint.SpawnClass`. 회사 PC 레벨의 Box Stand도 자동으로 새 클래스로 열린다(값 `BoxClass`·`LoadTarget`·태그 유지).<br>- **위치 뜻이 바뀐다**(바닥 → 박스 바닥). 그래서 `L_Main`은 Codex가 MCP로 연 뒤 그 Actor를 **+Z 75 cm**(지금 `StandTopHeightCm` 값)로 올리고 저장한다. 결과: Play 때 박스가 지금과 같은 월드 자리에 생긴다. 다른 값은 건드리지 않는다.<br>- 회사 PC 레벨은 사용자가 같은 만큼 올린다(§7) |
| D7 | **태그 `Car.BoxStand`는 그대로** | 시험과 진행 데이터가 이 태그로 찾는다. 바꾸지 않는다 |
| D8 | **Runner 다시 시작** | `VBoRScenarioRunner.cpp:776`의 Box Stand 반복을 새 클래스로. 함수 `ResetStand` → `ResetSpawn`(BlueprintCallable 유지) |
| D9 | **근사 차(P3 mock)의 받침대는 mock이 직접 그린다** | `P3VehicleMock`은 지금처럼 `RearBoxStandCm` 자리에 근사 받침대(엔진 큐브 60×60×5, 윗면 75 cm, `BlockAllDynamic`)를 **mock 자신의 메시 컴포넌트**로 두고, 생성 지점을 그 윗면(`RearBoxStandCm + (0,0,75)`)에 둔다. 개발 레벨의 모습·박스 월드 위치가 지금과 같다 |

## 1. 왜 하는가

- 받침대 메시를 바꾸면 C++가 위치·크기를 덮어써서 박스와 겹친다. 실제 차·실물 짐 받침은 Buck마다 다르다. 받침대는 사용자가 아무 메시로 놓고, 코드는 "어디에 생기는지"만 안다.
- 같은 방식으로 다른 물체(예: 다른 짐)도 생성 지점으로 놓을 수 있게 이름·칸을 일반화한다.

## 2. 범위

### 만들 것

- D1–D9, 시험 `vBoR.R10.*`(§4), 기존 시험 고침(§3 감사 표).

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| 박스(`AVBoRCarryObject`)의 잡기·놓기·착지·적재 판정 변경 | 생성 위치만 바뀐다 |
| 박스가 아닌 물체용 다시 채움·적재 규칙 | 지금 쓰는 곳 없음(D3) |
| 태그 이름·진행 데이터(`DA_Main_Scenario`) 변경 | D7 |
| `L_Main`에서 Actor 위치(+75 cm) 말고 다른 수정, 사용자 받침대 메시 배치 | 받침대는 사용자가 놓는다 |
| P1 짐 시험대(`P1LoadBench`)의 자체 받침대 | 별도 시험대, 이 클래스를 안 쓴다 |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- Play 때 박스의 월드 위치·회전·개수가 지금과 같다(`L_Main`, P3 mock 둘 다). 0.01 cm 띄움 그대로.
- 다시 채움 규칙(P5zf D1: 모든 손이 놓을 때까지 대기, 빈자리 1 cm 여유 검사), `Restart` 정리, `EndPlay` 정리 그대로.
- 박스 정책(`LoadTarget`, 배치 보조 끔, 바닥에서 사라짐) 그대로.

**기존 시험 감사(Claude 2026-11-16) — 고쳐도 되는 것은 이 표뿐이다:**

| 시험 | 지금 단언 | 고침 |
|---|---|---|
| `P5tBoxBlueprintTests.cpp:32-34`(픽스처), `:71-77` `P5tPlacement` | 박스 바닥 = 받침대 Actor + `StandTopHeightCm` + 0.01 | 박스 바닥 = 생성 지점 Actor + 0.01(같은 0.02 허용) |
| `P5t.E2MainStandClass` `:92-107` | `L_Main`에 `AVBoRBoxStand` 1개, 태그, `BoxClass == BP_Box_C` | 새 클래스 1개, 같은 태그, `SpawnClass == BP_Box_C`. **추가:** Play 때 박스 월드 위치가 옮기기 전과 같음(옮기기 전 값을 시험에 상수로 적는다) |
| `P5t.E8EmptyClass` `:205-215` | 경고 문구 `BoxClass is empty` | `SpawnClass is empty` |
| `P5zfBoxRefillTests.cpp:29-37`(픽스처) | `AVBoRBoxStand` | 새 클래스. 단언은 그대로 |
| `P5zf.E8FloorRemoval` `:166-177` | 놓는 높이 Z 35 → 받침대 Z 10을 바닥으로 사라짐 | 바닥이 D4로 바뀌므로 픽스처에 정렬 컴포넌트(`FloorWorldZCm` 10)를 두거나 놓는 높이를 새 바닥 기준으로. **기대(사라짐·다음 Tick 1개)는 그대로** |
| `P3FlowTests.cpp` P4d E1/E6–E9 `:1103-1245` | `E.Car->BoxStand`, `ResetStand` | 새 이름(`D8`·`D9`). 수치 단언(박스 바닥 75.01 등) 그대로 — mock이 생성 지점을 윗면에 두므로 같다 |
| `P4iCarBlueprintTests.cpp:259-271` | `AVBoRBoxStand`의 `LoadTarget` | 새 클래스 |
| `R3TogglesAndEventsTests.cpp:229` | `SpawnActor<AVBoRBoxStand>` | 새 클래스 |
| `P3VehicleMock.h:69`·`.cpp:335,382-385` | `BoxStand` 칸 | 새 클래스·D9 |

- 표 밖의 시험이 깨지면 멈추고 리뷰 파일 "질문"에 적는다.

## 4. 시험 — `vBoR.R10.*`

| ID | 상황 | 확인 |
|---|---|---|
| E1 | 생성 지점 Z 120, Yaw 30, `SpawnClass` = 크기 (50,40,20) 박스 | 박스 바닥 = 120.01, 가운데 XY = Actor XY, Yaw 30 |
| E2 | 에디터 표시 | 게임에서 보이는 컴포넌트·충돌 켜진 컴포넌트 없음. 선 상자 크기 = `BoxSizeCm`, 바닥 = Actor 위치. `SpawnClass` 바꾸면 크기 따라옴, 그때 물체 생성 없음 |
| E3 | 바닥 높이 | 정렬 컴포넌트 `FloorWorldZCm` 50 → 박스 `FloorHeightCm` 50. 정렬 컴포넌트 없음 → 0, 경고 1회 |
| E4 | 박스 아닌 클래스(충돌 상자 하나 가진 시험용 Actor) | 바닥이 Actor 위치에 맞게 1개 생성, `Restart`에서 다시 1개, 다시 채움 없음 |
| E5 | Redirect | 옛 이름 `/Script/vBoR.VBoRBoxStand`로 클래스를 찾으면 새 클래스(`FCoreRedirects` 또는 `FindObject` + redirect로 확인) |

## 5. 구현자가 정할 것

- 박스 아닌 Actor의 바닥 맞춤 방법(생성 뒤 충돌 경계로 한 번 옮기기 등).
- 처음 생성한 물체를 가리키는 칸 이름·타입(`SourceBox` 유지 또는 일반 이름). 시험 고침은 기계적으로.
- 선 상자 컴포넌트 종류(`UBoxComponent` 선만 등).

## 6. 완료 조건

1. D1–D9, §3 조건, §2 범위 밖 없음.
2. `vBoR.R10.E1`–`E5` 통과, §3 감사 표 밖의 기존 시험 변경 없이 `vBoR.*` 전체 통과(작업 끝에 한 번).
3. non-unity(`-Module=vBoR`)·unity 빌드 성공.
4. `L_Main` 저장 뒤 에디터 닫고 맵 로드 확인, `L_Main`에 `AVBoRBoxStand` 참조 없음.
5. 새 주석·로그·표시 영어(AGENTS.md).

## 7. 로컬에서 확인할 것 — 사용자

1. `L_Main`: 생성 지점(화살표·선 상자)이 예전 받침대 윗면에 있는지. 받침대 메시를 `StaticMeshActor`로 따로 놓고(충돌 켬 — 놓은 박스가 떨어지면 받쳐야 함) 생성 지점을 그 윗면에 맞춘다. Play → 박스가 받침대 위에 겹치지 않고 나오는지.
2. **회사 PC 레벨:** Box Stand가 자동으로 생성 지점으로 바뀐다. **75 cm(그 레벨의 `Stand Top Height Cm` 값) 올려야** 박스가 예전 자리에 나온다. 정렬 컴포넌트 `Floor World Z Cm`이 그 레벨 바닥 높이인지도 확인(D4, 박스가 바닥에서 사라지는 높이).
3. Quest 05 시험 `not-tested`.

## 8. 확인한 근거

| 쓸 것 | 위치 |
|---|---|
| 지금 받침대 | `Interaction/VBoRBoxStand.h:11-40`, `.cpp:20-26`(`OnConstruction`), `:46-51`(`StartPose`), `:53-66`(`SpawnBox`, `FloorHeightCm` `:61`), `:68-82`(`Tick` 다시 채움), `:96-110`(정리) |
| 박스 크기·바닥 칸 | `Interaction/VBoRCarryObject.h:101`(`BoxSizeCm`), `:114-117`(`bDestroyAtFloor`, `FloorHeightCm`) |
| 바닥 높이 | `Interaction/VBoRVehicleAlignmentComponent.h:84`(`FloorWorldZCm`) |
| Runner 다시 시작 | `Interaction/VBoRScenarioRunner.cpp:776` |
| 생성 순서 | 엔진 `Runtime/Engine/Private/ActorConstruction.cpp:997`(UCS), `:1005`(`OnConstruction`) |
| 근사 차 | `P3Probe/P3VehicleMock.h:65,69`, `.cpp:335,382-385` |

## 개정 이력

| 날짜 | 개정 | 내용 |
|---|---|---|
| 2026-11-16 | 처음 | 작성(사용자 결정: 받침대 없이 생성 지점만, 일반 이름) |

## 개정 1 (Claude 2026-11-16) — 기존 시험 감사 표 추가 (Codex 질문 답)

§3 감사 표에 아래를 더한다. 이 밖은 여전히 고치지 않는다.

| 시험 | 지금 단언 | 고침 |
|---|---|---|
| P4d E4 `P3FlowTests.cpp:1171-1182` | `E.Car->BoxStand->SourceBox` | 새 이름으로 참조 교체만 |
| P4d E1 `:1127` | 박스 바닥 75.01 (받침대 Actor 기준) | 수치·허용차 그대로, 기준을 mock 원래 받침대 자리(`RearBoxStandCm`, 바닥)로 |
| P4d E10 `:1141` | 장애물 = `S->Stand` 경계 | 새 mock 받침대 메시(D9) 경계로 교체만. 기대값 그대로 |
| P5t E3 `P5tBoxBlueprintTests.cpp:122` | `FloorHeightCm == 받침대 Z` | 픽스처에 생성 지점 Z와 다른 `FloorWorldZCm` 정렬 컴포넌트, `FloorHeightCm == 그 값`(D4) |
| P5t 경고 수집기 `:53` | `BoxClass is empty` 검색 | `SpawnClass is empty` 검색. E8 횟수 단언 그대로 |
