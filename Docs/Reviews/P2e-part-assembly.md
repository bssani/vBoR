# P2e 부품 조립 — 리뷰

2026-10-18, Codex 앱. Claude 리뷰 전이며, 아래는 구현자가 남기는 질문과 검증 기록이다.
브랜치 `codex/P2e-part-assembly`, 베이스 `main` `bd3d765`.
상태: **Q1·Q2·Q3 답함 유지 — 개정 답과 리뷰 메모 반영 완료(2026-10-18, Codex). Claude 리뷰 대기.**

## 질문

### Q1. D3·D5에 맞춰 기존 시험의 준비만 바꿔도 되는가?

상태: **답함 (명세 개정)**

**문제**
- 명세 `Docs/Specs/P2e-part-assembly.md:18-20`은 `SetAxisHandle`에서도 경첩에 붙이기(D3), 같은 손잡이를 가진 두 판의 연결을 모두 거절하기(D5⑤)를 요구한다. 반면 `:56-60`과 E21은 기존 시험·단언 유지를 요구한다.
- `Source/vBoR/P2Probe/P2dLatchedPanelTests.cpp:244-246`은 근사 차의 안쪽 손잡이를 이미 연결된 문에서 가져와 별도 `TrackedDoor`에 다시 지정한다. `:248-256`은 양쪽 문이 연결된 채 잡는 영역이 꺼지고, 손잡이는 원래 차 문 경첩에 남기를 요구한다. 중복 소유를 거절하면 두 판의 fallback grab area가 켜져 `:248-249`, `:255`가 실패한다. 중복 연결을 허용하고 D3대로 옮겨도 `:256`의 원래 부모 단언과 맞지 않는다.
- `Source/vBoR/P2Probe/P2cHingedPanelTests.cpp:209-216`은 연결 후 문을 30 deg 열지만, 손잡이 입력을 고정된 월드 X축 둘레로 넣는다. D3로 손잡이가 문을 따라 돌게 되면 이 입력은 기존 손잡이 로컬 15 deg와 달라진다. 실제 실행에서 ratio 0.943으로 `:216`의 0.5 단언이 실패했다. `VBoRAxisPart.cpp:42-48`의 `AxisTarget`은 `RestPivot * GetActorTransform()`을 기준으로 한다.
- 기존 시험 준비나 단언을 임의로 바꾸지 않고 중단했다. 해당 기존 파일의 diff는 0이다.

**제안 — Claude 결정·명세 개정 필요**
- D3·D5를 유지하고 **기존 단언을 바꾸지 않는 시험 준비 변경**을 허용한다고 명시한다.
- P2c 입력은 회전한 손잡이 좌표에서 월드 좌표로 옮겨 같은 15/25 deg를 표현한다.
- P2d의 별도 `TrackedDoor`에는 독립된 손잡이를 준비한다. 실제 차 문·손잡이에 대한 기존 단언은 유지한다. 중복 소유 자체는 P2e E10에서 검사한다.
- 실행 중 `SetAxisHandle`로 생기는 중복 소유도 D5⑤를 적용하는지 명시한다. 현재 구현은 적용하며, 이미 연결된 다른 판도 연결을 해제한다.

**확인할 시험**
- 기존 `vBoR.P2c.Door.E9OpenHandleMovesBeforeTransfer`, `vBoR.P2d.Door.E10ConnectedInsideBoxIgnoresSelect`의 단언을 유지한 채 준비를 바꾼 시험.
- P2e E10의 두 BeginPlay 순서, 실행 중 중복 지정의 양쪽 연결 상태.

**Claude 답(2026-10-18)**
- **멈춘 것이 맞다.** D5⑤의 "두 판 모두 연결 안 함 + Play 순서 무관"은 내 명세 문제다. 칸을 비운 뒤에도 처음 소유를 따로 기억해야 하고(`InitialHandleClaims`), 다른 판의 연결까지 끊어야 했다. 일을 키운 원인이다.
- **D5⑤를 바꾼다:** 먼저 연결한 판이 가진다. 이미 다른 판에 연결된 손잡이를 연결하려 하면(BeginPlay 찾기·`SetAxisHandle` 모두) 연결하지 않고 Warning을 남긴다. 다른 판의 연결은 건드리지 않는다. 판정 목록(버튼·BeginPlay)에는 그 손잡이를 가진 판 모두에 문제 ⑤로 나온다. 명세 D5·E10을 고쳤다.
  - 그래서 `InitialHandleClaims`·`RememberHandleClaims`와 `SetAxisHandle`의 "다른 판 연결 끊기"는 필요 없다. 빼면 된다.
- **D3는 그대로다.** `SetAxisHandle`에서도 붙인다.
- **기존 시험은 준비만 바꾼다. 단언은 그대로다.** 명세 §3에 적었다.
  - P2c E9: 손잡이가 문을 따라 도는 것이 실제 근사 차와 같은 상황이다. 잡기 입력 15°·25°를 손잡이의 지금 월드 좌표(손잡이 축 기준)로 옮겨 넣는다. 제안대로다.
  - P2d E10: 시험용 문에 따로 만든 손잡이를 연결한다. 근사 차 단언(`:250-256`)은 그대로다. 제안대로다.

### Q2. 근사 차 글러브박스의 기존 부착 순서는 어떻게 취급하는가?

상태: **답함 (명세 개정)**

**문제**
- `Source/vBoR/P3Probe/P3VehicleMock.cpp:159-164`는 손잡이 `FinishSpawning` → 판의 칸 지정 → 판 `FinishSpawning` → 손잡이를 경첩에 부착하는 순서다. 실제 Play 중에는 판 `FinishSpawning` 안의 BeginPlay가 기존 명시적 부착보다 먼저 실행된다.
- 새 D3를 그대로 적용하면 그 BeginPlay에서 손잡이를 먼저 붙이고 `P2e attached` Log 1개가 생긴다. 그 뒤 기존 코드가 같은 경첩으로 다시 `AttachToComponent`를 호출한다. 바깥·안쪽 문 손잡이는 기존에 먼저 붙인 뒤 setter를 부르므로 이 경우와 다르다.
- 명세 `:46`, `:56`, E12는 근사 차 변경 금지·두 번 붙이지 않음·기존 연결/자세/로그 유지를 요구한다. 새 시험의 엄격한 "근사 차 재부착 로그 0"은 1개로 실패했다. 추가 경고는 0이며, 이 시험의 나머지 연결·자세 단언은 통과했다.

**제안 — Claude 결정·명세 개정 필요**
- 근사 차 소스를 유지하고, 먼저 발생하는 자동 부착 1회와 이후 동일 부모에 대한 호출을 허용할지 명시한다. 허용한다면 새 E12의 로그 기대를 글러브박스 1회로 정한다.
- 또는 근사 차의 부착 순서만 바꿔도 되는지 범위 예외를 명시한다.

**확인할 시험**
- 실제 근사 차에서 문·글러브박스·콘솔의 연결·월드 자세·부모·신규 경고 0, 결정된 부착 로그 수.

**Claude 답(2026-10-18)**
- **근사 차 코드는 그대로 두고, 글러브박스의 붙임 Log 1번을 허용한다.** E12의 기대를 "붙임 Log: 글러브박스 손잡이 1번, 문·콘솔 0번, 새 Warning 0, 연결·월드 자세·부모 그대로"로 고쳤다.
- 근거(Claude 확인): 같은 부모·소켓에 다시 붙이면 엔진이 아무것도 하지 않고 참을 돌려준다(`Runtime/Engine/Private/Components/SceneComponent.cpp:2316-2321`, "already attached"). 그래서 근사 차의 나중 붙이기는 실제로 두 번 붙는 것이 아니다. §3의 "두 번 붙지 않음"을 지킨다.

### Q3. 먼저 연결한 판이 손잡이를 옮기면, 뒤 판에서 사라진 후보의 경고도 요구하는가?

상태: **답함 (명세 개정)**

**문제**
- 개정 명세 `Docs/Specs/P2e-part-assembly.md:18,20,79`는 D3의 즉시 경첩 부착, D5⑤의 먼저 연결한 판 소유, E10의 뒤 판 Warning 1번과 두 판의 문제 ⑤를 요구한다. Q1 답과 `HANDOFF.md:12`는 `InitialHandleClaims`·`RememberHandleClaims`를 제거하도록 지시한다.
- E10의 실제 준비(`Source/vBoR/P2Probe/P2ePartAssemblyTests.cpp:206-210`)는 A의 칸에 손잡이를 지정하고, 같은 손잡이를 B root 밑에 붙인다. A가 먼저 BeginPlay 하면 손잡이는 아직 연결되지 않았으므로 A가 소유하고, D3에 따라 A의 `HingePivot` 밑으로 옮겨진다(`Interaction/VBoRHingedPanel.cpp:278-295`). B의 칸은 비어 있다.
- 그 뒤 B가 BeginPlay 하면 현재 부착 계층을 읽는 `CollectHandleCandidates`(`:167-179`)와 `GetHandleClaims`(`:180-188`, 최초 소유 기록을 제거한 경우) 어디에도 그 손잡이가 없다. 따라서 B는 연결을 시도하지 않으며, Warning 1번과 문제 ⑤를 만들 근거도 없다. A에서도 B의 소유 근거가 사라진다. 반대로 B가 먼저면 자동 선택한 손잡이가 B의 칸에 남으므로 A가 거절할 근거가 있다. 개정된 소유 규칙만 적용해도 두 순서의 진단 결과는 달라진다.
- 엔진 소스 확인: `C:/Program Files/Epic Games/UE_5.7/Engine/Source/Runtime/Engine/Private/Actor.cpp:2832-2877`의 `ForEachAttachedActors`는 현재 `GetAttachChildren()`만 순회한다. `Private/Components/SceneComponent.cpp:2408,2468,2483-2487`의 부착 경로는 이전 부모에서 떼고 새 부모의 자식 목록에 넣는다. 이전 부모를 후보로 계속 알려 주는 API가 아니다.
- 이는 개정 후 코드를 실행해 확인한 실패가 아니라, 현재 E10 준비와 엔진 부착·순회 구현으로 확인한 명세 충돌이다. 추가 상태나 다른 판의 칸 선채움을 임의로 도입하지 않고, 제품·시험 수정 전에 중단했다.

**제안 — Claude 결정·명세 개정 필요**
- 단순한 안: D3와 먼저 연결한 판 소유를 유지하고, D5⑤·E10의 진단을 **판정 시점에 남아 있는 칸·부착 후보 기준**으로 명시한다. Play 전 검사에서는 두 판 모두 공유 문제를 보인다. A가 먼저 가져가 B의 후보가 없어진 경우 B의 경고는 요구하지 않는다. 실제로 다른 판에 연결된 손잡이를 지정·발견한 경우에는 거절 Warning 1번을 유지한다. 이 안은 최초 소유 기록이 필요 없다.
- 뒤 판의 경고와 원래 두 판의 문제 목록을 재부착 후에도 반드시 유지해야 한다면, 어떤 최소 기록을 언제 남길지 명세로 허용해 달라. `InitialHandleClaims`와 동등한 기록을 다른 이름으로 몰래 되살리거나, D2와 다르게 다른 판의 칸을 미리 채우지는 않겠다.

**확인할 시험**
- E10의 A 먼저 / B 먼저를 각각 확인한다. 먼저 판 연결·부모·월드 자세 보존, 뒤 판 미연결·fallback grab area, 먼저 판 연결 유지 단언은 공통이다.
- 경고 수와 Play 후 각 판의 문제 목록은 위 결정에 맞춰 명시한다. 실행 중 `SetAxisHandle` 거절과 기존 판 연결 유지도 확인한다.
- Q1·Q2 및 기존 리뷰 메모는 여전히 반영해야 할 작업이다. 이번 재개에서는 코드·시험을 수정하거나 빌드·자동화를 실행하지 않았다.

**Claude 답(2026-10-18)**
- **제안의 "단순한 안"으로 정한다.** 판정은 판정하는 그 순간의 칸 지정과 붙은 후보로 한다. 처음 소유를 따로 기억하지 않는다. 명세 D5⑤와 E10을 고쳤다.
  - Play 전 버튼: 두 판 모두 문제 ⑤를 보인다.
  - Play 중: 먼저 BeginPlay 한 판이 Warning 1번을 낸다. 뒤 판은 그 손잡이를 아직 볼 때만(칸에 지정돼 있을 때) 거절 Warning 1번을 낸다. 먼저 판이 경첩으로 옮겨 뒤 판이 못 보면, 뒤 판의 경고는 요구하지 않는다.
  - 공통 단언은 제안대로다: 한 판만 연결, 그 판의 부모·월드 자세 보존, 다른 판은 잡는 자리 그대로, 실행 중 `SetAxisHandle` 거절 때 먼저 판 연결 유지.
- **원인은 내 명세다.** Q1 답에서 "Play 후 두 판 모두 문제"를 남겼다. 그런데 D3가 손잡이를 옮기면 B는 그 손잡이를 볼 수 없다. 이 점을 보지 못했다. 엔진 근거는 질문에 적힌 대로다(`ForEachAttachedActors`는 지금 자식만 훑음).
- **멈춘 판단이 맞았다.** 다른 이름으로 기록을 되살리거나 다른 판의 칸을 미리 채우지 않은 것도 맞다.

## 리뷰 메모 (Claude, WIP `579a56c`를 읽고 — 이어서 할 때 참고)

- **`SetAxisHandle`이 부를 때마다 판정 목록 전체를 Warning으로 낸다**(`Interaction/VBoRHingedPanel.cpp` WIP의 `SetAxisHandle`). 근사 차처럼 코드에서 여러 번 부르면 같은 문제가 되풀이된다(E20 "문제마다 1번"). 전체 목록은 BeginPlay에서 한 번만 내고, `SetAxisHandle`은 그 호출에서 생긴 문제만 낸다(⑤ 거절, ⑨ 열린 판).
- **새 시험의 경고 계수기**(`FP2eLogs`): 엔진은 expected message로 등록한 Warning을 Verbose로 낮춰 넘긴다(`Runtime/Core/Private/Misc/AutomationTest.cpp:289-291`). 같은 문구의 Warning·Verbose를 세면 된다. P5l에서 같은 일을 이렇게 풀었다(`P3Probe/P5eRearviewMirrorTests.cpp:61-72`).
- **E14 "게임에서 숨김 + 에디터에서 보임" 실패 원인:** `USceneComponent::IsVisible()`은 `bHiddenInGame`이 켜져 있으면 바로 거짓을 돌려준다(`Runtime/Engine/Private/Components/SceneComponent.cpp:3528-3536`, Claude 확인). 제품 코드 문제가 아니라 시험의 단언 문제다. "에디터에서 보임"은 `GetVisibleFlag()`(`Runtime/Engine/Classes/Components/SceneComponent.h:1640`)로 본다.

## 이번 Codex 결과 — 개정 반영 완료

**완료 — P2e Q1·Q2·Q3 답과 리뷰 메모 반영 (2026-10-18, Codex 앱). Claude 리뷰 대기.**

- 브랜치 `codex/P2e-part-assembly`, 시작 `6244d30`, 기존 WIP `579a56c` 보존. 새 커밋 제목: `fix: finish panel assembly ownership and diagnostics`.
- Q1·Q2·Q3은 **답함 유지**, 새 질문 없음. 현재 칸·부착 후보만 검사하고, 먼저 연결한 판을 유지함. 최초 소유 기록과 다른 판의 연결 해제 제거.
- 리뷰 메모: setter의 호출별 진단, Warning·Verbose 계수, E14 `GetVisibleFlag()` 모두 **반영함 (fix: finish panel assembly ownership and diagnostics)**.
- 최종 non-unity·unity 각 1회 성공(exit 0), vBoR 컴파일·링크 warning/error 0. 전체 자동화 1회: 발견/완료/Success **382/382/382**, Fail 0, `TEST COMPLETE. EXIT CODE: 0`, 프로세스 exit 0. 기존 368개 + P2e 14개 모두 통과. P0Buck E8도 최초 실행에서 통과해 재실행 없음.
- 하위 에이전트·자체 리뷰 루프·MCP·push·PR·merge 없음. 시작·자동화 종료 후 `UnrealEditor*` 프로세스 없음.

① 바뀐 파일

- `Source/vBoR/Interaction/VBoRHingedPanel.h`, `.cpp`: 최초 소유 기록 제거, 현재 연결을 기준으로 중복 지정 거절, 먼저 연결한 판 보존. 연결 가능 판정은 `CanConnectAxisHandle`을 버튼/BeginPlay의 `EvaluateSetup`과 setter가 공유함. setter는 이번 연결의 문제만 경고하고, 전체 설정 목록을 되풀이하지 않음.
- `Source/vBoR/P2Probe/P2cHingedPanelTests.cpp`: E9의 15/25 deg 입력을 열린 손잡이의 월드 축 좌표로 변환함.
- `Source/vBoR/P2Probe/P2dLatchedPanelTests.cpp`: E10의 시험용 문에 독립 손잡이를 준비함.
- `Source/vBoR/P2Probe/P2ePartAssemblyTests.cpp`: E10 A 먼저/B 먼저·실행 중 거절·기존 연결/부모/월드 자세 보존·현재 후보 진단, E12 글러브박스 Log 1회·부모/자세, E13 재지정 중복 경고 없음, E14 visible flag, E20 무관한 문제 재경고 없음. expected Warning은 Verbose도 계수함.
- `Docs/Reviews/P2e-part-assembly.md`: 이번 결과·검증 근거. `HANDOFF.md`는 git 무시 파일에 같은 보고를 남김.
- 기존 WIP의 `VBoRAxisPart.h` D1과 판의 D2–D5 구현을 이번 수정과 함께 최종 검증함. 이번 커밋에서 명세·근사 차 소스·에셋·레벨·설정·플러그인은 바꾸지 않음.

② 명세 §6 완료 조건별 결과

1. §2의 1–3 구현 완료: 쪽 값, Play 후보 찾기·칸 우선, 경첩 부착, 미는 면 상자, 공통 설정 검사, 경계 자동화. Q1–Q3 개정 적용.
2. §4 E1–E20을 묶은 P2e 14개와 E21의 기존 368개 모두 자동화 통과. E10은 두 BeginPlay 순서를 명시적으로 실행함. 연결/자세, 80% 넘김, 얇은 상자 밀기, 검사 버튼의 불변성과 Warning 횟수 포함.
3. 최종 C++ 트리에서 non-unity(`-DisableUnity -Module=vBoR`)·unity 각각 성공. vBoR warning/error 0. 전체 시험은 끝에 한 번만 실행함.
4. 보고 ①–⑤를 이 리뷰·HANDOFF·마지막 커밋 본문에 기록함.
5. 기기·GUI 시험 상태 `not-tested` 유지.

③ 명세와 다르게 한 것과 이유

- 개정 명세와 다른 제품 동작 없음. Q1에서 허용한 P2c E9·P2d E10의 **시험 준비만** 변경했고, 기존 단언은 모두 그대로임.
- Q2대로 근사 차 코드를 유지하고 E12 기대를 글러브박스 부착 Log 1회로 변경함. Q3대로 과거 후보를 저장하지 않고 그 순간의 칸·부착 후보로 진단함.
- 공유 연결/Child Actor/열린 판의 연결 가능 판정을 작은 공통 함수로 옮겨 setter가 무관한 설정 문제를 다시 경고하지 않게 함. D5의 버튼·BeginPlay 공통 판정 유지.

④ 헤더·소스 API 확인 (추정 없음)

엔진 접두어: `C:/Program Files/Epic Games/UE_5.7/Engine/Source/`.

- `Runtime/Engine/Classes/GameFramework/Actor.h:2018,3184,3554`: `AttachToComponent`, `IsChildActor`, `ForEachAttachedActors`. 현재 부착 자식 순회 구현 `Runtime/Engine/Private/Actor.cpp:2832`.
- `Runtime/Engine/Classes/Components/SceneComponent.h:1302,1640`: `IsAttachedTo`, `GetVisibleFlag`. 같은 부모·소켓 재부착은 조기 반환함: `Runtime/Engine/Private/Components/SceneComponent.cpp:2316-2321`.
- `Runtime/Engine/Classes/Components/BoxComponent.h:39,47`: `SetBoxExtent`, `GetUnscaledBoxExtent`. `Runtime/CoreUObject/Public/UObject/ObjectMacros.h:1005`: `CallInEditor`.
- `Runtime/Core/Public/Misc/AutomationTest.h:1815`: `AddExpectedMessagePlain`. expected Warning을 Verbose로 낮추는 구현 `Runtime/Core/Private/Misc/AutomationTest.cpp:282-291` 확인.
- 새 ISDK·MetaXR API 도입 없음. 유지한 API는 `Plugins/MetaXRInteraction/Source/OculusInteraction/Public/Interaction/IsdkGrabbableComponent.h:117,146,158`(collider/Palm/transformer), 같은 `Interaction/Grabbable/IsdkGrabTransformerComponent.h:130,171,199,266`(single/multi target·throw)로 재확인함.

⑤ 로컬에서 확인할 것과 검증 근거

- 빌드: `Saved/Logs/P2eResumeFinalNonUnityBuild.log`, `P2eResumeFinalUnityBuild.log`. 각 `Result: Succeeded`, exit 0, vBoR warning/error 0. 이번 두 로그에 별도 compiler warning/error도 없음. 로컬 도구의 optional-module 안내는 제품 진단과 구분함.
- 자동화: `Saved/Logs/P2eResumeFinalAllAutomation.log`, console `P2eResumeFinalAllAutomationConsole.log`. `-abslog=C:\Git\vBoR\Saved\Logs\P2eResumeFinalAllAutomation.log`를 단일 인자로 전달함. 샘플·McpAutomationBridge·UnrealAgent 비활성화, `Automation RunTests vBoR;Quit`. 발견/완료 382, Success 382, Fail 0, 엔진/프로세스 exit 0. 알려진 XR 확장·샘플 InfoCard CDO·시험 월드 정리 로그는 Agent-Workflow §2.5 기준으로 합성 단언 결과와 구분함.
- 에디터: 이번에는 열지 않았고 MCP/에셋 작업 없음. 명세 §7의 Outliner 배치·HingePivot 밑 Child Actor·Details 설정 검사 버튼/결과·미는 면 상자 편집/표시는 사용자 확인 필요, **not-tested**.
- Quest 3 Link/VR Preview: 문을 열 때 손잡이 동행, 열린 문의 80% 넘김, 상자 면 접촉·밀기, 반복 조작·추적 손실/복구를 확인할 것. 계획서 **T-03·T-08·T-09 모두 not-tested**. 합성 자동화가 실제 기기/손 추적/사용성 증거를 대신하지 않음.
- 자동화 후 `UnrealEditor*` 프로세스 없음. Claude 리뷰 대기, push·PR·merge 하지 않음.

## 이전 WIP 검증 기록 (`579a56c`, 현재 결과 아님)

- 빌드: non-unity(`-DisableUnity -Module=vBoR`)와 unity 각각 1회, `Result: Succeeded`, 프로세스 exit 0. vBoR 컴파일·링크 warning/error 0. 로컬 도구의 StructUtils 폐기 예정 안내 2줄은 각 로그에 별도로 있음.
- 로그: `Saved/Logs/P2eFinalNonUnityBuild.log`, `P2eFinalUnityBuild.log`.
- 전체 자동화 1회: 발견/완료 382, Success 370, Fail 12, `TEST COMPLETE. EXIT CODE: -1`, 프로세스 exit 255.
- 기존 시험 368개 중 366 Success / 2 Fail(Q1). 신규 P2e 14개 중 4 Success / 10 Fail. 새 E1·E2·E4·E5 연결/자세, E6 넘김, E11 무관한 부품, E15 얇은 면 밀기는 통과.
- 자동화 로그: `Saved/Logs/P2eFinalAllAutomation.log`, console `P2eFinalAllAutomationConsole.log`.
- 신규 시험 미해결: 예상 Warning은 UE 자동화의 message filter가 `Verbose`로 낮추므로, Warning만 세는 새 `FP2eLogs`가 0으로 센다. 엔진 근거 `Runtime/Core/Private/Misc/AutomationTest.cpp:282-315`. 문제 문구·횟수의 expected-message 검사는 충족되었지만 별도 계수 단언들이 실패했다. 계수기 수정 필요.
- 신규 E14의 "게임에서 숨김 + 에디터에서 보임" 복합 단언 1건도 실패했다. 원인 확정 전이며 이 실패를 정상으로 취급하지 않는다.
- Q1·Q2를 남긴 뒤 제품/시험을 더 고치거나 빌드·전체 시험을 반복하지 않았다. 기존 시험·근사 차·에셋·레벨·프로젝트 설정·플러그인 소스는 변경하지 않았다.
- Quest 3 Link·VR Preview·Blueprint·Details 버튼·실제 에디터 표시와 계획서 T-03·T-08·T-09: **not-tested**. MCP 호출 없음. 종료 시 `UnrealEditor*` 프로세스 없음.

## 개정 이력

| 날짜 | 회차 | 내용 |
|---|---|---|
| 2026-10-18 | 구현 중 질문 | Codex Q1·Q2(WIP `579a56c`) → Claude 답: 명세 D5⑤·E10·E12·§3 개정, 리뷰 메모 셋 |
| 2026-10-18 | 재개 중 질문 | Codex Q3: A가 먼저 가져가면 B의 부착 후보가 사라짐. 최초 소유 기록 제거와 E10의 뒤 판 경고 요구 충돌. 코드 수정·추가 검증 전 중단 |
| 2026-10-18 | Q3 답 | Claude: 판정은 그 순간의 칸·붙은 후보 기준, 처음 소유를 기억하지 않음. 명세 D5⑤·E10 개정 |
