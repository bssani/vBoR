# P5q 시동 버튼·전동 사이드미러 — 리뷰

명세: [P5q-power-mirrors.md](../Specs/P5q-power-mirrors.md) · 브랜치: `codex/P5q-power-mirrors` · 베이스: `main@b749dd3`

상태: **1회차 리뷰 통과 — 문제 없음. 열린 항목 없음. 머지 승인 대기.** 질문 없음.

## 리뷰 1회차 (2026-10-21, Claude — `7aeb9ca`)

**결과: 통과.** 명세 D1–D9와 E1–E13을 지켰다. 범위 밖으로 커진 것은 없다.

**C++:**
- `UVBoRVehiclePowerComponent`(D1·D2·D4).
- `AVBoRFoldingMirror`(D5·D6).
- 진행자 `Power`(D3, 키 `E`)와 R 초기화(D7).
- 시트·`AVBoRHingedPanel`·근사 차·시나리오·Config는 바뀌지 않았다.

**에셋:**
- `BP_Car_Core`에 전원 컴포넌트 하나를 넣었다.
- BP 3개(`BP_StartButton`, `BP_SideMirror_L`·`_R`)를 만들었다.
- `L_Main`의 Actor는 47개에서 50개가 됐다. 기존 Actor는 그대로다(Codex가 바꾸기 전 기록과 비교).

**재현.** 에디터가 닫힌 것을 확인했고, 단계마다 에디터가 다시 열렸는지 보며 돌렸다.
- non-unity·unity `Result: Succeeded`, vBoR 경고·오류 0.
- `vBoR.*` **502/502**(P5q 13개 포함), `TEST COMPLETE. EXIT CODE: 0`. 로그 `Saved/Logs/P5qReview*.log`.

**Codex 토큰:** 한 세션에 **8.90M**, 하위 에이전트 0, 질문 없음.
- non-unity를 4번 빌드했다. 첫 회 컴파일 실패를 고친 뒤 성공이 3번이다.
- 에디터 작업 전 네이티브 시험은 3번 돌렸다. 시험 fixture를 고치느라 그랬다.
- 일부러 실패를 보려는 반복은 아니다. 이유는 보고 ③에 적혀 있다.

**확인한 것:**
- **전원 하나 규칙(D1).**
  - `FindUnique`가 같은 world에 등록된 실제 컴포넌트만 센다. `BeginPlay`에 한 번만 부른다.
  - 0개·2개면 `bAvailable`이 꺼져 `TogglePower`·`SetPowerOn` 둘 다 무시한다. 경고는 첫 컴포넌트 하나만 남긴다.
  - 0개일 때 경고는 runner가 남긴다.
- **시동 버튼(D2).** `TActorIterator<AVBoRPokeButton>`로 한 번 찾고 태그로 거른다. 여러 개면 모두 묶는다. `EndPlay`에 푼다.
- **같은 frame(D4).**
  - `GFrameCounter`로 토글을 하나로 합친다. 출처는 `Participant`·`Operator`·`Restart`·`Direct`로 Log에 남는다.
  - E6은 세 경우를 본다: 버튼 + 진행자 + 두 번째 버튼, 다음 frame에 진행자가 먼저, 버튼 둘.
- **진행자 조건(D3).** `Power` 검사는 시트 F·U와 같은 곳에 있다. 시작 전·전환 중 검사보다 뒤에 있어서, 시트와 같이 버린다.
  - 전원이 없거나 둘이면 "No vehicle power"로 거절한다.
  - `EVBoROperatorButton::Power`는 `Count` 앞에 있다. 키 `E`는 다른 곳에 묶여 있지 않다(`git grep EKeys::E`).
- **미러(D5).**
  - 축은 `HingeRotation` × local +Z다.
  - 에디터에서는 펼친 자리이고, Play를 시작하면 접힌다.
  - `FInterpConstantTo`로 속도 상한을 지킨다. 움직이는 중에 바뀌면 지금 각에서 방향만 바꾼다(E4).
  - 충돌은 없다.
- **접는 방향.**
  - L은 경첩 (40, −90, 100), 메시 (40, −100, 100), −65°다.
  - R은 경첩 (40, 90, 100), 메시 (40, 100, 100), +65°다.
  - +Z 회전으로 계산하면 둘 다 바깥 끝이 −X(차 뒤)로 간다. 명세 §5의 "차 몸 쪽(뒤)"과 맞다.
- **문 부착(D6).**
  - 부모가 `AVBoRHingedPanel`이면 `HingePivot`에 KeepWorldTransform으로 붙는다. 판 코드는 그대로다.
  - E10은 같은 문의 손잡이 연결(P2e)이 그대로인지도 본다.
- **R(D7).** 전원을 `SetPowerOn(false, Restart)`로 끈 뒤 미러를 `ResetMirror`로 둔다. 이벤트가 `Folding`으로 바꿔도, 바로 뒤의 `ResetMirror`가 접힘으로 끝낸다. 월드 순회는 R 때만 한다.
- **E13.** Play 없이 확인한다: Core의 전원 1개, 레벨 전체 전원 1개, 시동 버튼 1개(이름·HitAreaScale 기본값), 미러 L·R 하나씩, 부모 클래스, 루트 단위 변환, L의 부모 `BP_Door_FL`.
- **기존 단언.** P5i 시험 파일은 바뀌지 않았고 그대로 통과한다(근사 차 world에는 전원이 없어 `Power`가 꺼짐). P4i E1·E4·E5도 통과한다.
- 진행자 창이 스크롤되므로(`VBoROperatorPanel.cpp:147`), 칸이 하나 늘어도 잘리지 않는다.
- 주석 말투는 규칙대로다.

### 참고

1. **진행자 `Power`는 진행자 기록(`Record`)에 안 남는다.**
   - 시트 F·U는 "진행자 시트 접기"를 `Record`에 남긴다(`VBoRScenarioRunner.cpp:485`).
   - 전원은 `UE_LOG`의 출처 표시로만 남는다. 명세 D4는 Log만 요구했으므로 문제는 아니다.
   - 진행자 창의 기록 목록에서도 보이게 하려면 한 줄이다. 필요하면 다음 작업에 넣는다.
2. **근사 차 world마다 경고 하나가 생긴다.**
   - runner가 "Vehicle power: expected one component, found 0"을 남긴다. 해당하는 곳은 `L_P3_Flow`와, runner를 쓰는 P3–P5 시험 world다.
   - 명세 D1대로라 정상이다. Agent-Workflow §2.5의 "무시해도 되는 로그"에 적는다(가이드 갱신 때 같이).
3. **미러 경첩은 프로퍼티로 옮긴다.**
   - `HingeLocationCm`·`HingeRotation`으로 옮긴다. BP 뷰포트에서 `HingePivot` 컴포넌트를 끌어도 `OnConstruction`이 덮어쓴다.
   - `AVBoRHingedPanel`도 같은 방식이다. Blueprint-Guide에 적는다.
4. **참가자 시동 버튼은 시나리오 중에도 동작한다.**
   - 명세 D2에 모드 조건이 없다. 진행자 `Power`도 시작 뒤면 받는다(시트와 같음).
   - 평가 단계 중에 누르면 미러가 펼쳐진다. 지금은 해가 없다.
5. **진행자 창 버튼은 "Power on/off" 하나다.**
   - 지금 켜졌는지는 창에 보이지 않는다. 미러가 펼쳐졌는지로 안다.
   - VR 묶음 확인 때 진행자에게 불편한지 본다.
6. **배치는 MCP 조회 값과 계산으로만 확인했다.**
   - 스크린샷 `Saved/P5qRecovery/P5qMainMirrors.png`은 소리 아이콘이 많아 미러 자리를 눈으로 보기 어렵다.
   - 시동 버튼은 (27, −15, 91.6) cm, 운전석 오른쪽 대시보드 위다.
   - 미러와 버튼 자리는 VR에서 본다. T-08은 `not-tested`다.

## 질문

(없음)
