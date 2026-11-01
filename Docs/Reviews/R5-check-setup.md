# R5 설정 검사 — 리뷰

| 회차 | 날짜 | 대상 | 리뷰어 | 결과 |
|---|---|---|---|---|
| 1 | 2026-11-02 | `25f6e5e` feat: Runner 레벨 전체 설정 검사(R5) | Claude | **열린 항목 없음 — 통과.** 빌드: unity 최신(Target is up to date — Codex 최종 빌드와 같은 트리). `vBoR.*` **708/708**, Fail 0, `EXIT CODE: 0`(`Saved/Logs/ReviewR5All.log`). R5 13/13 |

## 1회차 (2026-11-02, `main...codex/R5-check-setup`)

### 확인한 것

- **AGENTS.md 제약:** 범위 안. 제품 변경은 Runner(`CheckSetup`·`SetupReport`, `FindP3Tag`의 선택 인자)와 `HingedPanel::EvaluateSetup`의 칸 불일치뿐. Play 동작·로그 변경은 D7 하나. 새 주석·로그·표시 이름 영어. 에디터·BP·에셋 변경 없음. 기존 시험 파일 변경 없음.
- **D1:** `CallInEditor` `Check setup`, Transient `SetupReport`(Category `Guidance|Setup`). Runner 상태를 쓰지 않는다 — E12가 Actor 수·`Targets`·기록·`ValidationProblems`·진행·키 바인딩·버튼 구독을 전후 비교.
- **D2:** `InitializeFlow` 본문은 그대로이고, `FindP3Tag`에 기본값 `nullptr`인 예측 목록 인자만 더했다(Play 호출은 인자 없음 → 동작 같음). 문구가 같은 것은 E4(정확한 두 줄)와 E5(Play `ValidationProblems`와 줄·순서 비교, 경로 포함)가 확인.
- **D3:** Play 전(`!HasBegunPlay()`)에만 키 테이블 `KeyClass`, 그 키의 두 버튼 칸, 놓인 키의 두 버튼 칸, 리프트게이트 두 버튼 칸의 CDO를 예측 목록에 넣는다. 빈 칸·Abstract·Deprecated 칸은 `CreateButton`처럼 뺀다. 같은 키 클래스를 쓰는 테이블이 둘이면 CDO가 두 번 들어가 2로 센다(맞음). 종류 판정은 CDO의 클래스로 Play와 같다. Play 중에는 실제 Actor만(E6 `found 3`, 5 아님).
- **D4:** 버튼 칸 클래스의 태그가 시나리오 태그·`Car.StartButton`이면 칸마다 한 줄(누가·칸·클래스·태그). 키 클래스 자체 태그는 설명하지 않는다(E3).
- **D5:** 시동 버튼 0개면 알림, 버튼 아닌 Actor·컴포넌트 태그는 문제, 전원 0 알림 / 2 이상 문제. 예측 버튼도 센다.
- **D6:** 미러 부모 조건은 미러 BeginPlay와 같다. 손잡이는 모든 문 `GetHandleClaims` 합집합으로 판정. 각 문 `EvaluateSetup`의 Problems·Notices를 이름을 붙여 포함(E11).
- **D7:** 기존 두 경우(바깥 칸 Inside, 안쪽 칸 Outside)는 같은 문구, 그 밖의 불일치(`None` 제외)는 새 문구, 칸마다 한 줄, slot wins 유지. 그립 칸에 Outside는 예전에 "opposite" 문구였는데 이제 새 문구다 — 이 경우를 쓰는 기존 시험은 없다(명세 §3 감사대로).
- **D8:** 헤더 1줄 + 문제 Warning·알림 Log를 한 줄씩, 모두 `setup check:` 접두(E13).

### 참고 (항목 아님)

1. **검사 코드가 `InitializeFlow`와 두 벌이다**(`VBoRScenarioRunner.cpp`의 `CheckSetup` 중 "Read-only counterpart" 부분, 약 50줄). `InitializeFlow`가 검사와 바인딩·기록을 섞어 하므로, Play 순서를 건드리지 않으려고 복사한 것이다(명세 §5 재량). 앞으로 `InitializeFlow`에 검사를 더하거나 문구를 바꾸면 `CheckSetup`도 같이 고쳐야 한다. **E5가 두 출력을 줄 단위로 비교하므로** 한쪽만 고치면 그 시험이 실패한다 — 그게 안전장치다. 다음에 Runner 검사를 고치는 명세에는 "두 곳 모두"를 적는다.
2. D4 설명 줄은 문제(Warning)로 센다. 버튼 칸 클래스에 시나리오 태그를 일부러 붙여 그 버튼 하나만 대상으로 쓰는 경우(found 1)에도 설명이 나온다. 지금 그렇게 쓰는 곳은 없다. 생기면 알림으로 내릴지 정한다.
3. **사용자 확인(not-tested):** `L_Main`의 Runner Details → `Check setup`. ① 지금 레벨의 결과(미러가 문 밑에 있는지, 그립이 연결됐는지 등) ② 버튼 BP Class Defaults에 시나리오 태그를 넣고 리프트게이트 버튼 칸에 넣었을 때 `found 3` + 설명 2줄(확인 뒤 되돌림) ③ Output Log를 `setup check`로 검색했을 때 모든 줄이 보이는지. 결과에 이해하기 어려운 줄이 있으면 알려 주면 문구를 고친다.

## 질문

없음.
