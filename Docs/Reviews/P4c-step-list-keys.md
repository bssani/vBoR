# P4c 단계 목록·키 규칙 — 인계 기록

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

## Codex 중단 결과 (2026-10-18)

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
