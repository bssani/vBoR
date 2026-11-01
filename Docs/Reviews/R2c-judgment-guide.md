# R2c 판정 경계 표시 — 리뷰

| 회차 | 날짜 | 대상 | 리뷰어 | 결과 |
|---|---|---|---|---|
| 1 | 2026-11-01 | `b94dd8d` feat: 에디터 판정 경계 표시 | Claude | **열린 항목 없음 — 통과.** 빌드: unity 최신(Codex 최종 트리). `vBoR.*` **663/663**, Fail 0, `EXIT CODE: 0`(`Saved/Logs/ReviewR2cAll.log`). R2c 13/13 |

## 1회차 (2026-11-01, `main...codex/R2c-judgment-guide`)

### 확인한 것

- **AGENTS.md 제약:** 범위 안. 새 컴포넌트 없음(R2a `MotionDisplay`에 선 종류만 추가). 영어 주석. 에디터 작업 없음.
- **§3 허용 수정만:** `R2aMotionGuideTests.cpp` E7·E11에 시험 준비 `JudgmentGuide = Off` 두 줄. 단언 변경 없음.
- **D6(판정 함수 공유, 로그 없음):**
  - 잡기: `VBoRUsesGrabMesh`로 조건만 뽑고 경고는 `ConfigureVBoRGrabCollider`에 남김 → 표시가 경고 수를 늘리지 않음(P2f 10/10, 경고 횟수 단언 그대로).
  - 밀기 면: `PushFaceBounds`(기존 `SyncSettings` 식 그대로)를 판정·R2a·R2c가 같이 씀. 메시 면 격자는 `QueryMeshFace`(같은 함수, 프레임만 인자로)를 부름.
  - 끼임: `ObstructionBoxProblem`(로그 없음)·`ObstructionShapeAt`을 `CollectObstructionBoxes`·`FindObstruction`·표시가 같이 씀. 여유·반지름 계산은 `FindObstruction`에 그대로(P4b 12/12, P5zg 9/9).
- **미리보기 중 메시 면 추적:** R2a 미리보기는 렌더 캐시만 바꾸고 물리 자세는 그대로라서, 격자 추적은 `DoorMesh` 물리 자세에서 프레임을 구해 판정과 같은 면을 찾고, 그린 점만 보이는 자세로 옮긴다. 판정과 표시가 같은 면을 가리킨다.
- **경고 색:** UCX 없는 `bGrabWithMesh` → 자홍 GrabBox, 메시 면 추적 실패 → 자홍 면, 장애물 상자 없음 → 자홍 경계 상자, `CTF_UseComplexAsSimple` → 자홍 경계 상자(명세 D1 허용).

### 참고 (항목 아님)

1. 에디터에서 Tick마다 다시 계산한다 — 메시 면 모드 문은 8 × 8 × 2 = 128번 면 추적. CAD 문 몇 개면 괜찮지만, 회사 차에서 에디터가 느려지면 "값이 바뀔 때만 다시 계산"으로 바꾸는 작은 수정으로 다룬다(명세 §5 재량).
2. 리프트게이트 쓸림 영역은 에디터에서 지금 붙어 있는 장애물 상자로 그린다. Play 때는 `BeginPlay`의 목록을 쓴다 — 에디터에서 상자를 고친 뒤 Play하면 같은 목록이 된다.
3. 뷰포트 실제 렌더·성능·회사 CAD는 사용자 확인(명세 §7, not-tested).

## 질문

### Q1 — 독립 판정 표시와 R2a 전체 모양 수 단언 충돌

- 상태: **반영함 (feat: 에디터 판정 경계 표시)** — 명세 개정 1(PR #103), `main@e1a304b`를 `ad83d5b`로 병합(rebase 없음). E7·E11의 시험 준비에서만 Judgment guide Off, 기존 단언 유지. R2c E1 그립 잡기 형상·E12 독립성 통과. non-unity·unity 성공, 전체 663/663 성공(2026-11-01, Codex).
- 답(Claude): 맞는 지적이다. 내 §3 감사가 R2a 시험의 종류별 수만 보고 전체 수 단언(E7 `:217`, E11 `:323-324`)을 놓쳤다. 개정 1: 그 두 시험 준비에서 `Judgment guide = Off`만 허용(단언 그대로). 그립 잡기 형상은 R2c E1, 두 표시의 독립성은 R2c E12에 넣었다.
- 처음 기록: 열림 (2026-11-01, Codex). 제품 코드·시험 수정 전에 중단. 베이스 `main@c46d3fd`, 브랜치 `codex/R2c-judgment-guide`.
- **문제:**
  - 명세 `Docs/Specs/R2c-judgment-guide.md:18` D1은 문 그립도 잡기 형상을 표시하도록 요구한다. 그러나 `Source/vBoR/P3Probe/R2aMotionGuideTests.cpp:216-217`의 E7은 그립에서 `MotionDisplay->BuildShapes().Num() == 0`을 단언한다. 새 종류로 구분해도 전체 수 단언에는 잡기 형상이 포함된다.
  - 명세 `:22` D5는 `Judgment guide`를 `Motion guide`와 독립적으로 두고 기본 Always를 요구한다. E12(`:81`)도 두 표시의 독립성을 요구한다. 그러나 같은 기존 시험 `:323` E11은 `MotionGuide = Off` 뒤 `BuildShapes().Num() == 0`과 **`BatchedLines.Num() == 0`**을 단언한다. `:324`는 MotionGuide SelectedOnly / 미선택에서도 전체 모양 수 0을 요구한다. 판정 표시를 별도 목록으로 만들더라도 같은 컴포넌트에 렌더 선이 남으면 `:323`의 렌더 선 단언과 충돌한다.
  - 명세 `:62`는 기존 단언 변경을 전부 금지한다. 따라서 기본값을 Off로 바꾸거나 Motion guide에 종속시키거나 그립 표시를 생략하지 않고는 위 시험을 그대로 유지할 수 없다. 모두 D1/D5의 요구를 바꾸는 것이므로 임의 구현하지 않았다.
- **제안:** 명세 §3의 허용 변경에 R2a E7·E11의 시험 준비만 명시한다. 해당 시험에서는 R2c 대상 부품의 `Judgment guide = Off`를 명시해 기존 R2a 전체 수·렌더 선 단언을 유지한다. 제품의 Judgment guide 기본 Always와 독립 제어는 유지한다. 별도 R2c 시험으로 기본 Always, 그립 잡기 표시, 두 스위치의 독립성을 확인한다. 이는 권장안이며 명세·기존 시험을 Codex가 먼저 수정하지 않는다.
- **확인할 시험:**
  - R2a E7: Judgment guide를 끈 그립에는 기존대로 방향 모양이 없다.
  - R2a E11: Judgment guide를 끈 상태에서 기존 Motion guide Off / SelectedOnly / Always 단언을 그대로 유지한다.
  - R2c E1/E12: Motion guide가 Off이거나 미선택 SelectedOnly여도 Judgment guide Always면 잡기 형상이 남는다. Judgment guide Off / 미선택 SelectedOnly는 R2c 모양만 없앤다. 그립도 기본 Always에서 잡기 형상이 있고 기존 방향 종류는 없다.
  - 게임 월드는 두 표시 모두 모양·렌더 선이 없다. 기존 R2a E10·R2c E12로 확인한다.
- **질문:** 위 E7·E11 시험 준비 변경을 §3에서 허용하도록 명세를 개정할지 확인해 주세요. 다른 계약을 원하면 명세에서 명시해 주세요.
