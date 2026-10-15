# P5b 리뷰 기록

명세: [Docs/Specs/P5b-gear-lever.md](../Specs/P5b-gear-lever.md) · 브랜치: `codex/P5b-gear-lever`

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
| 1 | 2026-10-15 | `c3254f1` 1축 부품 걸림 옵션, 근사 차 기어 레버·끝 버튼 | Claude | **열림 2건(R1-1 제품, R1-2 시험만).** 빌드·시험 재현: non-unity·unity 성공(바뀐 것이 없어 다시 컴파일한 파일 0 — Codex non-unity 로그에서 헤더를 쓰는 14개 파일 컴파일·경고 0 확인), `vBoR.*` **197/197**, EXIT CODE 0. **Codex 토큰 6.70M(캐시 97%), 약 14분, 하위 에이전트 0, exec 45, 전체 시험 1번(최종 트리).** 빌드가 여러 번인 것은 UHT 오류(배열에 `Units` 메타) 수정과 경계 오차 수정 뒤라 받아들인다 |
| 2 | 2026-10-15 | `7cc0e91` R1-1 알림을 `Tick`으로, R1-2 E8 비대칭 범위 | Claude | **열린 항목 없음 — 통과.** 빌드: non-unity·unity 성공(Codex R1 로그에서 마지막 소스 변경 뒤 non-unity 11개·unity 5개 묶음 컴파일, 경고 0). `vBoR.*` 첫 실행 197/198 — 실패 1은 P5b와 무관한 `vBoR.P0Buck.E8Persistence`(아래 2회차 참고), 다시 실행 **198/198**, EXIT CODE 0. **Codex 토큰 2.21M(캐시 93%), 약 5분, 하위 에이전트 0, exec 19, 빌드·전체 시험 각 1번(최종 트리)** |

### 1회차에 확인한 것

- **D1:** `EVBoRAxisRelease::Detent`를 끝에 더했다(`Return` 0·`Stay` 1 그대로, E13). 새 클래스는 없다. 걸림 코드는 `Release == Detent`일 때만 탄다. `Return`·`Stay` 범위·복귀·열림/닫힘은 바뀌지 않았고 기존 P2b·P2c·P2d·P5a 시험이 모두 통과한다.
- **D2 범위:** `UpdateConstraints`(`VBoRAxisPart.cpp:60-65`)가 첫·끝 걸림에 `bReverse` 부호를 곱해 SDK 범위를 만든다. 계산해 보면 비대칭 걸림에서도 맞다(아래 R1-2는 시험만). `AcceptTransform`(`:274`)도 같은 범위로 자른다. `MaxAngleDeg`는 쓰지 않는다(E8에서 1°로 두고 확인).
- **D3 전환:** 확정 위치 기준으로 위·아래 기준을 따로 쓰고(E7), 경계를 포함하며(E3), 한 번의 갱신에서 두 단계를 순서대로 넘는다(E5). 두 방향이 동시에 참일 수 없는 것은 D5 검사가 보장한다. 경계 오차 보정(최대 1e-4°, 여유의 1/4 이하)은 합성 입력의 float 오차만 받는 크기라 받아들인다.
- **D4:** 놓으면 양방향으로 확정 각도까지 가서 멈춘다(`:386-392`, E2·E3). 가는 중 다시 잡으면 그 자세에서 이어서 따른다(E10).
- **D5:** 검사 14종을 시험이 이유와 함께 각각 확인한다(E12). 잡기는 `PreInitializeComponents`에서 충돌·Palm 입력을 끄고, `HandlePointer`에서도 선택을 거부한다.
- **D6:** 걸림 부품은 `EvaluateState`에서 열림·닫힘을 건너뛰고 hover·잡기 색만 쓴다(E11). `GetOpenRatio`는 0(§5 허용).
- **D7:** 시작 자세를 `BeginPlay`에서 놓는다. E1이 `OnConstruction`을 두 번 돌려도 쉬는 축이 그대로임을 보인다.
- **D8 버튼:** 레버 `AxisPivot`의 자식이고, +Z가 차 +Y를 향하며, 레버와 함께 돈다. 단계 태그가 없어 진행기가 세지 않는다(E15, 실제 `InitializeFlow`로 확인). 눌러도 레버 각도·확정·잡기 상태가 그대로다. 버튼 쉬는 앞면은 Actor 로컬 값이라(`VBoRPokeButton.cpp:55`, `:100-102`) 움직이는 부모에서도 맞다.
- **D9:** 축 (39, −30, 70), 16 × 2 × 2 cm, 걸림 −20/0/+20°, 기준 15/15°. 옮기고 돌린 차에서 세 걸림 모두 레버·버튼이 대시보드·핸들 판과 겹치지 않는다(E14). `GearAxisRotation`은 실제 축이 미정이라 둔 자리값이라 범위 안이다.
- **P2d E16:** 제외 목록에 `GearLever`만 더했고 "문 손잡이 2개" 단언은 그대로다.
- **범위:** P3 단계 데이터·규칙, 레벨·에셋은 그대로다. 시험용 `friend` 선언이 제품 헤더 세 곳에 늘었지만 기존 방식(`FP3FlowTestAccess` 등)과 같다. 주석은 사용자 말투(`~함`, 근거 ID)다.

### 2회차에 확인한 것

- **R1-1:** `EvaluateDetent`는 index 확정·로그·대기 배열 추가만 한다(`VBoRAxisPart.cpp:256`). `Tick` 앞부분(`:341-347`)이 대기 배열을 옮겨 받아 순서대로 소리·이벤트를 낸다.
  - 이 Tick은 `TG_PostUpdateWork`라 SDK가 그 frame의 자세를 적용한 뒤다. 그래서 처리에서 읽는 레버 자세도 맞다.
  - 처리 중 Actor가 지워지면 멈춘다. 처리 중에 생긴 새 전환은 다음 Tick으로 간다(배열을 옮겨 순회).
  - 확정 위치 갱신은 그대로 바로 한다. 같은 frame 놓기 시험(`vBoR.P5b.R1SameFrameThresholdAndRelease`)이 상단 복귀와 이벤트 `{2}` 1번을 보인다.
- **R1-2:** E8이 대칭·비대칭(−15/0/+25°, 14/14°) × `bReverse` 끔·켬으로 SDK 원시 회전·값·world 자세를 본다. 부호 곱을 빼면 비대칭 경우가 +15°에서 멈춰 실패한다.
- 기존 이벤트 단언은 값을 바꾸지 않고 `Tick` 뒤로만 옮겼다. 제품 범위 계산은 바뀌지 않았다.
- **P5b와 무관한 실패 1건(항목 아님):** 첫 전체 실행에서 `vBoR.P0Buck.E8Persistence`가 40번 반복 저장 중 33번째에 `candidate replacement failed (OS error 5)`로 실패했다(`P0BuckAlignmentTests.cpp:213`).
  - P0-c 리뷰 M1("교체가 한 번 실패, 원인 모름")과 같은 현상이고, 오류 번호(5 = 접근 거부)가 처음 나왔다.
  - `MoveFileExW` 교체(`VBoRBuckAlignment.cpp:226`)가 방금 쓴 파일을 다른 프로세스(백신·색인기 추정)가 잠깐 열고 있을 때 실패하는 모양이다.
  - 다시 실행하면 통과했다. 이 브랜치는 그 코드를 건드리지 않았다. 따로 다룬다(HANDOFF).

## 항목

### R1-1 걸림 이벤트가 ISDK 포인터 `Move` 처리 안에서 나감 — 반영함 (`7cc0e91`)

**문제**
- `OnDetentChanged.Broadcast`와 `StateAudio->Play`(`VBoRAxisPart.cpp:256`)는 `EvaluateDetent`에서 부른다. `EvaluateDetent`는 `AcceptTransform`(`:275`) 안에서 불리고, `AcceptTransform`은 변환기 `UpdateTracked`(`:124`)에서 불린다.
- 실제 잡기에서 이 경로는 ISDK가 포인터 `Move` 이벤트를 처리하는 도중이다: `IsdkGrabTransformerComponent.cpp:129-130`(`Move` → `UpdateTransform`) → `:407`·`:414`(변환기 `UpdateTransform`).
- `OnDetentChanged`는 `BlueprintAssignable`이고 자식 Blueprint가 묶는 이벤트다(구조 원칙: 실제 부품은 자식에서 완성). 묶인 처리가 레버를 놓게 하거나 끄거나 지우면, SDK가 선택 배열을 도는 중에 SDK 상태가 바뀐다. P2b R1-3·D5에서 이미 겪은 재진입이다.
  - 이 클래스는 그래서 열림·닫힘 이벤트를 `Tick`의 `EvaluateState`에서만 낸다.
  - 취소도 `Tick`으로 미룬다(`:320-321` 주석, `:394` 주석).
  - 명세 §3 "ISDK 포인터 이벤트 안에서 SDK 상태를 바꾸지 않는다"와도 어긋난다.
- 이벤트 시점의 레버 자세도 어긋난다. SDK는 변환기가 돌려준 자세를 `UpdateTransform`이 끝난 뒤에 적용한다. 그래서 처리에서 레버 자세를 읽으면 이전 자세가 나온다.

**제안**
- 확정 위치(`DetentIndex`) 갱신은 지금 자리(`AcceptTransform`)에 둔다.
  - 판정까지 `Tick`으로 옮기면 안 된다. 손을 펴서 놓는 것(`:365` `ReleasePart`)이 `:393` `EvaluateState`보다 먼저 돌아서, 같은 frame에 기준을 넘고 놓으면 전환을 놓친다.
- 알림만 미룬다.
  - 바뀐 index를 순서대로 쌓아 둔다(예: 대기 배열).
  - `Tick`에서 순서대로 `OnDetentChanged`를 내고 소리를 낸다. 미루기 방식은 `bCancelPending`(`:321`)과 같다.
  - 이 Actor의 Tick은 `TG_PostUpdateWork`(`:139`)라 그 frame의 SDK 갱신 뒤에 돈다.
- 로그는 전환 시점(지금 자리)에 남겨도 된다.
- 시험 도우미 `Move`는 Tick을 돌지 않는다. 그래서 이벤트를 보는 시험은 `Tick`을 한 번 부른 뒤 확인한다.

**확인할 시험**
- E5를 고친다. 한 번의 `Move`로 −20° → `Tick` 전에는 index = 하단이고 이벤트·소리는 0이다. `Tick` 뒤에는 이벤트 `{1,0}` 순서, 소리 2번이다.
- 같은 frame 놓기를 새로 넣는다. `Move`로 +15°(기준) → 곧바로 `Release` → `Tick`. 상단으로 가고(20°까지 이동), 이벤트는 `{2}` 1번이다.
- 기존 E3·E4·E6·E7·E10·E11의 이벤트 단언은 `Tick` 뒤로 옮기고 값은 그대로 둔다.

**반영 내용 (Codex, 2026-10-15)**
- `EvaluateDetent`는 index 확정·로그·대기 index 추가만 한다. `Tick`에서 대기 배열을 옮겨 순서대로 소리·이벤트를 내고, 이벤트가 Actor를 지우면 그 Tick을 끝낸다. 콜백 중 새 전환은 다음 Tick으로 남긴다.
- E5: Tick 전 하단 확정·이벤트/소리 0, Tick 뒤 `{1,0}`·소리 2번, 다음 Tick 중복 없음. 같은 frame 기준 도달 → 놓기 시험을 추가해 상단 복귀·`{2}`·소리 1번을 확인한다.
- E3·E4·E6·E7·E10·E11은 Tick 뒤 알림을 확인한다. 검증 결과는 아래에 기록한다.

### R1-2 E8 SDK 범위 시험이 대칭 걸림이라 `bReverse` 부호를 못 잡음 — 반영함 (`7cc0e91`) (시험만)

**문제**
- `FP5bRanges`(`P5bGearLeverTests.cpp:200`)는 기본 걸림 −20/0/+20°로 `bReverse` 끔·켬을 본다.
- 범위가 대칭이라, `UpdateConstraints`(`VBoRAxisPart.cpp:60-65`)에서 부호 곱을 빼거나 첫·끝을 바꿔 써도 시험이 통과한다.
- 명세 E8은 "`bReverse` 끔·켬 둘 다"의 범위를 요구한다. 방향지시 레버가 같은 옵션을 다시 쓰므로 부호를 시험으로 묶어 둔다.

**제안**
- 비대칭 걸림으로 같은 시험을 한 번 더 돌린다. 예: −15 / 0 / +25°, 기준 14 / 14°.
  - D5 검사를 통과한다: 기준 ≤ 15, 14 + 14 = 28 > 25.
- ±50°로 밀면 값이 +25 / −15에서 멈춘다.
- SDK 원시 회전(`FP5bTestAccess::SDK`)은 `bReverse` 켬일 때 쿼터니언 각이 [−25, +15]가 된다.
- 제품 코드는 고치지 않는다(계산으로 맞음을 확인했다).

**확인할 시험**
- 위 비대칭 경우를 `bReverse` 끔·켬, 축을 돌려 둔 부품으로 넣는다.

**반영 내용 (Codex, 2026-10-15)**
- 기존 대칭 경우를 유지하고 −15/0/+25°·기준 14/14°를 추가했다. 두 경우 모두 회전해 둔 축과 `bReverse` 끔·켬에서 ±50° 입력의 SDK 원시 쿼터니언·확정 값·world 자세를 확인한다.
- 범위 계산 제품 코드는 바꾸지 않았다.

## Codex 반영 검증 (2026-10-15)

- R1-1·R1-2 반영 완료, 질문 없음. Claude 리뷰 2회차 대기.
- 최종 C++ 트리에서 non-unity(`-DisableUnity -Module=vBoR`)·unity 각각 1회: `Result: Succeeded`, 프로세스 exit 0, 컴파일 경고·오류 0.
- 전체 시험 1회: `vBoR` 198개 발견·198개 완료·198 Success, Fail 0, P5b 16/16. `TEST COMPLETE. EXIT CODE: 0`, 프로세스 exit 0, AutomationController/Test Error 0.
- 로그: `Saved/Logs/P5b-R1-nonunity-build.log`, `P5b-R1-unity-build.log`, `P5b-R1-full-tests.log`(git 무시).
- `StructUtils` 폐기 안내·샘플 CDO·OpenXR·임시 world context 로그는 기존 환경 잡음으로 분리함. Invalid detent 경고는 E12가 의도한 14종이며 각 이유·횟수 단언 통과.
- Quest 3 Link/VR Preview·T-08·Blueprint/Details는 `not-tested`. 하위 에이전트·자체 리뷰·에디터 MCP·push·PR·머지 없음.

## 참고 (항목 아님 — VR에서 볼 것)

- **레버가 얇다(2 × 2 cm).** 손바닥 잡기 판정은 반지름 4 cm 구다(`Plugins/MetaXRInteraction/Source/OculusInteraction/Public/Interaction/GrabDetectors/IsdkHandGrabDetector.h:134`). 잡는 상자 여유 1 cm(`GrabMarginCm`)로 충분히 잡히는지 본다.
- **끝 버튼 오누름:** 레버 끝을 감싸 쥘 때 검지 끝이 버튼 면을 지나가면 눌림 이벤트가 난다(명세 D8: 관계를 두지 않음). VR에서 얼마나 자주 나는지 본다(명세 §7-6).
- **놓을 때 튀어 보임:** 복귀 속도가 180 deg/s라, 15°에서 전환하고 놓으면 남은 5°를 약 0.03초에 채운다. "딸깍 걸리는" 느낌인지, 튀는 느낌인지 본다(`ReturnDegPerSecond`는 레버마다 바꿀 수 있다).
- **Details 정리(선택):** 걸림 프로퍼티 4개가 `Return`·`Stay` 부품 Details에도 보인다. `EditCondition`으로 `Release == Detent`일 때만 편집되게 하면 자식 Blueprint 작성자가 덜 헷갈린다. 다음 반영 때 같이 해도 되고 안 해도 된다.
- 명세 §7 순서대로 본다: 시인성·도달 → 기준 미만 놓기 → 상단 유지 → 연속 딸깍 2번 → 하단 유지 → 끝 버튼 → 기준·간격 체감 → 핸들 판 가림.

## 질문

(없음)
