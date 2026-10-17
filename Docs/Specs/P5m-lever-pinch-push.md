# P5m 레버 개정 — pinch로 잡기, 손가락·손으로 밀어 올리고 내리기

| 항목 | 값 |
|---|---|
| 단계 | P5 자유모드 실내 조작(U-09·U-11) — P5b 기어 레버·P5c 방향지시 레버 개정 |
| 결정 근거 | **사용자 VR(2026-10-17, U-13):** 스티어링 휠 뒤 레버를 손으로 밀거나 손가락으로 올리고 내리고 싶은데 손바닥 잡기로만 움직임. "lever는 grab보다는 pinch로", "손으로 lever를 밀어서 올리거나 내리게". **사용자 문답(같은 날):** pinch + 밀기, 손바닥 잡기는 끔(핸들을 돌리다 손바닥이 레버를 잘못 잡지 않게). 방향지시 레버도 같이 바꿈 |
| 베이스 브랜치 | `main` @ P5l 머지 뒤 |
| 작업 브랜치 | `codex/P5m-lever-pinch-push` |
| 리뷰 파일 | `Docs/Reviews/P5m-lever-pinch-push.md` (첫 리뷰 때 생김) |
| 상태 | 명세 — 구현 전 |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **1축 부품에 입력 옵션 셋** | `AVBoRAxisPart`에 `bAllowPalmGrab`(기본 켬), `bAllowPinchGrab`(기본 끔), `bAllowPush`(기본 끔)를 더한다. 기본값이 지금 동작이다. 그래서 문 손잡이·글러브박스 손잡이 같은 기존 부품은 바뀌지 않는다. 지금은 손바닥만 켜고 pinch는 끈다(`Interaction/VBoRAxisPart.cpp:193-194`). 설정이 잘못되면(걸림 설정 오류 등) 셋 다 끈다(P5b E12와 같음). 밀기는 회전(`Movement = Rotate`)일 때만 받는다. 직선 이동 부품에서 켜면 경고하고 무시한다 |
| D2 | **pinch 잡기** | 잡는 방식은 선택 순간 grabber에서 읽는다(`IsInputMethodActive`, ISDK `IsdkGrabberComponent.h:206`). 2축 부품이 같은 일을 한다(`Interaction/VBoRTwoAxisPart.cpp:130-139`의 `bPinch`). 지금 손바닥 전용인 곳을 그 방식에 맞게 바꾼다. 하나는 자세 읽기(`GetPalmGrabTransform` → pinch면 `GetPinchGrabTransform`, `IsdkGrabberComponent.h:167-168`, 지금 `Interaction/VBoRAxisPart.cpp:384-404`)다. 다른 하나는 손 펼침 확인(`ReadPalmGrab` → pinch면 `ReadPinchGrab`, `Interaction/VBoRHandRigPawn.cpp:42-74`, 지금 `Interaction/VBoRAxisPart.cpp:371-392`)이다. "다시 잡으려면 먼저 손을 폄"(`bNeedsPalmOpen`, `.h:142`)도 그 방식의 인식기로 본다. 추적 손실 보류·복구 기준 다시 잡기(P2b D7, P5b E9)는 잡는 방식과 상관없이 같다 |
| D3 | **밀기: 레버 윗면·아랫면을 넘어 들어온 손 점이 레버를 민다** | **손 점:** 문·거울과 같다. 추적된 원래 점(손바닥 + 손가락 끝 4개, `AVBoRHandRigPawn::ReadRawPalmAndTips`, `Interaction/VBoRHandRigPawn.cpp:133-154`)이고 신뢰도 High일 때만 쓴다. **면:** 축 피벗(`AxisPivot`) 좌표에서 쉬는 메시 bounds(`OnConstruction`이 잡기 상자에 쓰는 것, `Interaction/VBoRAxisPart.cpp:160-172`)를 쓴다. 회전 방향(축 × 레버 방향)으로 마주 보는 두 면, 곧 레버의 윗면·아랫면이 미는 면이다. **받는 조건**(문 `FindPush` `Interaction/VBoRDoorMotion.cpp:76-131`과 거울 `Interaction/VBoRTwoAxisMotion.cpp:39-64`의 규칙을 따름): ① 직전 frame에 그 면 밖에 있었거나, 같은 밀기를 잇는 중이다 ② 지금 면 안쪽에 있다. 레버 두께 + 여유보다 깊으면 통과로 보고 뺀다 ③ 축 방향·레버 길이 방향으로 bounds + 여유 안에 있다 ④ 축에서 `PushMinAxisDistanceCm`(자리값 2 cm) 이상 떨어져 있다. 손바닥 방향은 보지 않는다(손가락 끝으로 미는 동작, 거울과 같음). **한 번의 밀기는 점 하나·면 하나다**(거울 P5e R1-1). **각도:** 그 점이 미는 면 위에 머무는 각도로 정한다. 미는 방향으로만 바뀐다(문 `:123-129`와 같은 꼴). 그다음 걸림 범위(첫·끝 걸림 각도)로 자른다 |
| D4 | **밀기와 걸림은 잡기와 같은 길로** | 밀어서 바뀐 각도는 잡아서 바뀐 각도와 같은 처리로 들어간다(`AcceptTransform`의 걸림 분기 → `EvaluateDetent`, `Interaction/VBoRAxisPart.cpp:240-296`). 그래서 걸림 기준·히스테리시스·한 frame 여러 칸·소리·이벤트를 Tick 밖에서 내지 않는 규칙(P5b R1-1), 방향지시 차선 변경·재무장(P5c)이 그대로 맞는다. 지금 `EvaluateDetent`는 잡은 손이 있을 때만 돈다(`:242`). 미는 중에도 돌게 한다. **밀기가 끝나면**(점이 면 밖으로 나감·추적 손실·잡기로 넘어감) 놓았을 때와 같다. 확정된 걸림 자리로 돌아간다(`:413-419`). 걸림 기준을 못 넘겼으면 원래 자리, 넘겼으면 다음 자리다 |
| D5 | **잡기·밀기·다른 부품** | 어느 손이든 이 레버를 잡고 있으면 밀기를 받지 않는다. 미는 손이 그 자리에서 pinch하면 잡기로 넘어가고 밀기는 끝난다(튐 없이 기준 다시 잡기, 거울 P5e D5). **다른 것을 잡고 있는 손의 점은 밀기로 쓰지 않는다.** 그 손의 ISDK grabber가 무엇이든 잡고 있으면(`HasSelectedInteractable`, `IsdkGrabberComponent.h:130`) 그렇다. grabber는 손 rig의 `GetGrabber`(`Plugins/MetaXRInteraction/Source/OculusInteractionPrebuilts/Public/Rig/IsdkRigComponent.h:193`)로 읽는다. 레버는 휠 바로 뒤에 있어서, 휠을 쥔 손가락이 레버를 건드리는 일을 막는다 |
| D6 | **피드백** | 밀기 색(`PushingColor`, 문·거울과 같은 덮개)을 더한다. 걸림 소리·이벤트는 지금 그대로다. 디버그 문구에 잡는 방식(palm/pinch)·밀기 상태·밀기 시작 수를 더한다 |
| D7 | **근사 차** | 기어 레버(`P3Probe/P3VehicleMock.cpp:166-173`)와 방향지시 레버(`:182-190`)에 손바닥 끔·pinch 켬·밀기 켬을 준다. 자리·크기·걸림 값은 그대로다. 기어 레버 끝의 poke 버튼(`:175-180`)은 그대로다 — 버튼은 레버 끝면(레버 길이 방향)을 보고, 밀기 면은 윗면·아랫면이다 |

## 1. 왜 만드는가

- 실차에서 칼럼 레버는 손가락으로 툭 올리거나 내린다. 손바닥으로 움켜쥐는 일은 드물다.
- 레버가 휠 바로 뒤에 있어서 손바닥 잡기는 휠을 쥐려던 손이 레버를 잡는 오조작을 만든다.

## 2. 범위

### 만들 것

**1. 1축 부품** — `AVBoRAxisPart`
- D1–D6. 새 프로퍼티(옵션 셋, `PushMinAxisDistanceCm`, 밀기 두께 여유, `PushingColor`)는 편집 가능하게 둔다. 설정 검사는 `PreInitializeComponents`에서 한다(P5d R1-1).

**2. P3 근사 차** — `AP3VehicleMock`
- D7.

**3. 자동화 시험** — §4 전부

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| 문 손잡이·글러브박스 손잡이·콘솔 탭의 입력 바꾸기 | 사용자 요청은 레버뿐. 기본값으로 지금 동작 유지 |
| 직선 이동 부품 밀기 | D1 |
| 걸림 각도·기준·차선 변경 값 바꾸기 | P5b·P5c 그대로 |
| 휠 자동 해제(방향지시) | 사용자 결정(2026-10-16) — 넣지 않음 |
| 다른 부품 부모(문·거울·휠·노브) | 레버만 |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- 기본값 1축 부품(옵션을 건드리지 않은 것)의 동작·시험은 그대로다. 문 손잡이(P2b·P2c·P2d), 글러브박스(P5a)가 그렇다.
- 잡아서 움직일 때의 걸림 규칙·소리·이벤트·차선 변경·추적 손실은 그대로다. P5b E1–E15, P5c E1–E14의 "잡기" 경로가 그렇다.
- 보이는 크기·자리·poke 버튼은 그대로다.
- **기존 단언을 바꿔도 되는 곳 — 근사 차 레버의 입력 방식을 확인하던 곳만:**
  - `vBoR.P5b.E14E16ActualCarBoundsAndCleanup`·`vBoR.P5c.E13ActualCarPlacementAndBounds`가 근사 차 레버의 손바닥 허용을 보았다면 그 줄을 pinch로 바꾼다.
  - 잘못된 설정 시험(`P5b.E12`, `P5c.E11`)은 "손바닥 끔"에 "pinch 끔·밀기 끔"을 더한다.
  - 시험 안 레버를 손바닥 잡기로 움직이던 곳이 근사 차 레버를 쓰면, 시험 안에서 손바닥을 다시 켜고 보고 ③에 적는다.

## 4. 경계 상황과 그 시험

| # | 상황 | 기대 동작 | 자동화 시험 |
|---|---|---|---|
| E1 | 기본값 1축 부품 | 손바닥만 허용, pinch·밀기 없음(지금 그대로) | 설정 |
| E2 | 근사 차 기어·방향지시 | 손바닥 끔, pinch 켬, 밀기 켬. 실제 SDK pinch로 잡혀 돌고, 손바닥으로는 안 잡힘 | 설정·SDK |
| E3 | pinch로 잡고 위로 → 기준 넘김 → 놓음 / 기준 전에서 놓음 | 다음 걸림으로 / 원래 자리로 돌아감(P5b E2와 같은 시간) | 걸림·각도 |
| E4 | pinch를 펼침(인식기 꺼짐) / 추적 손실 중 SDK가 선택을 끝냄 | 놓음 / 손을 펼 때까지 보유(P2b D7) | 상태 |
| E5 | 검지 끝이 아래에서 레버 아랫면을 넘어 들어와 위로 밂(가운데 → 위 기준 넘김) → 손가락을 뺌 | 레버가 손가락을 따라 올라감(닿은 점이 아랫면 위에 머묾), 위 걸림 확정·소리·이벤트 1번, 빼면 위 걸림에 머묾 | 각도·걸림·이벤트 |
| E6 | 위에서 윗면을 밀어 내림 / 기준 전에서 손을 뺌 | 아래쪽으로 / 원래 걸림 자리로 돌아감 | 각도·걸림 |
| E7 | 방향지시: 아래에서 차선 변경 각도까지만 밀고 뺌 | 차선 변경 이벤트 1번, 가운데로 돌아옴(P5c와 같은 순서·재무장) | 이벤트 |
| E8 | 한 frame에 두 걸림 기준을 넘게 밂 | 순서대로 두 번 확정(P5b E5와 같음) | 걸림 |
| E9 | 축에서 2 cm 안 / 축 방향 bounds 밖 / 두께보다 깊이 통과 / 옆에서 미끄러져 들어옴 / 면 안에서 추적 복구 | 모두 안 밂 | 각도 |
| E10 | 미는 중 같은 손의 다른 점이 반대 면을 넘음 / 두 손이 동시에 밂 | 시작한 점·면만 / 먼저 시작한 손만 | 상태 |
| E11 | 한 손이 레버를 잡은 중 다른 손이 밂 / 미는 손이 그 자리에서 pinch | 밀기 무시 / 잡기로 넘어가고 튐 없음 | 상태·각도 |
| E12 | 휠(다른 grabbable)을 잡은 손의 손가락이 레버 아랫면을 넘음 | 안 밂. 그 손이 휠을 놓은 뒤 다시 들어오면 밂 | 상태 |
| E13 | 미는 중 추적 손실 → 복구 | 밀기 끝(놓은 것과 같이 걸림 자리로), 복구 순간 안 밂 | 추적 |
| E14 | 직선 이동 부품에 `bAllowPush` | 경고 1번, 밀기 무시 | 로그 |
| E15 | 밀기·잡기 덮개 색 | 각각 | 색 |
| E16 | 기존 시험 | §3의 곳 말고 모두 그대로 통과 | 전체 시험 |

## 5. 구현자가 정할 것

- 밀기 계산을 `AVBoRAxisPart` 안에 둘지, 거울·문처럼 따로 된 움직임 구조체로 뺄지(시험하기 쉬운 쪽).
- 밀기 두께 여유 값(자리값), 프로퍼티 이름.
- 회전 방향의 면을 bounds에서 고르는 방법(축 × 레버 방향, 또는 축에 수직인 두 bounds 축 중 짧은 쪽).

## 6. 완료 조건

1. §2의 1–3이 들어 있다.
2. §4 시험이 모두 자동화로 통과한다. 기존 `vBoR.*`도 통과한다(§3에 적은 곳은 새 기대값으로).
3. non-unity(`-Module=vBoR`)·unity 빌드에서 vBoR 경고·오류가 0이다. 전체 시험은 최종 트리로 한 번 돌린다.
4. AGENTS.md 보고 ①–⑤를 커밋 본문과 `HANDOFF.md`에 남긴다.
5. 시험 상태는 `not-tested`다.

## 7. 로컬에서 확인할 것 — 사용자(VR, 끝에 한꺼번에)

1. 기어 레버를 손가락으로 아래에서 밀어 올리고, 위에서 밀어 내린다. 걸림 소리가 나는지 본다.
2. 레버 끝을 pinch로 잡아 위·아래로 옮긴다. 손바닥으로 쥐면 안 잡히는지 본다.
3. 방향지시 레버를 살짝 밀어 차선 변경, 끝까지 밀어 걸림을 본다.
4. 휠을 쥐고 돌릴 때 레버가 저절로 밀리지 않는지 본다(E12의 VR 쪽).
5. 기어 레버 끝 버튼은 그대로 눌리는지 본다.

## 8. 확인한 API 근거

경로: `Source/vBoR/` 아래(`main` @ P3b 머지 뒤 줄 번호. P5l은 이 파일들을 고치지 않음), ISDK는 `Plugins/MetaXRInteraction/Source/` 아래.

| 쓸 것 | 위치 | 쓰임 |
|---|---|---|
| 잡는 방식 허용 | `Interaction/VBoRAxisPart.cpp:193-194` | D1 |
| 잡는 방식·자세 | `OculusInteraction/Public/Interaction/IsdkGrabberComponent.h:167-168`(pinch·palm 자세), `:206`(`IsInputMethodActive`), `:130`(`HasSelectedInteractable`) | D2·D5 |
| 손 rig grabber | `OculusInteractionPrebuilts/Public/Rig/IsdkRigComponent.h:193`(`GetGrabber`) | D5 |
| 손 펼침 인식기 | `Interaction/VBoRHandRigPawn.cpp:42-74`(`ReadPalmGrab`·`ReadPinchGrab`) | D2 |
| 2축 부품의 pinch·palm 처리(본보기) | `Interaction/VBoRTwoAxisPart.cpp:95-115`, `:130-139` | D2 |
| 손 점(손바닥 + 손가락 끝) | `Interaction/VBoRHandRigPawn.cpp:133-154`, `Interaction/VBoRDoorMotion.h`(`FVBoRDoorHand`) | D3 |
| 문 밀기 규칙(crossing·깊이·각도 식) | `Interaction/VBoRDoorMotion.cpp:76-131` | D3 |
| 거울 밀기(점 하나·손바닥 방향 안 봄) | `Interaction/VBoRTwoAxisMotion.cpp:39-93` | D3 |
| 걸림 처리 | `Interaction/VBoRAxisPart.cpp:240-281`(`EvaluateDetent`), `:283-296`(`AcceptTransform`), `:413-419`(놓은 뒤 걸림 자리로) | D4 |
| 잡기·놓기·손 펼침 | `Interaction/VBoRAxisPart.cpp:308-342`, `:355-427` | D2·D5 |
| 잡기 상자 bounds | `Interaction/VBoRAxisPart.cpp:160-178` | D3 |
| 근사 차 레버 | `P3Probe/P3VehicleMock.cpp:166-190` | D7 |
| 기존 시험 | `P3Probe/P5bGearLeverTests.cpp`, `P3Probe/P5cTurnSignalTests.cpp`, `P2Probe/P2bAxisPartTests.cpp` | §3 |

## 개정 이력

| 날짜 | 개정 | 내용 |
|---|---|---|
| 2026-10-17 | 처음 | 작성(사용자 VR·문답: pinch + 밀기, 손바닥 끔, 기어·방향지시 둘 다) |
