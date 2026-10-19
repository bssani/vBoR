# P4f 본 Pawn·GameMode — Codex 구현 기록

완료 — Claude 첫 리뷰 대기. 브랜치 `codex/P4f-main-pawn-gamemode`, 시작 `main@d920df2`.
명세 `Docs/Specs/P4f-main-pawn-gamemode.md`의 첫 구현이며, 기존 리뷰 항목은 없음. 질문 없음.

## ① 바뀐 파일

| 파일 | 변경 |
|---|---|
| `Source/vBoR/Interaction/VBoRPawn.h/.cpp` | P3 손바닥·Pinch 설정을 본 Pawn으로 옮김. 손 메시·재질 네 프로퍼티, 두 표시 적용, 매핑 실패 시 해당 손 기본값 복구·Warning 1회 |
| `Source/vBoR/Interaction/VBoRGameMode.h` | 기본 Pawn이 `AVBoRPawn`인 GameMode |
| `Source/vBoR/P3Probe/P3FlowGameMode.h` | `AP3PalmGrabPawn`의 부모를 `AVBoRPawn`으로 바꾸고 본문 제거. `AP3FlowGameMode`는 그대로 |
| `Source/vBoR/P3Probe/P4fPawnTests.cpp` | E1–E7 신규 자동화 8개(E5 두 개). Blueprint 로드뿐 아니라 실제 생성도 검사 |
| `Content/Main/Core/BP_vBoRPawn.uasset` | 부모 `AVBoRPawn`, 손 프로퍼티 네 칸 비움 |
| `Content/Main/Core/BP_vBoRGameMode.uasset` | 부모 `AVBoRGameMode`, 기본 Pawn `BP_vBoRPawn` |
| 이 파일 | 보고 ①–⑤와 초기 실패·최종 검증 기록 |

`HANDOFF.md`는 무시 파일에 별도 기록함. 레벨·Config·Plugins·기존 시험 파일 변경 없음.

## ② 명세 §6 완료 조건별 결과

| 조건 | 결과 |
|---|---|
| 1. §2 구현·BP 둘·시험 | 충족. D1–D5 구현 및 MCP 생성·저장·CDO 조회 완료 |
| 2. §4·기존 전체 시험 | 충족. 신규 8/8, 기존 425/425, 전체 433/433 Success·Fail 0·프로세스 exit 0 |
| 3. non-unity·unity | 충족. 최종 트리의 두 빌드 `Result: Succeeded`, exit 0. vBoR 컴파일·링크 경고/오류 0 |
| 4. 보고·MCP 종료 | 충족. 이 기록과 마지막 커밋 본문에 ①–⑤, HANDOFF에 결과. 에셋 둘 저장·에디터 종료 확인 |
| 5. 기기 시험 상태 | `not-tested` 유지. 합성 시험·MCP 조회를 Quest 3 동작 증거로 취급하지 않음 |

E1은 본 Pawn·P3 자식의 Palm/Pinch/근접 허용과 Ray/Distance 차단 및 기존 기본 Pawn의 poke 전용을 검사함.
E2는 기본 메시·각 슬롯 기본 재질·추적 손 숨김·합성 손 보임을 검사함.
E3는 반대 손 OpenXR 메시의 실제 SDK 매핑 Valid와 다른 손 유지, E4는 SkeletalCube 실패 후 두 표시·재질 복구와 Warning 정확히 1회를 검사함.
E5는 재질만 지정 및 호환 메시+재질의 모든 슬롯(두 슬롯) 적용, E6는 두 C++ GameMode 기본값, E7은 BP 부모·빈 프로퍼티·기본 Pawn·실제 생성과 자기 인스턴스 부착을 검사함.
E8은 전체 시험으로 확인함. 기존 단언 변경 없음.

## ③ 명세와 다르게 한 것과 이유 / 구현 상세

- 기능 범위·동작 이탈 없음. `AVBoRGameMode`는 헤더만 둠.
- 매핑 확인은 `Super::BeginPlay()` 뒤와 `TG_PostPhysics` Pawn Tick에서 함. 두 표시가 Valid이거나 기본 손으로 복구되면 Pawn Tick을 끔. ISDK 손 컴포넌트의 Tick·poke limiter·원본 손끝 읽기는 바꾸지 않음.
- 기본 손으로 복구할 때 해당 손의 두 메시를 ISDK 기본 경로에서 읽고 OverrideMaterials·기존 매핑을 비움. 실패한 원본 프로퍼티는 그대로 두고 손별 플래그로 재경고를 막음. 반대 손과 보임은 그대로임.
- 신규 E7의 실제 Blueprint 생성에서 중첩 ISDK 표시의 부모가 CDO 리그를 가리키는 `Template Mismatch during attachment`를 발견함. 새 Pawn의 `PreRegisterAllComponents`에서 미등록 손 표시 둘을 자기 리그에 `SetupAttachment`하는 작은 보정만 넣음. 기존 부모 Pawn·플러그인은 고치지 않음. E7은 양손의 부착 부모 Owner도 검사함.
- MCP는 `manage_asset.query_asset(lookup=exists)` → `manage_blueprint.create`(둘, GameMode 생성 시 DefaultPawnClass 지정) → `inspect.inspect_class(kind=cdo/class)` → `control_editor.save_all(assetPaths=둘)`을 사용함.
- create 응답에서 둘 다 `saved=true`, GameMode `appliedProperties=[DefaultPawnClass]`·실패 0 확인. CDO 조회에서 부모·손 네 칸 null·Pawn 지정 확인. 마지막 제한 저장은 이미 저장돼 `noop`, 다른 dirty 패키지 0. 빈 임시 월드에서 작업하고 레벨·프로젝트 설정은 건드리지 않음.
- 에디터를 닫은 뒤 실행 프로세스가 없는 것을 확인함. 남은 CrashReportClientEditor도 닫음.
- 에디터 생성 전 새 C++를 빌드하고 관련 7개 시험을 통과시킨 뒤 MCP를 수행함. 초기 실패 수정 및 Blueprint 생성 오류 수정으로 필요한 재빌드·재시험을 했으며, 아래에 최초·최종 결과를 모두 남김. 하위 에이전트·자체 리뷰·push·PR·머지 없음.

## ④ API·헤더 확인

추정으로 쓴 신규 API 없음. 아래 로컬 헤더·구현을 확인하고 실제 UE 5.7 빌드로 대조함.

| API / 근거 | 경로:줄 |
|---|---|
| 손 리그의 표시 조회 | `Plugins/MetaXRInteraction/Source/OculusInteractionPrebuilts/Public/Rig/IsdkHandRigComponent.h:90` |
| Tracked/Synthetic 표시 | 같은 Public 아래 `Rig/IsdkHandVisualsRigComponent.h:109,114` |
| 매핑 상태 enum·조회·초기화 | `Plugins/MetaXRInteraction/Source/OculusInteraction/Public/IsdkHandMeshComponent.h:47,147,154` |
| SDK BeginPlay 매핑·불일치 오류·메시 변경 후 재매핑 | 같은 모듈 `Private/IsdkHandMeshComponent.cpp:161,205,360` |
| 기본 XRHand_* 이름 | 같은 `Private/IsdkHandMeshComponent.cpp:306` |
| 기본 좌우 메시 경로 | 같은 `Public/IsdkContentAssetPaths.h:155,157` |
| Pinch 허용 | 같은 `Public/Interaction/IsdkGrabberComponent.h:185` |
| 중첩 표시의 원래 부착 | `Plugins/MetaXRInteraction/Source/OculusInteractionPrebuilts/Private/Rig/IsdkHandRigComponent.cpp:253,261` |
| 등록 전 보정·초기화·Tick 제어 | 엔진 `Runtime/Engine/Classes/GameFramework/Actor.h:3231,3123,2897` |
| 부착 API | 엔진 `Runtime/Engine/Classes/Components/SceneComponent.h:729` |
| 메시 교체·조회 | 엔진 `Runtime/Engine/Classes/Components/SkinnedMeshComponent.h:1130,1145` |
| 재질 초기화·슬롯·적용 | 엔진 `Runtime/Engine/Classes/Components/MeshComponent.h:101,113,116` |
| 컴포넌트 BeginPlay 선행 | 엔진 `Runtime/Engine/Private/Actor.cpp:4753` |
| 시험 기대 Warning의 정확한 개수 | 엔진 `Runtime/Core/Public/Misc/AutomationTest.h:1815` |

엔진 경로는 `C:/Program Files/Epic Games/UE_5.7/Engine/Source/` 기준임.

## ⑤ 로컬 확인 결과와 남은 사용자 시험

### 최종 증거

| 검사 | 결과·로그(`Saved/Logs/`) |
|---|---|
| 전체 모듈 non-unity 재컴파일 | `P4fVerifiedNonUnity.log`, `-DisableUnity -Module=vBoR`, Succeeded·exit 0 |
| unity 빌드 | `P4fVerifiedUnity.log`, Succeeded·exit 0 |
| Blueprint 생성 회귀시험 | `P4fBlueprintRegression.log`, 1/1 Success·Fail 0·exit 0 |
| 전체 자동화 | `P4fVerifiedAutomation.log`, 발견 433·완료 433·Success 433·Fail 0·TEST COMPLETE EXIT CODE 0·프로세스 exit 0 |
| MCP 읽기 증거 | `P4fMcpReadback.json` — BP 둘의 부모·CDO·저장 응답 |
| GUI 에디터 세션 | `P4fEditor.log` — 시작·두 에셋 저장·정상 종료 기록 |

두 최종 빌드의 vBoR 경고/오류는 0. 로컬 도구의 StructUtils 폐기 예정 의존 안내는 빌드마다 2줄이며 별도임.
자동화에서는 샘플·McpAutomationBridge·UnrealAgent를 모두 끔. 알려진 OpenXR/샘플 CDO/시험 World 정리 잡음은 Workflow §2.5와 구분함.
E4는 SDK가 두 표시에서 내는 본 이름 오류 2개와 vBoR Warning 1개를 정확히 기대 메시지로 검사함(로그에서는 Verbose로 기록됨). 플러그인 로그나 기존 단언을 완화하지 않음.

### 초기 실패와 수정

- `P4fNonUnityBuild.log`: 신규 시험의 AddExpectedMessagePlain 인자 순서 오류로 exit 6. 헤더 확인 뒤 수정하고 `P4fNonUnityBuildRetry.log`에서 성공함.
- `P4fTargetedAutomation.log`: 7개 발견, 5개 Success 뒤 신규 다중 재질 fixture가 같은 TArray 내부 원소를 Add해 assert·프로세스 exit 3. 별도 복사본을 Add하도록 수정함.
- `P4fTargetedAutomationRetry.log`: E1–E6 관련 7/7 Success·exit 0.
- `P4fAllAutomation.log`: 첫 전체 실행은 432/433 Success·Fail 1, TEST COMPLETE EXIT CODE -1·프로세스 exit 255. 실패는 신규 E7 Blueprint 실제 생성의 Template Mismatch이며 기존 425개는 모두 통과함.
- 등록 전 부착 보정 뒤 E7 단독 1/1 및 최종 전체 433/433을 확인함. OS error 5 간헐 실패·기존 시험 실패 없음.

### 사용자 확인 — not-tested

- **에디터 Details 육안 확인:** BP_vBoRPawn의 `손 표시` 네 프로퍼티, BP_vBoRGameMode의 DefaultPawnClass. 이번에는 MCP CDO 조회와 자동화 에셋 로드·생성으로 확인했으며 Details 화면을 조작한 증거는 아님. 스크린샷 대신 MCP 읽기 결과를 남김.
- **명세 §7:** L_P3_Flow의 GameMode를 임시로 BP_vBoRGameMode로 바꿔 VR Preview하고 저장하지 않음. 기본 손, 키 Pinch, 문 손바닥 잡기, 버튼 poke를 확인할 것. 사용자가 준비한 XRHand_* 메시와 재질, 잘못된 메시의 기본 손 복구·Warning, poke limiter를 볼 것.
- **Quest 3 Link 및 계획서 T-01·T-03·T-07·T-09·T-10:** 손 위치·부품 조작·원본 손끝 도달·추적 손실·키 입력을 실제 장비로 확인할 것. 이번 기기 시험은 모두 `not-tested`.
- L_Main·차량 BP·진행자 정렬 저장은 다음 작업이며 시작하지 않음.

## 질문

없음.
