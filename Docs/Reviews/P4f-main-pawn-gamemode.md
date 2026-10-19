# P4f 본 Pawn·GameMode — 리뷰

명세: [P4f-main-pawn-gamemode.md](../Specs/P4f-main-pawn-gamemode.md) · 브랜치: `codex/P4f-main-pawn-gamemode` · 베이스: `main@d920df2`

상태: **1회차 리뷰 — 문제 1개(R1-1), Claude가 직접 반영함(`92c3c04`, 사용자 2026-10-19 "작은 작업은 너가"). 열린 항목 없음. 머지 승인 대기.**

## 리뷰 1회차 (2026-10-19, Claude — `6f1b042`)

**결과: 문제 1개(R1-1). 나머지는 명세대로다.** Codex의 구현 기록(보고 ①–⑤)은 커밋 `6f1b042` 본문에 있다.
- D1–D5가 모두 들어갔다. `AP3PalmGrabPawn`은 부모만 바뀌고 본문이 없다. `AVBoRHandRigPawn`·`AP3FlowGameMode`는 그대로다.
- 범위 밖 변경은 없다. 레벨·Config·Plugins·기존 시험 파일은 그대로이고, 기존 단언도 바뀌지 않았다.

**재현(R1-1 반영 트리).** 에디터가 닫힌 것을 확인했다. 바뀐 C++ 5개의 시각을 갱신한 뒤, 단계마다 에디터가 다시 열렸는지 보며 돌렸다.
- non-unity(`-DisableUnity -Module=vBoR`): `Result: Succeeded`, vBoR 경고·오류 0. 로그 `Saved/Logs/ReviewP4fNonUnity.log`.
- unity: `Result: Succeeded`, vBoR 경고·오류 0. 로그 `ReviewP4fUnity.log`.
- `vBoR.*`: 433/433(P4f 8개 포함), `TEST COMPLETE. EXIT CODE: 0`. 로그 `ReviewP4fAll.log`.
- 빌드가 끝난 뒤 `VBoRPawn.cpp` 주석의 줄 번호 하나(`161-163` → `161-164`)를 고쳤다. 주석만 바뀌어서 다시 빌드하지 않았다.

**Codex 토큰:** 8.66M(캐시 97%), 2026-10-19 14:18–14:32 UTC, 약 13분. 하위 에이전트는 0이다.

**확인한 것:**
- **D1 Pinch 순서**(`Interaction/VBoRPawn.cpp`의 `PreInitializeComponents`):
  - 부모(`Interaction/VBoRHandRigPawn.cpp:96-113`)가 손바닥 모드에서 Pinch를 끈다. 그 뒤 `Super`가 끝나고 두 손 Pinch를 다시 켠다. 옛 `AP3PalmGrabPawn`과 순서가 같다.
  - E1이 본 Pawn과 P3 자식 둘 다, 그리고 `AVBoRHandRigPawn` 기본의 손바닥 꺼짐까지 본다.
- **D2 메시·재질:**
  - `PreInitializeComponents` 때 컴포넌트는 이미 등록돼 있다. 그래서 `SetSkinnedAssetAndUpdate`가 렌더 상태까지 바꾼다.
  - ISDK 표시 생성자(`OculusInteractionPrebuilts/Private/Rig/IsdkHandVisualsRigComponent.cpp:32-47`)는 재질을 넣지 않는다. 메시를 넣을 때 `EmptyOverrideMaterials`를 불러도 잃는 값이 없다.
  - E5는 재질 칸 둘인 메시로 "모든 칸"을 확인한다.
- **D3 대체:**
  - ISDK `InitializeSkeleton`(`OculusInteraction/Private/IsdkHandMeshComponent.cpp:205-224`)은 늘 Valid나 Invalid로 끝난다. 이 함수는 컴포넌트 `BeginPlay`(`:161-164`)에서 불리고, 컴포넌트 `BeginPlay`는 Actor `BeginPlay` 안에서 먼저 돈다. 그래서 `Super::BeginPlay()` 뒤 한 번 보면 된다(R1-1의 근거).
  - 복구할 때는 기본 메시를 넣고 `ClearMappingState`로 None을 만든다. 그 뒤 ISDK Tick의 `UpdateMappingState`(`:360-383`)가 관절 소스가 있을 때 기본 메시로 다시 매핑한다.
  - E4는 SDK 오류 2개(두 표시)와 vBoR Warning 정확히 1개를 기대값으로 센다.
- **E7에서 찾은 `Template Mismatch` 보정**(`PreRegisterAllComponents`):
  - 원인은 리그 생성자의 중첩 `SetupAttachment`다(`OculusInteractionPrebuilts/Private/Rig/IsdkHandRigComponent.cpp:253-267`). Tracked·Synthetic 표시는 생성자에서 붙이지 않아(`IsdkHandVisualsRigComponent.cpp:32-47`) 해당이 없다.
  - 엔진 `SetupAttachment`(`Runtime/Engine/Private/Components/SceneComponent.cpp:2227-2247`)는 부모가 이미 같으면 아무것도 하지 않는다. 그래서 C++ Pawn과 에디터 재등록에서 ensure가 나지 않는다.
  - ensure는 한 세션에 한 번만 뜬다. 그래서 E7 단독 실행(`P4fBlueprintRegression.log`)이 깨끗한 것으로, 다른 중첩 부착 불일치가 없다는 것을 확인했다.
- **E6·E7:** C++ 기본값과 BP 부모·빈 손 칸·`DefaultPawnClass == BP_vBoRPawn_C`를 본다. BP 인스턴스를 실제로 띄워 부착 부모의 Owner까지 확인한다.

### R1-1 Pawn Actor Tick을 매핑 확인에 써서 BP 자식의 Event Tick이 돌지 않음 — 반영함 (Claude, `92c3c04`)

**문제:**
- 위치: `Interaction/VBoRPawn.cpp:15-18`(`6f1b042`). 생성자에서 `bStartWithTickEnabled = false`, `TickGroup = TG_PostPhysics`로 바꾸고, `:105`에서 `SetActorTickEnabled(bMappingPending)`을 부른다.
- 발생 조건: 위 D3대로 매핑은 `BeginPlay`에서 늘 끝난다. 그래서 `bMappingPending`은 실제로 늘 거짓이고, Pawn Tick은 `BeginPlay` 직후 꺼진다. Tick 경로는 쓰일 일이 없다.
- 영향: 산출물 `BP_vBoRPawn`에 Event Tick을 넣으면 돌지 않는다. Class Defaults에서 "Start with Tick Enabled"를 켜도 `BeginPlay`가 다시 끈다. Tick 그룹도 부모 `APawn` 기본(PrePhysics, 켜짐 — `Runtime/Engine/Private/Pawn.cpp:48-49`)에서 바뀌었다. 지금 BP에 로직이 없어 시험은 통과하지만, 원인을 찾기 어려운 함정이다.

**제안:** `Tick` override와 Tick 설정을 지우고, `Super::BeginPlay()` 뒤에 한 번만 확인한다.

**확인할 시험:**
- E2: Pawn의 Tick 설정(`bCanEverTick`·`bStartWithTickEnabled`·`TickGroup`)이 `AVBoRHandRigPawn` 기본과 같다. `BeginPlay` 뒤에도 Tick이 켜져 있다.
- E7: BP 인스턴스의 Tick이 `BeginPlay` 뒤에 켜져 있다.

**반영:** Claude가 직접 했다(사용자 2026-10-19 "작은 작업은 너가 하고, 그 외 작업은 codex").
- `VBoRPawn.h/.cpp`에서 `Tick`과 Tick 설정을 지웠다. 매핑은 `BeginPlay`에서만 확인한다.
- 손별 플래그는 남겼다. 시험 접근자로 다시 불러도 Warning은 한 번이다.
- 위 두 시험을 E2·E7에 단언으로 더했다. 고치기 전 코드라면 둘 다 실패한다(`SetActorTickEnabled(false)`).
- 명세 §3에 "Pawn Tick은 부모 기본 그대로"를 더했다(개정 이력).

### 참고 (막지 않음)

1. **리뷰 파일:** Codex가 구현 기록을 리뷰 파일로 만들었다. 내용은 커밋 본문과 같았다. 리뷰 파일은 Claude가 회차를 쓰고, Codex는 항목 상태와 "질문"만 고친다. 그래서 그 기록은 이 파일에서 지웠다(같은 내용이 `6f1b042` 본문에 있다). 다음부터 보고는 커밋 본문과 `HANDOFF.md`에만 쓴다.
2. **E4 "다시 시도해도 반복 안 함"은 약하다.** 복구 뒤 매핑이 None이라, 손별 플래그가 없어도 다시 경고하지 않는다. 지금처럼 `BeginPlay`에서 한 번만 확인하면 문제는 없다.
3. **`Template Mismatch` 보정은 `AVBoRPawn`에만 있다.** `AVBoRHandRigPawn`을 부모로 BP를 만들면 같은 ensure가 난다. P0–P2 Pawn은 C++ 그대로라 지금은 해당이 없다. 산출물 Pawn BP는 `AVBoRPawn`의 자식으로 만든다.
4. **사용자 확인**은 명세 §7 그대로다(에디터 Details, 임시 GameMode로 VR Preview, 준비한 손 메시, poke limiter). VR 묶음 확인에 넣는다. 시험 상태는 `not-tested`다.
