# P4c 단계 목록·키 규칙 — 리뷰

명세: [P4c-step-list-keys.md](../Specs/P4c-step-list-keys.md) · 브랜치: `codex/P4c-step-list-keys` · 베이스 `main` `3b02f1e`
상태: **R1-1 반영함 (fix: record start after alignment reapply). Claude 재리뷰 대기.**

## 리뷰 1회차 (2026-10-18, Claude — `0a26f34`)

**결과: 열림 1건(R1-1).** 나머지는 명세대로다.
- D1 규칙 둘, D2 키 태그, D4·D5 에셋 19개가 들어갔다.
- D3 시작 기록은 한 경로(A로 흐름 시작)만 빠졌다. 이 경로는 내 명세 D3의 목록에서도 빠져 있었다.
- 기존 단언은 바뀌지 않았다. 생성자 기본 단계·TV 기본표·레벨·다른 에셋도 바뀌지 않았다.

**재현(에디터가 닫힌 것을 확인한 뒤, 바뀐 C++ 7개의 시각을 갱신):**
- non-unity(`-DisableUnity -Module=vBoR`): `Result: Succeeded`, 경고·오류 0. 로그 `Saved/Logs/ReviewP4cNonUnity.log`.
- unity: `Result: Succeeded`, 경고·오류 0. 로그 `ReviewP4cUnity.log`.
- `vBoR.*`: 389/389(P4c 7개 포함), `TEST COMPLETE. EXIT CODE: 0`. 로그 `ReviewP4cAll.log`. 끝난 뒤 `UnrealEditor` 프로세스 없음.
- E9는 저장된 에셋을 새 프로세스에서 읽어 19개의 모든 칸을 단언한다. 그래서 바이너리 `.uasset` 내용도 이 실행으로 확인됐다.

**Codex 토큰:**
- 1차(Q1에서 멈춤): 3.70M, 14:07–14:14 UTC.
- 재개: 5.62M(입력 5.60M 중 캐시 5.44M, 약 97%), 14:18–14:25 UTC.
- 합계 약 9.3M이다. 하위 에이전트는 0이다. 빌드와 전체 시험은 끝에 한 번씩 돌렸다.

**확인한 것:**
- **D1 규칙**(`Interaction/VBoRScenarioRunner.cpp:218`, `:254-259`, `:299-304`):
  - 대상은 `P3Actor` → `AVBoRKeyFob`로 바꾼다. 키가 아니면 기존 "종류 불일치"로 시작을 막는다(E7).
  - 키 이벤트는 `AddUniqueDynamic`으로 묶는다. 다시 초기화할 때와 `EndPlay`에서 푼다(`UnbindKeys`, `:720-729`, `:173`, `:188`). 01·02가 같은 키를 가리켜도 한 번만 묶인다(E6).
  - 기준값은 `EnterStep`에서 잡는다(`:335`). 01에서 누른 것과 뒤 키를 누른 것은 세지 않는다(E5).
- **D2**(`P3Probe/P3VehicleMock.cpp:101-102`, `:254`): 키 액터 태그 둘, 버튼 태그 넷 그대로다(E11).
- **D3**(`:321-328`, `:466-472`):
  - 01을 떠날 때만 "거치지 않음"을 남긴다(`StepIndex == 0 && Index != 0`).
  - 다시 기다리는 것은 `bNewAttempt`를 켠 호출뿐이다: 흐름 시작 `:277`, O `:427`, R 기준 시점 `:472`, R 뒤 복귀 `:413`.
  - R은 `bStartHeadPending`을 먼저 끄므로 R 순간에 줄이 남지 않는다(E8.3·E8.4).
  - ←는 기본값(끔)이라 같은 시도를 유지한다(E8.1·E8.2).
- **D4·D5 에셋:** MCP `inspect.set_property`로 `Steps`만 썼다. 고치기 전 사본·SHA256이 리뷰 파일에 있다. E9가 §0-1 표 전체(문구·규칙·태그·확인·`MarkerTag`·`RouteTag`·기준 시점·TV 문구·자리값)를 단언하고, E10이 근사 차와 맞춰 시작 차단 0·TV 전체 19를 본다.

### R1-1 — 정렬 다시 적용(A)으로 흐름이 시작되면 시작 기록을 기다리지 않는다 — 반영함 (fix: record start after alignment reapply)

**문제**
- 위치: `Interaction/VBoRScenarioOperator.cpp:189`의 `FinishReapply`가 `EnterStep(0)`을 인자 없이 부른다. 기본값은 "새 시도 아님"이다(`Interaction/VBoRScenarioRunner.h:150`).
- 발생 조건:
  - 첫 Tick에 정렬이 적용돼 있지 않으면 흐름이 기다린다(`VBoRScenarioRunner.cpp:275-276`).
  - 그때 진행자가 A를 누르고 정렬이 적용되면 흐름이 시작된다(기존 시험 `vBoR.P5i.E16ReapplyStartsWaitingFlow`, `P3Probe/P5iOperatorPanelTests.cpp:266-271`).
  - `InitializeFlow`가 `bStartHeadPending`을 꺼 두었으므로(`:188`) 이 경로로 시작한 01은 기록을 기다리지 않는다.
- 영향: 시작 표시에 서도 "시작 위치"가 남지 않는다. 표시를 안 지나고 01을 떠나도 "거치지 않음"이 남지 않는다. 그 시도의 시작 기록이 통째로 빠진다.
  - P4c 전에는 `EnterStep(0)`이 늘 기다리게 했으므로, 이것은 퇴행이다. 명세 §3("처음 지나는 경우가 지금과 같다")에 어긋난다.
- 원인: 내 명세 D3이 흐름 시작 경로를 "InitializeFlow, O"로만 적고 A를 빠뜨렸다. Codex는 그 목록대로 했다. 명세 D3도 고쳤다(개정 이력 "1회차 리뷰").

**제안**
- `FinishReapply`의 호출을 `EnterStep(0, true)`로 바꾼다. 흐름이 아직 시작 전일 때만 지나는 줄이라(`!bRunning && !bFreeMode`) 실행 중인 흐름에는 영향이 없다.
- 다른 `EnterStep(0)` 호출은 모두 확인했다(grep): `:277`·`:413`·`:427`·`:472`는 켬, `:357`(다음 단계)·`:458`(←)은 끔이 맞다. 남은 것은 이 한 줄이다.

**확인할 시험**
- P4c E8에 한 경우를 더한다(또는 새 시험).
  - 준비: 정렬이 "확인 필요"인 채로 시작해 흐름이 기다리게 한다. P5i E16의 준비(`P5iOperatorPanelTests.cpp:266-271`)와 같다.
  - A → 전환 끝 → 01 시작 → 기다림 켜짐.
  - 시작 표시에 서면 "시작 위치" 1줄. 또는 표시를 안 지나고 →로 01을 떠나면 "거치지 않음" 1줄.
- P5i E16의 단언은 바꾸지 않는다.

### 참고 1 — VR 확인 때 볼 것 (명세 §7에 더함)

1. **R은 키를 테이블로 돌려놓지 않는다**(명세 범위 밖). R을 누를 때 참가자가 키를 쥐고 있으면, 01은 다음 frame에 바로 끝난다(E2와 같은 동작). 진행자가 R 뒤 01을 다시 보이고 싶으면 참가자에게 키를 내려놓게 한다. 불편하면 P4d·P4e 때 "R에서 키도 되돌림"을 정한다.
2. **02는 앞 키만 센다**(대상 `Car.FrontKey`). 01은 앞 키를 쥐어야 끝나므로 보통 같은 키다. 참가자가 01 뒤 키를 내려놓고 뒤 키로 바꿔 누르면 02가 넘어가지 않는다. 그때는 진행자가 →로 넘긴다.
3. **01 표시 고리가 키 아래 바닥에 생긴다**(`MarkerTag` `Car.FrontKey`). 화살표는 키 위 160 cm다. 키가 테이블 위(약 75 cm)에 있으니 화살표 끝이 약 235 cm에 있다. 테이블이 잘 보이는지 본다.

## Codex R1-1 반영 결과 (2026-10-18)

**완료 — P4c R1-1 반영 (2026-10-18, Codex 앱). Claude 재리뷰 대기.**

- 브랜치 `codex/P4c-step-list-keys`, 시작 HEAD `cfcbe51` 위 새 커밋.
- 커밋 제목: `fix: record start after alignment reapply`.
- R1-1: **반영함 (fix: record start after alignment reapply)**. 질문 없음.
- 빌드 둘 성공. 전체 자동화 최초 389/390 성공·기존 P0Buck 간헐 실패 1건, 해당 시험 단독 재실행 1/1 성공. 상세 종료 코드는 ⑤에 기록함.
- 하위 에이전트·자체 리뷰 루프·에디터 MCP·에셋 작업 없음. push·PR·머지 없음.

### ① 바뀐 파일

- `Source/vBoR/Interaction/VBoRScenarioOperator.cpp:189`: 대기 중 A로 흐름을 시작할 때 `EnterStep(0, true)`로 새 시도를 시작함.
- `Source/vBoR/P3Probe/P5iOperatorPanelTests.cpp:276`: `vBoR.P4c.E8ReapplyStartAttempt` 추가. 정렬 확인 필요 → A → 전환 완료 → 01 진입·기록 대기, 시작 표시 방문 시 시작 위치 1줄·fallback 0줄, 미방문 시 01 이탈 fallback 1줄·시작 위치 0줄을 확인함.
- `Docs/Reviews/P4c-step-list-keys.md`: R1-1 반영 상태·이번 결과.
- `HANDOFF.md`는 git 무시 파일에 결과를 별도 기록함. 이번 에셋·레벨 변경 없음.

### ② 명세 §6 완료 조건별 결과

1. **충족:** 기존 P4c 구현에 개정 D3의 A 시작 경로를 반영함.
2. **R1-1 시험 충족 / 최초 전체 실행 실패 이력 있음:** 새 P4c E8과 기존 P5i E16 성공, P4c 8/8 성공. 전체 발견·완료 390, 성공 389, 실패 1(P0Buck E8Persistence의 기존 OS error 5). HANDOFF 지시대로 해당 시험만 한 번 재실행해 1/1 성공함. 전체 390/390 성공 실행으로 표기하지 않음.
3. **충족:** 최종 C++ 트리의 non-unity·unity 각 1회 Succeeded·프로세스 exit 0, vBoR 컴파일·링크 경고/오류 0.
4. **충족:** 보고 ①–⑤를 이 리뷰·HANDOFF 및 마지막 커밋 본문에 기록함.
5. **유지:** Quest 3 Link/VR·T-06·T-10은 `not-tested`.

### ③ 명세와 다르게 한 것과 이유

- 제품 동작 이탈 없음. 제안대로 호출 한 줄만 바꿈.
- 리뷰의 “E8에 추가 또는 새 시험” 중 새 P4c E8 시험을 택함. 기존 `FP5iWorld`의 임시 저장 파일·Stage 합성 XR 준비를 재사용하려고 `P5iOperatorPanelTests.cpp`에 배치함. P5i E16의 함수·단언은 HEAD와 동일함을 문자열 비교로 확인함.
- 기존 P4c E8·그 밖의 단언은 변경 없음. 사용자 BuckEye.json 대신 기존 시험 fixture의 임시 경로만 사용함.
- AGENTS.md에 따라 의도적 실패 빌드·red 실행은 하지 않음. 빌드·전체 시험은 최종 트리에서 각각 한 번, 알려진 실패의 단독 재실행만 추가함.

### ④ API·헤더 확인

새 ISDK·MetaXR API 없음. 실제 로컬 파일과 빌드로 확인했으며 추정 API 없음.

- 프로젝트 `Source/vBoR/Interaction/VBoRScenarioRunner.h:150`, `.cpp:321,327`: EnterStep의 새 시도 인자가 시작 기록 대기를 켬.
- `Source/vBoR/Interaction/VBoRScenarioRunner.h:88`: 기존 FP5iTestAccess friend.
- `Source/vBoR/Interaction/VBoRVehicleAlignmentComponent.h:30,46`: RequestReapply·AdvanceSwitch, `P3Probe/P5iTestFixture.h:14-16`: 시험별 저장 경로 override.
- `Source/vBoR/Interaction/VBoRBuckAlignment.h:69`: Save.
- 엔진 기준 `C:/Program Files/Epic Games/UE_5.7/Engine/Source/`:
  - `Runtime/Core/Public/Misc/OutputDevice.h:145`: Serialize.
  - `Runtime/Core/Public/Misc/OutputDeviceRedirector.h:70,77,88`: AddOutputDevice·RemoveOutputDevice·FlushThreadedLogs.
  - `Runtime/Core/Public/Math/TransformVectorized.h:555`: GetRelativeTransform.

### ⑤ 검증 기록 / 로컬에서 확인할 것

- `Saved/Logs/P4cR1NonUnityBuild.log`: `-DisableUnity -Module=vBoR`, 7 actions·변경 cpp 2개 개별 compile, Succeeded·프로세스 exit 0.
- `Saved/Logs/P4cR1UnityBuild.log`: 49 actions·Module.vBoR.4.cpp/6.cpp compile, Succeeded·프로세스 exit 0.
- 두 빌드 로그의 warning/error 검색 0.
- `Saved/Logs/P4cR1AllAutomation.log`: 샘플·로컬 도구 2개 비활성, 발견 390·완료 390·성공 389·실패 1. `P0Buck.E8Persistence`의 반복 교체 30번째 OS error 5만 실패. 시험 exit -1·프로세스 exit 255.
- `Saved/Logs/P4cR1PersistenceRetry.log`: 해당 시험만 한 번 재실행, 발견·완료·성공 1·실패 0, 시험/프로세스 exit 0. 기존 저장 코드 수정 없음.
- 새 `P4c.E8ReapplyStartAttempt` 및 기존 `P5i.E16ReapplyStartsWaitingFlow` 성공. OpenXR 확장·샘플 InfoCard CDO·MetaXR empty engine version 잡음은 Workflow §2.5와 같고 시험 실패와 구분함.
- 시작·빌드 전·자동화 종료 뒤 UnrealEditor 프로세스 0. 에디터·MCP·Blueprint·에셋·레벨 작업 없음.
- 사용자 확인: 정렬 확인 필요 상태에서 A로 01을 시작한 뒤 시작 표시 방문/미방문 기록, 명세 §7의 키 01·02 및 TV 진행 표시 확인. 실제 Quest 3 Link/VR·TV 외관·T-06·T-10은 `not-tested`.


## 질문

### Q1 — 기본 단계 개수 13 ↔ 14 (답함, 2026-10-18 Codex → Claude)

**문제:** 명세 `Docs/Specs/P4c-step-list-keys.md:19` D4는 생성자 기본값을 13개라고 적고, `:101` E12는 새 객체의 단계가 13개임을 요구한다. 반면 베이스 `main@3b02f1e`의 `Source/vBoR/Interaction/VBoRScenarioAsset.cpp:10-25`에는 `Add`가 **14번** 있다. 명세 `:70`, `:80`은 생성자 기본 단계와 기존 단언을 바꾸지 못하게 한다. 따라서 E12의 13개 단언과 기본값 보존을 동시에 만족할 수 없다.

MCP `inspect.get_property`로 `/Game/P3/DA_P3_Scenario.DA_P3_Scenario`의 `Steps`를 먼저 읽었으며, 결과도 **14개**다. 사용자 편집 차이를 발견한 것이 아니라 **실제 생성자 기본값과 동일한 14개**임을 확인했다.

- ID 순서: `01, 02, 07, 08, 09, 10, 11, 12, 13, 14, 15, 18, 19, 20`.
- 규칙 순서: `Approach, Confirm, DoorUnlocked, DoorOpened, Confirm, Seated, DoorLatched, Confirm, ButtonPressed, Viewpoint, Viewpoint, DoorOpened, Confirm, DoorLatched`.
- `Instruction`, `TargetTag`, `ConfirmBy`도 생성자와 동일함. 모든 `SpectatorText`, `RouteTag`는 비어 있음. `MarkerTag`는 01의 `Car.DriverDoor`만 있음. `bRequireBaseViewpoint`는 18만 켜짐. 모든 `DoorOpenDeg=45`, `SeatedSeconds=1.5`.
- 전체 읽기 응답: `Saved/P4cScenarioBefore.json`. 변경 전 바이너리 사본: `Saved/P4cScenarioBefore.uasset` (둘 다 git 무시).
- 현재 에셋과 사본 SHA256 동일: `1C7997F939E1F10642F864EC03E8B9A92FE05DBB27E3D9121CC721345591E5AE`.

**제안 / 필요한 결정:** Claude가 D4·§1·§2·E12의 기존 단계 수를 **14개**로 정정하고, 기존 14개는 유지한 채 에셋에 03–06·16을 더해 19개로 바꾸는 것으로 명세를 고쳐도 되는가? 이 결정 전에는 에셋을 쓰거나 기본 단계·기존 단언을 바꾸지 않는다.

**확인할 시험:** 정정되면 E12는 새 객체 14개·01 `Approach` 보존을 단언하고, E9는 에셋 19개와 §0-1 전체 필드, E10은 근사 차 검증·TV 전체 19를 확인한다. 현재 추가한 E12는 명세 그대로 13개 단언이므로 아직 실행하지 않았다.

**Claude 답(2026-10-18)** — 상태: **답함 (명세 개정)**
- **제안대로 한다.** 생성자 기본 단계는 14개다(01·02·07–15·18–20). 내가 13개로 잘못 셌다. 명세 문제다. D4·§1·§2·E12를 14로 고쳤다(개정 이력 "구현 중 Q1 답").
- 에셋은 지금 생성자 기본값과 같으므로(읽은 결과·SHA256 기록 그대로) 사용자 편집이 없다. **§0-1 표의 19개로 바꾼다.** 03–06·16을 더하고, 01·02(규칙·태그·문구·TV·`MarkerTag`)와 07(`RouteTag`)을 고친다. 나머지는 지금 값 그대로다.
- E12는 새 객체 14개·01 `Approach`를 단언한다. E9·E10은 제안대로다.
- 기존 시험 중 이 에셋을 읽는 것은 없다(`main`의 `P3FlowTests.cpp`에 `/Game/P3` 0건, Claude 확인). 그래서 에셋을 19개로 바꿔도 기존 단언은 그대로다.
- 멈춘 판단, 에셋을 쓰기 전에 사본·해시를 남긴 것 모두 맞았다. 재개 순서도 적은 대로다.

## Q1 구현 반영 상태

**반영함 (feat: add nineteen-step scenario and key rules)** — E12 14개, 생성자 보존, 에셋 19개 적용. 아래 완료 결과 참조.

## Codex 완료 결과 (2026-10-18)

**완료 — P4c 구현과 Q1 정정 반영. Claude 리뷰 대기.**

- 브랜치 `codex/P4c-step-list-keys`, 재개 HEAD `9ace8f0`. 기존 미커밋 WIP를 보존하고 E12를 정정한 뒤 에셋까지 완료함.
- 마지막 커밋 제목: `feat: add nineteen-step scenario and key rules`.
- Q1: **반영함 (feat: add nineteen-step scenario and key rules)**. Claude 답대로 생성자 기본값 14개 유지·E12 14개 단언·에셋 19개 적용. 새 질문·열린 리뷰 항목 없음.
- 하위 에이전트·자체 리뷰 루프 없음. push·PR·merge 없음.

### ① 바뀐 파일

- `Source/vBoR/Interaction/VBoRScenarioAsset.h`: 기존 enum 끝에 `KeyHeld`·`KeyUnlockAndLock` 추가.
- `Source/vBoR/Interaction/VBoRScenarioRunner.h`, `.cpp`: 키 대상 검사, 이벤트 누적·단계 진입 기준값, 초기화/EndPlay 해제. 시작 기록은 새 흐름·R 뒤에만 다시 기다리고 ←는 같은 시도를 유지함.
- `Source/vBoR/Interaction/VBoRKeyFob.h`: 합성 시험용 friend. 키 잡기·버튼·물리 동작 변경 없음.
- `Source/vBoR/P3Probe/P3VehicleMock.h`, `.cpp`: 앞·뒤 키 Actor 태그 추가. 버튼 태그 4개 유지.
- `Source/vBoR/P3Probe/P3FlowTests.cpp`: P4c 자동화 7개(E1–E12) 추가. 기존 단언은 그대로임.
- `Content/P3/DA_P3_Scenario.uasset`: 19개 단계, 01·02 키 규칙/TV 문구/대상, 01 표시, 07 경로.
- `Docs/Reviews/P4c-step-list-keys.md`: Q1 반영 상태·완료 보고. `HANDOFF.md`는 git 무시 파일에 별도 갱신.
- 생성자 `VBoRScenarioAsset.cpp`, TV 기본표, 다른 에셋·레벨·설정·플러그인 소스 변경 없음.

### ② 명세 §6 완료 조건별 결과

1. **충족:** D1–D3 C++와 D4·D5 에셋, §4 자동화 모두 구현함.
2. **충족:** 최종 트리 전체 `vBoR.*` 발견 389·완료 389·Success 389·Fail 0. P4c 7/7과 기존 382/382 통과. E9가 에셋 19개 전체 필드, E10이 근사 차 검증/시작 01/TV 전체 19, E11 태그 6개, E12 기본값 14개/Approach 보존을 확인함. E13은 기존 전체 시험으로 확인함.
3. **충족:** 정정 후 non-unity(`-DisableUnity -Module=vBoR`)·unity 각각 1회 Succeeded·프로세스 exit 0, vBoR 컴파일·링크 경고/오류 0. 에셋 저장·에디터 종료 뒤 전체 자동화 1회, `TEST COMPLETE. EXIT CODE: 0`·프로세스 exit 0.
4. **충족:** 보고 ①–⑤를 리뷰·HANDOFF와 마지막 커밋 본문에 기록함.
5. **유지:** Quest 3 Link/VR·T-06·T-10은 `not-tested`. 에셋 조회·합성 시험·스크린샷을 기기 시험으로 보지 않음.

### ③ 명세와 다르게 한 것과 이유 / MCP

- 개정 명세와 다른 제품 동작 없음. Q1 답대로 E12만 13 → 14로 고침. 기존 생성자·기존 단언 변경 없음.
- 실행 capability: `inspect.get_property`로 전후 Steps 조회, `inspect.set_property`로 이 에셋의 Steps만 쓰기(`saved=true`), `control_editor.save_all`에 이 에셋만 지정(이미 저장돼 noop, 다른 dirty package 0), `control_editor.open_asset`·`control_editor.screenshot`으로 화면 확인.
- search → describe로 계약을 확인했고 구조체 배열은 UE export text 배열로 지원됨을 로컬 도구 소스로 확인함. Python·콘솔·PIE 사용 없음. 도구 소스는 읽기만 함.
- 시작 시 사용자 에디터 없음. C++ 빌드 뒤 연 PID 46160에서 작업함. 저장 뒤 일반 종료 신호로 닫히지 않아 Workflow §2.5·§2.10대로 /F 종료. 종료 완료를 확인한 뒤 자동화를 시작함. 자동화 뒤 `UnrealEditor*` 프로세스 0.

### ④ API·헤더 확인

신규 ISDK·MetaXR API 없음. 다음 실제 파일을 확인했으며 추정 API 없음.

- 프로젝트 `Source/vBoR/Interaction/VBoRKeyFob.h:51,64-65`: GetHoldingHand·키 이벤트. `.cpp:151-170`: 안 쥔 손 필터/Broadcast. `VBoRPokeButton.cpp:119`: PressFilter.
- 엔진 기준 `C:/Program Files/Epic Games/UE_5.7/Engine/Source/`:
  - `Runtime/Core/Public/Delegates/DelegateSignatureImpl.inl:1197,1250,1278`: IsAlreadyBound·AddUniqueDynamic·RemoveDynamic.
  - `Runtime/CoreUObject/Public/UObject/UObjectGlobals.h:2135`: LoadObject.
  - `Runtime/Engine/Classes/GameFramework/Actor.h:2076,2330,2474`: ActorHasTag·Destroy·GetRootComponent.
- MCP 구조체 배열 근거: `Plugins/McpAutomationBridge/Source/McpAutomationBridge/Private/Foundation/BridgeHelpers/Properties/McpAutomationBridgeHelpersPropertyApplyArrays.h:17,112-134`. 프로젝트 의존성으로 추가하지 않음.

### ⑤ 검증 기록 / 로컬에서 확인할 것

- 이번 재개 빌드: `Saved/Logs/P4cResumeNonUnityBuild.log`(6 actions, P3FlowTests.cpp 개별 compile 1)·`P4cResumeUnityBuild.log`(48 actions, Module.vBoR.6.cpp compile 1). 둘 다 Succeeded·exit 0. 앞선 WIP 전체 컴파일 근거는 기존 `P4cNonUnityBuild.log`(82 actions, compile 79)·`P4cUnityBuild.log`(57 actions, compile 10)에 보존돼 있음.
- 로컬 도구/project의 기존 StructUtils 폐기 안내와 vBoR 진단을 구분함. vBoR 컴파일·링크 경고/오류 0.
- 최종 자동화: `Saved/Logs/P4cFinalAutomation.log`, 389/389 성공·실패 0·시험/프로세스 exit 0. P0Buck E8Persistence 최초 실행 성공, 재실행 없음. OpenXR API 1.1→1.0 폴백·없는 XR 확장·샘플 InfoCard CDO·MetaXR empty engine version 잡음은 Workflow §2.5와 같으며 시험 결과와 구분함.
- **수정 전 에셋:** 기존 사본과 SHA256 `1C7997F939E1F10642F864EC03E8B9A92FE05DBB27E3D9121CC721345591E5AE` 동일. MCP 재조회도 생성자와 같은 14개(01·02·07–15·18–20), 사용자 편집 차이 없음. 재조회 `Saved/P4cResumeScenarioBefore.json`, 원래 사본 `Saved/P4cScenarioBefore.uasset` 보존.
- **수정 후 에셋:** SHA256 `7210D3A6A940FAADFCBF099D0C342CC3BDDB79B4D779B9E6ACB4FF75F3359560`. `Saved/P4cScenarioAfter.json` 전 필드를 입력값과 대조(저장 시 FText가 NSLOCTEXT로 바뀐 표현만 정규화), E9가 새 프로세스에서 저장된 에셋을 읽어 다시 검증함.
- 에디터 로그 `Saved/Logs/P4cResumeEditor.log`. 스크린샷 `Saved/Screenshots/P4cScenarioAfter.png`: DA_P3_Scenario Details의 19 Array elements·All Saved 확인. 세부 필드와 순서는 JSON·E9로 확인함.
- 사용자 확인: 명세 §7대로 에셋 19개/문구, VR 01 키 집기, 02 열림·잠금 뒤 확인, 03–06·16 진행자 확인, TV 01·02 영어/진행 막대 19. **VR·실제 손 추적·TV 외관·T-06·T-10은 not-tested.** 새 질문 없음.

## 이전 Codex 중단 결과 (2026-10-18, Q1 답 이전 기록)

- 브랜치 `codex/P4c-step-list-keys`, 베이스 `3b02f1e`. **미커밋 WIP**이며 push·PR·merge 없음. Claude 명세 정정 대기.
- C++ 단계에서 D1·D2·D3 및 E1–E12 시험 코드를 작성한 뒤, 빌드 둘을 완료했다. 에디터 단계에서 위 Q1을 발견하여 **에셋 쓰기 전에 중단**했다. 전체 자동화는 실행하지 않았다.
- 열린 리뷰 지적은 없었음(이 파일은 이번에 생성). Q1만 열림. 하위 에이전트·자체 리뷰 루프 없음.

### ① 바뀐 파일

- `Source/vBoR/Interaction/VBoRScenarioAsset.h`: enum 끝에 `KeyHeld`, `KeyUnlockAndLock` 추가.
- `Source/vBoR/Interaction/VBoRScenarioRunner.h`, `.cpp`: 키 대상 검사·이벤트 횟수/단계 기준값·초기화/EndPlay 해제, 시작 시도 구분. ←는 시작 기록을 유지하고, R은 이전 시도를 버린 뒤 01 시작에서 다시 기다림.
- `Source/vBoR/Interaction/VBoRKeyFob.h`: 시험용 friend 추가(키 동작 변경 없음).
- `Source/vBoR/P3Probe/P3VehicleMock.h`, `.cpp`: 앞·뒤 키 Actor 태그 추가, 버튼 태그 유지.
- `Source/vBoR/P3Probe/P3FlowTests.cpp`: E1–E12용 자동화 7개 추가. 기존 시험의 단언 변경 없음.
- 이 리뷰 파일과 git 무시 `HANDOFF.md`.
- **`Content/P3/DA_P3_Scenario.uasset` 변경 없음.** 생성자 `.cpp`, TV 기본표, 레벨·다른 에셋·설정·플러그인도 변경 없음.

### ② 명세 §6 완료 조건별 상태

1. **부분:** D1–D3 C++·시험 코드 작성, D4·D5 에셋 쓰기는 Q1로 미실행.
2. **미완료:** 전체 자동화 미실행. 추가 E12는 명세와 현행 기본값의 충돌로 통과 불가능한 상태.
3. **빌드 완료 / 시험 미완료:** non-unity(`-DisableUnity -Module=vBoR`)·unity 모두 `Result: Succeeded`, 프로세스 exit 0, vBoR 컴파일·링크 경고/오류 0. 최종 에셋 트리 시험은 아직 없음.
4. **부분:** 이 파일·HANDOFF에 보고함. 미완료 작업이므로 커밋 없음.
5. **유지:** Quest 3·VR 및 계획서 T-06·T-10은 `not-tested`.

### ③ 명세와 다른 점 / MCP

- 범위 확대 없음. AGENTS.md의 명세 충돌 시 중단 규칙에 따라 Q1에서 멈춤. 임의로 14개 단언으로 바꾸지 않음.
- 사용한 실행 capability는 **읽기 `inspect.get_property`** 하나(`Steps`)다. `inspect.set_property`의 계약은 조회했지만 실행하지 않았다. MCP Python·에셋 저장·PIE·레벨 수정 없음.
- 빌드 시점에는 에디터가 닫혀 있었음. 빌드 뒤 직접 연 에디터 PID 44408을 읽기 확인 후 닫음. 일반 종료 신호 뒤 프로세스가 남아 `/F`로 종료함(미저장 편집 없음).

### ④ API·헤더 확인

- 신규 ISDK·MetaXR API 도입 없음. 프로젝트 키 API: `Source/vBoR/Interaction/VBoRKeyFob.h:51,64-65` (`GetHoldingHand`, 두 이벤트), `.cpp:151-170` (쥐지 않은 손 필터와 Broadcast), `VBoRPokeButton.cpp:119` (`PressFilter`).
- 엔진 기준 경로 `C:/Program Files/Epic Games/UE_5.7/Engine/Source/`:
  - `Runtime/Core/Public/Delegates/DelegateSignatureImpl.inl:1197,1250,1278`: `IsAlreadyBound`, `AddUniqueDynamic`, `RemoveDynamic`.
  - `Runtime/CoreUObject/Public/UObject/UObjectGlobals.h:2135`: 시험의 `LoadObject`.
  - `Runtime/Engine/Classes/GameFramework/Actor.h:2076,2330,2474`: `ActorHasTag`, `Destroy`, `GetRootComponent`.
- 위 API는 소스 및 실제 로컬 빌드 둘로 확인함. MCP 기능은 search → describe 계약 확인 후 get_property 실행.

### ⑤ 검증 기록 / 로컬에서 확인할 것

- `Saved/Logs/P4cNonUnityBuild.log`: non-unity 82 actions(compile 79), `Result: Succeeded`, exit 0. 모듈 소스·헤더 시각을 갱신해 파일별 재컴파일함.
- `Saved/Logs/P4cUnityBuild.log`: unity 57 actions(compile 10, vBoR unity 단위 6개 포함), `Result: Succeeded`, exit 0.
- 두 빌드 모두 vBoR 컴파일·링크 경고/오류 0. 로컬 도구 `McpAutomationBridge`·project의 기존 `StructUtils` 폐기 예정 의존 안내 각 2줄은 별도 환경 잡음.
- `Saved/Logs/P4cEditor.log`: `/Game/P3/DA_P3_Scenario` 읽기 성공. 샘플 비활성화 시 기존 `IsdkInfoCardComponent` CDO 잡음은 Workflow §2.5와 같음. 에디터 UI·TV 모양·VR 동작은 확인하지 않음. 스크린샷 없음(에셋 변경 전 중단, 조회 JSON으로 기록).
- **재개 순서:** Q1 명세 정정 → E12 정정 → C++ 변경분 빌드 → 에셋이 여전히 기본값인지 다시 읽기 → 19단계 쓰기·저장·조회 → 에디터 닫기 → 최종 `vBoR.*` 한 번 → 결과·커밋·HANDOFF 갱신. 시작 시 에디터 열림 여부를 다시 확인할 것.
- 사용자 확인은 명세 §7: 에셋 19개·문구, 키 집기로 01 완료, 02 열림·잠금 뒤 확인, 자리 단계 진행자 확인, TV 영어 문구·19개 진행 막대. 계획서 **T-06·T-10 및 Quest 3 Link/VR는 `not-tested`**.

## 개정 이력

| 날짜 | 회차 | 내용 |
|---|---|---|
| 2026-10-18 | 구현 중 질문 | Codex Q1(생성자 기본 단계 14개) → Claude 답: 명세 13 → 14 정정 |
| 2026-10-18 | 1회차 | `0a26f34` 리뷰. 열림 1건 R1-1(A로 흐름 시작 때 시작 기록을 기다리지 않음 — 명세 D3 빈틈), 참고 1(VR 셋) |
