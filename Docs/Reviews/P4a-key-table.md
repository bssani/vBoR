# P4a 리뷰 기록

명세: [Docs/Specs/P4a-key-table.md](../Specs/P4a-key-table.md) · 브랜치: `codex/P4a-key-table`

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
| 1 | 2026-10-15 | `275d212` 키·미니 테이블·근사 차 연결 | Claude | **열림 3건(R1-1~R1-3).** 구조와 SDK 경로는 명세대로다. 빌드·시험 재현: non-unity·unity 성공, vBoR 경고·오류 0(컴파일은 Codex 빌드로 최신. Codex 로그에서 새 파일 컴파일·경고 0 확인), `vBoR.*` **155/155**, EXIT CODE 0. Codex가 하지 않은 최종 트리 전체 실행을 이번에 대신 했다. **Codex 토큰 7.80M(캐시 97%), 약 63분, 하위 에이전트 0, exec 48** |

### 1회차에 확인한 것

- **D1·D2 이벤트만:** 키에는 상태가 없다. `OnUnlockPressed`·`OnLockPressed`가 누른 키를 넘기고, 근사 차가 두 키를 모아 다시 낸다(E2·E13).
- **D3 쥐지 않은 손만:**
  - 필터를 버튼(`PressFilter`)에 둔 것이 맞다. 진행기는 단계를 시작할 때 태그로 버튼을 찾아 `OnButtonPressed`에 묶는다(`VBoRScenarioRunner.cpp:177-190`). 키 이벤트만 걸렀다면 안 쥔 키를 눌러도 02가 끝났을 것이다.
  - 실제 손 리그의 poke interactor에도 손 태그가 붙는다(`Plugins/MetaXRInteraction/Source/OculusInteractionPrebuilts/Private/Rig/IsdkPokeInteractionRigComponent.cpp:64-71`). 그래서 시험이 grabber로 주입한 손 구분이 VR에서도 같다.
- **D4 자유 6DoF:**
  - 자유 변환기와 `SingleGrabTransferToSecond`를 쓴다. 두 손 잡기가 생기지 않으므로 양손 스케일도 없다.
  - SDK 크기 제약을 끈 이유(X 값을 세 축에 씀)는 E1이 "크기 그대로"로 확인한다.
  - P3 리그 pinch 켬: 다른 부품은 grabbable 쪽에서 pinch를 거부한다(`VBoRAxisPart.cpp:179`, `VBoRCarryObject.cpp:85`, `VBoRHingedPanel.cpp:192`). 원거리·레이 차단은 그대로다(E1 pinch).
  - **명세 빈틈(Claude):** P3 리그가 pinch를 막고 있다는 것을 명세에 적지 않았다. Codex가 찾아 보고 ③에 적었다. 맞는 판단이다.
- **D5 던지기:**
  - SDK가 놓을 때 `PostEvent` 안에서 속도를 넣는다. 그래서 grabbable 자식에서 `Super::PostEvent` 뒤에 속도를 0으로 한다. SDK 취소·재선택 호출은 없다.
  - E6(High면 SDK 속도 그대로), E7(창 안 Low → 0, 이유 로그), E7 창 크기·창 밖 Low, E7b(추적 손실로 놓임)를 실제 SDK 표본으로 본다.
- **D6 되돌리기:** 같은 Actor를 옮기고 속도를 0으로 한다. 묶어 둔 이벤트와 버튼 Actor가 그대로다(E8).
- **D7 시작 위치:** 시작 표시는 차 앞 `StartOutsideCm`에서 차 −X를 바라본다. 표시판도 앞쪽에서 시작 위치를 향한다(E12).
- **D8 태그:** 4개가 하나씩만 일치한다(E12). 생성자 기본 단계는 바뀌지 않았다.
- **그 밖:**
  - 키 버튼의 메시는 충돌이 꺼져 있다(`VBoRPokeButton.cpp` `BaseMesh`·`ButtonMesh` NoCollision). 그래서 붙은 버튼이 키 물리를 밀지 않는다.
  - Buck 정렬은 차가 아니라 폰을 옮긴다(`P0BuckBench.cpp:170`). 그래서 테이블에 붙지 않은 물리 키가 정렬 때 남겨지는 일은 없다.
  - 시험 로그의 `DestroyActor: World has no context` 경고는 임시 시험 월드를 정리할 때 늘 나오는 잡음이다(P2d 실행에도 190줄).
- **범위:** 버튼 `PressFilter`와 P3 리그 pinch는 §2 목록 밖이다. 다만 D3·D8과 D4에 필요하고, 보고 ③에 적혀 있어 받아들인다. 표시판 문구는 R1-2.

## 항목

### R1-1 안 쥔 키를 찌르면 이벤트는 없는데 눌림 표시·소리는 난다 — 열림

**문제**
- `PressFilter`(`VBoRPokeButton.cpp:119`, `VBoRKeyFob.cpp:82`)는 `OnButtonPressed`만 막는다. 눌림 확인은 ISDK가 따로 낸다:
  - 소리: `PointerAudio`가 `Poke`의 Select에 바로 반응한다(`VBoRPokeButton.cpp:77-78`).
  - 버튼이 들어가는 움직임: `ButtonVisual`(`:76`).
  - 덧입힘 색: `HandleState`(`:105-114`).
- 발생 조건: 테이블 위(안 쥔) 키의 버튼을 찌른다(E4). 또는 키를 집으려고 뻗은 손이 버튼을 스친다.
- 영향:
  - 촉감이 없어서 이 표시와 소리가 "눌렸다"는 유일한 확인이다(AGENTS.md "확정된 제약").
  - 01·02에서 참가자가 키를 집지 않고 테이블 위에서 누르면, 딸깍 소리와 눌림이 보이는데 아무 일도 일어나지 않는다. 고장으로 보이거나 참가자가 "눌렀다"고 답하게 된다.
- 쥔 손이 누르는 경우(E3)도 같다. 다만 이쪽은 아래 "제안"대로 이번에는 막지 않는다.

**제안**
- 키를 쥐지 않은 동안 두 버튼의 `Poke`를 끈다.
  - `UIsdkInteractableComponent::Activate`/`Deactivate`가 API 인스턴스를 켜고 끈다(`Plugins/MetaXRInteraction/Source/OculusInteraction/Public/Interaction/IsdkInteractableComponent.h:141-147`).
  - 활성 조건은 `IsActive() && IsVisible()`이다(`Private/Interaction/IsdkInteractableComponent.cpp:199-202`).
  - 꺼진 동안은 hover·select가 생기지 않아 소리·눌림·색이 모두 나지 않는다.
- 켜고 끄기는 키 `Tick`에서 `HoldingHand`를 보고 한다.
  - `HoldingHand`는 잡기 포인터 이벤트 안에서 바뀐다. 그 안에서 다른 interactable 상태를 바꾸지 않는다(P2b R1-3과 같은 이유). 한 프레임 늦어도 된다.
  - 버튼을 만든 직후(BeginPlay)에는 꺼 둔다.
- `PressFilter`는 그대로 둔다. 켜지기 전 한 프레임과 쥔 손 경우를 막는다.
- 쥔 손의 표시는 이번에 막지 않는다.
  - ISDK에는 interactor별로 interactable을 거르는 공개 API가 없다(`IsdkInteractableComponent.h`·`IsdkPokeInteractor.h`에 해당 API 없음). 막으려면 손 리그를 건드려야 한다.
  - 명세 §7-4(쥔 손가락이 버튼을 덮어도 눌리지 않음)를 VR에서 보고 정한다.

**확인할 시험**
- E4 고침: 테이블 위 키의 두 버튼 `Poke->IsActive()`가 false다.
- 잡기 Select 직후(Tick 전)에는 아직 false, 키 Tick 한 번 뒤 true다. 놓은 뒤 Tick → false.
- E7b(추적 손실로 선택이 끝남) 뒤 Tick → false. E8(되돌림) 뒤에도 false.
- 기존 E2·E3·E5·E11·E13은 그대로 통과한다. 주입 이벤트는 활성 여부와 상관없이 들어간다. 그래도 잡은 뒤 키 Tick을 한 번 넣어 실제 순서에 맞춘다.

### R1-2 시작 표시판 문구를 한국어에서 영어로 바꿈 — 명세 밖 — 열림

**문제**
- `P3VehicleMock.cpp` BeginPlay의 표시판 글이 "시작 위치 / 근사 형상·자리값"에서 "Start / Approximate geometry"로 바뀌었다.
- 명세 D7은 자리와 방향만 옮긴다. 보고 ③에도 없다.
- 참가자·진행자가 보는 글이고, 사용자가 VR에서 본 문구다.

**제안**
- 원래 문구로 되돌린다.
- 바꿔야 할 이유가 있었다면(글꼴 등) 되돌리지 말고 아래 "질문"에 적는다.

**확인할 시험**
- 없음(되돌리기만).

### R1-3 바닥 판정이 키 원점(메시 피벗) 높이다 — 메시 피벗·키 자세에 따라 안 돌아옴 — 열림

**문제**
- `VBoRKeyFob.cpp:186`은 `GetActorLocation().Z`(루트 = 키 메시 피벗)를 테이블 원점 Z + 2 cm와 비교한다.
- 근사 큐브는 피벗이 가운데라서 눕히면 0.75 cm, 옆으로 세우면 1.5 cm라 맞는다. 그러나 다음 경우에는 안 맞는다:
  - 끝으로 서거나 차·테이블 다리에 기대 멈추면 가운데가 3 cm 이상이다. 그래서 바닥에 있어도 돌아오지 않는다.
  - 자식 Blueprint에서 실제 키 메시로 바꾸면 피벗이 어디 있는지 모른다(예: 고리 쪽 끝). 피벗이 아랫면에서 2 cm 넘게 떨어져 있으면 바닥에 닿아도 돌아오지 않는다.
- 피벗에 기대는 판정이다. AGENTS.md "움직이는 부품은 메시 피벗에 기대지 않는다(U-09)"와 같은 문제다. 되돌림 자리(`VBoRKeyTable.cpp:31-33`)는 이미 bounds 아랫면을 쓴다.

**제안**
- 키 메시의 world bounds 아랫면을 테이블 원점 Z + `FloorMarginCm`와 비교한다.
  - 값은 `KeyMesh->Bounds.GetBox().Min.Z`를 쓴다. 물리로 움직여도 매 프레임 갱신된다.
- 여유 2 cm는 그대로 둬도 된다. 아랫면 기준이면 자세와 관계없다.

**확인할 시험**
- E8에 하나 더 넣는다. 키를 Y축으로 90° 돌려 끝으로 세우고, 아랫면을 테이블 원점 Z에 둔다(가운데는 여유보다 높다). 그러면 되돌아와야 한다.
- E10은 그대로다. 바닥보다 높은 곳에서는 돌아오지 않는다.

## 참고 (항목 아님 — VR에서 볼 것)

- **움직이는 키에서 ISDK poke가 잘 잡히는지(명세 §7-3)는 이번이 처음 확인이다.** 버튼은 1.2 cm, 두 버튼 사이 간격은 1.8 cm다. 잘 안 눌리거나 둘이 같이 반응하면 그대로 알려 준다.
- **바닥 높이:** 판정 바닥은 테이블 원점 Z다. 테이블 원점은 근사 차 원점 높이다. `L_P3_Flow`의 실제 바닥이 차 원점보다 2 cm 넘게 높으면 키가 돌아오지 않는다. 그때는 차 Actor 높이와 바닥을 먼저 본다.
- **테이블 자리:** 앞 테이블은 차 코 앞 100 cm, 운전석 쪽 90 cm에 있다. 뒤 테이블은 차 뒤 100 cm, 같은 쪽 90 cm에 있다. 뒤 테이블이 리프트게이트(P4b)·박스(P1b) 동선을 막는지 그 명세 때 본다(자리값).
- **던지기:** 키는 작고 얇아서(1.5 cm) 세게 던지면 상판(3 cm)이나 바닥을 뚫고 지나갈 수 있다. 뚫고 떨어져도 높이 판정으로 돌아온다.
- 명세 §7 순서대로 본다: 시작 위치 → 앞 키 손바닥·pinch → 다른 손으로 열림·잠금 → 쥔 손가락 덮기 → 놓기·던지기·되돌림 → 뒤 키 → 시야 밖에서 돌아온 직후 놓기.

## 질문

(없음)
