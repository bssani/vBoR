# P2b 리뷰 기록

명세: [Docs/Specs/P2b-axis-part.md](../Specs/P2b-axis-part.md) · 브랜치: `codex/P2b-axis-part`

**Codex가 할 일:**
- 상태가 `열림`인 항목을 위에서부터 반영한다.
- 항목의 세 칸은 이렇게 다룬다:
  - **문제**는 반드시 해결한다.
  - **제안**은 권장이다. 다르게 하면 보고 ③에 이유를 적는다.
  - **확인할 시험**은 새로 넣거나 고칠 자동화 시험이다.
- 같은 브랜치에 새 커밋으로 한다. 반영한 항목의 상태를 `반영함 (<커밋 제목>)`으로 바꿔 같은 커밋에 넣는다. 커밋 본문에 보고 ①~⑤를 쓴다.
- AGENTS.md "일을 키우지 않는다"를 지킨다: 하위 에이전트 없이 혼자, 빌드·전체 시험은 끝에 한 번.
- 판단이 필요하면 맨 아래 "질문"에 적고 멈춘다.
- push·`main` 머지는 하지 않는다. 끝나면 `HANDOFF.md`의 "Codex 결과"를 채운다.

## 회차

| 회차 | 날짜 | 리뷰 대상 | 리뷰어 | 결과 |
|---|---|---|---|---|
| 1 | 2026-10-13 | `a4062eb` 1축 부품 부모·문 연결·문 메시 피벗 보정·P3 손잡이·콘솔 뚜껑 | Claude | **2건, 둘 다 문 메시 피벗 보정(D4) 쪽.** 빌드·시험 재현: non-unity(`.cpp` 42개)·unity 성공, vBoR 경고·오류 0, `vBoR.*` **107/107**(P2b 10). 1축 부품·문 넘기기·P3 연결은 명세대로이고, E1–E18 시험이 있다. P2 시험대의 `DoorSizeCm`이 더는 문 메시를 바꾸지 못한다(R1-1). 문 면 고르기 한 곳이 아직 대칭 두께를 가정한다(R1-2). **토큰 7.04M(캐시 6.80M), 22분, 하위 에이전트 0, exec 46** |
| 1+ | 2026-10-14 | 사용자 VR(`a4062eb`) | Claude | **R1-3(크래시, 가장 먼저)·R1-4 추가.** 콘솔 뚜껑을 놓는 순간 에디터 크래시: `UIsdkGrabberComponent::ReleaseByInputMethod` null 접근(`IsdkGrabberComponent.cpp:768`). 크래시 전: 07은 손잡이로 5.5 s 만에 완료(열림 0.928). 08(문 45°)은 32 s, 손잡이 잡기 8번 뒤 완료 — 문 쪽 잡기 기록이 없어 원인을 볼 수 없다(R1-4) |

### 1회차에 확인한 것

- **빌드·시험 재현.** 에디터를 닫고, vBoR 소스 수정 시각을 갱신한 뒤 빌드했다.
  - non-unity(`-DisableUnity -Module=vBoR`): `Result: Succeeded`, vBoR 경고·오류 0.
  - unity: 성공, 경고·오류 0.
  - 시험: `vBoR.*` **107/107 Success**, `TEST COMPLETE. EXIT CODE: 0`. 로그: `Saved/Logs/P2b1ClaudeAutomation.log`.
- **피벗 보정(D3):** `PartMesh`의 상대 변환을 `MeshRestFromRoot × RestPivot⁻¹`로 다시 계산한다(`VBoRAxisPart.cpp:151-152`). 축 컴포넌트를 옮겨도 메시는 제자리다.
  - ISDK 변환기에는 쉬는 축 좌표를 부모로 넘긴다(`AxisTarget`, `:36-44`). 범위가 쉬는 자세 기준이 된다.
  - 회전 clamp 부호를 SDK에 맞췄다(`:54-55`, `IsdkTransformerUtils.h:295-297`).
- **ISDK 설정:**
  - 손바닥만 쓰고 pinch는 끈다(`:171-172`).
  - 던지기를 끄고, 먼저 잡은 손만 받는다(`:26-27`).
  - 잡기 상자는 `WorldDynamic`에만 Overlap한다(`:135-138`).
  - 메시가 없으면 잡기를 끈다(`:160-164`, `:171`).
- **복귀·머묾:** 복귀는 시간 기준이다(`:294-299`). 머묾은 놓은 자리에 선다. 둘 다 시험이 있다(E6·E7).
- **추적 보류·rebase:** 잡은 손이 High가 아니면 멈춘다. 복구 frame은 rebase만 하고 버린 이동량을 기록한다(`:98-114`, `:71-89`, `:268-277`).
  - SDK가 추적 손실로 선택을 끝내도 손바닥을 펼 때까지 보유한다(`:280-292`). 시험 E8이 있다.
- **문 넘기기(D5):**
  - 열림 문턱이나 풀린 문이면 `BeginGrabFromAxisPart` → `FVBoRDoorMotion::BeginGrab`으로 같은 손이 문을 잡는다(`VBoRAxisPart.cpp:302-305`, `VBoRHingedDoor.cpp` `BeginAxisGrab`).
  - 문을 놓으면 손잡이가 복귀한다. 걸쇠가 걸리면 잡기를 취소하고, 손바닥을 펼 때까지 다시 못 잡는다.
  - 연결된 쪽 문 손잡이 상자는 충돌·잡기·이벤트를 모두 끈다.
  - E10–E13 시험이 있다.
- **SDK 재진입 방지:** `ForceCancel`을 Move delegate 안이 아니라 Tick에서 부른다(`:301-305`). 맞는 판단이다.
- **P3 근사 차:**
  - 바깥 손잡이: 액터는 차 원점에 있고 축·메시만 손잡이 자리다. 문 `HingePivot`에 붙어 문과 함께 돈다. 문에 연결돼 있다.
  - 콘솔 뚜껑: 머묾, 100°, 뒤쪽 경첩.
  - E17은 07 → 08을 같은 손으로 끝낸다.
- **범위:** 명세 밖으로 커진 것은 없다. P3 단계 데이터·규칙, 레벨·에셋은 그대로다.

## 열린 항목

### R1-3 ISDK "놓음" 이벤트 안에서 `ForceCancel`을 불러 에디터가 크래시한다 (사용자 VR, 가장 먼저) — 열림

**문제**
- **위치:** `Source/vBoR/Interaction/VBoRAxisPart.cpp`
  - `:227-229`: `HandlePointer`의 Unselect 처리에서 `ReleasePart()`를 부르고, 그 안 `:239`에서 `GrabComponent->ForceCancel()`을 부른다.
  - `:222`: Select 처리에서 손이 High가 아니면 바로 `ForceCancel()`을 부른다. 같은 종류다.
- **발생 조건:** 손바닥 잡기 인식기가 놓음을 알릴 때. 그리고 부품의 `HandlePointer`가 grab transformer보다 먼저 그 이벤트를 받을 때. 두 핸들러는 같은 이벤트 delegate에 묶여 있어서 실행 순서가 정해져 있지 않다.
- **경로**(크래시 덤프 `Saved/Crashes/UECC-Windows-448D8635…`, 2026-10-13 15:02 UTC, 콘솔 뚜껑 `VBoRAxisPart_1` 놓은 직후):
  1. `UIsdkHandPalmGrabRecognizer::UpdateState`(`IsdkHandPalmGrabRecognizer.cpp:118`)가 놓음을 알린다. → grabber `ReleaseByInputMethod`가 `PostEvent(Unselect)`를 보낸다(`IsdkGrabberComponent.cpp:765`).
  2. 우리 `HandlePointer`(Unselect)가 `ReleasePart`를 부르고, 그 안에서 `ForceCancel`이 불린다. → `CancelGrabEvent.Broadcast`(`IsdkGrabTransformerComponent.cpp:150-153`)
  3. grabber `HandleGrabbableCancelEvent`(`IsdkGrabberComponent.cpp:891-899`)가 안쪽 `ReleaseByInputMethod`를 부른다. → `ResetGrabState`로 `GrabbedComponent = nullptr`
  4. 바깥 `ReleaseByInputMethod`로 돌아와 `:768` `GrabbedComponent->GetGrabTransformer()`가 null을 읽는다. → `EXCEPTION_ACCESS_VIOLATION`
- **영향:** VR 시험 중 에디터가 꺼진다. 손잡이에서는 이번에 안 났지만, 같은 경로라 순서에 따라 날 수 있다.
- **원인:** 구현이 Move delegate 안에서는 `ForceCancel`을 부르지 않게 했다(`:301-305` 주석). 그런데 포인터 이벤트 처리(`HandlePointer`)에서는 같은 원칙을 지키지 않았다. 시험은 SDK가 먼저 놓는 경로(인식기 → grabber → Unselect → 우리 처리 → `ForceCancel`)를 우리 핸들러가 먼저 받는 순서로 돌리지 않았다.

**제안**
- **ISDK 포인터 이벤트 처리 안에서는 SDK 상태를 바꾸는 호출(`ForceCancel` 등)을 하지 않는다.** 표시만 남기고 Tick에서 한다(`:301-305`와 같은 원칙).
  - Unselect·Cancel은 SDK가 이미 놓는 중이다. 우리 쪽 상태만 정리하고 `ForceCancel`은 부르지 않는다.
  - Select에서 추적이 낮아 거절할 때는 "취소 예약"만 하고, 다음 Tick에서 `ForceCancel`을 부른다.
- `ReleasePart`를 Tick에서 부를 때(손바닥 펴기 감지)와 이벤트에서 부를 때를 나눈다. 예: 인자 하나, 또는 예약 플래그.
- 문 쪽 `CancelSelections`도 같은 원칙을 따르는지 확인한다. `EndDoorHold` → `ReleasePart` → `ForceCancel`이 SDK 이벤트 안에서 불리는 경로가 없어야 한다.

**확인할 시험**
- 실제 grabber가 부품을 잡은 상태에서, **우리 `HandlePointer`가 transformer보다 먼저 이벤트를 받게** 묶는다. 그리고 SDK 쪽에서 놓는다(손바닥 놓음 → `ReleaseByInputMethod`). 크래시가 없고, 부품이 놓이고, grabber가 잡지 않은 상태다.
- Select 거절(추적 Low) 경로도 같은 순서로 돌려 크래시가 없다. 다음 Tick에 SDK 선택이 취소된다.
- `HandlePointer`가 도는 동안 `ForceCancel`이 0번 불린다(호출 수를 세어 확인).

### R1-4 문을 손잡이로 넘겨 잡은 뒤의 잡기 시작·끝을 기록하지 않아, 08이 왜 오래 걸렸는지 볼 수 없다 (사용자 VR) — 열림

**문제**
- **위치:** `Source/vBoR/Interaction/VBoRHingedDoor.cpp` `BeginAxisGrab`, Tick의 넘김 끝(`HoldingAxisPart->EndDoorHold(false)`), `CancelSelections`
  - 문이 잡히고 놓이는 것을 로그에 남기지 않는다. 남는 것은 부품의 `released` 줄뿐이다.
- **발생 조건:** 늘 그렇다.
- **영향:**
  - 사용자 VR에서 08이 손잡이 잡기 8번, 32 s 뒤에 끝났다.
  - 첫 넘김은 약 1 s 만에 끝났다(열림 15:01:32.98, 손잡이 놓임 15:01:34.06).
  - 그 뒤 열림 0.88·0.84·1.00이 다시 나왔다. 문이 다시 잠겼다는 뜻이다(걸쇠).
  - 손바닥이 펴져서 놓였는지, 추적이 끊겼는지, 걸쇠가 걸렸는지 알 수 없다.
- **원인:** 명세 측정 항목에 문 넘기기 기록이 없었다(Claude).

**제안**
- 문이 손잡이로부터 잡기를 시작할 때와 끝날 때 한 줄씩 남긴다.
  - 시작: 어느 손잡이(바깥·안쪽)와 손, 잠김 여부, 각도
  - 끝: 이유(손바닥 펴짐·추적 손실 보류·걸쇠·기타), 각도, 잡은 시간
- 로그 분류는 지금 문 측정과 같은 곳이나 `LogAxisPart`. 구현자가 정한다.

**확인할 시험**
- 넘김 시작 → 손바닥 펴기 → 끝 이유 "손바닥 펴짐"과 각도가 기록된다.
- 넘김 중 문을 닫아 걸쇠 → 끝 이유 "걸쇠"가 기록된다.

### R1-1 P2 시험대의 `DoorSizeCm`이 더는 문 메시와 문 면을 바꾸지 못한다 — 문 크기 값이 두 군데로 갈렸다 — 열림

**문제**
- **위치:**
  - `Source/vBoR/P2Probe/P2DoorBench.cpp:76-85`: 시험대는 문을 deferred로 만든다. 그리고 `FinishSpawning` **전에** `DoorMesh->SetRelativeLocation/Scale3D`로 메시를 `DoorSizeCm`에 맞춘다.
  - `Source/vBoR/Interaction/VBoRHingedDoor.cpp` 새 `OnConstruction`: `FinishSpawning` 안에서 불리고, 문 메시를 `MeshRestFromRoot`로 다시 놓는다. `MeshRestFromRoot`의 기본값은 110 × 10 × 105 cm에 고정돼 있다(`VBoRHingedDoor.h`).
  - `VBoRHingedDoor.cpp:92`: `Motion.WidthCm/HeightCm`은 `DoorSizeCm`에서 오지만, 이제 아무 데서도 읽지 않는다. `ThicknessCm`은 바로 아래에서 bounds로 덮어쓴다.
- **발생 조건:** 시험대(또는 문 자식)의 `DoorSizeCm`을 기본값과 다르게 바꿀 때. 지금 레벨에는 바꾼 값이 없어 보이는 변화는 없다. `L_P2_Door`·`L_P3_Flow`에 `DoorSizeCm`이 저장돼 있지 않다.
- **영향:**
  - 시험대에서 문 크기를 바꾸면 손잡이 상자(`:83-84`)와 시험대 틀은 새 크기를 따른다. 문 메시와 밀기 문 면은 110 × 10 × 105 그대로라, 손잡이가 문 밖에 뜨고 민 자리와 보이는 문이 어긋난다.
  - 문 Details에 남은 `DoorSizeCm`은 아무 일도 하지 않는다. 사용자가 이 값을 고쳐도 반응이 없다. "메시·값만 바꾸면 적용"(AGENTS.md)과 어긋나고, 명세 §3 "P2 동작 그대로"를 어긴다.
- **원인:** 명세 D4가 "문 면은 메시 bounds에서, 기본 자리 메시는 지금 값과 같게"만 말했다. 기존에 문 크기를 정하던 곳(`DoorSizeCm`, 시험대)을 어떻게 할지 적지 않았다(Claude).

**제안**
- 문 크기를 정하는 곳을 하나로 한다. **문 메시 자리(`MeshRestFromRoot`)와 메시 bounds가 기준**이다.
- 시험대는 `FinishSpawning` 전에 `Door->MeshRestFromRoot`를 정한다. 값은 위치 (X/2, 0, Z/2), 스케일 `DoorSizeCm / MeshSize`다. 메시 상대 변환을 직접 바꾸지 않는다.
- 문의 `DoorSizeCm`은 지우거나, "기본 자리 메시 크기" 하나로만 쓴다(예: 생성자에서 `MeshRestFromRoot` 기본값을 만들 때만). 구현자가 고른다. 다만 값이 둘이 되지 않게 한다.
- 쓰지 않는 `Motion.WidthCm/HeightCm`은 정리한다. 남기면 bounds와 같게 채운다.

**확인할 시험**
- 시험대 `DoorSizeCm` (90, 8, 100)으로 문을 만든다. 문 메시 bounds, 밀기 문 면(`FaceMinCm/FaceMaxCm`), 손잡이 상자가 모두 90 × 8 × 100 문에 맞는다.
- 기본값 (110, 10, 105)에서는 지금과 같다(기존 P2 시험 유지).

### R1-2 문 면 고르기가 아직 "Y = 0 둘레 대칭 두께"를 가정한다 — 비대칭 문 메시에서 바깥·안쪽 면을 잘못 고른다 — 열림

**문제**
- **위치:** `Source/vBoR/Interaction/VBoRDoorMotion.cpp:199`: `if (FMath::Abs(Prev.Y) >= ThicknessCm * 0.5) { Face = Prev.Y >= 0 ? 0 : 1; }`
  - 밀기 판정(`FindPush`)은 이번에 `FaceMinCm.Y`·`FaceMaxCm.Y` 평면으로 바꿨다(`:70`, `:83`). 그런데 어느 면으로 다가오는지 고르는 이 줄은 옛 대칭 가정 그대로다.
- **발생 조건:** 문 메시 bounds가 `HingePivot` 기준 Y = 0에 대칭이 아닐 때. 차 원점 피벗 실제 문 메시가 그렇다. 경첩 축이 문 바깥 면 근처에 있고 안쪽 트림이 두꺼우면, bounds Y가 −20 ~ +5 cm 같은 식이 된다.
- **영향:**
  - 바깥 면 앞(Y = +8)에서 다가오는 손을 "면 안"으로 보고(|8| < 12.5) 손바닥 방향으로 대신 고른다.
  - 문 안 깊은 쪽(Y = −15)에 있는 손은 안쪽 면으로 고른다.
  - 그 결과 밀기가 잡히지 않거나 반대 면으로 판정된다.
  - 기본 자리 메시(±5 cm)는 대칭이라 지금 시험에는 드러나지 않는다.
- **원인:** 명세 D4가 "밀기 판정용 문 면은 메시 bounds에서"라고 했지만, 면을 쓰는 곳을 다 짚지 않았다(Claude). 구현이 `FindPush`만 바꿨다.

**제안**
- 이 줄도 같은 평면을 쓴다:
  - `Prev.Y >= FaceMaxCm.Y`이면 바깥 면(0)
  - `Prev.Y <= FaceMinCm.Y`이면 안쪽 면(1)
  - 그 사이면 지금처럼 정하지 않는다
- `ThicknessCm`을 다른 곳(`:89` 깊이 한도)에서 쓰는 것은 bounds 두께로 그대로 둬도 된다.
- `FaceMinCm`·`FaceMaxCm`·`ThicknessCm`을 쓰는 곳을 한 번 훑어서 남은 대칭 가정이 없는지 본다.

**확인할 시험**
- 비대칭 문 면(`FaceMinCm.Y` = −20, `FaceMaxCm.Y` = +5)에서:
  - 바깥 면 앞 Y = +8에서 다가와 미는 손 → 바깥 면, 닫히는 쪽으로 민다.
  - 안쪽 면 앞 Y = −23에서 다가와 미는 손 → 안쪽 면, 열리는 쪽으로 민다.
  - 면 사이(Y = −8)에서 나타난 손 → 밀지 않는다(P2 D2 조건 3).
- 기본 대칭 문은 지금과 같다(기존 P2 시험).

## 참고 (항목 아님)

- **쉴 때 overlay:** 부품이 쉬고 있을 때 overlay 색을 투명(`FLinearColor::Transparent`)으로 두고 재질은 계속 씌운다(`VBoRAxisPart.cpp:316-319`). 버튼 부모는 쉬면 overlay를 아예 뗀다(`VBoRPokeButton.cpp:109`). VR에서 평소 부품이 원래 재질 그대로 보이면 그대로 둬도 된다. 어둡게 덮이면 버튼처럼 뗀다.
- **Blueprint 자식에서 메시 자리:** `PartMesh`·`DoorMesh`는 뷰포트에서 옮겨도 생성 때마다 `MeshRestFromRoot`로 되돌아간다. 메시 자리는 이 값으로, 회전 중심은 `AxisPivot`(문은 `HingePivot`)을 옮겨서 정한다. 사용자 안내에 적는다.
- **실제 문 메시의 밀기 면은 bounds 상자다.** 휜 문은 보이는 표면보다 몇 cm 앞에서 밀기가 잡힐 수 있다. 실제 데이터로 VR에서 볼 항목이다(명세 범위 안의 근사).

## 질문

(없음)
