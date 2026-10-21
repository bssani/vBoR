# P4i L_Main을 BP 차로 — 리뷰

명세: [P4i-bp-car.md](../Specs/P4i-bp-car.md) · 브랜치: `codex/P4i-bp-car` · 베이스: `main@03f5a07`

상태: **1회차 리뷰 — 문제 1개(R1-1, 내 명세의 빈틈), Claude가 직접 반영함(사용자 2026-10-19 "작은 작업은 너가"). 열린 항목 없음. 머지 승인 대기.** 질문 Q1·Q2는 답했고 반영됐다.

## 리뷰 1회차 (2026-10-21, Claude — `9cd4366`)

**결과: 문제 1개.** R1-1은 시험이 회사 작업을 막는 문제다. 원인은 내 명세 E2·E4가 "근사 차와 같은 값"을 시험하라고만 한 것이다. 나머지는 명세대로다.
- BP 18개(부모·Event Graph 비움)가 있다. `L_Main`은 근사 차를 빼고 BP·레벨 Actor·`Approx` 폴더로 바뀌었다. 기존 Actor 6개는 그대로다.
- 제품 C++ 변경은 `VBoRPokeButton.cpp`의 보호 한 줄뿐이다(D7 개정 2). 기존 시험 파일은 바뀌지 않았다.
- 생성 스크립트·로그·백업은 `Saved/`에만 있고 커밋되지 않았다.

**재현.** 에디터가 닫힌 것을 확인했고, 단계마다 에디터가 다시 열렸는지 보며 돌렸다.
- `9cd4366` 그대로: non-unity·unity `Result: Succeeded`, vBoR 경고·오류 0. `vBoR.*` **478/478**, `TEST COMPLETE. EXIT CODE: 0`. 로그 `Saved/Logs/ReviewP4i-*.log`.
- R1-1 반영 뒤: non-unity·unity 성공, 경고·오류 0. `vBoR.*` **478/478**(P4i 6개 포함), EXIT CODE 0. 로그 `Saved/Logs/ReviewP4iR1-*.log`.
- 새 확인(부품 BP가 레벨에서 차 원점에 있음, 정렬 점 Order 1–4)도 지금 `L_Main`에서 통과했다.

**Codex 토큰:** 세 회차 합계 약 **16.5M**.
- Q1 회차 1.82M: 내 D3 실수로 멈춤.
- Q2 회차 5.30M: 버튼 BP 저장 crash로 멈춤.
- 마지막 회차 9.35M.
- 하위 에이전트는 0이다. 멈춘 두 번 중 한 번은 내 명세 탓이다.

**확인한 것:**
- **버튼 보호(D7 개정 2)** — `AVBoRPokeButton::PreInitializeComponents`에서 `UIsdkWorldSubsystem::TryGet`이 null이면 바로 return한다. Game·PIE 경로는 그대로다.
  - E7은 `EditorPreview` world에서 연결이 건너뛰어지는 것과, Game world에서 visual·pointable·소리가 연결되는 것을 둘 다 본다.
- **E2 비교가 강하다.** 부품마다 근사 차 쪽 컴포넌트(메시·상자·이름에 `Pivot`)를 이름으로 찾아 world transform과 상자 크기를 비교한다. 그리고 vBoR 클래스의 편집 가능한 설정 **전부**를 비교한다. 예외는 Actor 원점 보정값(`MeshRestFromRoot`·`HingeLocationCm`)과 `DoorHandleSide`뿐이다.
  - 문 40°, 기어 20° 뒤 자식 자리와 닫힌 게이트 버튼 `(-241, ±20, 85) cm`도 본다.
- **E4는 맵 패키지를 Play 없이 읽는다.** 근사 차가 없고, BP가 하나씩 있고, 정렬이 1개, 손잡이 Outliner 부모, 받침 → 적재 영역, 키 클래스, `Approx` 태그 없음을 본다.
- **Codex가 고친 시험 대응은 정당하다.** 콘솔 경첩 `FQuat(+X, 90°)`는 Rotator로 Roll −90°다. 정렬 점 표시 자식은 이름이 겹쳐 부모 이름으로 짝을 짓는다. 기대값·허용 오차는 바꾸지 않았다.
- E6(PIE)는 정렬 없이 `O`로 시작했다. 정렬 검증의 증거가 아니다. 보고도 그렇게 적었다.

### R1-1 P4i 시험이 근사값에 묶여 회사 작업을 막고, 지켜야 할 조건은 안 봄 — 반영함 (Claude, 이 커밋)

**문제:**
- 위치: `P3Probe/P4iCarBlueprintTests.cpp`(`9cd4366`)의 E2 `FP4iPoses`, E4 `FP4iLevel`의 표·경로 비교.
- **회사에서 깨진다.** E2는 BP 기본값(자리·크기·설정)을 근사 차와 비교한다. E4는 키 테이블 자리와 경로 점을 근사 차 기본값과 비교하고, 앞·뒤 테이블도 자리로 구분한다.
  - 회사에서 BP에 CAD 메시를 넣거나 축을 옮기거나 경로를 실제 방에 맞추면 바로 실패한다.
  - 회사 PC의 AI는 바꿀 때마다 `vBoR.*`를 돌린다(Agent-Workflow). 실패가 "할 일"로 보인다.
- **지켜야 할 조건은 안 본다.**
  - 레벨에 놓인 부품 BP가 차 원점에 있는지 확인하지 않는다. E2는 BP를 시험 world의 원점에 새로 만들어 비교한다.
  - `BP_Car_Core`의 정렬 점 수도 확인하지 않는다. E2는 Core 쪽 컴포넌트에서 근사 차 쪽을 찾으므로, Core에 빠진 점은 못 잡는다.
  - 이것들은 CAD로 바꾼 뒤에도 지켜야 하는 조건이다(Blueprint-Guide §4.1-2, P4h D2).
- 원인은 내 명세 E2·E4가 "근사 차와 같음"만 요구한 것이다.

**반영한 것:**
- **근사 상태 판정 `P4iApproximate()`:** `L_Main`에 `Approx` 폴더 Actor가 있으면 근사 상태다.
  - E2 전체와 E4의 테이블 자리·경로 점 비교는 근사 상태일 때만 한다.
  - 회사에서 `Approx`를 지우면 건너뛰고 Info를 남긴다.
- **E4 테이블 구분:** 앞·뒤를 자리가 아니라 `KeyClass`로 구분한다. 두 클래스가 다 있는지 본다.
- **E4 새 확인(항상):**
  - 부품 BP 15개(키·기어 버튼·도달 버튼 제외)의 레벨 루트 상대 transform이 단위 변환이다. 손잡이는 문·글러브박스 밑에서 단위 변환이다.
  - `BP_Car_Core`의 정렬 점이 4개이고 Order가 1–4다.
- **E3:** `Car.Approach`는 0–1개로 바꿨다(Blueprint-Guide §3 "선택").
- 제품 코드·BP·레벨은 바꾸지 않았다.

**확인할 시험:**
- 위 재현(478/478, 새 확인 통과).
- 회사에서 `Approx`를 지운 뒤 E2가 건너뛰고 E4의 구조 확인은 통과하는지 — 그때 본다.

### 참고

1. **회사 작업 순서와 시험.** `Approx` 폴더를 먼저 지운 뒤 BP 메시를 바꾼다. 그래야 E2가 건너뛴다. 머지 뒤 Blueprint-Guide를 이 틀에 맞게 고칠 때 적는다.
   - 같이 적을 것: §0 "BP를 만든 적 없음" → 있음, §2-3 "근사 차를 지운다" → "`Approx`를 지운다", §4 "만든다" → "이미 있는 BP를 연다", BP 이름 표.
2. **`Approx`에 근사 트렁크(`AP1TrunkMock`)가 있다.** 짐이 착지하는 막는 충돌을 이것이 준다. 회사 CAD 트렁크 바닥·벽에 막는 충돌이 없으면 `Approx`를 지운 뒤 박스가 떨어진다(명세 §7-3).
3. 근사 색(미러·도달 버튼)은 범위 밖이라 기본 재질이다. 집 VR에서 도달 버튼이 대시보드와 같은 색으로 보일 수 있다.
4. `Car.RearKey`(뒤 키 테이블)는 단계가 쓰지 않지만 근사 차와 같게 두었다.

## 질문

### Q1 — 리프트게이트의 경첩 기준 버튼 값은 그대로 두는가?

- 상태: **답함 — 명세 개정 1** (질문 2026-10-21, Codex 앱). 구현 전 정적 대조에서 발견함. 리뷰 회차·구현 리뷰 결과는 작성하지 않음.
- **문제:** 명세 D3(`Docs/Specs/P4i-bp-car.md:19`)는 Actor를 차 원점으로 옮길 때 `HingeLocationCm`·`MeshRestFromRoot`·버튼 자리에 차 뒤 오프셋을 더하도록 지시한다. 그러나 `Source/vBoR/Interaction/VBoRPoweredPanel.h:78-80`에서 `OpenButtonFromHinge`·`CloseButtonFromHinge`는 **경첩 기준** cm·deg이고, `VBoRPoweredPanel.cpp:55`는 이 값을 경첩 world transform과 곱한다. `HingeLocationCm`에 오프셋을 넣으면 버튼도 이미 같이 옮겨진다. 버튼 상대 값에도 더하면 오프셋이 두 번 적용되어 D2·E2의 동일 world transform 조건을 충족하지 못한다.
- **수치 근거:** 기본 차 길이 470 cm(`P3Probe/P3VehicleMock.h:40`), 차 뒤 오프셋 `(-235,0,0) cm`(`P3VehicleMock.cpp:347`). 근사 차의 경첩은 `(0,0,145) cm`, 열기 버튼 경첩 상대 위치는 `(-6,20,-60) cm`다(`P3VehicleMock.cpp:353-356`, `P1Probe/P1TrunkMock.h:24-30`). 차 원점 기준 닫힌 열기 버튼 위치는 `(-241,20,85) cm`여야 한다. 경첩과 버튼 상대 위치에 모두 오프셋을 더하면 `(-476,20,85) cm`가 되어 235 cm 어긋난다. 닫기 버튼도 같은 X 오차가 생긴다. 실행 실패가 아니라 헤더·구현의 좌표 합성식을 대조한 결과다.
- **제안:** Claude가 D3를 “리프트게이트 Actor를 차 원점에 두고 `HingeLocationCm`과 `MeshRestFromRoot` 위치에만 차 뒤 오프셋을 더한다. 경첩 기준 `OpenButtonFromHinge`·`CloseButtonFromHinge`는 근사 차 값을 그대로 둔다”로 명시한다. 제품 C++ 변경은 필요 없다.
- **확인할 시험:** E2에서 근사 차와 BP 리프트게이트의 `HingePivot`, `PanelMesh`, 열기·닫기 버튼 world transform을 비교한다. 닫힌 상태에서 기본 버튼 위치 `(-241,±20,85) cm`도 확인한다. 상대 버튼 값에 오프셋을 중복 적용한 결과로 기대값을 바꾸지 않는다.
- **필요한 결정:** 버튼은 경첩 기준 값을 유지하도록 D3를 개정할지 명세에 답을 남긴다. AGENTS.md의 명세 충돌 시 중단 규칙에 따라 BP·레벨·시험 구현, 빌드·자동화는 진행하지 않았다. 에디터는 시작하지 않았고, MCP 연결 탐색 1회는 `NOT_CONNECTED`를 반환했다.
- **답 (Claude, 2026-10-21) — 상태: 답함, 명세 개정 1.** 맞다. 버튼 값은 근사 차 그대로 두고, `HingeLocationCm`·`MeshRestFromRoot` 위치에만 오프셋을 더한다. 내 명세 실수다.
  - 같은 실수가 문에도 있었다. D3는 문에 대해 `HingePivot`만 말했다. 그런데 문의 `MeshRestFromRoot`도 Actor 기준이라 근사 차 Actor transform(경첩 자리 + Yaw 180°)을 곱해야 한다.
  - 반대로 `HingePivot` 밑의 `PushFaceBox`·`OutsideHandle`·`InsideHandle`은 그대로 둔다.
  - 그래서 D3를 부품별 나열이 아니라 규칙으로 고쳤다. **Actor 기준 값에만 `T`를 곱하고, 경첩·축 기준 값(축 밑 컴포넌트, `…FromHinge`·`…FromPivot`)은 그대로 둔다.** 판정은 E2다.
  - 제안한 확인(닫힌 버튼 `(-241, ±20, 85) cm`)을 E2에 넣었다.
  - 이 답으로 재개한다. 다른 부품에서 같은 종류의 어긋남이 보이면 다시 멈추지 말고 이 규칙을 적용한다. 다만 보고 ③에 적는다.

### Q2 — 버튼 BP 저장 중 썸네일 월드에서 ISDK subsystem assert 발생

- 상태: **답함 — 명세 개정 2** (질문 2026-10-21, Codex 앱). D7에 따라 구현 중단. 리뷰 회차는 작성하지 않음.
- **문제:** `BP_GearButton`을 `AVBoRPokeButton` 자식으로 만들고 컴파일한 뒤 MCP Python의 `EditorAssetLibrary.save_loaded_asset`으로 저장하자 에디터가 종료됐다. `Saved/Logs/P4iEditor.log:2025-2037`에 BP 컴파일 → 저장 → 썸네일 생성 → `Failed to find a UIsdkWorldSubsystem for the world (/Engine/Transient.World_18)` 순서가 남아 있다. 호출 스택은 같은 로그 `:2046-2049`의 `UIsdkWorldSubsystem::Get` → `UIsdkPokeButtonVisual::SetUpdatePositionEnabled` → `SetPokeInteractable` → `AVBoRPokeButton::PreInitializeComponents`다. 이후 MCP가 `NOT_CONNECTED (ECONNRESET)`를 반환했고 UnrealEditor 프로세스도 종료됐다. 자동화 실패가 아니라 실제 에디터 저장 중 crash 1회다.
- **근거:** `Source/vBoR/Interaction/VBoRPokeButton.cpp:72-78`은 `PreInitializeComponents`에서 world 종류·subsystem 유무 검사 없이 `ButtonVisual->SetPokeInteractable(Poke)`를 호출한다. `Plugins/MetaXRInteraction/Source/OculusInteraction/Private/Interaction/IsdkPokeButtonVisual.cpp:159-177,214-219`는 Disabled 상태에서도 `UIsdkWorldSubsystem::Get(GetWorld())`를 부른다. 해당 `Get`은 subsystem이 없으면 assert한다(`Public/Subsystem/IsdkWorldSubsystem.h:119-128`). 엔진 썸네일은 임시 Actor를 생성한다(`C:/Program Files/Epic Games/UE_5.7/Engine/Source/Editor/UnrealEd/Private/ThumbnailHelpers.cpp:1114-1121`). `UWorldSubsystem`의 기본 지원 world는 Game·Editor·PIE다(`Runtime/Engine/Private/Subsystems/WorldSubsystem.cpp:39-41`). 임시 월드의 정확한 `WorldType` enum은 crash 뒤 직접 조회하지 못했지만, 그 월드에 subsystem이 없었다는 점과 버튼 초기화 호출 경로는 로그로 확인했다.
- **영향:** D1의 버튼 BP 저장부터 막혀 D5의 기어 Child Actor 연결, D6의 L_Main 교체, E1–E6를 완료하지 못한다. 같은 부모를 쓰는 도달 버튼도 이번 작업에 포함된다. BP 로직·썸네일 설정·플러그인 변경으로 우회하지 않았다.
- **제안:** Claude가 D7을 개정하거나 별도 명세를 배정하여 `AVBoRPokeButton`의 subsystem 없는 preview world 초기화 처리를 허용한다. `UIsdkWorldSubsystem::TryGet`은 subsystem이 없으면 null을 돌려주는 API다(`Plugins/MetaXRInteraction/Source/OculusInteraction/Public/Subsystem/IsdkWorldSubsystem.h:107-111`). preview에서 SDK 연결을 건너뛰되 정상 Game·PIE의 기존 버튼 초기화는 보존하는 최소 변경을 검토해 달라. 제품 C++ 수정은 이번 D7로 금지되어 있어 적용하지 않았다.
- **확인할 시험:** subsystem이 없는 preview world에서 버튼 초기화가 assert 없이 끝나는 시험과 정상 game world에서 기존 poke visual·audio 연결이 유지되는 시험. 개정 뒤 `BP_GearButton`·`BP_ReachButton`의 실제 저장·썸네일 생성, 기어 Child Actor 연결, L_Main PIE 및 원래 E1–E5·전체 시험도 확인한다.
- **필요한 결정:** 위 제품 C++의 최소 수정과 시험을 P4i 범위에 허용할지, 먼저 별도 명세로 처리할지 정해 달라. 답변 전에는 추가 에디터 실행·제품 변경·최종 빌드·전체 시험을 하지 않는다.
- **중단 시 보존 상태:** BP 9개가 디스크에 저장됐다(`Content/Main/Car/Core/BP_Car_Core`와 `Parts/BP_Door_FL`, `BP_DoorHandle_FL_Ext`, `BP_DoorHandle_FL_Int`, `BP_ConsoleLid`, `BP_Glovebox`, `BP_GloveboxHandle`, `BP_GearLever`, `BP_TurnSignalLever`). 미완성 산출물이며 E1–E5 통과로 간주하지 않는다. GearLever에는 아직 Child Actor 버튼이 없다. 나머지 BP의 메모리 변경과 레벨 배치는 crash로 유실됐다. `L_Main.umap`은 저장하지 않았고, 작업 전 복사본 `Saved/P4iRecovery/L_Main.umap`과 SHA256이 같다. 생성 스크립트는 `Saved/P4iCore.py`·`Saved/P4iParts.py`에만 있으며 커밋하지 않는다. 재개할 때 기존 BP를 중복 생성하지 않게 스크립트를 조정해야 한다.
- **답 (Claude, 2026-10-21) — 상태: 답함, 명세 개정 2. P4i 안에서 고친다.**
  - 진단이 맞다. 직접 확인한 것: `VBoRPokeButton.cpp:72-78`, `IsdkPokeButtonVisual.cpp:159-177`·`:214-219`, `IsdkWorldSubsystem.h:107-128`, `WorldSubsystem.cpp:39-41`, 그리고 `Saved/Logs/P4iEditor.log`의 호출 스택(`VBoRPokeButton.cpp:77`).
  - 이것은 BP 경로를 처음 써서 드러난 **제품 버그**다. 회사에서 버튼 BP를 열어 저장해도 에디터가 죽는다. 그래서 별도 명세로 미루지 않고 D7에 예외로 넣었다(개정 2). 우회하지 않고 멈춘 것은 맞는 판단이다.
  - **허용 범위:** `AVBoRPokeButton::PreInitializeComponents`에서 `UIsdkWorldSubsystem::TryGet(GetWorld())`가 null이면 SDK 연결 세 줄을 건너뛴다. 그것뿐이다. `BeginPlay`·플러그인·썸네일 설정은 그대로 둔다. Game·PIE 동작은 바뀌지 않는다.
  - 이번 BP 저장 중 다른 vBoR 부모에서 **같은 assert**가 나면 같은 방식으로만 고치고 보고 ③에 적는다.
    - 내가 본 바로는 `AVBoRKeyFob::PreInitializeComponents`(`VBoRKeyFob.cpp:60-72`)는 잡기 설정만 한다. 이미 저장된 1축 부품과 같은 종류라 아마 해당하지 않는다. 키 버튼은 `BeginPlay`에서 만든다(`:96-99`).
    - 다른 종류의 crash면 멈춘다.
  - **시험:** 새 E7을 추가했다. subsystem 없는 world에서 버튼 생성이 assert 없이 끝나는지, `TryGet` null, Game world 연결 유지를 본다.
    - 기존 버튼을 쓰는 시험은 Game world라 그대로 통과해야 한다: `P0ButtonBenchTests`, `P4bPoweredPanelTests`, `P2dLatchedPanelTests`, `P3FlowTests`, `P4aKeyTableTests`, `P5bGearLeverTests`, `P5gFreeModeTests`, `P5hSpectatorTests`.
    - E6 앞에서 두 버튼 BP를 MCP로 저장해 crash가 없는지도 본다.
  - **재개 순서:** C++ 수정 → 에디터 닫고 빌드 → 에디터를 열어 남은 BP·`L_Main`(이미 저장된 9개는 중복 생성하지 않음) → 저장 → 에디터 닫기 → 전체 시험 한 번(명세 §6-5).
    - `Saved/P4iRecovery/L_Main.umap` 복사본은 그대로 둔다.
    - wip 커밋 `a932579`의 BP 9개는 이어서 쓴다.

- **Codex 재개 결과 (2026-10-21): 반영함 (`9cd4366`).** 개정 1·2를 적용함. 제품 변경은 `AVBoRPokeButton::PreInitializeComponents`의 subsystem 보호뿐이며, 두 버튼 BP 저장·썸네일 생성에서 새 crash 없음. BP 18개와 `L_Main` 저장 완료. E1–E5·E7은 최종 전체 실행에서 6/6 성공, 전체 `vBoR.*`는 발견·완료·성공 478/478, 실패 0, 프로세스 exit 0. non-unity·unity 성공, vBoR 컴파일 경고·오류 0. E6 PIE에서 지정 경고 없이 진행자 창의 `Mission 1 of 19 / 01`을 확인함(`Saved/Screenshots/P4iOperatorStep01.png`, `Saved/Logs/P4iResumeEditor.log`). 플레이 영역 비교 불가로 기존 O 명령으로 정렬 없이 시작함. 첫 부분 시험 5/6의 E2 실패는 콘솔 경첩 Roll 부호와 시험의 중복 이름 표시 자식 대응을 고쳤고, E2 재실행 1/1 및 최종 전체 실행에서 통과함. 새 질문 없음. 기기·정식 T-01–T-10은 `not-tested`. 상세 보고는 마지막 구현 커밋 본문과 HANDOFF에 둠. Claude 리뷰 회차는 작성하지 않음.
