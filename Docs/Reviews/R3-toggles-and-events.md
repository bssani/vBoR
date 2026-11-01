# R3 연결 끄고 켜기 + BP 이벤트·명령 — 리뷰

| 회차 | 날짜 | 대상 | 리뷰어 | 결과 |
|---|---|---|---|---|
| 1 | 2026-11-01 | `8707ad3` feat: 미러 전원 연동 선택과 부품 이벤트 | Claude | **열린 항목 없음 — 통과.** 빌드: unity 최신(Target is up to date — Codex 최종 빌드와 같은 트리). `vBoR.*` **695/695**, Fail 0, `EXIT CODE: 0`(`Saved/Logs/ReviewR3All.log`). R3 10/10 |

## 1회차 (2026-11-01, `main...codex/R3-toggles-and-events`)

### 확인한 것

- **AGENTS.md 제약:** 범위 안. 제품 변경은 미러·등받이·리프트게이트·HingedPanel의 프로퍼티·이벤트·명령뿐. Runner·전원 컴포넌트 변경 없음. 새 주석·표시 이름 영어. 에디터·BP·에셋 변경 없음. 기존 시험 파일 변경 없음(§3 "고쳐도 되는 단언 없음" 지킴).
- **D1:** `bFollowVehiclePower`(기본 true). 끄면 BeginPlay가 `ResetMirror` 뒤 바로 끝나 `FindUnique`·구독·`No vehicle power` 경고가 없다(`VBoRFoldingMirror.cpp` BeginPlay). 켬은 기존 경로 그대로(E2가 경고 0 확인, P5q 경고 수 3 시험 통과).
- **D2:** 전원 핸들러와 `Fold`/`Unfold`가 같은 `RequestMotion`을 쓴다. 같은 방향 이동 중·끝 상태면 false(소리·이벤트 없음), 반대면 지금처럼 모터 소리를 바꾸고 방향 전환. 개정 1의 "끝 상태 전원 무시"(E5)를 지킨다. 기존 흐름에서는 시동이 언제나 반대 상태에서 오므로 결과가 같다.
- **D3:** `SetState`가 같은 상태면 아무것도 하지 않고, 바뀌면 한 번 낸다(미러·등받이). 미러 Tick의 도착은 소리 처리 뒤 상태 이벤트. 리프트게이트 `PublishState`는 그대로 같은 상태도 낸다(E7 "Legacy gate state published even unchanged").
- **D4:** 네 클래스 모두 Reset 끝에 `OnReset`, 순서는 상태 → `OnReset`(E7). 처음 시작 억제:
  - **명세 §5 예시가 틀렸다.** 나는 "`HasActorBegunPlay()`가 true일 때만"을 예로 들었지만, UE 5.7 `AActor::BeginPlay`는 `ReceiveBeginPlay()` 뒤 자기 끝에서 `HasBegunPlay`로 바꾼다(`Engine/Source/Runtime/Engine/Private/Actor.cpp:4791-4793`). 그래서 파생 클래스의 `Super::BeginPlay()` **뒤**에서 부르는 Reset은 이미 true다. Codex가 이를 엔진 소스로 확인하고, BeginPlay 안에서 Reset을 부르는 미러·리프트게이트에 `TGuardValue<bool> bInitializingPlay`를 더했다(§5 재량 안).
  - 등받이는 Reset을 `OnConstruction`에서만 부르고(BeginPlay 없음) HingedPanel은 BeginPlay에서 `ResetDoor`를 부르지 않으므로, `HasActorBegunPlay()`만으로 충분하다. E8이 생성·시작(전원 꺼짐·켜짐)의 이벤트 0을 확인한다.
  - 같은 이유로 BP의 `Event BeginPlay`(= `ReceiveBeginPlay`)에서 바인딩해도 시작 때 Reset·상태 이벤트는 받지 않는다. 의도와 같다.
- **D5:** Runner 다시 시작 대상·순서 변경 없음. E9가 대상 부품 각 1번, 콘솔 0번, 기존 다시 시작 결과를 확인. E10이 켬에서 전원 끔 → `Folding` → `Folded` → `OnReset` 순서와 모터 요청 정리를 확인.

### 참고 (항목 아님)

1. `Fold()`/`Unfold()`는 같은 방향으로 **움직이는 중**에도 false다(명세는 "끝 상태면 false"만 적음). "움직이기 시작했으면 true"의 뜻에 맞고 E3이 확인하므로 그대로 둔다. BP에서 반환값을 "이미 그쪽으로 가는 중"과 구분해 쓸 일이 생기면 `GetState`로 본다.
2. **사용자 확인(not-tested):** 명세 §7 — 미러 BP에서 `Follow vehicle power`를 끄고 Play, 시동을 눌러도 안 움직이는지. 다른 BP 조건(예: 키 버튼)으로 `Unfold`. `OnStateChanged`·`OnReset`에 조명 등을 붙이고 진행자 R로 초기화되는지.

## 질문

없음.
