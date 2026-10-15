# P1b 리뷰 기록

명세: [Docs/Specs/P1b-box-load.md](../Specs/P1b-box-load.md) · 브랜치: `codex/P1b-box-load`

**Codex가 할 일:**
- 상태가 `열림`인 항목을 위에서부터 반영한다.
- 항목의 세 칸은 이렇게 다룬다:
  - **문제**는 반드시 해결한다.
  - **제안**은 권장이다. 다르게 하면 보고 ③에 이유를 적는다.
  - **확인할 시험**은 새로 넣거나 고칠 자동화 시험이다.
- 같은 브랜치에 새 커밋으로 한다. 반영한 항목의 상태를 `반영함 (<커밋 제목>)`으로 바꿔 같은 커밋에 넣는다. 커밋 본문에 보고 ①~⑤를 쓴다.
- AGENTS.md "일을 키우지 않는다"를 지킨다: 하위 에이전트 없이 혼자, 빌드·전체 시험은 끝에 한 번(최종 트리로).
- 판단이 필요하면 맨 아래 "질문"에 적고 멈춘다.
- push·`main` 머지는 하지 않는다. 끝나면 `HANDOFF.md`의 "Codex 결과"를 채운다.

## 회차

| 회차 | 날짜 | 리뷰 대상 | 리뷰어 | 결과 |
|---|---|---|---|---|
| 1 | 2026-10-15 | `58a6b2a` 박스 크기·보충·바닥 소멸·여러 박스 집계 | Claude | **열림 1건(R1-1).** 나머지는 명세대로다. 빌드·시험 재현: non-unity·unity 성공, vBoR 경고·오류 0(컴파일은 Codex 빌드로 최신. Codex 로그에서 바뀐 파일 컴파일·경고 0 확인), `vBoR.*` **159/159**, EXIT CODE 0. **Codex 토큰 5.16M(캐시 96%), 약 11분, 하위 에이전트 0, exec 36, 빌드·전체 시험 각 1번(최종 트리)** |
| 2 | 2026-10-15 | `c1b0d3f` 목표 높이를 박스 높이에 맞춤 | Claude | **열린 항목 없음 — 통과.** R1-1 반영 확인. 빌드·시험 재현: non-unity·unity 성공, vBoR 경고·오류 0(Codex 로그에서 바뀐 파일 컴파일·경고 0 확인), `vBoR.*` **159/159**, EXIT CODE 0. **Codex 토큰 2.80M(캐시 95%), 약 13분, 하위 에이전트 0, exec 25, 빌드·전체 시험 각 1번(최종 트리)** |

### 2회차에 확인한 것

- **R1-1:** `GoalPose()` 높이가 `트렁크 바닥 + BoxSizeCm.Z / 2 + GoalOffsetCm.Z`다. `GoalOffsetCm.Z`는 여유(기본 0)이고 주석으로 뜻을 밝혔다.
  - 기본 30 cm 박스의 목표는 그대로 바닥 + 15 cm다. 그래서 기존 `PlacementAndReset`(목표 (45, 0, 90))이 그대로 통과한다.
  - 크기 시험의 수동 보정 줄을 지웠다. 이제 높이 40 cm는 목표 아랫면 = 트렁크 바닥이고 `IsStaticFit` true다. 80 cm는 false다. 40 cm에서 보조를 켜면 시작하고 끝까지 안착한다.
  - 레벨 `L_P1_GolfBag`에 `GoalOffsetCm`을 바꿔 저장한 흔적이 없다고 Codex가 보고했다(바이트 검색, GUI 로드는 아님). 기본값만 쓰므로 뜻이 바뀌어도 영향이 없다.
- **범위:** 리뷰 항목 밖으로 바뀐 것이 없다.

### 1회차에 확인한 것

- **D1 잡기·이동:** `AVBoRCarryObject`의 손 추종·두 손·기울기 제한·막힘·추적 보류는 그대로다. 짐 부모 기본 메시가 이미 Cube라서, 시험대가 원통을 넣던 줄만 지웠다. 양옆 두 손의 Yaw는 손 선 회전량을 따른다(`HandsTrackingAndAssist`).
- **D2 크기:**
  - `BoxSizeCm`로 root scale을 정한다. 메시·simple collision·`LoadTarget::GoalSizeCm`이 같은 값을 쓴다(`SizeMeshCollisionAndFit`).
  - 목표 높이는 따라가지 않는다 → R1-1.
- **D3 보조 끔:** `bUsePlacementAssist`(기본 끔)는 `ReleaseCarry`의 `CanAssist` 앞에서만 막는다(`VBoRCarryObject.cpp:382`). 그래서 켜면 기존 경로 그대로다. 기존 보조 시험 둘은 옵션을 켜고 유지했다.
  - 시험대의 목표 안내(`Target->bShowGuide`)를 껐다. 명세에 없는 변경이지만, 보조가 꺼진 채 여러 개를 채울 때 한 자리를 보여 주면 "여기에 놓으라"로 읽힌다. 그래서 받아들인다.
- **D4 밀지 않음:** 물리 없음. 든 박스의 스윕이 놓인 박스에 막히고, 수직 낙하는 아래 박스 위에서 멈춘다(`DropStackAndHeldCollision`).
- **D5 보충:**
  - 받침 박스를 처음 들면 `bSourceLifted`를 켠다. 모든 박스의 충돌 bounds가 출고 자리(박스 크기 + 1 cm)에서 벗어나야 새 박스를 만든다.
  - 그래서 들고 있는 박스나 되돌려 둔 박스와 겹치지 않고, 한 박스로는 한 번만 보충된다. 추적 보류 중에는 보충하지 않는다(E10).
- **D6 바닥 소멸:**
  - 손에 없고(보류 포함) 낙하 중이거나 밖에 내려앉은 박스만 본다. 충돌 아랫면이 시험대 원점 Z(물리 바닥) + 1 cm 아래로 내려가면 그 박스를 없앤다.
  - 없애기 전에 낙하 결과를 기록해 측정이 남는다(`SupplyFloorAndReset`). 메시 피벗이 아니라 bounds를 쓴다(P4a R1-3과 같은 원칙).
- **D7:**
  - `B`는 모든 박스의 측정을 한 번씩 보존하고 지운 뒤 하나를 둔다. 반복해도 중복되지 않는다.
  - `R`은 재배치와 누적 초기화다. 입구 통과는 박스마다 따로 센다(`Crossings`).
- **범위·이름:** `Bag`·`BagStand`·`BagClass`·키 이름은 기존 참조 때문에 남겼다(명세 §5 허용). 사용자에게 보이는 문구는 박스로 바뀌었다. 원통에 기대던 기존 시험은 시험 안에서 원통 형상과 크기를 직접 넣어 유지했다(명세 §3).

## 항목

### R1-1 목표 높이가 박스 높이를 따르지 않음 — 높이를 바꾸면 "does not fit"·보조 불가 — 반영함 (`c1b0d3f`): align target height with box size)

**문제**
- 목표 자세의 높이는 `트렁크 바닥 + GoalOffsetCm.Z`이고, 기본값 15 cm는 30 cm 박스의 절반이다(`P1LoadBench.cpp:170-174`, `P1LoadBench.h:60`).
- `IsStaticFit`는 그 자세의 박스가 아무것과도 겹치지 않는지 본다(`VBoRLoadTarget.cpp:61-67`).
- 발생 조건: `BoxSizeCm.Z`를 30이 아닌 값으로 바꾼다(예: 40 cm). 그러면 목표 자세의 박스가 트렁크 바닥에 5 cm 묻힌다.
- 영향:
  - 판넬·로그의 "Static target pose"가 트렁크에 들어가는 박스에도 "overlap / does not fit"으로 나온다(`P1LoadBench.cpp:358`, `:218`, `:396`). 사용자가 크기를 바꿔 가며 볼 값이다(D2).
  - 안착 보조를 켜도 `CanAssist`가 `IsStaticFit`를 요구하므로(`VBoRCarryObject.cpp:382`) 보조가 아예 시작되지 않는다. 명세 D3·E12 "켜면 지금 동작 그대로"가 높이 30 cm에서만 맞는다.
- 시험 `SizeMeshCollisionAndFit`가 이 문제를 가린다: 크기를 바꿀 때 `Bench->GoalOffsetCm.Z = Size.Z * 0.5`를 손으로 넣는다(`P1BoxLoadTests.cpp:44`). 명세 D2 "크기를 바꾸면 … 적재 판정이 함께 바뀐다"를 시험이 대신 해 주고 있다.

**제안**
- `GoalPose()`의 높이를 `트렁크 바닥 + BoxSizeCm.Z / 2 + GoalOffsetCm.Z`로 계산한다. `GoalOffsetCm.Z`는 여유(기본 0)로 뜻을 바꾼다. X·Y는 그대로다.
- 툴팁이나 주석에 "Z는 박스 아랫면에서 바닥까지 여유(cm)"라고 적는다.
- `L_P1_GolfBag`의 시험대가 이 값을 바꿔 저장했을 수 있다. 확인은 레벨을 열지 않고 해도 된다. 기본값만 쓰고 있으면 영향이 없다. 바뀌어 있으면 보고 ③에 적는다.

**확인할 시험**
- `SizeMeshCollisionAndFit`에서 `GoalOffsetCm.Z` 수동 설정 줄을 지운다. 그다음 두 크기마다 확인한다:
  - 목표 자세 박스의 아랫면이 트렁크 바닥 높이다.
  - 트렁크(안쪽 높이 70 cm)에 들어가는 20 × 30 × 40 cm는 `IsStaticFit`이 true다.
  - 높이 80 cm는 false다.
- 높이 40 cm 박스에서 보조를 켜고 목표 근처에서 놓으면 보조가 시작된다(E12를 30 cm 밖에서 한 번).

**Codex 반영 (2026-10-15)**
- `GoalPose()` 높이를 `트렁크 바닥 + BoxSizeCm.Z / 2 + GoalOffsetCm.Z`로 변경. `GoalOffsetCm.Z` 기본값은 0 cm이며 박스 아랫면의 바닥 여유라는 주석을 추가함. X·Y 유지.
- `SizeMeshCollisionAndFit`의 높이 수동 보정을 제거함. 두 크기마다 목표 자세의 충돌 아랫면과 트렁크 바닥 높이 일치, 40 cm 높이의 정적 적합/80 cm 높이의 부적합을 확인하도록 보강함. 40 cm 박스는 목표 위 5 cm에서 opt-in 보조 시작과 안착 완료까지 확인함.
- `Content/P1/L_P1_GolfBag.umap` 바이트의 ASCII·UTF-16 검색에서 `GoalOffsetCm` 이름 없음. 저장된 해당 프로퍼티 override 흔적 없음. 레벨 열기·저장·에디터/MCP 작업 없음. 실제 GUI 로드는 `not-tested`.
- 새 ISDK·MetaXR API 없음. 엔진 `FTransform::TransformPosition`·`AddToTranslation`은 `Engine/Source/Runtime/Core/Public/Math/TransformVectorized.h:565`, `:899` 및 `TransformNonVectorized.h:535`, `:830`에서 확인함.
- 최종 코드 검증: non-unity(`-DisableUnity -Module=vBoR`) 17.33 s·unity 79.52 s 각 1회 성공, 프로세스 0, compiler/linker 경고·오류 0. `vBoR.*` 전체 시험 1회 159/159 Success, Fail 0, TEST COMPLETE EXIT CODE 0/프로세스 0. `vBoR.P1Box` 4/4 포함. 로그: `Saved/Logs/P1bR1NonUnityBuild.log`, `P1bR1UnityBuild.log`, `P1bR1Automation.log`. 로컬 도구 StructUtils 의존 폐기 안내와 headless 시작의 샘플 CDO/OpenXR/Slate 로그는 시험 assertion 실패와 구분함. Quest 3/T-02/T-09·Blueprint Details/GUI 맵 로드는 `not-tested`.

## 참고 (항목 아님 — VR·다음 명세에서 볼 것)

- **트렁크에 박스가 있으면 "Static target pose"가 "overlap"으로 바뀐다.** 목표 자리가 한 군데라 첫 박스가 그 근처에 놓이면 그 뒤로는 계속 overlap이다. 보조가 꺼져 있으면 진단 문구일 뿐이다. 판넬에서 헷갈리면 VR 뒤에 문구를 정한다.
- **바닥 여유 1 cm:** P1 레벨은 레벨 바닥이 물리 바닥보다 92 cm 아래라서 박스가 물리 바닥 높이까지 떨어져 사라진다. 다른 레벨(P4 통합)에서 바닥 충돌이 물리 바닥보다 1 cm 넘게 높으면, 박스가 그 위에 멈춘 채 사라지지 않는다. 그때는 `FloorToleranceCm`을 먼저 본다.
- 명세 §7 순서대로 본다: 받침에서 한 손·양옆 두 손 → 보충 → 트렁크 안 낙하 → 쌓기·밀지 않음 → 밖에서 놓아 사라짐 → `B`.

## 질문

(없음)
