# P4d 차 뒤 구간 — 리뷰

명세: [P4d-rear-section.md](../Specs/P4d-rear-section.md) · 브랜치: `codex/P4d-rear-section` · 베이스 `main` `ae9404b`
상태: **1회차 리뷰 통과(2026-10-19, Claude — `ebfcc66`). 열린 항목 없음.**

## 리뷰 1회차 (2026-10-19, Claude — `ebfcc66`)

**결과: 통과. 열린 항목 없음.** D1–D7이 명세대로 들어갔다.
- 기존 단언은 §3이 허용한 두 곳만 바뀌었다.
  - P3b E10 끝 방향: −X → +X.
  - P4c E9: 03 `RouteTag`, 04·06의 규칙·태그·확인.
- 근사 차 소스 밖의 기존 부품(`AVBoRPoweredPanel`·`AVBoRLoadTarget`·`AP1TrunkMock`·P1 시험대)은 바뀌지 않았다. `VBoRCarryObject.h`에는 시험용 friend 한 줄만 더했다.
- 에셋은 03·04·06만 바뀌었다. E12(P4c E9)가 19개의 모든 칸을 단언한다.

**재현(에디터가 닫힌 것을 확인한 뒤, 바뀐 C++ 9개의 시각을 갱신):**
- non-unity(`-DisableUnity -Module=vBoR`): `Result: Succeeded`, 경고·오류 0. 로그 `Saved/Logs/ReviewP4dNonUnity.log`.
- unity: `Result: Succeeded`, 경고·오류 0. 로그 `ReviewP4dUnity.log`.
- `vBoR.*`: 398/398(P4d 8개 포함), `TEST COMPLETE. EXIT CODE: 0`. 로그 `ReviewP4dAll.log`. 끝난 뒤 `UnrealEditor` 프로세스 없음.

**Codex 토큰:** 7.29M(입력 7.26M 중 캐시 7.05M, 약 97%), 2026-10-18 15:00–15:12 UTC로 약 11분. 하위 에이전트는 0이다. 빌드와 전체 시험은 끝에 한 번씩 돌렸다.

**확인한 것:**
- **D1 받침**(`Interaction/VBoRBoxStand.cpp`):
  - P1 시험대 로직을 그대로 옮겼다: 출고 자세는 윗면 + 0.1 mm, 1 cm 여유 보충, 보류 중 보충 안 함, 안착 보조 끔, 바닥에서 사라짐, 박스 정리 순서(이벤트 풀기 → `ResetToPose` → `Destroy`).
  - 출고 박스가 바닥에서 사라지면 `SourceBox`가 비고, 다음 Tick에 자리가 비었으므로 새로 놓는다.
  - 받침 충돌은 `BlockAllDynamic`이다(P1은 질의만). 그래서 던진 키도 받침 위에 멈춘다. 박스끼리도 WorldDynamic으로 막으므로 박스가 받침에 서는 것은 같다.
- **D2 조립**(`P3Probe/P3VehicleMock.cpp` `CreateRearSection`):
  - 트렁크는 차 뒤 끝 (−235, 0, 0), 안쪽 +X다.
  - 게이트 축·메시·버튼은 P1 시험대와 같은 식이다.
  - 적재 영역은 트렁크 안이다(E1: 반 크기 (45, 65, 35), 중심 (−190, 0, 110)).
  - 뒤 벽 판은 트렁크 바닥(75 cm)까지 줄였다.
  - 게이트 끼임 찾기는 박스 메시만 센다(`VBoRPoweredPanel.cpp:146-150`). 그래서 받침·트렁크 판이 게이트를 막지 않는다.
- **D3 규칙**(`Interaction/VBoRScenarioRunner.cpp:221`, `:303-304`): 상태가 `Open`·`Closed`일 때만이다. E4는 실제 박스를 입구에 걸쳐 끼임으로 다시 열리게 하고, 그동안 06이 대기하는 것을 본다.
- **D4 경로:**
  - 03은 제안 점 그대로다.
  - 07은 셋째 점 Y를 −170 → −165로 바꿨다. 마지막 구간을 +X로 곧게 해서 끝 방향 단언(0.01)을 맞추려는 것이다. 재량 안이다(보고 ③).
  - E10은 두 경로를 1 cm 간격으로 표본화해 받침·뒤 키 테이블과 2D 10 cm 이상인지 확인한다.
- **D5**(`:683-719`): 모를 때만 `SampleRoute`에서 다시 읽는다. 알림은 바뀔 때만 기록한다. "밖"이 되면 다시 읽지 않는다. → 참고 1.
- **D6 R**(`:467-472`): 받침을 먼저 정리하고 게이트를 초기화한다. 대상은 단계 대상과 `Car.Liftgate` 태그다. E8·E9가 게이트 열림/닫히는 중 × 기준/동승석 네 경우에서 박스·게이트·문·키·01을 본다.
- **D7 에셋:** `inspect.set_property`로 `Steps`를 쓰고 다시 읽어 비교했다. 고치기 전 내용이 P4c 표와 같았다(보고 ⑤).

### 참고 1 — 영역을 나중에 알게 되어 경로가 보일 때 위 화살표가 갱신되지 않는다 (P4e에 묶어 고침)

- **위치:** `CheckRouteArea`(`Interaction/VBoRScenarioRunner.cpp:690-714`)가 경로를 보이게 해도 `UpdateMarker()`(`:658-681`)를 부르지 않는다. 단계에 들어갈 때는 `EnterStep`이 부른다.
- **영향:** "영역 모름"으로 들어갔다가 나중에 알게 되면, 바닥 화살표와 끝 고리는 보이지만 경로 끝 위 화살표(`MarkerArrow`)는 숨은 채로 남는다. 그 단계에 `MarkerTag`가 있으면 원래 표시도 그대로 남는다(지금 에셋에는 경로와 `MarkerTag`를 같이 쓴 단계가 없다).
- **얼마나 날지:** 03에 들어가는 순간 플레이 영역을 모를 때뿐이다. Link는 보통 처음부터 영역을 준다.
- **원인:** 내 명세 D5가 "보이게 한다"만 적고 표시 갱신을 말하지 않았다. E11도 경로만 본다.
- **할 일:** 다음 명세 P4e(같은 흐름 파일을 고침)에 넣는다. 보이게 바뀔 때 `UpdateMarker()`를 부르고, E11에 화살표 단언을 더한다. 왕복 한 번(약 3–4M 토큰)을 따로 쓸 만큼은 아니라고 봤다.

### 참고 2 — 근사 차 적재 목표 크기가 받침 박스 크기를 따르지 않는다

- `CreateRearSection`이 `LoadTarget->GoalSizeCm`을 (45, 35, 30)으로 직접 적는다. 받침 `BoxSizeCm` 기본값과 같지만 따로 움직인다.
- 지금은 영향이 없다. 안착 보조가 꺼져 있고 05는 진행자 확인이라 목표 자세를 쓰지 않는다. 박스 크기를 바꿀 일이 생기면 그때 받침 값을 쓰게 바꾼다.

### 참고 3 — VR 확인 때 볼 것 (명세 §7에 더함)

1. **R은 손에 든 박스도 없앤다.** E8이 그렇게 확인한다(사용자 ③). 참가자가 박스를 들고 있을 때 R을 누르면 박스가 손에서 사라진다. 진행자가 알고 누르면 된다.
2. **받침까지 거리:** 서는 자리 (−330, 0)에서 받침 가까운 모서리까지 가로 약 110 cm다. 한 걸음 옮겨야 한다. 멀면 `RearBoxStandCm`만 줄인다(경로와 10 cm 조건은 E10이 다시 본다).
3. **03 끝 고리에 서면 열린 게이트 아래 끝이 앞 약 25 cm·높이 약 140 cm를 지난다**(근사 게이트 70 cm). 명세 §7-2와 같다.

## Codex 구현 보고 (2026-10-19)

### ① 바뀐 파일

- `Source/vBoR/Interaction/VBoRBoxStand.h`, `.cpp`: Blueprintable 받침. 크기·윗면 높이·박스 클래스/크기·적재 영역 편집값, 1 cm 여유 보충, 보류 중 보충 금지, 바닥 소멸 설정, ResetStand/EndPlay 정리.
- `Source/vBoR/Interaction/VBoRScenarioAsset.h`: enum 끝에 PoweredOpen/PoweredClosed 추가.
- `Source/vBoR/Interaction/VBoRScenarioRunner.h`, `.cpp`: 게이트 대상 검사·상태 규칙, 영역 모름 재조회, R에서 받침 정리 후 게이트 초기화.
- `Source/vBoR/P3Probe/P3VehicleMock.h`, `.cpp`: 뒤 트렁크·게이트·적재 영역·받침 조립 및 종료 정리, 뒤 벽 판 축소, 03·07 경로.
- `Source/vBoR/Interaction/VBoRCarryObject.h`: 새 시험의 기존 잡기/놓기 함수 접근용 friend 하나만 추가. 박스 동작 변경 없음.
- `Source/vBoR/P3Probe/P3FlowTests.cpp`: P4d 시험 8개 추가, §3에서 허용한 기존 기대값 두 곳 수정.
- `Content/P3/DA_P3_Scenario.uasset`: 03 RouteTag, 04·06 Rule/TargetTag/ConfirmBy만 수정.
- 이 보고 파일. `HANDOFF.md` 결과는 git 무시 파일에 별도 기록.

### ② 명세 §6 완료 조건별 결과

1. **충족:** §2의 받침·근사 차·흐름·에셋·자동화 시험 구현.
2. **충족:** 아래 §4 대응 시험과 기존 시험 전체 통과. 최종 트리 발견 398, 완료 398, Success 398, Fail 0, `TEST COMPLETE. EXIT CODE: 0`, 프로세스 exit 0. 기존 390개와 P4d 신규 8개이며 재실행 없음.
3. **충족:** 최종 C++ 트리에서 non-unity(`-DisableUnity -Module=vBoR`)·unity 각 1회 `Result: Succeeded`, 프로세스 exit 0. vBoR 컴파일·링크 경고/오류 0. C++ 빌드 → MCP 에셋 수정·저장·에디터 종료 → 전체 시험 1회 순서.
4. **충족:** ①–⑤를 이 파일·HANDOFF·마지막 커밋 본문에 기록.
5. **유지:** Quest 3 Link/VR 및 계획서 T-02·T-04·T-06은 `not-tested`.

| 명세 §4 | 통과한 자동화 |
|---|---|
| E1·E10 | `vBoR.P4d.E1E10RearAssemblyAndRoutes`: 조립·태그 유일성·적재 공간·출고 박스·뒤 벽·경로 검사, 두 경로를 1 cm 간격으로 표본화해 받침/뒤 키 테이블과 2D 10 cm 이상, 03 끝 60–120 cm·트렁크 폭 안, 07 시작·끝 |
| E2·E3 | `vBoR.P4d.E2E3PoweredOpen`: 닫힘·대기·열리는 중에는 대기, Open 뒤 다음 처리 frame 완료, 이미 열린 단계 진입 frame에는 대기 |
| E4 | `vBoR.P4d.E4ClosedAndObstruction`: 닫힘에서만 완료, 실제 박스 끼임으로 재개방한 경우 대기 |
| E5 | `vBoR.P4d.E5TargetValidation`: 두 규칙의 종류 불일치·없는 태그 시작 차단 |
| E6·E7 | `vBoR.P4d.E6E7SupplyFloorResetCleanup`: 출고 공간 겹침·1 cm 경계·보류 중 보충 금지, 비면 1개 보충, 바닥 소멸·ResetStand·EndPlay 정리 |
| E8·E9 | `vBoR.P4d.E8E9Restart`: 트렁크 2개+손 1개 정리, 게이트 열림/닫히는 중 × 기준/동승석 시점 4경우, 새 박스 1개·게이트 닫힘·문 잠김·키 유지·복귀 뒤 01 |
| E11 | `vBoR.P4d.E11UnknownAreaRecovery`: 계속 모름 1회 기록, 앎/안이면 표시·알림 제거, 앎/밖이면 1회 알림 변경, 밖 판정 후 재조회 안 함 |
| E12 | 기존 `vBoR.P4c.E9ScenarioAsset`: 19개 단계의 모든 필드 검사, 허용한 03·04·06 기대값만 변경 |
| E13 | `vBoR.P4d.E12E13AssetRearFlow`: 실제 에셋 03 → 04 열기 → 05 진행자 확인 → 06 닫기 → 07 경로 표시 |
| E14 | 전체 `vBoR.*` 398/398 |

### ③ 명세와 다르게 한 것과 이유

- 제품 동작의 명세 이탈 없음. P1 시험대·트렁크·게이트·박스·적재 영역 동작은 변경하지 않고 재사용.
- 재량 범위 D4: 07 제안 점 중 셋째 Y를 −170 → −165 cm로 정함. 마지막 구간을 +X로 맞춰 허용된 끝 방향 단언을 충족. 끝점 (−65,−165)은 유지.
- 허용된 기존 단언 ① P3b E10 끝 방향 −X → +X, ② P4c E9의 03 RouteTag 및 04·06 Rule/TargetTag/ConfirmBy 변경. 그 밖의 기존 단언 변경 없음.
- 신규 시험은 기존 FP3World·합성 Stage XR 준비를 재사용하려고 P3FlowTests.cpp에 둠. CarryObject에는 시험 접근 friend만 추가.
- MCP capability: `inspect.get_property`로 전체 Steps 읽기 → `inspect.set_property`로 Steps 구조체 문자열 배열 쓰기·자동 저장(`saved=true`) → 다시 읽어 원문 비교. `control_editor.save_all`은 이 에셋만 지정했으며 이미 저장돼 noop·다른 dirty package 0. `control_editor.open_asset`·`control_editor.screenshot`으로 에셋 창 확인.
- Python 없이 기본 속성 capability가 구조체 배열을 처리함. 읽어 둔 원문을 보존해 03·04·06의 지정 필드만 치환했으며 다른 16개 단계는 원문과 동일함.
- 하위 에이전트·자체 리뷰 루프·의도적 red 실행 없음. push·PR·머지 없음. 레벨·다른 에셋·프로젝트 설정·로컬 도구 플러그인 변경 없음.

### ④ API·헤더 확인

새 ISDK·MetaXR API 직접 호출 없음. 기존 부품 API는 아래 헤더로 확인하고 실제 로컬 빌드로 검증함. 추정 API 없음.

- `Source/vBoR/Interaction/VBoRPoweredPanel.h:53–60`: RequestOpen/RequestClose/ResetPanel/GetState/GetAngleDeg/GetObstructionCount. 축·메시·버튼 편집값은 :69–83.
- `Source/vBoR/Interaction/VBoRCarryObject.h:78–89`: ResetToPose·GetCollisionBoundsInPose·GetCarrySession. :91–115 크기/보조/바닥/LoadTarget, :149 OnGrabbed.
- `Source/vBoR/Interaction/VBoRLoadTarget.h:18–31`: GoalSizeCm·bShowGuide·LoadingRegion.
- `Source/vBoR/P1Probe/P1TrunkMock.h:18–30`: 안쪽 +X, 트렁크 치수·바닥 높이.
- 엔진 루트 `C:/Program Files/Epic Games/UE_5.7/Engine/Source/`:
  - `Runtime/Engine/Classes/Engine/World.h:3735`: SpawnActorDeferred.
  - `Runtime/Engine/Classes/GameFramework/Actor.h:2096,2104,2330`: Tick prerequisite 추가/해제·Destroy.
  - `Runtime/Engine/Classes/Components/SceneComponent.h:429,468,593`: 상대 위치/scale·월드 transform.
  - `Runtime/Engine/Classes/Components/BoxComponent.h:39,47`: SetBoxExtent·GetUnscaledBoxExtent.
  - `Runtime/Engine/Classes/Components/SplineComponent.h:570,737,745`: SetSplinePoints·거리별 위치/방향.
  - `Plugins/Runtime/XRBase/Source/XRBase/Public/HeadMountedDisplayFunctionLibrary.h:403`: Stage GetPlayAreaBounds.

### ⑤ 로컬 확인 결과와 남은 사용자 시험

- 빌드 로그: `Saved/Logs/P4dNonUnityBuild.log`, `P4dUnityBuild.log`. 두 실행 성공, exit 0, vBoR 경고/오류 0. 로컬 McpAutomationBridge/프로젝트의 StructUtils 폐기 안내 각 2줄은 별도 잡음이며 도구 소스는 수정하지 않음.
- 자동화 로그: `Saved/Logs/P4dAllAutomation.log`(:1434 발견 398, :14386 종료 코드 0), 콘솔 `P4dAllAutomationConsole.log`. 398/398 Success·Fail 0. 알려진 샘플 InfoCard CDO·XR 확장/미연결·합성 World 정리 로그는 Workflow §2.5 범위이며 시험 실패와 구분. P0Buck 간헐 실패도 이번에는 발생하지 않음.
- **에셋 수정 전 P4c §0-1 표와 19개 전체 필드 일치.** 원본 `Saved/Logs/P4dAssetBefore.json`, 재조회 결과 `P4dAssetAfter.json`. 변경된 단계는 03·04·06뿐이며 나머지 16개는 원문 비교 일치.
- 에디터 로그 `Saved/Logs/P4dAssetEditor.log`. 확인 화면 `Saved/Screenshots/P4dScenarioAsset.png`: 에셋 창 19 elements·All Saved. 펼친 각 행의 값은 MCP 전체 Steps 재조회와 자동화 에셋 시험으로 확인. 사용자 레벨은 열거나 저장하지 않음.
- 시작 에디터 닫힘. 에셋 저장 후 띄운 PID 48936을 정상 종료 요청으로 닫았고, 자동화 완료 뒤 UnrealEditor/UnrealEditor-Cmd 프로세스 없음.
- 사용자가 에디터/Quest 3에서 볼 것: 기존 L_P3_Flow의 03 동승석 쪽 반 바퀴와 07 뒤→운전석 경로, 회사 방 영역 판정, 게이트 열기/닫기 버튼 도달·머리 간격, 받침의 손 도달·박스 보충/적재·끼임 재개방, R 복귀·초기화. 실제 Blueprint/레벨 표시·손 추적·조작감과 T-02·T-04·T-06은 `not-tested`. 합성 시험을 기기/실차 평가 증거로 취급하지 않음.

## 질문

없음. Claude 첫 리뷰 대기.

## 개정 이력

| 날짜 | 회차 | 내용 |
|---|---|---|
| 2026-10-19 | 1회차 | 통과(`ebfcc66`), 열린 항목 없음. 참고 1(영역을 나중에 알 때 표시 갱신 — P4e에 묶음), 참고 2(적재 목표 크기), 참고 3(VR 셋) |
