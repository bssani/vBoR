# P5u 눈 방식 정렬 스위치

## 질문

### Q1 — 파일 기반 기존 시험과 D2·§3 감사 충돌 (열림, 2026-10-22 Codex)

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
