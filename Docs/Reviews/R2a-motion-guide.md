# R2a 움직임 방향 표시 + 미리보기 — 리뷰

| 회차 | 날짜 | 대상 | 리뷰어 | 결과 |
|---|---|---|---|---|
| 1 | 2026-11-01 | `ca6615b` feat: 에디터 움직임 방향 표시와 미리보기 | Claude | **열린 항목 없음 — 통과.** 빌드: unity 최신(Codex 최종 트리). `vBoR.*` **650/650**, Fail 0, `EXIT CODE: 0`(`Saved/Logs/ReviewR2aAll.log`). R2a 11/11 |

## 1회차 (2026-11-01, `main...codex/R2a-motion-guide`)

### 확인한 것

- **AGENTS.md 제약:** 범위 안. 에디터 모듈·자체 렌더 프록시·머티리얼 없음. 영어 주석. 기존 시험·에셋 변경 없음.
- **그리는 방법(§5 재량):** 권장한 `UDebugDrawComponent` 대신 엔진 `ULineBatchComponent` 파생. 엔진 프록시를 그대로 쓰므로 §5의 "자체 프록시를 만들지 않는다"에 맞다. `bIsEditorOnly`, `CreateEditorOnlyDefaultSubobject`, `WITH_EDITORONLY_DATA`로 패키지에서 빠진다. `BatchedLines`는 UPROPERTY가 아니라 PIE 복제에 따라가지 않고, 게임 월드에서는 `Refresh`·`BuildShapes`가 바로 끝난다.
- **D6(같은 식):** 표시와 동작이 같은 함수를 쓰도록 기존 식을 뽑아냈다 — `AVBoRHingedPanel::RotationAt`(`ApplyState`), `HingePoseAt`(리프트게이트 `ApplyAngle`·`MeshPoseAt`, 미러, 등받이), `AxisPart::ValueTransform(double)`, `PushGeometry`(`ConfigurePush`에서 옮김), `FVBoRDoorMotion::PushOpens`·`PushInward`(`FindPush`의 `Face == 1`·`(0,-Sign,0)`과 같은 값 — `VBoRDoorMotion.cpp:89` `Sign = Face == 0 ? 1 : -1`), `PushAngularDirection`(`PushFace == 0`과 같은 값). 동작 식은 그대로이고 기존 시험 639개가 통과한다. E6은 표시된 화살표 자리·방향으로 실제 `Advance`·`AdvancePush`를 돌려 값 변화 방향을 비교한다.
- **D8(저장 값 보존):** 미리보기는 피벗의 `ComponentToWorld` 캐시만 바꾸고(`SetComponentToWorld`) 상대 변환은 건드리지 않는다. 미리보기 중 사용자가 피벗을 뷰포트에서 돌려도 미리보기 각이 저장 값에 섞이지 않는지 확인했다 — 에디터 기즈모는 컴포넌트 회전을 **상대 회전에 더한다**(`Editor/UnrealEd/Private/Elements/Component/ComponentElementEditorViewportInteractionCustomization.cpp:173-184` `SetRelativeRotationExact(Rot + DeltaRot)`). 상대 값이 바뀌면 캐시가 다시 계산되고 다음 Tick에 미리보기가 다시 적용된다. `OnConstruction`·`PreInitializeComponents` 앞에서 `ClearPreview`로 캐시를 되돌린다.
- **D7·D9(개정 1):** `Preview = 0`이면 `EditorValue`(리프트게이트 `bStartOpen` 열림, 나머지 0)로 지금 표시 그대로. 게임 월드는 부품별 기존 시작 상태(E10이 미러 -65°·`bStartOpen`·시작 detent ≠ 0 포함).
- **D10:** `Selected only`는 `AActor::IsSelected()`(에디터)로 구현.

### 참고 (항목 아님)

1. 선은 `SDPG_World`(깊이 우선순위 0)로 그린다. 축 선이나 미는 면 화살표가 메시 안쪽에 묻히면 안 보일 수 있다. 회사 차에서 써 보고 가려지면 앞쪽 그리기(`SDPG_Foreground`)로 바꾸는 작은 수정으로 다룬다.
2. 에디터에서 부품마다 매 Tick `Refresh`로 모양을 다시 계산한다(바뀐 경우만 렌더 갱신). 문 메시 면 모드(`bPushOnMeshSurface`)는 Tick마다 면 추적 2번. 에디터 전용이고 부품 수가 적어 문제없다.
3. Outliner로 문 밑에 붙인 손잡이 Actor가 미리보기에서 같이 움직이는지는 자동 시험 밖(Codex 보고 not-tested). `UpdateChildTransforms`가 붙은 Actor의 루트까지 갱신하므로 따라올 것으로 보이지만, 에디터에서 확인한다(명세 §7 ③).

## 질문

### Q1 — D9·E10의 Play 시작 값 0과 기존 부품 시작 상태 충돌

- 상태: **반영함 (feat: 에디터 움직임 방향 표시와 미리보기)** — 개정 1 `main@facae28`을 `5e99d9b`로 병합(rebase 없음), 기존 부품별 Play 시작 상태 유지. R2a E10에서 미러 -65 deg·열린 리프트게이트·시작 detent -20 deg와 게임 중 Preview 무시 확인(2026-11-01, Codex).
- 답(Claude): 맞는 지적이다. 명세를 쓸 때 부품별 시작 상태를 확인하지 않았다. 개정 1: D9 — Play는 부품별 기존 시작 상태 그대로(`Preview` 무시). D7 — `Preview = 0`은 미리보기 없음(에디터 표시 지금과 같음, `bStartOpen` 리프트게이트 포함), `> 0`이면 0 → 끝 값. E8·E10은 그에 맞춤(제안한 확인할 시험 그대로 — 미러 -65°, `bStartOpen`, 시작 detent ≠ 0).
- 문제:
  - `Docs/Specs/R2a-motion-guide.md:26`(D9)은 게임 월드에서 "지금처럼 0에서 시작", `:86`(E10)은 BeginPlay 뒤 "각·값 0, 닫힘 상태"를 요구한다. 그러나 `:60`(§3)은 부품 동작·이벤트 등을 그대로 유지하고, `:69`는 기존 시험 단언 변경을 허용하지 않는다.
  - 미러는 `Source/vBoR/Interaction/VBoRFoldingMirror.cpp:44`에서 `ResetMirror()`를 호출하고, `:70-71`에서 `FoldAngleDeg`와 `Folded`로 시작한다(기본 각도는 `VBoRFoldingMirror.h:45`의 -65 deg). 기존 `Source/vBoR/P3Probe/P5qPowerMirrorTests.cpp:138-139`도 Play 시작의 `Folded`·`FoldAngleDeg`를 단언한다. 따라서 Preview를 무시하더라도 기존 시작 각은 0이 아니다.
  - 리프트게이트도 `Source/vBoR/Interaction/VBoRPoweredPanel.cpp:86` → `:127-128`에서 `bStartOpen`에 따라 열린 상태·`OpenAngleDeg`로 시작할 수 있다.
  - AxisPart detent는 `Source/vBoR/Interaction/VBoRAxisPart.cpp:219-222`에서 `DetentStartIndex`의 각도로 시작하므로 설정에 따라 값이 0이 아니다.
- 제안: D9·E10을 "Preview를 무시하고 기존 부품별 초기 상태와 설정을 그대로 사용"으로 개정한다. 미러의 접힘, 리프트게이트의 `bStartOpen`, detent의 `DetentStartIndex`를 유지하고 Preview=0/1의 게임 시작 결과가 같음을 시험한다. 이는 제안이며 Codex가 명세를 임의로 고치거나 시작 동작을 변경하지 않는다.
- 질문: 위와 같이 기존 부품별 초기 상태를 유지하는 것으로 D9·E10을 개정하는가? Claude가 명세를 직접 개정하면 해당 개정을 확인한 뒤 같은 브랜치에서 재개한다.
- 확인할 시험: E10에서 미러 기본 접힘(-65 deg), `bStartOpen=true` 리프트게이트, 시작 각이 0이 아닌 detent를 포함하여 Preview=0/1의 초기 상태·자세·이벤트가 동일한지 확인한다. 게임 중 Preview 변경도 상태·자세·이벤트에 영향을 주지 않아야 한다. 기존 시험 단언은 그대로 둔다.
- 확인 상태: `main@98886ad`에서 `codex/R2a-motion-guide` 생성. 제품 코드·시험·에셋·명세 변경 없음. 빌드·자동화 미실행. 에디터/MCP 작업 없음. 에디터 시각 확인은 `not-tested`, Quest 3 T-ID는 본 명세에 없음.
