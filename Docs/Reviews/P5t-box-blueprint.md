# P5t 박스 BP — 리뷰

명세: [P5t-box-blueprint.md](../Specs/P5t-box-blueprint.md) · 브랜치: `codex/P5t-box-blueprint` · 베이스: `main@4885952`

상태: **1회차 리뷰 — 통과. 문제 없음, 참고 4개.** 열린 항목 없음. 질문 없음. 머지 승인 대기.

## 리뷰 1회차 (2026-10-21, Claude — `4f8ffe5`)

**결과: 통과.** 명세의 범위·결정·완료 조건대로다. 명세 밖으로 커진 것은 없다.

**바뀐 것:**
- **받침(`VBoRBoxStand`):**
  - `BoxSizeCm` 프로퍼티와 그 정리를 지웠다.
  - `GetBoxSizeCm()`이 `BoxClass` CDO의 크기를 절댓값·축별 최소 1 cm로 읽는다. 시작 자리(`StartPose`)와 보충 칸(`Tick`)이 이 값을 쓴다(D3).
  - 만들 때 크기를 덮어쓰지 않는다. 목표·보조 끔·바닥 소멸·바닥 높이 덮어쓰기는 그대로다.
  - `BoxClass`가 비면 `BeginPlay`에서 경고한다(D5).
- **박스(`VBoRCarryObject`):** `BeginPlay`가 `WarnIfOffCenterPivot(GetMeshLocalBounds(), GetActorScale3D())`를 부른다. 1 cm를 넘으면 이름·거리·고칠 방법을 담아 경고한다(D4).
- **에셋:**
  - `Content/Main/Car/Parts/BP_Box`를 새로 만들었다. 부모는 `AVBoRCarryObject`이고 Event Graph는 비었다. 값은 기본 그대로다(E1·E6으로 확인).
  - `L_Main` 받침의 `BoxClass`가 `BP_Box_C`다(E2).
- **시험:**
  - `vBoR.P5t.E1`–`E8`과 시험용 자식 `AP5tSizedBox`(20×30×40 cm, 보조 켬)를 넣었다.
  - 기존 시험과 `P4iBlueprints()` 목록은 바뀌지 않았다.

**재현.** 에디터가 닫힌 것을 확인했고, 단계마다 에디터가 다시 열렸는지 보며 돌렸다.
- non-unity·unity `Result: Succeeded`. 로그 `Saved/Logs/P5tReviewNonUnity.log`·`P5tReviewUnity.log`.
  - 두 빌드 모두 다시 컴파일할 것이 없어 링크만 했다. Codex가 같은 소스로 만든 오브젝트를 빌드 도구가 그대로 썼다.
  - 그래서 경고는 Codex 로그에서 확인했다. non-unity는 `P5tNonUnityBuildFirst.log`가 바뀐 세 파일을, 최종 로그가 시험 파일을 컴파일했다. unity는 `Module.vBoR.*`를 컴파일했다. vBoR 경고·오류는 0이다.
- `vBoR.*` **536/536**, `TEST COMPLETE. EXIT CODE: 0`. 로그 `Saved/Logs/P5tReviewAll.log`.
  - P5t 8개가 들어 있다.
  - 받침·박스 기존 시험(P4d·P4i·P1Box 18개)도 통과했다.

**Codex 토큰:** 한 세션에 **5.81M**이고, 하위 에이전트는 0이다.
- 빌드는 non-unity·unity 각 3번 돌렸다. 앞의 두 번은 새 시험의 로그 관찰 도우미를 고치느라 다시 했다.
- 전체 시험은 3번 돌렸다(534 → 535 → 536). 실패 원인과 고친 것은 보고 ③에 그대로 있다.
  - 예상 Warning이 Verbose로 낮아져 전달됐다(엔진 `AutomationTest.cpp:289-291`).
  - 비동기 로그 버퍼를 읽기 전에 비우지 않았다.
- MCP는 Python 파일 한 번으로 했다(승인 1회).

**확인한 것:**
- **크기(D3).** E3에서 시험 클래스 크기가 그대로 남고, 바닥이 받침 윗면 + 0.01 cm다. E4에서 +41 cm는 아직 차 있고, +50 cm면 하나 보충되어 원래 자리에 놓인다. 기본 클래스 크기가 45×35×30이라 P4d E1·E6의 75.01 cm와 +31 cm 경계도 그대로 통과한다.
- **`BP_Box`로 받침(E5).** 만든 박스가 `BP_Box_C`이고 크기를 CDO에서 읽는다. 팀이 크기를 바꿔도 E5는 통과한다.
- **지금과 같음(E6).** `Approx`가 있을 때 `CarryMesh`의 Static Mesh와 vBoR 쪽 Edit 프로퍼티(소리 칸 포함)를 C++ 기본과 비교한다. P4i E2와 같은 규칙이다.
- **`L_Main.umap`(+10 KB).** 이전과 새 맵의 이름표를 비교했다.
  - 새로 들어간 것은 `BP_Box`·`BoxClass`, 그리고 P5r·P5s가 부품에 넣은 오디오 컴포넌트 이름들이다. 맵을 다시 저장하면서 배치된 BP의 새 하위 객체가 처음 기록된 것이다.
  - 빠진 이름은 저장 시각과 진행기 경로 한 줄(같은 줄이 새 저장 시각으로 바뀜)뿐이다.
- **`BP_Box` 에셋.** 이름표의 `K2Node_Event_0~2`·`DefaultSceneRootNode`는 기존 BP(`BP_SideMirror_L`·`BP_StartButton`·`BP_Liftgate`)에도 똑같이 있다. MCP로 만든 BP에 남는 흔적이고, 실제 노드가 0개인지는 E1이 본다. `Development/` 참조는 없다(P4i E5 통과).
- 시험 전용 접근(`FP5tTestAccess`)은 `WITH_DEV_AUTOMATION_TESTS` 안에 있다. 주석 말투는 규칙대로다.

### 참고

1. **피벗 경고는 박스마다 난다.** 받침은 박스를 들 때마다 새 박스를 만든다. 피벗이 틀린 메시면 들 때마다 Output Log에 한 줄씩 쌓인다. 명세 D4("BeginPlay에서 한 번")대로이고, 고치면 경고가 사라지므로 그대로 둔다.
2. **E7은 판정 함수만 시험한다.** `BeginPlay`가 늘린 scale을 넘기는지는 코드로만 확인했다(`VBoRCarryObject.cpp` `BeginPlay` 첫 줄). 엔진 기본 메시가 모두 가운데 피벗이라 실제 경로를 시험할 메시가 없다. 명세 §5에서 허용한 방식이다.
3. **집 PC에서 `BP_Box` 값을 바꾸면 `vBoR.P5t.E6`이 실패한다.** `Approx`가 있는 동안은 근사 값과 같아야 하기 때문이다. 명세 §7-2에 적은 의도된 동작이다. 부품 BP 소리 칸의 P4i E2와 같은 종류다(Blueprint-Guide §4.4).
4. **Blueprint-Guide는 머지 뒤 Claude가 고친다.** 고칠 곳:
   - §0 BP 수(23 → 24)
   - §4.2 표에 `BP_Box` 줄
   - §4.4 박스 줄과 "박스 BP가 필요하다 — 확인 안 함" 문단
   - §5 받침 줄(`BoxClass` = `BP_Box`)
   - 메시를 바꿀 때의 조건(피벗 가운데, simple collision 상자·convex)

## 질문

(없음)
