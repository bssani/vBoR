# P4h 4점 정렬 — 리뷰

명세: [P4h-point-alignment.md](../Specs/P4h-point-alignment.md) · 브랜치: `codex/P4h-point-alignment` · 베이스: `main@f5228bf`

상태: **1회차 리뷰 — 문제 3개(R1-1·R1-2·R1-3), Claude가 직접 반영함(`b25535f`, 사용자 2026-10-19 "작은 작업은 너가"). 열린 항목 없음. 머지 승인 대기.**

## 리뷰 1회차 (2026-10-20, Claude — `8d51f82`)

**결과: 문제 3개.** R1-1·R1-2는 내 명세의 빈틈에서 나왔다. R1-3은 구현 버그로, R1-2를 고치며 더한 시험이 찾았다. 나머지는 명세대로다.
- D1–D9가 모두 들어갔다. 에디터·에셋·Config·플러그인 변경은 없다.
- 기존 단언은 바뀌지 않았다. P0-c 벤치는 version 2 처리와 시험용 `friend` 한 줄만 더했다.
- Codex는 보고를 커밋 본문과 HANDOFF에만 썼고, 리뷰 파일은 만들지 않았다(P4f 참고 1을 지켰다).

**재현(R1-1–R1-3 반영 트리).** 에디터가 닫힌 것을 확인했다. 바뀐 C++의 시각을 갱신한 뒤, 단계마다 에디터가 다시 열렸는지 보며 돌렸다.
- 첫 회(R1-1·R1-2만 반영): 빌드 둘은 성공했고 `vBoR.*`는 446/447이었다. 실패는 새 E13E14의 "Child hidden in game"이고, 이것이 R1-3이다. 로그 `Saved/Logs/ReviewP4hAll.log`.
- 최종(R1-3 반영):
  - non-unity(`-DisableUnity -Module=vBoR`): `Result: Succeeded`, vBoR 경고·오류 0. 로그 `ReviewP4h2NonUnity.log`.
  - unity: `Result: Succeeded`, vBoR 경고·오류 0. 로그 `ReviewP4h2Unity.log`.
  - `vBoR.*`: 447/447(P4h 14개 포함), `TEST COMPLETE. EXIT CODE: 0`. 로그 `ReviewP4h2All.log`.

**Codex 토큰:** 4.75M(캐시 96%), 2026-10-19 15:37–15:48 UTC(KST 10-20 00:37–00:48), 약 11분. 하위 에이전트는 0이다.
- 첫 non-unity 두 번은 새 코드의 컴파일 오류로 실패했다(`TArray`의 `Units` 메타, `TObjectPtr`의 `auto*`). 고친 뒤 최종 트리로 한 번 돌렸다(보고 ③).

**확인한 것:**
- **D3 풀이**(`Interaction/VBoRBuckAlignment.cpp`의 `FitPoints`):
  - θ = atan2(Σ교차, Σ내적), t = w̄ − R·p̄, Z = 바닥이다. 명세 식과 같다.
  - Yaw 부호는 `FRotator(0,θ,0)`로 그대로 쓴다. E1은 30°와 −30°, −120°, 179°를 본다.
  - 모든 점이 한 곳에 겹치면 실패로 돌려준다(교차²+내적²≈0). 퍼짐은 Stage XY 최대 거리다.
- **D2 기록**(`Interaction/VBoRPointAlignment.cpp`의 `AdvancePointCapture`):
  - world 손끝을 Pawn 역변환으로 Stage에 옮긴다. 추적이 끊기거나, 원점이 Stage가 아니거나 바뀌면 그 점만 취소한다. 흩어짐이 한계를 넘으면 거절하고 같은 순서에 머문다.
  - 시간은 `FPlatformTime::Seconds`(가상 함수 — 시험이 바꿔 넣음)로 잰다. frame 수와 무관하다(참고 1).
- **D4 저장**(`VBoRBuckAlignment.cpp`의 `Load`·`Save`):
  - version 1·2를 나눠 읽고 쓰고, 원자 교체는 그대로다. version 2는 점마다 `Order` 중복·흩어짐·frame 수를 검사한다.
  - P0-c 벤치(`P0Probe/P0BuckBench.cpp:142-150`)는 version 2를 "No saved alignment"로 두고 이유를 보인다.
- **D5 적용**(`BasePawn`·`FitCaptured`):
  - 지금 차 점으로 다시 풀고, 개수·`Order`가 다르면 "alignment points changed", 차이가 한계를 넘으면 "alignment points moved"다. 둘 다 Pawn을 옮기지 않는다.
  - version 2는 `BaseEye`를 요구하지 않는다.
- **D6 전환:**
  - version 1 기준 Pawn은 옛 식 그대로다(`CalculatePawnOnFloor(저장, BaseEye, BaseEye)`·`CalculatePawn`). 시점은 기준 + (시점 − 기준 좌석점)이다.
  - 근사 차는 `Car.DriverView`가 없어 기준 좌석점이 `BaseEye`다. 시점 Yaw가 기준과 같으므로, P3 E11의 0.1 mm·0.01° 단언이 그대로 맞는다.
- **D7 진행자:**
  - 점 명령은 흐름 명령 셋으로 큐에 들어간다(P5i E2 그대로). `Before the flow starts only`는 러너에서, 나머지 이유는 정렬 컴포넌트에서 낸다.
  - 저장은 `bPointSavePending`으로 `FinishReapply`의 흐름 시작을 건너뛴다. `A`·`O`·단계 진입·자유모드 진입에서 `FlowStarted()`가 표시를 숨긴다.
  - P5i 시험 Pawn(`APawn`)에서는 `CanReadPointTip`이 거짓이라 `Capture point`가 꺼진다. 그래서 E3·E5·E6 집합이 그대로다.
- **D9:** 근사 차 점 4개의 자리·`Order`·`Label`이 명세와 같다. 안내 문구에 `저장값 없음` 접두와 `P0-c`가 남아 있다.

### R1-1 기준 좌석점이 없으면 좌석 전환이 조용히 아무것도 안 함 — 반영함 (Claude, `b25535f`)

**문제:**
- 위치: `Interaction/VBoRVehicleAlignmentComponent.cpp`의 `PawnFor`(`8d51f82` `:34-41`)와 `SwitchRejection`(`:146-151`).
- 발생 조건: version 2 저장이고, 차에 `Car.DriverView`도 `BaseEye`(눈 점)도 없을 때다.
  - `SwitchRejection`은 version 2에서 `BaseEye`를 보지 않아 동승석 전환을 받는다.
  - `PawnFor`는 기준 좌석점이 없으면 차이를 더하지 않는다. 그래서 Pawn은 운전석 그대로인데, `CurrentViewpoint`와 진행자 창은 "Passenger"가 된다.
- 영향: 회사 차에서 일어날 수 있다. 내가 설계 눈 위치를 "선택"이라고 안내했고(Company-PC-Guide §4), 운전석 SgRP에 `Car.DriverView`를 빠뜨리면 이 상태다. 14·15번이 아무 경고 없이 운전석 시점으로 진행된다.
- 같은 함수의 둘째 문제: `BasePawn`이 실패하면 `PawnFor`가 `FTransform::Identity`를 돌려준다. 그러면 `AdvanceSwitch`가 Pawn을 **월드 원점으로** 옮긴다. 정렬 뒤 누가 점을 옮기는 등 드물지만, 결과가 나쁘다.
- 명세 D6에 "둘 다 없을 때"가 없었다. 내 명세의 빈틈이다.

**제안:**
- `SwitchRejection`: 기준 복귀가 아니면 기준 좌석점을 요구한다(`Driver seat reference missing`). 진행자 창 버튼도 같은 이유로 꺼진다.
- `PawnFor`는 성공 여부를 돌려준다. 실패하면 `AdvanceSwitch`가 Pawn을 두고 `NeedsCheck`로 바꾼다. 그러면 다음 Tick에 기존 "정렬 확인 필요 → 전환 버림" 길(`:205-211`)이 암전을 풀고 끝낸다.

**확인할 시험:** `vBoR.P4h.E9bMissingSeatReferenceAndFailedBase`
- version 2, 눈 점·`Car.DriverView` 없음: 동승석 거절(이유 일치), 기준 복귀는 허용, Pawn 그대로.
- 전환 중 정렬 점을 30 cm 옮김: Pawn 그대로, `NeedsCheck`("alignment points moved"), 전환 버림.

**반영:** Claude가 직접 했다. `PawnFor(시점, 결과, 실패)`로 바꾸고, `SwitchRejection`·`AdvanceSwitch`를 위 제안대로 고쳤다. 근사 차 결과는 그대로다(기준 `BaseEye`가 있으므로).

### R1-2 정렬 점이 에디터에서도 안 보임 — 반영함 (Claude, `b25535f`)

**문제:**
- 위치: `Interaction/VBoRAlignPointComponent.cpp` 생성자의 `SetVisibility(false,true)`(`8d51f82` `:18`).
- 영향: 에디터 뷰포트에서도 구와 번호가 그려지지 않는다(에디터는 `bVisible`을 보고, `bHiddenInGame`은 게임에서만 본다).
  - D1의 주 사용처는 "차량 Blueprint에서 CAD 메시 자리에 점을 붙이는 것"이다. 점이 안 보이면 컴포넌트를 하나씩 골라 기즈모로만 맞춰야 하고, 네 점을 한눈에 확인할 수 없다.
  - 내 명세가 "기본은 숨김"이라고만 써서 그렇게 읽혔다. Codex는 명세대로 했다.

**제안:** 생성자에서 `SetHiddenInGame(true,true)`만 쓴다. `ShowPoint`는 `bHiddenInGame`만 바꾼다. 게임 안 동작(D8)은 같다.

**확인할 시험:** `vBoR.P4h.E13E14MockMarkers`
- 기본: 에디터 보임(`GetVisibleFlag`) + 게임 숨김(`IsVisible` 거짓). 자식(구·번호)도 같다.
- 기록 중: 게임에서 보임. 취소: 게임 숨김, 에디터 보임 유지.

**반영:** Claude가 직접 했다. `PointHand`에 툴팁(0 = 왼손, 1 = 오른손)도 더했다. Details에 숫자만 보여서 어느 손인지 알 수 없었다.

### R1-3 표시의 구·번호가 게임에서 늘 보임(참가자 흐름 중에도) — 반영함 (Claude, `b25535f`)

**문제:**
- 위치: `Interaction/VBoRAlignPointComponent.cpp` 생성자(`8d51f82` `:18`).
- 원인: 생성자의 `SetVisibility(false,true)`·`SetHiddenInGame(true,true)`는 자식에게 전파되지 않는다. 생성자에서는 `SetupAttachment`가 부모만 정하고 `AttachChildren`은 등록 때 채워지기 때문이다. 그래서 정렬 점 자신만 숨고, 실제로 그려지는 구(`UStaticMeshComponent`)와 번호(`UTextRenderComponent`)는 보임 상태 그대로다.
- 영향: 근사 차 운전석 방석 모서리에 노란 번호와 구가 **참가자 흐름 내내** 보인다. D8("참가자 흐름 중에는 보이지 않는다")을 어긴다.
  - `ShowPoint`는 등록 뒤에 불리므로 전파가 된다. 그래서 한 번 `P`를 눌렀다가 취소하면 그때부터는 숨는다.
  - Codex의 시험은 정렬 점 자신의 보임만 봐서 이걸 놓쳤다(자식은 충돌·태그만 봄).

**제안:** 생성자에서 자식 둘에도 직접 `SetHiddenInGame(true)`를 건다. 런타임 전환은 지금처럼 전파로 한다.

**확인할 시험:** `vBoR.P4h.E13E14MockMarkers`
- 자식이 둘(구·번호)인지 먼저 센다. 그래야 자식 단언이 빈 반복이 되지 않는다.
- 기본: 자식도 게임 숨김. 기록 중: 자신과 자식 모두 게임 보임. 취소: 모두 게임 숨김.

**반영:** Claude가 직접 했다. 고치기 전 코드로는 이 시험의 "Child hidden in game"이 점마다 실패했다(`ReviewP4hAll.log`, 446/447). 고친 뒤 통과했다.

### 참고 (막지 않음)

1. **기록 창 시간:** 머리 기록 창은 게임 `DeltaSeconds`로 재는데, 점 기록은 벽시계(`FPlatformTime::Seconds`)로 잰다. 둘 다 frame 수와 무관하다. 다만 PIE 일시정지가 창 안에 끼면 벽시계는 그 시간을 센다. 설치 때 일시정지할 일은 없어서 그대로 둔다.
2. **BP 안 중첩 부착 위험:** 정렬 점은 생성자에서 자식(구·번호)을 `SetupAttachment(this)`로 만든다. P4f에서 같은 구조(ISDK 리그 → 손 표시)가 **네이티브 부모의 BP 자식**에서 `Template Mismatch` ensure를 냈다(P4f 리뷰).
   - 근사 차는 C++ 그대로라 해당이 없다.
   - 회사 차 BP에 이 컴포넌트를 넣을 때(또는 P4g 뒤 "BP 만드는 법" 문서를 쓸 때), BP를 실제로 띄워 ensure가 없는지 본다.
3. **사용자 확인**은 명세 §7 그대로다(집: P×4 → 거절 이유 → 한계 키워 저장 → 표시·재짚기 → A 시작, P0-c 거절과 C 덮어쓰기). 이번 반영으로 에디터 뷰포트에서 근사 차의 노란 번호와 작은 구가 보여야 한다. 시험 상태는 `not-tested`다.
