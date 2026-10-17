# P2e 부품 조립 — 리뷰

2026-10-18, Codex 앱. Claude 리뷰 전이며, 아래는 구현자가 남기는 질문과 검증 기록이다.
브랜치 `codex/P2e-part-assembly`, 베이스 `main` `bd3d765`.
상태: **Q1·Q2 답함(2026-10-18, Claude) — 명세 개정함. Codex가 같은 브랜치에서 이어서 구현한다.**

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

## 리뷰 메모 (Claude, WIP `579a56c`를 읽고 — 이어서 할 때 참고)

- **`SetAxisHandle`이 부를 때마다 판정 목록 전체를 Warning으로 낸다**(`Interaction/VBoRHingedPanel.cpp` WIP의 `SetAxisHandle`). 근사 차처럼 코드에서 여러 번 부르면 같은 문제가 되풀이된다(E20 "문제마다 1번"). 전체 목록은 BeginPlay에서 한 번만 내고, `SetAxisHandle`은 그 호출에서 생긴 문제만 낸다(⑤ 거절, ⑨ 열린 판).
- **새 시험의 경고 계수기**(`FP2eLogs`): 엔진은 expected message로 등록한 Warning을 Verbose로 낮춰 넘긴다(`Runtime/Core/Private/Misc/AutomationTest.cpp:289-291`). 같은 문구의 Warning·Verbose를 세면 된다. P5l에서 같은 일을 이렇게 풀었다(`P3Probe/P5eRearviewMirrorTests.cpp:61-72`).
- **E14 "게임에서 숨김 + 에디터에서 보임" 실패 원인:** `USceneComponent::IsVisible()`은 `bHiddenInGame`이 켜져 있으면 바로 거짓을 돌려준다(`Runtime/Engine/Private/Components/SceneComponent.cpp:3528-3536`, Claude 확인). 제품 코드 문제가 아니라 시험의 단언 문제다. "에디터에서 보임"은 `GetVisibleFlag()`(`Runtime/Engine/Classes/Components/SceneComponent.h:1640`)로 본다.

## 검증 기록

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
