# R0 정렬 저장 — 파일 교체가 잠깐 거부되면 다시 시도

| 항목 | 값 |
|---|---|
| 단계 | 구조 보수 R0(`Docs/Refactor-Plan.md` §4). 시험 계획 T-ID 없음 — 정렬 저장(P0-c E8, P4h D4)의 신뢰성 |
| 결정 근거 | **간헐 실패:** `vBoR.P0Buck.E8Persistence`가 전체 시험에서 여러 번 실패했다(P1c·P4c·P5b·P5i·P5k·P5zb·P5zg 리뷰). 매번 `candidate replacement failed (OS error 5)`이고 다시 돌리면 통과한다. 실패 위치는 40번 반복 교체 중 8·17·24·30·33번째 등 일정하지 않다.<br>**원인(추정, 근거 아래):** 방금 쓴 대상 파일을 백신(Defender)·색인기가 잠깐 열고 있으면 `MoveFileExW(MOVEFILE_REPLACE_EXISTING)`가 `ERROR_ACCESS_DENIED`(5)를 낸다. 시험만의 문제가 아니라 **실제 Buck 정렬 저장**(`VBoRPointAlignment.cpp:153`)도 같은 경로다.<br>**엔진 선례:** UE 자체 `FFileManagerGeneric::Move`도 실패하면 0.5 s씩 쉬며 다시 시도한다(`Engine/Source/Runtime/Core/Private/HAL/FileManagerGeneric.cpp:302-339`) |
| 베이스 브랜치 | `main` |
| 작업 브랜치 | `codex/R0-buck-save-retry` |
| 리뷰 파일 | `Docs/Reviews/R0-buck-save-retry.md` (첫 리뷰 때 생김) |
| 상태 | 명세 작성 |
| 에디터 작업 | **없음** |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **교체가 일시 오류로 실패하면 짧게 쉬고 다시 시도한다** | `FVBoRBuckAlignmentStore::Save`의 `MoveFileExW`(`VBoRBuckAlignment.cpp:313`)가 `ERROR_ACCESS_DENIED`(5) 또는 `ERROR_SHARING_VIOLATION`(32)로 실패하면 다시 시도한다. **최대 시도 횟수와 간격은 구현자**(D4 한도 안). 다른 오류 코드는 지금처럼 바로 실패 |
| D2 | **다시 시도해도 실패하면 지금과 같다** | 후보 파일 삭제, `Reason = "candidate replacement failed (OS error N); previous save retained"`(마지막 오류 코드), `false`. 이전 저장 파일은 그대로 |
| D3 | **다시 시도해서 성공하면 경고 로그 한 줄** | 몇 번째 시도에서 성공했는지와 앞선 오류 코드를 `Warning`으로 남긴다(회사 PC에서 얼마나 자주 생기는지 보기 위함). `Reason`은 성공 때처럼 비운다 |
| D4 | **기다리는 총 시간 ≤ 0.5 s** | 저장은 게임 스레드에서 정렬 때 한 번 한다(설치자 조작). 0.5 s 멈춤은 허용. 잠긴 대상(시험 `:221-231`)은 이 시간만큼 기다린 뒤 실패해도 된다 |
| D5 | **나머지 그대로** | 후보 파일 쓰기 → 검산(`Load`) → 원자 교체 순서, 검산 실패 때 교체하지 않음, Windows 아닌 플랫폼 분기, 파일 형식(version 1·2) |

## 1. 왜 하는가

- 전체 시험이 가끔 실패하면 리뷰 때마다 "무관한 간헐 실패"인지 다시 판단해야 한다. 구조 보수(R2~)는 시험 632개에 기대므로 전체 시험이 믿을 만해야 한다.
- 같은 실패가 회사 PC에서 Buck 정렬 저장 때 나면 설치자는 4점을 다시 찍어야 한다.
- 시험을 고치거나 반복 횟수를 줄이는 것은 제품 경로의 위험을 그대로 둔다. 그래서 제품 코드를 고친다.

## 2. 범위

### 만들 것

- `FVBoRBuckAlignmentStore::Save`의 교체 다시 시도(D1–D4).
- 다시 시도를 결정적으로 시험할 수 있는 시험용 접근(§5).
- 시험 `vBoR.R0.*`(§4).

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| `IFileManager::Move`로 바꾸기 | 대상을 먼저 지워 원자 교체가 깨진다(코드 주석 `:307`, P0-c §3-7) |
| 후보 파일 쓰기(`SaveStringToFile`)·`Load`에 다시 시도 | 실패 보고가 없었다. 필요하면 따로 |
| `E8Persistence` 반복 횟수·단언 변경 | §3 |
| 다른 저장 파일(진행 기록 등) | 해당 없음 |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- 교체가 끝내 실패하면 **이전 저장 파일이 그대로** 남고 후보 파일은 지워진다.
- 잘못된 후보(무한대 yaw 등)는 지금처럼 교체 전에 거절된다.
- **기존 시험 감사(Claude 2026-11-01, `FVBoRBuckAlignmentStore` 검색).**
  - **바뀌지 않아야 하는 시험:** `P0Probe/P0BuckAlignmentTests.cpp`의 `vBoR.P0Buck.E8Persistence` 전체(`:186-233`) — 반복 교체 40번(`:207-214`), 잘못된 후보 거절(`:217-220`), **잠긴 대상 거절**(`:221-231`, `FILE_SHARE_READ`만 주고 열어 둠 → 다시 시도해도 계속 잠겨 있으므로 `false`와 이전 값 유지가 그대로 성립). 그 밖의 `vBoR.P0Buck.*`, `vBoR.P4h.*`(점 방식 저장), `vBoR.P3c.*`.
  - **고쳐도 되는 단언: 없음.** 위 시험을 고쳐야 통과하면 멈추고 리뷰 파일 "질문"에 적는다.
- 잠긴 대상 시험이 D4 한도(≤ 0.5 s)보다 오래 걸리면 안 된다.

## 4. 시험 — `vBoR.R0.*`

진짜 백신 잠금은 재현할 수 없으므로, 교체 호출을 시험에서 바꿔 끼워 실패를 흉내 낸다(§5).

| ID | 상황 | 확인 |
|---|---|---|
| E1 | 교체가 오류 5로 2번 실패한 뒤 성공 | `Save` = `true`, `Reason` 비어 있음, 새 값이 읽힘, 후보 파일 없음, 교체 호출 3번 |
| E2 | 오류 32로 1번 실패한 뒤 성공 | E1과 같음(호출 2번) |
| E3 | 오류 5가 계속 | `Save` = `false`, `Reason`에 `OS error 5`, 이전 값이 읽힘, 후보 파일 없음, 호출 수 = 구현자가 정한 최대 횟수 |
| E4 | 다른 오류(예: 3 `ERROR_PATH_NOT_FOUND`) 1번 | 바로 실패(호출 1번), 이전 값 유지 |
| E5 | E3의 경과 시간 | ≤ 0.5 s + 시험 여유(구현자가 정함, 예: 0.2 s) |
| E6 | 시험용 교체 함수를 끼운 뒤 시험이 끝남 | 원래 교체로 돌아옴 — 다음 시험(`E8Persistence` 등)이 진짜 `MoveFileExW`를 쓴다 |
| E7 | D3 로그 | E1에서 `Warning` 한 줄(시도 횟수 포함). 로그 확인 방법은 `AddExpectedMessage` 등 구현자 |

## 5. 구현자가 정할 것

- 최대 시도 횟수와 간격(D4 안). 예: 5번 × 50 ms, 또는 점점 늘리기.
- **시험용 교체 함수를 끼우는 방법.** 예: `FVBoRBuckAlignmentStore` 안 정적 함수 포인터(기본값 = `MoveFileExW` 호출)를 시험 접근 구조체(`friend`)만 바꾸게 하고, 범위를 벗어나면 되돌리는 가드. 제품 코드에 공개 API로 노출하지 않는다.
- 쉬는 방법(`FPlatformProcess::Sleep`).
- 새 시험 파일 위치(`P0Probe/` 권장).

## 6. 완료 조건

1. D1–D5 구현, §3 조건 지킴.
2. `vBoR.R0.E1`–`E7` 통과, `vBoR.P0Buck.*` 변경 없이 통과.
3. non-unity(`-Module=vBoR`)·unity 빌드 성공, `vBoR.*` 전체 통과(작업 끝에 한 번).
4. 새 주석·로그 영어(AGENTS.md).

## 7. 로컬에서 확인할 것

- 빌드: 위 3.
- 에디터: 없음.
- 전체 시험이 이 뒤로도 `E8Persistence`에서 실패하면 로그의 OS error 번호와 D3 경고 유무를 리뷰 파일에 적는다(다른 원인이면 따로).
- Quest 3: 없음(회사 PC 정렬 저장 때 D3 경고가 나오면 알려 주기만).
