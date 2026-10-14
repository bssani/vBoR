# P2d 리뷰 기록

명세: [Docs/Specs/P2d-latched-panel.md](../Specs/P2d-latched-panel.md) · 브랜치: `codex/P2d-latched-panel`

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
| 1 | 2026-10-14 | `1c800b9` 잠금 해제 `Unlock`·쪽별 잡는 자리·안쪽 레버·콘솔 탭 | Claude | **열림 1건(R1-1, 시험만).** 제품 코드는 지적 없음. 빌드·시험 재현: non-unity(`.cpp` 45개)·unity 성공, vBoR 경고·오류 0, `vBoR.*` **136/136**(P2d 12·P2c 13·P2b 13·P2Door 11·P3Flow 18). Codex가 하지 않은 마지막 전체 재실행을 이번에 대신 했다. **토큰 5.51M(캐시 96%), 20분, 하위 에이전트 0, exec 37** |

### 1회차에 확인한 것

- **D1 잠금 옵션:** 저장 이름 `bUseLatch`는 그대로다. 표시 이름은 "닫히면 잠김 — 손잡이·버튼으로만 열기"이고 툴팁이 붙었다. 문 자식의 기본값(켬)도 그대로다.
- **D2 쪽별 잡는 자리:**
  - `IsGrabAreaEnabled` = 그 쪽 사용 옵션 켬 **그리고** 연결한 손잡이 없음. `UpdateGrabAreas`가 충돌·overlap·표시·Palm 입력을 함께 끈다.
  - 포인터 처리(`HandlePointer`)도 같은 조건으로 막는다. 연결한 손잡이에 쓰던 처리를 하나로 합친 것이다.
  - `SyncSettings`가 매 Tick 이 함수를 부른다. ISDK `SetGrabInputMethodAllowed`는 값이 같으면 아무것도 하지 않는다(`IsdkGrabbableComponent.cpp:330-342`). 그래서 버튼 이벤트 안에서 `Unlock` → `SyncSettings`가 불려도 SDK 상태는 바뀌지 않는다.
- **D3 `Unlock`:** `FVBoRDoorMotion::Unlock`으로 갈라냈고, `BeginGrab`도 같은 경로를 쓴다. 그래서 풀림 수·`LastUnlockHandle`·걸쇠 재무장이 손잡이로 풀 때와 같다.
  - 잠금이 꺼져 있거나 이미 풀렸으면 `false`를 돌려주고 아무 일도 하지 않는다(E6).
  - `ApplyState`는 풀림이면 소리와 이벤트만 낸다. 잠김이 바뀌지 않으므로 `CancelSelections`도 부르지 않는다. E5가 실제 SDK 잡기를 유지한 채 취소 0번을 확인한다.
- **D4 경고:** 열 수단이 없는 조건이 "쪽마다 (사용 옵션 또는 연결한 손잡이)가 둘 다 없음"과 같다. E7은 `AddExpectedMessagePlain` 기본값으로 정확히 1번을 본다(`AutomationTest.h:1815`).
- **D5 안쪽 레버:**
  - 바깥 손잡이를 거울로 비춘 꼴이다(같은 크기, 축 X, `bReverse` 없음 = 아래 끝이 차 +Y 실내 쪽으로).
  - 자리 (−50, −83.5, 85) cm는 문 로컬 (85, −9, 50) — 옛 `AVBoRHingedDoor` 안쪽 상자 자리 그대로다. 문 안쪽 면(차 Y −87.5)에서 2 cm 떨어져 있다.
  - E8·E9·E12가 레버 방향, 풀림 손잡이 = 안쪽, 같은 손으로 밀어 열기·끌어 닫기, 11→18→20을 본다.
- **콘솔 뚜껑:** 잠금 켬, 아래 잡는 자리 끔. 탭은 앞 가장자리(차 X −10 cm)에 4 cm 튀어나와 있고 뚜껑 `HingePivot`의 자식이다.
  - 탭 연결(`ConsoleLid->OutsideAxisHandle`)은 뚜껑 `FinishSpawning` 전에 한다. 그래서 BeginPlay 경고가 나지 않는다.
  - `ConnectDoor`는 필드만 넣기 때문에, 탭의 `FinishSpawning`이 뒤에 와도 연결이 끊기지 않는다(`VBoRAxisPart.cpp:252-255`).
- **범위:** 레벨·에셋·P3 단계 데이터와 규칙은 그대로다. 명세 밖으로 커진 것이 없다.
- **명세 빈틈(Claude):** §3에 "바뀌는 시험"으로 P2c E13만 적었다. 그런데 P2b P3 시험(`P2bAxisPartTests.cpp:405`)도 같은 콘솔의 잠금 끔을 보고 있었다. Codex가 D1에 맞게 고치고 보고 ③에 적었다. 맞는 판단이다.

## 항목

### R1-1 실제 근사 차 콘솔 구성이 시험에 묶여 있지 않음 — 열림

**문제**
- 이 명세의 핵심은 사용자가 VR에서 지적한 "닫힌 콘솔이 밀리고 아래로 잡힘"을 근사 차에서 막는 것이다. 그런데 실제 `Car->ConsoleLid` 구성을 확인하는 단언이 없다.
- `P3VehicleMock.cpp:102`(`bUseInsideGrabArea = false`)나 `:109`(`OutsideAxisHandle = ConsoleTab`)를 지워도 실패할 시험이 없다. 시험 코드를 읽고 확인했고, 실제로 지워 돌려 보지는 않았다.
  - E2·E3는 시험용 판 `P2dLid`(`P2dLatchedPanelTests.cpp:34-41`)가 같은 값을 직접 넣고 확인한다.
  - 탭도 복사본이다(`P2dCopyPart`, `:116`).
  - E10은 문 안쪽 상자는 실제 차로 보지만, 뚜껑은 탭이 붙어 있는지만 본다(`:241`).
- `P2cHingedPanelTests.cpp:284` "Actual P3 lid accepts edge grab"은 `Motion.BeginGrab(…, Handle 0, …)`을 바로 부른다. 지금 실제 뚜껑에는 이 길이 없다. 바깥 자리는 탭이 연결돼 꺼졌고, 안쪽 자리는 옵션으로 꺼졌다. 그런데도 이름은 실제 P3 동작처럼 읽힌다.

**제안**
- P2c E13(실제 차를 이미 띄움)에 실제 `Car->ConsoleLid`로 단언을 더한다:
  - `OutsideAxisHandle == Car->ConsoleTab`, 탭의 `LinkedDoor == ConsoleLid`(`FP2dTestAccess` friend로 읽음)
  - 바깥·안쪽 상자 `NoCollision`·overlap 끔·`OutsideGrab`/`InsideGrab` Palm 불허. E10의 문 쪽 단언과 같은 꼴이다.
  - `InsideGrab`에 선택 이벤트를 넣어도 잠김 그대로, 풀림 0(E2를 실제 뚜껑에)
- 실제 탭 → 실제 뚜껑 넘김은 E12가 실제 문 레버에 한 방식(`P3FlowTests.cpp:138-145`, `FP2dTestAccess::Begin/Move` → `Transfer`)을 그대로 쓴다.
  - 확인: 풀림 1, `LastUnlockHandle == 0`, 같은 손 `GrabHand`
  - 그 뒤 `FP2dTestAccess::Advance`로 손을 들면 뚜껑이 열린다.
- `:284` 줄은 이 탭 넘김으로 바꾼다. 아니면 Motion을 직접 부른다는 것이 드러나게 이름을 바꾼다(예: "Motion clamps 100 deg").
- 제품 코드는 바꾸지 않는다.

**확인할 시험**
- `vBoR.P2c.P3.E13ConsolePushAndGrab`(또는 새 `vBoR.P2d.P3.*`): 위 단언. 확인 방법은 `:102`나 `:109`를 잠시 지우면 실패해야 한다(커밋하지 않음, 한 번만).

## 참고 (항목 아님 — VR에서 볼 것)

- **콘솔 탭은 잡는 손 위치에 따라 잘 안 열릴 수 있다.**
  - 탭 축에서 탭 중심까지 2 cm다. ISDK 회전 변환기는 축 둘레의 손 각도로 값을 낸다.
  - 손바닥을 탭 **위에** 얹고 위로만 들면 각이 조금만 변한다. 예: 축 앞 2 cm·위 4 cm에서 5 cm 들면 약 14°인데, 열림 문턱은 24°(30°의 80%)다.
  - 앞에서 손가락을 걸어 들면 잘 열린다(축 앞 5 cm에서 3 cm 들면 약 31°).
  - 문 바깥 손잡이도 같은 구조인데 VR에서 금방 열렸다(0.93–1.00). 그래서 확인만 한다.
  - 안 열리면 탭을 위로 미는 직선(`Movement = Translate`, `Axis = Z`, `MaxTravelCm` 3–4 cm)으로 바꾸는 것을 먼저 본다. 손 위치와 관계없이 든 높이만 본다.
- **안쪽 레버 자리:** 운전자 눈보다 뒤로 30 cm·바깥으로 46.5 cm·아래로 30 cm다(옛 안쪽 상자 자리). 앉아서 손이 어깨 뒤로 가야 하면 `InsideHandlePivotCm`을 앞으로 옮긴다(자리값). 실제 차량 데이터가 오면 그 값으로 바꾼다.
- 명세 §7 순서대로 본다: 콘솔(닫힌 채 위·아래 밀기 → 탭 → 같은 손으로 들기 → 눌러 닫아 다시 잠김), 안쪽 문(레버로 풀고 밀어 열기 18, 레버 당긴 채 끌어 닫기 11).

## 질문

(없음)
