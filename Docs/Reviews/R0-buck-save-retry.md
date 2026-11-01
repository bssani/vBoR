# R0 정렬 저장 교체 다시 시도 — 리뷰

| 회차 | 날짜 | 대상 | 리뷰어 | 결과 |
|---|---|---|---|---|
| 1 | 2026-11-01 | `d83ea08` fix: 정렬 저장 교체 일시 오류 재시도 | Claude | **열린 항목 없음 — 통과.** 빌드: unity 최신(Target is up to date — Codex 최종 빌드와 같은 트리). `vBoR.*` **639/639**, Fail 0, `EXIT CODE: 0`(`Saved/Logs/ReviewR0All.log`). R0 7/7, P0Buck 14/14 |

## 1회차 (2026-11-01, `main...codex/R0-buck-save-retry`)

### 확인한 것

- **AGENTS.md 제약:** 범위 안. 제품 변경은 `FVBoRBuckAlignmentStore::Save`의 교체 부분과 헤더의 시험 hook 선언뿐. 새 주석·로그 영어. 하위 에이전트·에디터 작업 없음.
- **명세 D1–D5:**
  - D1: 오류 5·32만 다시 시도(`VBoRBuckAlignment.cpp:345`), 그 밖의 오류는 바로 끝. 최대 5번, 간격 50 ms.
  - D2: 끝내 실패하면 기존 경로 그대로 — 후보 삭제, 마지막 오류 번호로 `Reason`, `false`.
  - D3: 2번째 시도 이후 성공하면 `Warning` 한 줄(시도 번호, 직전 오류). `Reason`은 기존 `Reason.Reset()`으로 비움.
  - D4: 쉬는 횟수 최대 4번 × 50 ms = 200 ms(마지막 시도 뒤에는 쉬지 않음) ≤ 0.5 s.
  - D5: 후보 쓰기 → 검산 → 교체 순서, Windows 아닌 분기 그대로.
- **시험 hook:** `WITH_DEV_AUTOMATION_TESTS && PLATFORM_WINDOWS`에서만 있는 private 정적 함수 포인터, friend 접근 구조체로만 바꿈, RAII 가드로 원복(§5 권장과 같음). 패키지·Shipping에는 없다.
- **§3 기존 시험:** `P0BuckAlignmentTests.cpp` 변경 없음. `E8Persistence`의 잠긴 대상 시험은 다시 시도 뒤에도 거절(시험 통과).
- **로그 범주 `LogTemp`:** `Interaction/`에 이미 9곳에서 쓰는 방식이라 지적하지 않는다.

### 참고 (항목 아님)

1. `E1`과 `E7`, `E3`과 `E5`는 같은 시나리오를 한 번 더 돈다(경고 확인·시간 확인이 시나리오 함수 안에 함께 있음). 명세 ID를 하나씩 채운 것이라 그대로 둔다.
2. 실제 백신 잠금으로 다시 시도가 일어나는지는 자동 시험으로 볼 수 없다. 회사 PC에서 정렬 저장 때 `replacement succeeded on attempt` 경고가 나오면 빈도를 알려 주기만 하면 된다. 앞으로 전체 시험에서 `E8Persistence`가 다시 실패하면 오류 번호를 적는다(5·32가 아니면 다른 원인).

### 질문

없음.
