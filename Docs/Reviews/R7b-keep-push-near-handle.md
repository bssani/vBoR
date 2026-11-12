# R7b 손잡이 근처 — 밀던 손 이어가기 — 리뷰

## 1회차 (2026-11-12, `516e00d`) — 통과

**판정:** 명세 D1–D5와 완료 조건 충족. 범위 밖 변경 없음(제품 변경은 `VBoRDoorMotion.h/.cpp`뿐, 기존 시험 변경은 R7 E4 대체 하나). 고칠 항목 없음.

**확인한 것:**
- 빌드·시험 로그: `R7b-non-unity-UBT.log`·`R7b-unity-UBT.log` `Result: Succeeded`, `R7b-Automation.log` 성공 716·`TEST COMPLETE. EXIT CODE: 0`.
- D1: 이어감은 그 손의 밀기로 각도가 실제로 바뀐 프레임에만 켜진다(`MoveTo` 뒤 `AngleDeg != Before`, `PushHand == H` — 래치로 닫힌 프레임은 `PushHand`가 비워져 켜지지 않음). 우회 조건은 `PushSide[Hand] == Face`까지 본다.
- D2: 관성이 이기는 분기는 이어감을 건드리지 않는다. E2가 "관성이 앞섬 → `PushHand` 비워짐 → 범위 안에서 따라잡음"을 그대로 재현한다.
- D3: ① 면 이탈 — 기존 `PushSide` 해제 자리 + **밀던 손이 한 프레임에 완전히 빠지는 경우**(그때는 `PushHand == H`라 기존 해제가 안 걸림)도 그 프레임에 끈다. 다음 프레임에 바로 다시 닿는 손을 새 밀기로 본다 — 명세에 없던 빈틈을 잘 막았다. ② 잡기 ③ 추적 끊김(프레임 시작에 손마다) ④ 래치·리셋 모두 끈다.
- 잡기 시작이 **두 손 모두**의 이어감을 끈다(명세는 "그 손"). 기존 `BeginGrab`이 이미 두 손의 `PushSide`를 비우므로(`VBoRDoorMotion.cpp:50`) 우회 조건(`PushSide == Face`)이 어차피 성립하지 않는다 — 동작 차이 없음.

### 참고 1 — 다른 손이 미는 프레임의 `NearHandle` 기록 (명세 빈틈, Claude)

- **사실:** 한 손이 이미 문을 민 프레임에는 지금까지 다른 손을 판정하지 않고 기록도 `Count`로 비웠다. 명세 E6("오른손 `NearHandle` 1")을 맞추려고, 다른 손이 금지 범위 안이면 `FindPush`를 불러 `NearHandle`만 기록하게 바뀌었다(`VBoRDoorMotion.cpp` `if (bFound) { … RecordIgnore(H, Reason == NearHandle ? … ) }`). 판정 결과는 쓰지 않고, `FindPush`가 바꾸는 손별 값(`bContactNear`·`ContactFace`·`PalmAngleDeg`)은 바로 되돌린다. 밀기 우선순위·`PushStarts`는 그대로다.
- **영향:** 기록(`Ignored contacts:`의 near handle 수)만 달라진다 — 두 손 중 한 손이 미는 동안 다른 손이 손잡이 근처에 있으면 이제 센다. 다른 무시 이유는 여전히 세지 않는다.
- **판단:** 명세 §2 "기록 의미 변경 하지 말 것"과 E6이 서로 어긋난 것은 명세 쪽 실수다. 동작에 영향이 없고 E6이 요구한 것이라 **그대로 둔다.** 명세 E6에 이 기록 변화를 적는다(Claude 명세 개정).

### 다음

- `main` 머지: 리뷰 통과. 50% 동결(11/16) 전에 넣을지는 사용자가 정한다(Quest T-03 확인 전 `not-tested`).
- 사용자: 문 BP `No push near handle` 2–3 cm 조정(상황 a), Quest에서 명세 §7 ①–③.
