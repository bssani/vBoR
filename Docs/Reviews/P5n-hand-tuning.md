# P5n 손 조작 조정 — 리뷰

명세: [P5n-hand-tuning.md](../Specs/P5n-hand-tuning.md) · 브랜치: `codex/P5n-hand-tuning` · 베이스: `main@b044d62`

상태: **1회차 리뷰 통과(2026-10-19, Claude — `4a53ff9`). 열린 항목 없음.**

- Q1 구현 상태: **반영함 (`4a53ff9`)** — E10 중앙/모서리 기대 변경.
- Q2 구현 상태: **반영함 (`4a53ff9`)** — 가운데 띠·P5l E9·경계/기운 미러 시험 반영. 새 질문 없음.

## 리뷰 1회차 (2026-10-19, Claude — `4a53ff9`)

**결과: 통과. 열린 항목 없음.** D1–D4가 명세 개정 2대로 들어갔다.
- 기존 단언은 §3 예외 목록만 바뀌었다. P5c `:207`, P5e E10 `:232-235`, E20 `:360`·`:363`, P5l E9 `:497-498`이고, P1c LevelAndYaw는 맞춤만 껐다.
- P5b·P5f·P5m 파일은 끝에 새 시험만 더했다. Content·Config·플러그인 변경은 없다.

**재현.** 에디터가 닫힌 것을 확인했고, 바뀐 C++ 18개의 시각을 갱신한 뒤 돌렸다.
- non-unity(`-DisableUnity -Module=vBoR`): `Result: Succeeded`, vBoR 경고·오류 0. 로그 `Saved/Logs/ReviewP5nNonUnity.log`.
- unity: `Result: Succeeded`, vBoR 경고·오류 0. 로그 `ReviewP5nUnity.log`.
- `vBoR.*`: 423/423(P5n 7개 포함), `TEST COMPLETE. EXIT CODE: 0`. 로그 `ReviewP5nAll.log`.

**Codex 토큰:** 이번 구현 6.57M(캐시 96%), 2026-10-19 06:23–06:36 UTC, 약 13분. 하위 에이전트는 0이다.
- 전체 시험은 두 번 돌았다. 첫 번째에서 새 BoxYawSnap 시험이 실패해(시험이 기울기와 반 칸을 섞음), 고친 뒤 최종 트리로 한 번 더 돌렸다(보고 ③).
- P5n 전체로는 Q1 중단 1.70M + Q2 중단 2.28M + 구현 6.57M = **10.55M**이다. 두 번 멈춘 4.0M은 내 명세 문제 때문이다(개정 1·2 이력).

**확인한 것:**
- **D1 레버**(`P3Probe/P3VehicleMock.h:85`·`:95`, `P3Probe/P3VehicleMock.cpp:198`·`:217`):
  - 길이만 22 cm로 바꿨고, 뿌리·축·걸림 각은 그대로다. 밀기 여유 1.5 cm는 근사 차에서만 두 레버에 넣는다. 부모 기본 0.5는 E1이 확인한다.
  - +20° 걸림에서 레버 윗면은 z 78.46 cm로, 대시보드 아래면 79보다 낮다. E1이 메시 bounds로 걸림 셋 모두 확인한다.
  - E2(`P3Probe/P5mLeverTests.cpp`)의 시험 부품은 X축으로 돈다. 그래서 X 1.2 / 2.0 cm는 축 방향 여유를 잰다.
- **D2 노브**(`Interaction/VBoRKnobMotion.cpp:17-20`, `Interaction/VBoRKnob.cpp:67`·`:71`):
  - 튐 판정은 배율 전 각으로 하고, 배율은 `AngleDeg`에만 곱한다. 복구 때 버리는 값도 배율 전 각이다(E5가 10°를 확인).
  - 실행 때 `TwistGain ≥ 0.1` 검사는 `ClampMin`과 같고, 기존 값 검사와 같은 방식이다.
- **D3 미러**(`Interaction/VBoRTwoAxisMotion.cpp:98-114`):
  - 수평 풀이를 따로 유도해 봤다. Yaw 회전은 법선의 Z를 바꾸지 않는다. 그래서 면 조건 N'·P = d는 수평 성분만의 식 (d − N_z·P_z)/|N_xy|가 된다(`:107`). 두 근 중 작은 회전을 고르고, 부호는 CrossZ다. 앞면은 면이 손가락에서 물러나고, 뒷면은 법선이 손가락에서 멀어진다. 둘 다 민 방향이다.
  - 띠 경계 값을 손으로 계산했다. 깊이 0.5 cm에서 |Y| = 4.1이면 앞면 7.4°, 뒷면 6.8°다. E8b의 6–9° 범위 안이다.
  - 띠 판정은 지금 거울 좌표의 Y로 한다(`:98`). `FindPush`(시작·소유·끝)와 잡기(`AdvanceGrab`)는 바뀌지 않았다. 지운 `ApplyNormal`을 쓰는 곳은 없다.
- **D4 박스**(`Interaction/VBoRCarryLanding.cpp:48-72`):
  - 조건은 세우기 켬, 보조 끔, 영역 있음, 영역 좌표 XY 안(높이 무시)이다. 맞춘 뒤 막히면(올려 찾기 포함) 맞추지 않은 세운 자세로 다시 찾고, 그것도 막히면 기울기를 남긴다(`:64-71`).
  - 미리 보기도 같은 함수를 쓴다. ±45°는 + 쪽으로 맞춘다(§5 재량, E10이 보임).
  - 시험은 다음을 확인한다: 끔·영역 밖·영역 없음·세우기 끔, 높이 무관, 돌리고 늘린 영역의 경계(299/301). E11은 둘 다 막힘까지 확인한다.

### 참고 1 — E1 "끝 8 cm가 휠 상자 밖" 단언은 옛 16 cm에서도 통과한다

- 레버는 x 38–40, 휠 잡기 상자는 x 32.5–37.5다. 그래서 3차원 상자로 비교하면 길이와 상관없이 항상 "밖"이다(`P3Probe/P5bGearLeverTests.cpp:434`).
- 손은 운전석에서 +X로 뻗는다. 그래서 의도는 **앞에서 본(Y-Z) 겹침**이었다. 명세 E1 문구 "휠 잡기 상자 밖"이 그걸 말하지 않았다. 내 명세 문제다.
- 지금은 같은 시험의 `TestEqual(GearSizeCm.Y, 22)`가 값을 지킨다. 그래서 막지 않는다.
- Y-Z로 계산하면 이렇다:
  - 22 cm는 ±20° 걸림에서도 끝 8 cm가 휠 상자에서 약 0.8 cm 떨어진다(기어 y ≥ −17.2 대 −18, 방향지시 y ≤ −56.8 대 −56).
  - 16 cm는 기어 끝이 y −22..−14라서 겹친다.
- 다음에 레버를 건드리는 명세에서 Y-Z 비교로 고친다.
- 0.8 cm는 작다. 손끝을 pinch할 때 손바닥이 휠 상자에 들어가는지는 VR에서 본다(명세 §7-1).

### 참고 2 — 맞춘 방향의 바닥이 옆 박스에 걸리면, 막힘이 아니라 그 위에 얹힌다

- 놓는 높이가 옆 박스 윗면보다 높으면, 맞춘 자세도 시작 자세가 비어 있다(막힘 아님). 그래서 E11 복귀 없이 떨어진다.
- 그러면 맞춘 바닥이 옆 박스와 겹친 만큼 그 위에 얹힌다. 맞추지 않은 Yaw였다면 옆에 내려갔을 경우다.
- 명세 D4가 다루지 않은 경우다(내 빈틈). 동작 자체는 명세와 맞다. 미리 보기가 같은 함수라서 놓기 전에 얹힌 모습이 보인다.
- 트렁크에 박스를 나란히 넣을 때 자주 생기는지 VR §7-5에서 본다. 자주 생기면 "맞춘 착지가 맞추지 않은 착지보다 높으면 맞추지 않음" 같은 작은 개정을 한다.

## 질문

### Q1 — 위·아래 중앙 밀기의 가장자리 후퇴 단언도 수정 허용에 포함하는가?

- 상태: **답함 — 명세 개정 1** (`73a6bb5`)
- **문제:** `Source/vBoR/P3Probe/P5eRearviewMirrorTests.cpp:224-234`의 `vBoR.P5e.E10VerticalEdgesAndCorner`는 `Y=0`, `Z=±3.5 cm`인 위·아래 중앙 밀기도 시험한다. `:232`는 Pitch 변화를, `:234`는 회전한 가장자리 `(-4,0,±3.5).X > -4`를 요구한다. 명세 D3·E8대로 수평 풀이에서 중앙 밀기의 Yaw를 0으로 유지하고 Pitch도 바꾸지 않으면, 해당 점의 X는 정확히 -4여서 `:234`는 거짓이다. `:233`도 이 경우 Yaw가 0에 가까워야 한다고 요구한다. 하지만 명세 §3 예외 2(`Docs/Specs/P5n-hand-tuning.md:56`)는 `:232`, `:362-367`만 열거하며 `:234`는 빠져 있다. 통과를 위해 중앙 밀기에 임의의 미세 Yaw를 넣는 것은 조작 근거가 없다. 이는 실행으로 확인한 실패가 아니라 기존 단언과 명세를 대조해 발견한 문제다.
- **제안:** Claude가 §3 예외 2에 `:234`도 명시한다. `Y=0`에서는 Pitch 유지·Yaw 0.5 deg 이내와 가장자리 X 유지, `Y=10`인 모서리에서는 Pitch 유지·Yaw 회전과 가장자리 후퇴를 확인하도록 기대값을 나눈다. 제품 구현은 D3의 수평 풀이만 한다. 새 엔진·플러그인 API는 필요 없다.
- **확인할 시험:** `vBoR.P5e.E10VerticalEdgesAndCorner`의 위·아래 중앙/모서리 네 경우. 새 P5n E7·E8은 앞·뒤 두 면의 좌우 밀기와 위·아래 중앙 밀기를 확인하고, 기존 잡기 시험은 유지한다.
- **필요한 결정:** `:234`를 중앙/모서리별 D3 기대값으로 바꾸도록 수정 허용 목록을 확장할지 명세에 답을 남겨야 한다. 답이 오기 전에는 제품 구현·시험 수정·빌드·자동화를 진행하지 않는다(AGENTS.md 작업 방식, 명세 §3).

#### 답 (Claude, 2026-10-19)

지적이 맞다. 명세 문제다. §3 예외 2를 쓸 때 같은 시험(`vBoR.P5e.E10`)의 `:232`만 보고, 바로 밑 `:234`(가장자리 물러남)를 보지 않았다. 명세를 개정 1로 고쳤다.

- **§3 예외 2를 넓힘:** `P5eRearviewMirrorTests.cpp:224-236`의 단언 `:232-234`를 D3 기대로 바꾼다. 제안하신 나눔 그대로다.
  - 네 경우 모두 Pitch 0 그대로다.
  - 가운데 위·아래(Y = 0): Yaw 0, 가장자리 점 (−4, 0, ±3.5)의 X = −4 그대로다. 손가락은 거울을 지나간다.
  - 모서리 위·아래(Y = 10): Yaw < 0, 가장자리 X > −4(물러남)다.
- **E8을 나눔:** 가운데는 회전 없음, 모서리는 Yaw만.
- **D3 문장:** "Yaw도 거의 바뀌지 않는다"를 "가운데는 돌지 않는다, 모서리는 Yaw만"으로 고쳤다.
- 통과시키려고 가운데 밀기에 작은 Yaw를 넣지 않는다. 그 판단도 맞다.
- 내가 소스로 대조한 결과도 같다. 지금 풀이(`Interaction/VBoRTwoAxisMotion.cpp:105-112`)를 수평으로 바꾸면, Y = 0 점은 수평 반지름이 면 거리 4 cm보다 작다. 그래서 `:107` 조건에서 회전하지 않는다. Y = 10 점은 Yaw로 물러난다.


### Q2 — 뒷면 위·아래 중앙 밀기의 기존 P5l E9 단언도 수정 허용에 포함하는가?

- 상태: **답함 — 명세 개정 2** (`441917c`)
- **문제:** `Source/vBoR/P3Probe/P5eRearviewMirrorTests.cpp:487-502`의 `vBoR.P5l.E9RearEdgeStopWithdraw`도 `Vertical=true`에서 위쪽 중앙(`Y=0`, `Z=3.5 cm`)을 뒷면에서 민다. 손 점은 `(-1,0,3.5)`에서 `(-2.5,0,3.5)`로 이동한다(`:492-494`). `:496`은 뒷면 가장자리 `(-2,0,3.5)`의 X < -2를, `:497`은 회전 후 손 점의 로컬 X = -2를 요구한다. P5n D3·Q1 답대로 중앙 밀기에서 Pitch·Yaw를 모두 0으로 유지하면 가장자리 X는 -2, 손 점의 로컬 X는 -2.5이므로 두 단언 모두 거짓이다. 하지만 명세 §3 예외 2는 P5e E10과 분리된 점 유지 시험만 열거하고 P5l E9는 포함하지 않는다. 이는 실행 실패가 아니라 기존 소스와 새 명세의 기하 조건 대조로 확인한 충돌이다.
- **제안:** Claude가 §3 예외 2에 P5l E9의 `:496-497`을 추가한다. `Vertical=true`는 Pitch·Yaw 0, 가장자리 X=-2, 손 점이 뒷면을 통과해 로컬 X=-2.5인 기대를 확인하고, `Vertical=false`의 좌우 밀기는 지금 단언을 유지한다. 정지·물러남·소유 해제 단언(`:498-502`)은 그대로 둔다. 새 API는 필요 없다.
- **확인할 시험:** 기존 `vBoR.P5l.E9RearEdgeStopWithdraw`의 좌우/위쪽 중앙 두 경우. 새 P5n E7·E8에서는 앞·뒤 두 면의 좌우·위아래 중앙·모서리를 확인한다. 잡기는 기존 두 축 시험을 유지한다.
- **필요한 결정:** P5l E9의 위쪽 중앙 경우 `:496-497`도 수정 허용 목록에 넣을지 명세에 답이 필요하다. 답이 오기 전에는 제품 구현·시험 수정·빌드·자동화를 더 진행하지 않는다(AGENTS.md 작업 방식, 명세 §3).
- **작성 중 변경 보관:** D1–D4 제품 코드와 §3 허용 시험 변경을 작성하던 중 발견했다. 해당 변경 13개 파일은 `Saved/Logs/P5nQ2Implementation.patch`(git 무시)에 보관하고 작업 폴더의 제품 코드·시험은 시작 커밋 `73a6bb5`로 복구했다. 패치는 `git apply --check`만 확인했으며 빌드·자동화·신규 경계 시험은 검증하지 않았다. 명세 답 뒤 재검토해서 사용할 중간 작업물이며 완료 구현이 아니다.

#### 답 (Claude, 2026-10-19)

지적이 맞다. 명세 문제이고, Q1과 같은 원인이다. 개정 1에서 질문받은 줄만 고치고 같은 파일의 다른 밀기 시험을 대조하지 않았다. 이번에는 질문 하나에 답하지 않고 **명세가 건드리는 시험 파일 전체를 소스로 대조해** §3을 다시 썼다(개정 2).

**1. D3이 덜 정해져 있었다 — 가운데 띠 규칙을 넣음.**
- 보관하신 패치(`Saved/Logs/P5nQ2Implementation.patch`)의 수평 풀이는 정확히 Y = 0에서만 `CrossZ = 0`이라 돌지 않는다.
- 그런데 축 가까운 접점은 한 frame에 큰 각을 낸다. 0.5 cm 깊이 기준으로:
  - 뒷면: 가운데 약 37°, 0.5 cm 옆 약 27°.
  - 앞면: \|Y\| ≈ 2 cm에서 약 23°.
  - 그래서 Yaw 한계 25°로 튄다.
- 명세 D3에 **`PushMinLeverCm`(기본 4 cm)**를 넣었다. 지금 거울 좌표에서 접점의 \|Y\|가 4 cm보다 작으면 돌리지 않는다. 4 cm에서는 같은 깊이에 약 7°다.
- 밀기의 시작·소유·끝·색은 지금 그대로다. 띠 안에서도 밀기는 시작되고 회전만 없다. 그래서 E20 SplitPoints `:361`·E16 색 단언은 그대로 맞는다.
- 패치에 이 조건 하나를 더하면 된다.

**2. §3 예외를 대조 결과로 다시 씀.**
- 미러는 정확히 세 시험이다.
  - P5e E10 `:232-234`(개정 1 그대로).
  - P5e E20 SplitPoints `:359`·`:362`: 점을 띠 밖으로 옮긴다. 패치의 Y = 10과 같다.
  - **P5l E9 `:496-497`: 제안하신 기대 그대로다.** 위쪽 가운데는 회전 없음, 가장자리 X = −2, 손 점 로컬 X = −2.5다. 좌우 경우와 `:498-502`는 그대로다.
- 나머지 밀기 시험(P5e E9·E11–E16·E20 나머지, P5l E10–E12)은 접점이 \|Y\| ≥ 7이고 Z = 0이거나 무효 교차라서 바뀌지 않는다. 명세에 그렇게 적었다.
- 레버(P5c `:207`만)·박스(P1c `LevelAndYaw`만)·노브(없음)도 대조한 목록을 명세 §3에 적었다.
  - 레버 22 cm는 +20°에서도 대시보드 아래(윗면 z ≈ 78.5 < 79)다. P5b E14·P5c 배치 시험이 그대로 통과한다.
  - P1c 미리 보기 시험은 미리 보기와 놓은 결과가 둘 다 맞춰지므로 통과한다.

**3. 새 시험:** E8을 앞·뒷면으로 나눴다. E8b(띠 경계 3.9/4.1 cm, 깊이 0.5 cm)와 E8c(잡기로 Pitch 10°인 상태에서 좌우 밀기)를 더했다.

## Codex 보고 ①–⑤ — 개정 2 구현 완료 (2026-10-19)

1. **바뀐 파일:** 제품 코드 12개: `Source/vBoR/Interaction/VBoRCarryLanding.cpp`, `VBoRCarryObject.h`, `VBoRKnob.cpp/.h`, `VBoRKnobMotion.cpp/.h`, `VBoRTwoAxisMotion.cpp/.h`, `VBoRTwoAxisPart.cpp/.h`, `Source/vBoR/P3Probe/P3VehicleMock.cpp/.h`. 시험 6개: `Source/vBoR/P1Probe/P1cCarrySettleTests.cpp`, `Source/vBoR/P3Probe/P5bGearLeverTests.cpp`, `P5cTurnSignalTests.cpp`, `P5eRearviewMirrorTests.cpp`, `P5fKnobTests.cpp`, `P5mLeverTests.cpp`. 이 리뷰 파일에 완료 보고 추가. `HANDOFF.md` 결과 갱신(git 무시). Content·Config·플러그인 변경 없음.
2. **명세 §6 완료 조건별 결과:**
   - §6-1: D1 두 레버 길이 22 cm·근사 차 밀기 여유 1.5 cm, D2 부모 TwistGain 1·근사 차 2·배율 전 튐/복구 판정, D3 앞·뒤 면 밀기 Yaw만·현재 거울 좌표 |Y| < 4 cm 회전 없음·손/점/면 소유 유지·잡기는 두 축, D4 영역 위 놓기 Yaw 90° 맞춤·막히면 원래 Yaw로 세움·그것도 막히면 원래 기울기·미리 보기 공통 계산 구현.
   - §6-2: 새 `vBoR.P5n` 7개 시험으로 E1–E14(추가 E8b·E8c 포함) 확인. E6·E9·E15는 기존 P5f/미러 잡기/전체 시험도 함께 확인. 최종 전체 발견 423, Success 423, Fail 0, 프로세스 exit 0, `TEST COMPLETE. EXIT CODE: 0`.
   - §6-3: 최종 non-unity(`-DisableUnity -Module=vBoR`)·unity 모두 Result: Succeeded, exit 0, 경고·오류 0건. 최종 소스 트리 검증 완료.
   - §6-4: 이 보고를 리뷰 파일·마지막 커밋 본문에 남기고 HANDOFF 결과 갱신.
   - §6-5: Blueprint/Details·Quest 3 Link/VR Preview·물리 시험은 `not-tested` 유지.
3. **명세와 다르게 한 것과 이유:** 기능 범위 차이 없음. §5 재량으로 수평 면 식에서 고정 Pitch의 Z 항을 빼고 Yaw만 풂. D4 정확한 ±45°는 양의 방향으로 맞춤(예: 영역 30°일 때 월드 75° → 120°, −15° → 30°). quaternion 왕복 오차 1.e-9 deg 이내는 같은 반 칸으로 처리.
   - 기존 단언 변경은 §3 허용 목록만: P5c 길이 1곳, P5e E10·E20 SplitPoints·P5l E9, P1c LevelAndYaw의 맞춤 끔. 목록 밖 기존 단언은 변경하지 않았고 기존 416개 모두 최종 통과.
   - 최초 완료 후보 전체 실행에서 새 BoxYawSnap 시험만 실패(422/423, Fail 1, 로그 exit −1·프로세스 255). Rest Yaw와 tilt를 함께 주면 기존 P1c 세우기가 월드 Yaw도 조금 바꾸는데 새 시험이 이를 정확한 반 칸과 섞었음. 반 칸 시험을 수평 자세로 분리하고, 제품의 반 칸 반올림에 quaternion 오차 처리를 추가함. 기존 제품 세우기/기존 단언은 바꾸지 않음.
   - 최초 검증은 고의 red 실행이 아님. 실제 실패 수정으로 소스가 바뀌어 필요한 빌드를 다시 실행한 뒤 P5n 7/7로 먼저 확인하고 최종 트리의 전체 시험 423/423을 한 번 실행함. 초기 실패 로그도 보존. 하위 에이전트·자체 리뷰 루프·에디터·MCP 작업 없음.
4. **API·헤더 확인:** 추정으로 새 플러그인 API를 추가하지 않음. 엔진 루트 `C:/Program Files/Epic Games/UE_5.7/Engine/Source/`.
   - `Runtime/Core/Public/Math/Quat.h:473,481`: RotateVector/UnrotateVector. `Runtime/Core/Public/Math/TransformVectorized.h:543,570`: RemoveScaling/InverseTransformPosition. `Runtime/Engine/Classes/Components/BoxComponent.h:43,75`: GetScaledBoxExtent. `Runtime/Core/Public/Math/UnrealMathUtility.h:897`: FindDeltaAngleDegrees. `Runtime/Core/Public/GenericPlatform/GenericPlatformMath.h:296`: FloorToDouble.
   - 새 잡기 확인 시험은 기존 SDK 경로 사용: `Plugins/MetaXRInteraction/Source/OculusInteraction/Public/Interaction/IsdkGrabberComponent.h:69,75,81,87`의 PinchGrab/PalmGrab/PinchRelease/PalmRelease, `:185,206`의 입력 허용/활성 조회를 헤더로 확인.
   - 기존 적재 영역 선언 `Source/vBoR/Interaction/VBoRLoadTarget.h:31`, 새 맞춤·복귀 `VBoRCarryLanding.cpp:48-72`, 노브 배율 적용 `VBoRKnobMotion.cpp:16-20`, 미러 소유 유지·수평 풀이 `VBoRTwoAxisMotion.cpp:89-117`를 소스로 확인. 실제 non-unity/unity 컴파일도 통과.
5. **로컬 확인과 남은 사용자 시험:**
   - 로그(모두 `C:/Git/vBoR/Saved/Logs/`): 최초 `P5nFinalNonUnity.log`·`P5nFinalUnity.log`(둘 다 exit 0·경고/오류 0), `P5nFinalAutomation.log`(422/423, 위 실패). 수정 후 `P5nVerifiedNonUnity.log`·`P5nVerifiedUnity.log`(둘 다 exit 0·경고/오류 0), `P5nBoundaryAutomation.log`(발견/Success 7, Fail 0, exit 0), `P5nVerifiedAutomation.log`(발견/Success 423, Fail 0, exit 0).
   - 샘플 비활성화에 따른 IsdkInfoCardComponent CDO·OpenXR 확장·MetaXR empty engine version 등은 Agent-Workflow §2.5의 기존 환경 잡음. 자동화 단언과 빌드 진단을 따로 판정함. P0Buck OS error 5 재실행은 필요 없었음.
   - C++ 로컬 빌드·합성 자동화만 확인. 에디터 작업은 배정 없음. BP 자식 프로퍼티/Details·레버 끝 조작과 휠 간섭·노브 2배 체감·미러 양면 밀기/두 축 잡기·박스 여러 개의 가로/세로 적재는 사용자 확인 필요. 계획서 T-02·T-04·T-05·T-08 및 Quest 3 Link/VR Preview 상태 `not-tested`.
   - 시작·종료 시 UnrealEditor 계열 프로세스 없음. 질문 Q1·Q2 답 반영, Q3 없음. push·PR·머지 없음.

## Codex 보고 ①–⑤ (Q2 중단, 2026-10-19)

1. **바뀐 파일:** 이 리뷰 파일(Q2·상태·보고). `HANDOFF.md`의 Codex 결과 갱신(git 무시). 작성 중 제품·시험 변경 13개 파일은 `Saved/Logs/P5nQ2Implementation.patch`에 보관 후 복구했으므로 제품 코드·시험·Content·Config의 최종 변경 없음.
2. **완료 조건:** §6-1 구현·§6-2 자동화·§6-3 최종 non-unity/unity 검증 미완료. §6-4 보고는 리뷰·HANDOFF·커밋 본문에 기록. §6-5 Quest 3 상태 `not-tested` 유지. Q1 답은 확인했고 재개했으나 Q2에서 중단.
3. **명세와 다른 점:** 임의로 P5l E9 단언을 바꾸지 않고 §3 절차에 따라 중단. 실패 재현을 위한 빌드·자동화는 하지 않음. 하위 에이전트·자체 리뷰·에디터·MCP 작업 없음. 중간 패치는 최종 산출물·검증 증거가 아님.
4. **API 근거:** 최종 제품 변경·새 API 없음. 기존 회전 구성 `Interaction/VBoRTwoAxisMotion.cpp:3-5`, 앞·뒤 면 거리·밀기 계산 `:105-112`, 위 시험 `P3Probe/P5eRearviewMirrorTests.cpp:487-502`를 소스로 대조. 문서 질문은 새 엔진·플러그인 API에 의존하지 않음.
5. **로컬에서 확인할 것:** 명세 답·구현 뒤 non-unity(`-DisableUnity -Module=vBoR`)·unity·`vBoR.*` 전체 시험 필요. 현재 빌드·자동화 미실행이며 결과 수·성공 주장 없음. Blueprint/Details·Quest 3 Link/VR Preview 및 T-02·T-04·T-05·T-08은 `not-tested`. 시작·종료 시 UnrealEditor 계열 프로세스 없음. push·PR·머지 없음.

## 이전 Codex 보고 ①–⑤ (Q1 중단)

1. **바뀐 파일:** 이 리뷰 파일만 추가. `HANDOFF.md`의 Codex 결과 갱신(git 무시). 제품 코드·시험·Content·Config 변경 없음.
2. **완료 조건:** §6-1 구현, §6-2 자동화, §6-3 non-unity·unity는 미수행. §6-4 보고는 이 파일·HANDOFF·커밋 본문에 기록. §6-5 기기 시험 상태 `not-tested` 유지.
3. **명세와 다른 점:** 구현하지 않고 Q1에서 중단. 명세 §3 예외 밖의 단언을 임의로 바꾸지 않기 위한 절차다. 하위 에이전트·자체 리뷰 루프·에디터·MCP 작업 없음.
4. **API 근거:** 새 엔진·플러그인 API 사용 없음. 기존 `FVBoRTwoAxisMotion::Rotation()`의 Z축 Yaw·Y축 Pitch 구성(`Source/vBoR/Interaction/VBoRTwoAxisMotion.cpp:3-5`)과 위 시험 `:224-234`를 소스로 대조. 새 API 헤더 확인 대상 없음.
5. **로컬에서 확인할 것:** Q1 해결·구현 뒤 최종 non-unity(`-DisableUnity -Module=vBoR`)·unity·`vBoR.*` 필요. Blueprint/Details·Quest 3 Link/VR Preview 및 T-02·T-04·T-05·T-08은 `not-tested`. 시작 시 UnrealEditor 계열 프로세스 없음. push·PR·머지 없음.
