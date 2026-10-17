# P1c 박스 놓기 개정 — 리뷰

명세: [P1c-box-settle.md](../Specs/P1c-box-settle.md) · 브랜치: `codex/P1c-box-settle`

## 리뷰 1회차 (2026-10-18, Claude — `7e92473`)

**결과: 열림 1건(R1-1).** D1–D3의 동작은 명세대로다. 남은 것은 박스를 들고 있는 동안의 frame 비용 하나다.
- 기존 시험 파일과 단언은 바뀌지 않았다. `bLevelOnRelease`를 끈 기존 시험도 없다(보고 ③). `vBoR.P1Carry.DropToSurface`, `ContactDropOnSlope`는 그대로 통과한다.
- 들고 있을 때의 접촉·미끄러짐·회전 밀어냄, 안착 보조, 공급·`B`·`R`·바닥 사라짐 코드는 바뀌지 않았다(D4, §3).
- Codex 보고는 커밋 본문에 있다. 첫 구현이라 리뷰 파일이 없어서 Claude가 이 파일을 만들었다.

**재현(에디터가 닫힌 것을 확인한 뒤, 바뀐 C++ 5개의 시각을 갱신):**
- non-unity(`-DisableUnity -Module=vBoR`): `Result: Succeeded`, 경고·오류 0. 로그 `Saved/Logs/ReviewP1cNonUnity.log`.
- unity: `Result: Succeeded`, 경고·오류 0. 로그 `ReviewP1cUnity.log`.
- `vBoR.*`: 367/367(P1c 8개 포함), `TEST COMPLETE. EXIT CODE: 0`. 로그 `ReviewP1cAll.log`. 끝난 뒤 `UnrealEditor` 프로세스 없음.

**Codex 토큰:** 4.39M(입력 4.37M 중 캐시 4.19M, 약 96%), 2026-10-17 15:10–15:22 UTC로 약 11분. 하위 에이전트는 0이다. 실패를 고치려고 다시 돌린 것은 둘이다. 하나는 non-unity 실패(`AddExpectedMessagePlain` 인자, C2665)다. 다른 하나는 전체 시험 366/367이었다. 일반 Log는 expected message로 세지지 않아서, 직접 세는 방식으로 바꿨다. 규칙 안이다.

**확인한 것:**
- **D1 세우기**(`Interaction/VBoRCarryLanding.cpp:13-18`): (현재 회전 × 쉬는 회전⁻¹)의 Yaw만 남기고 쉬는 회전을 곱한다. 쉬는 회전이 없으면 Identity를 쓴다.
  - 쉬는 회전은 `BeginPlay`와 `ResetToPose`에서만 정한다(`VBoRCarryObject.cpp:133`, `:494`). 그래서 기울어진 채 놓였던 박스를 다시 잡아도 기준이 바뀌지 않는다.
  - `LevelAndYaw`가 확인하는 것: 쉬는 회전이 Roll 90°인 경우, 쉬는 회전이 없는 경우, 낙하 중 모든 Tick의 `IsPoseClear`.
- **D2 올림**(`:19-46`): +Z로만 찾는다. 처음 빈 높이와 그 직전 막힌 높이 사이를 0.001 cm까지 좁힌다. 못 찾으면 기울인 자세를 그대로 두고, 놓기마다 로그를 1번 남긴다(`VBoRCarryObject.cpp:398-408`).
  - 세운 자세는 `IsPoseClear`로 다시 확인한 뒤 바로 적용한다. 중간 회전은 없다(§5 재량).
- **D3 미리 보기**(`:72-93`): 놓기와 같은 `CalculateReleaseDrop`을 쓴다.
  - sweep은 실제 `Mesh`이고, 시작 관통을 다루는 규칙과 0.005 cm 여유가 `TryMoveToPose`(`VBoRCarryObject.cpp:226-229`)와 같다.
  - 색은 `IsObjectInside`와 같은 영역 좌표·크기로 `ContainsPose`를 부른 결과다(`VBoRLoadTarget.cpp:69-75`와 같은 꼴). 호출하려고 `friend` 한 줄을 더했고, 판정 코드는 그대로다.
  - 숨기는 경우: 손에 없음, 추적 보류, 보조 켬, 끔, 첫 닿는 곳 없음, 바닥 사라짐 높이 이하.
  - 윤곽은 root `Mesh`의 자식이고 절대 변환이라 박스 크기(root scale)와 같다. 충돌·overlap·그림자가 없고, ISDK 잡기 collider는 실제 `Mesh` 그대로다(`PreviewIsolation`).
- **E7 일치:** 미리 보기 자세와 실제로 놓고 멈춘 자세를 네 시험이 0.01 cm·0.01° 안으로 비교한다. 바닥, 쌓기, 올림, 막힘 대체의 네 경우다.

### R1-1 — 들고 있는 동안 미리 보기가 frame마다 겹침 검사를 최대 150번 넘게 돈다 · 반영함 (`78f3ac1`)

- **문제:**
  - **위치:** 미리 보기는 박스를 든 동안 매 Tick 놓기 계산 전체를 다시 한다(`VBoRCarryObject.cpp:559` → `VBoRCarryLanding.cpp:72-93`). 그 안의 올림 탐색은 0.1 cm 간격 선형이다(`VBoRCarryLanding.cpp:26`).
  - **발생 조건:** 세운 자세가 겹치는 동안이다. 트렁크 턱·옆벽·다른 박스 가까이에서 기울여 들고 있을 때가 그렇다. 15 cm 안에 빈자리가 없으면(E5 꼴) 탐색이 끝까지 돈다.
  - **얼마나 도는가:** 그 frame에 `IsPoseClear`가 154번이다. 탐색 150번에 시작·세움·착지 확인, 그리고 색 판정이 2번 더 부른다(`:69`, `:84`). `IsPoseClear` 1번은 overlap 1번과 겹친 부품마다 sweep 1번이라, 장면 질의로는 300번이 넘는다.
  - **영향:** 자동화 로그 시각으로 어림잡았다. 막힌 탐색 2번을 하는 `LevelBlocked`가 25 ms, 5 cm 탐색 2번을 하는 `LevelLift`가 19 ms다. 그래서 상자 셋뿐인 시험 월드에서도 `IsPoseClear` 1번이 약 30 µs다. 막힌 동안 frame당 4–5 ms다(어림, 실제 차 충돌 형상에서는 더 늘 수 있다). 90 Hz 한 frame이 11 ms이니, 참가자가 트렁크 입구에서 박스를 맞추는 바로 그때 frame이 떨어질 수 있다.
  - **두 번째 원인(작음):** 미리 보기를 매 갱신마다 먼저 숨기고(`:74`) 끝에서 다시 보인다(`:92`). 엔진은 보임 값이 바뀔 때마다 렌더 상태를 다시 만들게 표시한다(`Runtime/Engine/Private/Components/SceneComponent.cpp:3561-3568` → `:3547-3549` `MarkRenderStateDirty`, Claude 확인). 그래서 보이는 동안 frame마다 렌더 상태를 다시 만든다.
  - 원인 일부는 내 명세다. D3이 "계산은 놓기와 같은 함수"라고만 했고, 그 함수가 frame마다 돈다는 점과 비용을 적지 않았다.
- **제안:**
  - 탐색 간격을 넓힌다. 예: 1 cm 간격(최대 15번)으로 처음 빈 높이를 찾고, 지금의 이분 축소(0.001 cm)를 그대로 쓴다. 1 cm보다 얇은 빈 틈은 놓친다. 0.1 cm 간격도 0.1 cm보다 얇은 틈은 놓치니 같은 종류의 절충이다. 다른 방법도 좋다. 예: 한도 높이 자세가 비어 있으면 거기서 아래로 sweep 한 번. 그러면 보고 ③에 이유를 적는다.
  - 색 판정의 `IsPoseClear`(`:84`)는 `CalculateReleaseDrop`이 참을 돌려줄 때 이미 확인한 것(`:69`)이라 뺄 수 있다.
  - 보임은 결과를 먼저 정하고, 값이 바뀔 때만 `SetVisibility`를 부른다.
  - D2 결과는 그대로다: 가장 낮은 빈 높이(0.001 cm 안), 실패 때 기울인 채, 어느 Tick에도 겹치지 않음.
- **확인할 시험:**
  - E5 배치(`LevelBlocked`)에서 미리 보기 한 번 갱신하는 동안의 `IsPoseClear` 호출 수에 상한을 둔다. 예: ≤ 30. 실제 상한은 고른 방법에 맞게 정하고 보고에 적는다.
    - 계수기는 자동화 빌드에만 둔다. `UVBoRAxisGrabComponent::CancelCalls`(`VBoRAxisPart.cpp:32-34`, `#if WITH_DEV_AUTOMATION_TESTS`)와 같은 꼴이다.
  - E4 배치(`LevelLift`)에서 같은 상한과 결과(Z = 85 cm ±0.01)를 본다.
  - 보이는 상태에서 연속 두 번 갱신하면, 두 번째에는 렌더 상태가 다시 더러워지지 않는다. `UActorComponent::IsRenderStateDirty()`로 본다.
  - 기존 P1c 8개는 그대로 통과한다.

### Codex R1-1 반영 (2026-10-18)

- `VBoRCarryLanding.cpp`: +Z 탐색 간격을 제안대로 1 cm로 넓히고, 첫 빈 높이와 직전 막힌 높이의 이분 축소는 0.001 cm로 유지했다. 놓기와 미리 보기는 같은 함수를 쓴다. 겹침 없는 자세만 적용하며 실패하면 기울인 채 떨어뜨리는 동작은 그대로다.
- 색 판정에서 중복 `IsPoseClear`를 없앴다. 보임 결과를 먼저 계산하고 값이 달라질 때만 `SetVisibility`를 부른다.
- `VBoRCarryObject.h/.cpp`: `WITH_DEV_AUTOMATION_TESTS` 안에만 `PoseClearCalls` 계수기를 두었다.
- `P1cCarrySettleTests.cpp`: 기존 E4·E5 단언을 유지하고 갱신당 호출 상한 28회를 추가했다. 기본 한도 15 cm에서 시작·세움 2회 + 탐색 최대 15회 + 이분 축소 최대 10회 + 착지 1회다. 실제 E4·E5는 각각 18회였다. E4의 Z = 85 cm ±0.01 및 E7 미리 보기 일치 단언도 통과했다.
- 새 `PreviewRenderState` 시험은 render state 생성·보임 변경 시 dirty 발생을 먼저 확인하고, end-of-frame 처리 후 연속 갱신에서 `IsRenderStateDirty()`가 거짓인지 확인했다. headless에서도 빈 단언이 되지 않게 했다.
- 제안과 다른 구현 없음. 1 cm보다 얇은 빈 틈은 탐색에서 놓칠 수 있다는 제안의 절충을 그대로 적용했다. 빈 구간을 찾은 뒤 경계 정밀도는 0.001 cm다. 상한 28회는 기본 15 cm 한도의 시험 배치 기준이며, 임의로 늘린 `LevelLiftMaxCm` 전체에 대한 고정 상한은 아니다.
- 최종 non-unity(`-DisableUnity -Module=vBoR`)·unity 각각 1회 성공, warning/error 0, exit 0. 로그: `Saved/Logs/CodexP1cR1NonUnity.log`, `CodexP1cR1Unity.log`.
- 최종 `vBoR.*` 1회: 발견 368, Success 368, Fail 0, P1c 9/9(기존 8개 + 신규 1개), `TEST COMPLETE. EXIT CODE: 0`, 프로세스 exit 0. 로그: `Saved/Logs/CodexP1cR1All.log`. E8Persistence 재실행 사유 없음. 종료 후 `UnrealEditor*` 프로세스 없음.
- 자동화의 샘플 InfoCardWidget 누락, 임시 월드 `World has no context`, 잘못된 설정을 다루는 시험의 경고는 빌드 진단·실패와 구분했다. 자동화 Error 0, 단언 실패 0이다.
- API 헤더 직접 확인: UE 5.7 `Engine/Source/Runtime/Engine/Classes/Components/SceneComponent.h:912` (`SetVisibility`), `Components/ActorComponent.h:1051` (`IsRenderStateDirty`), `:1152` (`RecreateRenderState_Concurrent`), `:1158` (`IsRenderStateCreated`), `Engine/World.h:3358` (`SendAllEndOfFrameUpdates`). 새 플러그인 API 없음.
- Quest 3·VR Preview·Blueprint·GUI 및 T-02·T-08·T-09는 `not-tested`. 실장면의 frame 시간 개선량은 측정하지 않았다. 질문 없음, Claude 리뷰 대기.

### 참고 — VR 확인 때 볼 것 (명세 §7에 더함)

1. **올림은 놓는 순간 바로 일어난다.** 턱 위에서 놓으면 박스가 세워지면서 최대 15 cm 위로 튄 뒤 떨어진다(§5에서 고른 "즉시"). 거슬리면 ≤ 0.15 s 돌리기로 바꿀 수 있다.
2. **`LevelLiftMaxCm`은 15 cm 고정이다**(`VBoRCarryObject.h:97`). 명세 자리값이 "박스 높이의 절반"이었는데, 기본 박스 높이 30 cm의 절반을 상수로 넣었다. 그래서 `BoxSizeCm`을 바꿔도 따라가지 않는다. 박스 크기를 바꿔 쓸 때 같이 맞추면 된다. 지금은 고치지 않는다.
3. **윤곽 재질은 ISDK 덮개 재질(`GrabbableLit`)을 본 재질로 쓴다.** 트렁크 안에서 깊이가 읽히는지, 박스와 겹쳐 보일 때 헷갈리지 않는지 본다.

## 리뷰 2회차 (2026-10-18, Claude — `78f3ac1`)

**결과: 통과. 열린 항목 없음.** R1-1이 제안대로 들어갔다. 1회차 뒤 바뀐 것은 R1-1 범위뿐이다(`git diff 2644dd5 78f3ac1`: 탐색·미리 보기 갱신, 자동화 전용 계수기, 시험).

**재현(에디터가 닫힌 것을 확인한 뒤, 바뀐 C++ 4개의 시각을 갱신):**
- non-unity(`-DisableUnity -Module=vBoR`)·unity: 둘 다 `Result: Succeeded`, 경고·오류 0. 로그 `Saved/Logs/ReviewP1cR2NonUnity.log`, `ReviewP1cR2Unity.log`.
- `vBoR.*` 첫 실행: 367/368. 실패 하나는 `vBoR.P0Buck.E8Persistence`의 "Repeated replacement 24: candidate replacement failed (OS error 5)"다. P1c와 무관한 기존 간헐 실패(P0-c M1)다. 그 시험만 다시 돌리니 통과했다. 로그 `ReviewP1cR2All.log`, `ReviewP1cR2E8.log`. 나머지 367개(P1c 9개 포함)는 통과했다. 끝난 뒤 `UnrealEditor` 프로세스 없음.
- 갱신 한 번의 `IsPoseClear` 수는 E4·E5 모두 18번이다(시험 `AddInfo`). 1회차의 154번에서 줄었다.

**Codex 토큰:** 2.34M(캐시 약 94%), 2026-10-17 15:36–15:41 UTC로 약 4분. 하위 에이전트 0, 재실행 0.

**확인한 것:**
- **탐색**(`Interaction/VBoRCarryLanding.cpp:26`): 1 cm 간격, 최대 15번이다. 처음 빈 높이와 직전 막힌 높이 사이를 0.001 cm까지 좁히는 부분은 그대로다(최대 약 10번).
  - E4의 Z = 85 cm ±0.01, 모든 Tick의 `IsPoseClear`, E7 미리 보기 일치가 그대로 통과한다.
- **색 판정**(`:86`): 겹침 검사를 뺐다. `CalculateReleaseDrop`이 참을 돌려줄 때는 착지 겹침을 이미 확인했으므로 결과는 같다.
- **보임**(`:97`): 보일지를 먼저 정하고, 값이 바뀔 때만 `SetVisibility`를 부른다.
  - `PreviewRenderState`는 nullrhi에서 빈 단언이 되지 않도록 렌더 상태를 강제로 만든다. 그다음 보임을 바꾸면 dirty가 되는 것을 양성 대조로 먼저 보고, 연속 갱신에서는 dirty가 되지 않는 것을 본다. 시험이 실제로 무엇인가를 잡는다.
- **계수기**(`VBoRCarryObject.h:217-219`, `.cpp:177-179`): `WITH_DEV_AUTOMATION_TESTS` 안에만 있다. 배포 빌드에는 들어가지 않는다.
- 상한 28번은 기본 `LevelLiftMaxCm` 15 cm 기준이다. 한도를 키우면 탐색 횟수도 1 cm당 1번씩 는다(보고 ③). 1회차 참고 2(한도가 박스 크기를 따르지 않음)와 함께 VR 때 본다.

## 개정 이력

| 날짜 | 회차 | 내용 |
|---|---|---|
| 2026-10-18 | 1회차 | 열림 1건 R1-1(들고 있는 동안 미리 보기의 frame 비용 — 원인 일부는 명세 D3). 참고 셋(즉시 올림, 올림 한도 고정, 윤곽 재질) |
| 2026-10-18 | 2회차 | 통과, 열린 항목 없음. R1-1 반영 확인(`78f3ac1`, 갱신당 IsPoseClear 154 → 18번). E8Persistence 간헐 실패 1번(P0-c M1, 다시 돌려 통과) |
