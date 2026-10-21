# P5u 눈 방식 정렬 스위치

## 질문

### Q1 — 파일 기반 기존 시험과 D2·§3 감사 충돌 (답함 — 명세 개정 2, 2026-10-22 Claude)

**문제**

- 명세 D1·D2는 기본값 0에서 `LoadSetupAlignment`가 version 1 파일을 적용하지 않도록 요구한다. 반면 §3·E11·완료 조건 2–3은 기존 시험 파일을 수정하지 않고 모두 통과하도록 요구한다.
- §3 감사에 빠진 파일 읽기 경로가 있다. `Source/vBoR/P3Probe/P5iOperatorPanelTests.cpp:97-105`의 `FP5iWorld`는 기본 version 1인 `Saved`를 임시 파일에 쓰고 `NotifyBeginPlay`를 호출한다. `UP5iStoredAlignment`는 경로만 바꾸므로 실제 `LoadSetupAlignment`를 거친다. 직접 `InitializeAlignment`를 호출하는 경로가 아니다.
- 따라서 기본값 0에서 이 fixture의 흐름·좌석·재적용 시험은 정렬이 없어 실패한다. 예: 같은 파일 `:303-307`은 파일 재적용 뒤 흐름 시작을 요구하고, `:637-643`은 재적용으로 저장 자세·HomeShift 복원을 요구한다. D2를 지키면 이 전제를 만족할 수 없다.
- 최종 전체 실행: 발견/완료 547/547, 성공 525, 실패 22. 기존 536개 중 514 성공/22 실패. 신규 P5u 11/11 성공이나 **명세 E11(기존 회귀 전체)은 실패**다. `E11LegacyBenchBackupCleanup` 성공은 기존 회귀 통과를 뜻하지 않는다.
- 실패 범위는 P3c 4개, P4c 1개, P5i 16개, P5o 1개다. 모두 위 파일의 `FP5iWorld`를 사용한다. 정확한 이름·실패 기대값은 `Saved/Logs/P5uAllAutomation.log`에 있다. 로그 `TEST COMPLETE. EXIT CODE: -1`, 프로세스 exit 255.

**제안 / 질문**

파일 기반 version 1 흐름을 전제로 하는 `FP5iWorld`에서만 `vBoR.EyeAlignment`를 켜고 수명 종료 때 원래 값으로 복구하도록 시험 도우미를 수정해도 되는가? §3의 수정 허용 목록에 해당 fixture를 명시하고, 기존 기대값과 기본값 0의 P5u 차단 시험은 유지하는 방향을 권장한다. 필요한 높이·환경 관련 기존 기대값도 Claude가 감사해 명세에 적어야 한다. 제품 코드에 자동화 전용 차단 예외를 넣거나 기본값을 1로 바꾸지는 않는다.

관련 엔진 API 근거: `Engine/Source/Runtime/Core/Public/HAL/IConsoleManager.h:633`(`GetInt`), `:750`(`Set`), `:768`(현재 우선순위). 현재 구현에서 새 콘솔 변수 등록·읽기는 같은 헤더 `:1826`, `:1844`를 사용한다.

**확인할 시험**

- 명세 개정 뒤 허용된 fixture 변경 범위만 반영하고, P3c/P4c/P5i/P5o의 위 회귀와 P5u를 확인한다. 기존 기대값은 완화하지 않는다.
- 기본값 0의 version 1 파일 차단, 직접 초기화 경로 유지, version 2 불변을 계속 검사한다.
- 이번 실행 이후 빌드·전체 시험 재실행, 기존 시험 수정, D5 설정 파일·에디터 확인은 하지 않고 중단했다. `Saved/Config/WindowsEditor/Engine.ini`는 만들지 않았다. BP·레벨·Config·플러그인 변경도 없다.

**답 (Claude, 2026-10-22) — 제안대로 한다. 원인은 내 명세 §3 감사다.**
- 감사에서 "기존 시험은 모두 `InitializeAlignment`에 직접 넣는다"고 썼다. 하지만 파일에서 읽는 시험 도우미가 둘 있었다. 그중 `FP5iWorld`가 version 1을 파일로 읽는다.
- 이번에는 인용된 줄만 보지 않고, `SavedAlignmentPath`를 바꾸는 도우미를 모두 다시 찾았다.
  - `UP4hTestAlignment`는 파일에 version 2만 쓰고, version 1은 직접 넣는다(`P4hPointAlignmentTests.cpp:212-214`). 바꾸지 않는다.
  - `FP5iWorld`만 바꾼다.
- **허용(명세 §3 개정 2):** `FP5iWorld` 도우미 본문만 고친다.
  - 생성자에서 `NotifyBeginPlay` 전에 `vBoR.EyeAlignment`를 1로 켠다.
  - 소멸자에서 world를 지운 **뒤에** 원래 값으로 되돌린다. 먼저 되돌리면 D6의 끄는 감지가 정리 중에 돈다.
  - 시험 본문·기대값은 바꾸지 않는다. 본보기는 같은 파일의 `FP3cEnabled`(`:518-523`)지만, 0이 아니라 원래 값으로 되돌린다.
- **높이 기대값은 그대로다.**
  - 근사 차 눈 점 높이 115 cm(`P3VehicleMock.h:41`)와 저장값 높이 115 cm(`P5iOperatorPanelTests.cpp:98`)가 같다. 그래서 눈 기준(D3)과 바닥 기준이 모두 Pawn Z = 0이다(`VBoRBuckAlignment.cpp:61-68`).
  - 차를 Yaw로만 돌리는 시험(`:548`·`:566`)도 같다. E19(`:351`)는 이미 `CalculatePawn`과 같기를 기대한다.
  - 문장 기대는 `Contains`와 같은 시험 안의 전후 비교뿐이라, D3 표시가 붙어도 깨지지 않는다.
- 제품 코드에 자동화 전용 예외를 넣지 않고 기본값도 0 그대로라는 판단에 동의한다.
- 이어서 할 것:
  - 도우미를 고친다.
  - 빌드와 전체 시험을 한 번 돌린다(E11).
  - D5 에디터 확인을 한다.
  - 실패가 남으면 다시 멈춘다.
