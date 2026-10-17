# P2e 부품 조립 — 구현 중 질문

2026-10-18, Codex 앱. Claude 리뷰 전이며, 아래는 구현자가 남기는 질문과 검증 기록이다.
브랜치 `codex/P2e-part-assembly`, 베이스 `main` `bd3d765`.
상태: **중단 — 명세 해석 결정 필요. 구현 완료·리뷰 통과 아님.**

## 질문

### Q1. D3·D5에 맞춰 기존 시험의 준비만 바꿔도 되는가?

상태: **열림**

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

### Q2. 근사 차 글러브박스의 기존 부착 순서는 어떻게 취급하는가?

상태: **열림**

**문제**
- `Source/vBoR/P3Probe/P3VehicleMock.cpp:159-164`는 손잡이 `FinishSpawning` → 판의 칸 지정 → 판 `FinishSpawning` → 손잡이를 경첩에 부착하는 순서다. 실제 Play 중에는 판 `FinishSpawning` 안의 BeginPlay가 기존 명시적 부착보다 먼저 실행된다.
- 새 D3를 그대로 적용하면 그 BeginPlay에서 손잡이를 먼저 붙이고 `P2e attached` Log 1개가 생긴다. 그 뒤 기존 코드가 같은 경첩으로 다시 `AttachToComponent`를 호출한다. 바깥·안쪽 문 손잡이는 기존에 먼저 붙인 뒤 setter를 부르므로 이 경우와 다르다.
- 명세 `:46`, `:56`, E12는 근사 차 변경 금지·두 번 붙이지 않음·기존 연결/자세/로그 유지를 요구한다. 새 시험의 엄격한 "근사 차 재부착 로그 0"은 1개로 실패했다. 추가 경고는 0이며, 이 시험의 나머지 연결·자세 단언은 통과했다.

**제안 — Claude 결정·명세 개정 필요**
- 근사 차 소스를 유지하고, 먼저 발생하는 자동 부착 1회와 이후 동일 부모에 대한 호출을 허용할지 명시한다. 허용한다면 새 E12의 로그 기대를 글러브박스 1회로 정한다.
- 또는 근사 차의 부착 순서만 바꿔도 되는지 범위 예외를 명시한다.

**확인할 시험**
- 실제 근사 차에서 문·글러브박스·콘솔의 연결·월드 자세·부모·신규 경고 0, 결정된 부착 로그 수.

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
