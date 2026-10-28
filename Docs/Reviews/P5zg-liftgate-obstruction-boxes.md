# P5zg 리프트게이트 끼임 판정 상자 — 리뷰

## 1회차 (2026-10-28, Claude) — 통과 (문제 없음, 참고 3개)

- 리뷰한 커밋: `e01af6f` feat: 리프트게이트 판정 상자 지원. 베이스 `main@bdca41b`(명세 개정 2).

### 확인한 것

| 항목 | 결과 |
|---|---|
| D1 | `UVBoRObstructionBoxComponent`(부모 `UBoxComponent`, `BlueprintSpawnableComponent`, 표시 이름 `Obstruction Box`). 생성자에서 게임 숨김·`NoCollision`·모든 채널 무시·겹침 이벤트 끔·내비 영향 없음. 모양 데이터만 |
| D2 | `BeginPlay`에 한 번 모음(`CollectObstructionBoxes`). 유효 상자가 있으면 `FindObstruction`이 상자만 보고, 0개면 기존 메시 경계 코드를 한 줄도 바꾸지 않고 그대로 탐 |
| D3 | 상자 자세 = `HingePivot` 기준 상대 변환(지금 경첩 회전을 뺀 값, 중간 부모 포함) × 중간 각 경첩 × Actor — `MeshPoseAt`과 같은 방식. 크기 = unscaled extent × 합성 배율 절댓값. 여유는 상자마다 그 상자의 경첩 거리로 기존 식 그대로 |
| D4 | `IsAttachedTo(HingePivot)`가 아니면, 또는 월드 크기 한 축이라도 0이면 경고 1번(이름·이유) 후 제외. 남은 것이 0개면 메시 경계 |
| D5 | BeginPlay 로그 `obstruction basis: mesh bounds (approximate)` / `N obstruction boxes` |
| D6 | 닫힐 때만, 박스 `Mesh`만, 1° 분할·되돌림·소리·이벤트·횟수 그대로 |
| §3 기존 시험 | 기존 시험 파일 변경 0. 제품 변경은 리프트게이트 2파일 + 새 컴포넌트 + 시험 friend 한 줄 |
| 새 시험 | `vBoR.P5zg.E1`–`E9` 9개, 명세 표와 일치. E3 1.5°(개정 2), E4는 중간 SceneComponent를 거친 자손까지, E5는 음수 배율까지 확인 |
| 빌드·시험 재현 | `e01af6f`: unity 빌드 up to date(Succeeded). `vBoR.*` 632개 중 **631 통과**, `vBoR.P0Buck.E8Persistence` 1개 실패(`Repeated replacement 24: candidate replacement failed (OS error 5)`, `Saved/Logs/P5zgReviewAll.log:2281`) — 참고 1. `vBoR.P0Buck`만 다시 돌려 **14/14**(`P5zgReviewP0Buck.log`). Codex 재개 실행은 632/632 |

### 문제

없음.

### 참고 (고치지 않아도 됨)

1. **`P0Buck.E8Persistence` 세 번째 간헐 실패**(P5zb·P5ze 리뷰와 같은 OS error 5, Windows 파일 교체 거부). P5zg와 무관. 세 번째라 mid review 뒤 명세 목록 맨 앞으로 올린다(HANDOFF).
2. **판정 상자는 BeginPlay에 한 번만 모은다.** Play 중 BP에서 상자를 추가·이동하는 경우는 다루지 않는다(명세 §5 권장대로). 에디터에서 상자를 고치면 Play를 다시 시작해야 반영된다.
3. **사용법 문서가 아직 없다.** `BP_Liftgate`에 상자를 넣는 순서(`HingePivot` 밑, 닫힌 자세에서 판 모양을 따라 1~3개, 트렁크 안쪽 빈 공간은 비움)는 명세 §7에만 있다. Blueprint-Guide에 그립(P5ze)과 함께 넣는다(Claude, 사용자 요청 시).
- Quest 3(T-02, `not-tested`): 명세 §7 ①–③. 원인 추정(굽은 경계)은 회사 PC `obstructions=` 로그로 아직 확인 전이다.

## 질문

### Q1 — D3·D6 유지 시 E3의 작은/큰 Tick 각도 차가 1°를 넘음

- 상태: **반영함 (feat: 리프트게이트 판정 상자 지원)** (2026-10-28, Codex 구현 중 발견. 리뷰 회차 아님)
- **반영 확인(2026-10-28, Codex):** 개정 2 `main@bdca41b`를 작업 브랜치에 fast-forward 병합(rebase 없음). 제품 알고리즘·fixture 그대로, E3 단언만 1.5°로 반영. E8은 `IsVisibleInEditor()`로 확인. 재개 후 non-unity 성공 12.46 s, unity 성공 21.15 s, 컴파일 경고·오류 0. 전체 발견/완료/성공 **632/632/632**, 새 E1–E9 **9/9**, 실패 0, 로그·프로세스 exit 0. 각 빌드·전체 시험 1회. 로그 `Saved/Logs/P5zg-Resume-NonUnity.log`, `P5zg-Resume-Unity.log`, `P5zg-Resume-Full.log`. 열린 질문 없음. 아래 실패·미검증 기록은 Q1 중단 당시 이력이다. Quest 3 T-02 `not-tested`.
- **답(2026-10-28, Claude): 기준이 틀렸다. 알고리즘은 그대로, E3 기준만 ≤ 1.5°로 고친다.** 명세 개정 2(PR #95, `main@bdca41b`). 두 멈춤 각은 실제 닿는 각 θ에 대해 각각 [θ, θ + 검사 간격 + 반 간격 여유] 안에 있으므로 차 상한은 1° + 0.5° = 1.5°다. 기존 메시 경계도 같은 1.10°라는 확인이 근거로 충분했다 — 시험 자리를 바꾸지 않고 멈춘 판단이 맞다. 1°는 Claude가 근거 없이 정했다.
  - 재개: `main`을 이 브랜치에 병합(rebase 금지) → E3 단언을 1.5°로 → E8 수정 줄 포함 빌드·전체 시험 1회 → 커밋. 이 답을 적은 이 파일도 커밋에 같이 넣는다.
- **문제:** `Source/vBoR/Interaction/VBoRPoweredPanel.cpp:193`은 D3대로 상자 중심의 경첩 거리 + extent 길이를 반지름으로 하고, 현재 검사 구간의 절반 각도를 곱해 margin을 구한다. `:311` 부근 Tick은 D6대로 기존 `CeilToInt(Travel)` 분할을 그대로 사용한다. 새 E3 시험(`Source/vBoR/P1Probe/P5zgObstructionBoxTests.cpp:104`)에서 작은 Tick(1/60 s)의 분할 폭은 약 0.354°, 큰 Tick(4 s)은 1°여서 margin과 마지막 허용 자세가 달라진다. 끼임 시 각도는 **54.90° / 56.00°**, 차 약 **1.10°**로 E3의 **≤1°** 조건을 만족하지 못했다. 양쪽 모두 이벤트 1회, 박스 식별, 즉시 열림, 허용 자세의 비관통, 박스 불이동, 재열림 완료는 통과했다.
- **재현 형상:** 경첩 `(0,0,145)` cm, 판정 상자 경첩 상대 위치 `(-2.5,0,-35)` cm / extent `(2.5,65,35)` cm. 박스 Actor 상대 위치 `(-25,15,110)` cm / 크기 `(20,20,20)` cm. Gate Actor는 Yaw 37°, 위치 `(200,-100,90)` cm. 이는 기존 P4b E6 박스 위치를 재사용한 새 fixture이다. 큰 메시 경계는 `(1.2,1.3,0.7)` 배율, 판정에는 사용하지 않는다.
- **질문:** D3의 현재 구간별 margin 식과 D6의 기존 1° 이하 분할을 그대로 유지하면서 E3의 ≤1°를 요구하는지, 새 상자 경로만 분할/검출을 정밀하게 바꿀지, E3 허용 차를 개정할지 결정이 필요하다. 시험 자리·크기를 바꿔 우연히 통과시키거나 기준을 완화하지 않고 멈췄다. 기존 메시 경계 경로는 수정하지 않았다.
- **확인할 시험:** E3의 작은 Tick·4 s Tick 각도 차, 두 자세 비관통, 기존 P4b E6/E9 불변. 현재 전체 시험 **발견/완료 632/632, 성공 630, 실패 2**. 기존 623개 전부 통과. 새 E3와 E8 실패. 로그 `Saved/Logs/P5zg-Full.log:22737`, `:22751`, `:22762`. 기존 P4b E6도 메시 경계에서 같은 **54.90° / 56.00°**가 기록되지만 각도 차 assertion은 없다(`:8183`, `:8198`). 새 상자 변환만의 오류가 아니라 기존 margin/분할 방식의 특성임을 함께 확인했다.
- **E8 별도 수정:** `IsVisible()`은 `bHiddenInGame`이면 false를 반환하므로 에디터 가시성 확인 API로 잘못 사용했다(`Engine/Source/Runtime/Engine/Private/Components/SceneComponent.cpp:3528-3536`). `IsVisibleInEditor()`로 수정했다(`:3452-3456`, 선언 `Classes/Components/SceneComponent.h:1154`). 제품 수정은 없음. Q1 중단 때문에 이 새 시험 한 줄은 **빌드·재실행하지 않음**. 숨김·NoCollision·overlap off·태그 없음·들고 통과·Blocked 없음은 첫 실행에서 통과했다.
- **현재 상태:** 구현/새 시험 미커밋 상태. 첫 non-unity는 새 시험의 `AddExpectedMessagePlain` 인자 타입 오류로 실패했고, 헤더(`Core/Public/Misc/AutomationTest.h:1815`)에 맞게 고친 뒤 non-unity 성공(3.06 s), unity 성공(24.27 s), 컴파일 경고·오류 0. 전체 시험 1회만 실행. 종료 로그 -1 / 프로세스 255. 에디터/MCP/에셋 작업, 기존 시험 수정, push·PR·merge 없음. Quest 3 T-02 `not-tested`.
