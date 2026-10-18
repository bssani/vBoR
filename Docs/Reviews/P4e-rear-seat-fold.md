# P4e 2열 시트 폴딩 — 리뷰

명세: [P4e-rear-seat-fold.md](../Specs/P4e-rear-seat-fold.md) · 브랜치: `codex/P4e-rear-seat-fold` · 베이스 `main` `2025098`

상태: **1회차 리뷰 통과(2026-10-19, Claude — `988a7d0`). 열린 항목 없음.**

## 리뷰 1회차 (2026-10-19, Claude — `988a7d0`)

**결과: 통과. 열린 항목 없음.** D1–D7이 명세 개정 1대로 들어갔다.
- 기존 단언은 §3 예외 한 곳만 바뀌었다: P5i E6 Expected에 `FoldSeat`를 더했다(`P3Probe/P5iOperatorPanelTests.cpp:173`).
- 16 규칙·에셋·생성자 단계·TV 표는 바뀌지 않았다. Content·Config도 바뀌지 않았다.

**재현.** 에디터가 닫힌 것을 확인했고, 바뀐 C++ 11개의 시각을 갱신한 뒤 돌렸다.
- non-unity(`-DisableUnity -Module=vBoR`): `Result: Succeeded`, vBoR 경고·오류 0. 로그 `Saved/Logs/ReviewP4eNonUnity.log`.
- unity: `Result: Succeeded`, vBoR 경고·오류 0. 로그 `ReviewP4eUnity.log`.
- `vBoR.*`: 406/406(P4e 8개 포함), `TEST COMPLETE. EXIT CODE: 0`. 로그 `ReviewP4eAll.log`.
- 끝난 뒤 `UnrealEditor` 프로세스는 남지 않았다.

**Codex 토큰:** 두 세션을 합쳐 5.97M이다. 하위 에이전트는 0이다(세션 기록에는 도구 정의만 있고 호출은 없다). 빌드와 전체 시험은 끝에 한 번씩 돌렸다.
- Q1 중단: 1.53M(캐시 91%), 2026-10-18 15:54–15:57 UTC.
- 구현: 4.44M(캐시 97%), 16:02–16:10 UTC, 약 8분.

**확인한 것:**
- **D1 시트**(`Interaction/VBoRFoldingSeat.cpp`):
  - 메시 자리: `MeshRestFromRoot.GetRelativeTransform(FTransform(HingeLocationCm))`로 경첩 기준 자세를 낸다. 각도가 0이면 메시 월드 자세가 `MeshRestFromRoot × Actor`와 같다. E4가 Actor를 Yaw 37°로 돌리고, 메시 rest에도 회전을 준 상태에서 확인한다.
  - 회전은 `FQuat(Y, +rad)`이다. 경첩 위 점 (0,0,60)이 Actor 앞(+X)으로 40 cm 넘게 가고 아래로 내려간다(E4).
  - Tick은 `FInterpConstantTo`를 쓰고 0–한계로 묶는다. 엔진 구현을 확인했다(`UnrealMathUtility.h:1483-1497`): 남은 거리의 제곱이 `UE_SMALL_NUMBER`보다 작으면 목표값을 그대로 돌려준다. 그래서 끝점에 늦어도 한 frame 뒤 정확히 닿고, 상태가 바뀐다. 큰 Tick 하나(100 s)도 넘치지 않는다(E1).
  - 명령은 상태 하나만 본다: 섬 → 접기, 접힘 → 펴기. 움직이는 중에는 반대 명령도 무시한다(E3).
  - `Units="deg/s"`는 엔진이 아는 단위다(`UnitConversion.cpp:51`).
  - 근사 큐브를 기본값으로 둔 것은 다른 부모(`VBoRPoweredPanel.h:71`)와 같다.
- **D2 근사 2열**(`P3Probe/P3VehicleMock.cpp:111-123`, 브랜치 줄):
  - 운전석 판 9·10(`:94-95`)과 같은 오프셋이다: 방석 eye −70, 등받이는 뒤로 28·아래로 40. 경첩은 등받이 아래 끝 (−128, 0, 48)이다.
  - 폭 177은 차체 옆판 안쪽(±90.5)에 들어간다.
  - 접힌 끝은 x ≈ −68이다. 운전석 등받이 뒷면(−52)에 닿지 않는다.
  - E5가 선 상태·접힌 상태 모두 차체 상자 안이고 적재 영역과 겹치지 않음을 확인한다. → 참고 1.
- **D3 버튼·키:**
  - enum 두 개(`VBoROperatorPanel.h:13`, `VBoRScenarioRunner.h:23`)와 단축키 표 모두 끝에 붙였다. `FlowAction`도 매핑한다. 그래서 `CommandName`이 "Fold rear seat"를 낸다.
  - F·U `BindKey`를 더했고, P5i E2가 버튼 = 키를 확인한다.
  - "Seat folding" 칸은 칸 전체 폭 `Pair`라서 버튼마다 316이다. E23의 폭 가정과 같다.
- **D4 판정**(`Interaction/VBoRScenarioOperator.cpp:121-132`):
  - 판정은 전환 중 검사(`:120`) 바로 뒤에 있다. 시작 전(`:105-106`)·암전(`:82`)·지난 token·참가자 권한은 앞의 검사가 그대로 거른다.
  - 버림 종류: 시트 없음은 `Rule`, 움직일 시트 없음은 `None`(기록 줄 없음).
  - E7이 9개 상태 × 두 버튼에서 확인한다: 판정, 창 스냅샷, 스타일, 버림 종류, 센 값, 기록. 시험이 실제로 `Dispatch`를 거친다.
  - 시트 명령은 `SeatAction`에 들어가지 않았다. 그래서 R1 Snapshot의 "스캔 2회"도 그대로다.
- **D5 R**(`Interaction/VBoRScenarioRunner.cpp:481`): 게이트 초기화 다음, 문 초기화 전에 모든 시트를 세운다. 기록 줄에 "시트 세움"을 더했다. E9가 접히는 중·접힘 두 경우에서, 태그 없는 시트도 같이 확인한다.
- **D6:** E6E8이 16(진행자 확인 단계)에서 F를 누른다. 확인 잠금 중에도 시트가 접히고, 단계는 그대로 대기한다.
- **D7**(`:717`):
  - 영역이 "안"이 되면 `UpdateMarker()`를 부른다. `UpdateMarker`는 `CheckRouteArea`를 다시 부르지 않으므로 되부름이 없다.
  - E10은 고치기 전 코드라면 실패하는 시험이다. 처음에 고리가 보이는 것을 확인하고, 영역을 알게 된 다음 frame에 화살표가 끝 + 160 cm로 가고 고리가 숨는 것을 본다.
- **로그:** 시트의 `UWorld::DestroyActor: World has no context` 경고가 32건이다. 박스 받침(32건)·게이트(47건)와 같은 시험 월드 정리 잡음이다(Agent-Workflow §2.5). P4d 로그와 비교하면 새로 생긴 것은 시트뿐이다.

### 참고 1 — 접힌 등받이가 경첩 쪽에서 방석 안으로 들어간다 (근사 형상)

- **위치:** 경첩이 방석 높이의 가운데(z 48)에 있다. 방석은 z 43–53이다(`P3Probe/P3VehicleMock.cpp:116-122`). 운전석 판과 같은 오프셋을 쓴 결과이고, 명세 D2 조건(차체 안, 적재 영역 밖)은 지킨다.
- **모습:** 80°로 접으면 등받이 아랫면이 방석 뒤 끝에서 약 8 cm 아래(z ≈ 44.6)로 들어간다. 방석 앞 끝(z ≈ 53)으로 가며 0이 된다. 윗면은 방석 윗면 바로 위(z 52.8 → 62)에 온다. 그래서 등받이가 방석에 비스듬히 꽂힌 것처럼 보인다.
- **할 일:** 지금은 없다. 사용자가 "그냥 접히기만 하면 돼"라고 했고, 실제 차 데이터가 오면 등받이 CAD와 경첩 값으로 바뀐다. VR 확인(명세 §7-2) 때 거슬리면 경첩을 방석 윗면(z 53)으로 올리는 한 줄이면 된다.

### 참고 2 — 시트는 ←·→·단계 이동으로 세워지지 않는다

- R(D5)과 F6(레벨 다시 열기)만 세운다. 리프트게이트와 같다. 진행자가 16에서 접고 그대로 넘어가면, 18–20 동안 접힌 채로 있다.
- 다음 참가자는 R로 시작하므로 그때 선다. VR 확인 때 진행 흐름에서 불편한지만 본다.

### Q1 — 새 버튼을 허용할 때 기존 P5i 활성 버튼 집합 단언을 확장해도 되는가?

- 상태: **반영함 (`988a7d0`)** — Claude 답: 명세 개정 1 `1c96cfa`.
- **문제:** P4e D2는 `AP3VehicleMock`이 선 상태의 시트를 만들게 하고, D3는 `VBoROperator::Shortcuts()` 끝에 `FoldSeat`·`UnfoldSeat`를 추가하게 한다. D4는 자유모드에서 접기를 허용한다. 그러나 `Source/vBoR/P3Probe/P5iOperatorPanelTests.cpp:113-119`의 `Enabled()`는 **모든 단축키**를 순회하며 Expected에 없는 버튼은 비활성이라고 단언한다. 같은 파일 `:172-173`의 `vBoR.P5i.E6FreeMode` Expected에는 새 버튼이 없다. 이 fixture는 `:86,101`에서 실제 근사 차를 만들고 BeginPlay하므로 D2대로 구현하면 접을 수 있는 시트가 존재한다. 따라서 D2·D3·D4를 만족하면 E6의 새 FoldSeat 단언은 반드시 실패한다. 명세 §3 `Docs/Specs/P4e-rear-seat-fold.md:58`의 "기존 단언은 바꾸지 않는다"와 완료 조건 2를 동시에 만족할 수 없다. 빌드·시험 실행으로 만든 실패가 아니라 소스 대조 결과다.
- **제안:** Claude가 명세 §3에 예외를 명시한다. 기존 버튼의 기대값·단언은 보존하되, 새 시트 버튼만 P5i의 활성 버튼 집합에 추가할 수 있도록 허용한다. 최소 필수 변경은 P5i E6 Expected의 FoldSeat 추가다. 시험에서 시트를 삭제하거나 Shortcuts 순회를 제한하여 새 버튼을 누락시키는 우회는 하지 않는다.
- **함께 정할 것:** D4의 조기 판정 위치대로라면 시점 전환 암전 중에도 접기를 허용하게 된다. 이 경우 P5i E5(`:169-170`, Info만 허용)에도 FoldSeat 추가가 필요하다. 시점 전환 중 F·U 허용 여부를 명세에 명시해 달라. 시작 전 허용은 §5 재량이지만, 허용을 택하면 E3(`:150-152`) Expected도 새 FoldSeat로 확장해야 한다. Codex가 앞서 제시한 시작 전 허용 선택은 구현하지 않은 상태이며, 결정 뒤 시험으로 명시할 예정이다. 레벨 다시 열기 암전 E14(`:251-252`)의 전부 비활성은 D4와 일치하므로 유지한다.
- **확인할 시험:** P5i E3·E5·E6에서 기존 버튼 기대값 유지와 새 버튼의 명시된 허용 범위, E14의 전체 비활성 유지. P4e E7은 시트 없음·선 상태·접힘·움직이는 중·시점 전환·레벨 재열기·확인 잠금·시작 전·자유모드를 확인한다. P5i E2는 새 버튼과 실제 게임 키 F·U가 같은 진행자 명령을 큐에 넣는지 확인한다.

#### 답 (Claude, 2026-10-19)

지적이 맞다. 명세 문제다: D3를 쓸 때 P5i E2(버튼 = 키)만 보고, 단축키를 모두 도는 `Enabled()` 시험(E3·E5·E6·E14)을 보지 않았다. 명세를 개정 1로 고쳤다. 바뀐 곳:

1. **D4 — 판정 자리를 옮김.** 새 명령은 시점 전환 중 검사(`VBoRScenarioOperator.cpp:117`) **뒤**에서 판정한다(확인·다음·처음부터와 같은 자리). 앞의 검사를 그대로 타므로:
   - 시작 전: 버림(`:102-103`). §5의 "시작 전에 받을지" 재량은 없앴다. 그래서 **E3은 그대로 통과**한다.
   - 시점 전환 암전 중: 버림(`:117`). 그래서 **E5는 그대로 통과**한다. 암전이 짧아 다시 누르면 된다.
   - 레벨 다시 열기 암전: 지금처럼 버림(`:79`). **E14는 그대로 통과**한다.
   - 그 뒤 새 판정 두 가지:
     - 시트 없음 → `Rule`.
     - 움직일 시트 없음(이미 접힘·섬에서 펴기·움직이는 중) → `None`. 기록 줄을 남기지 않고 센 값도 늘리지 않는다. 버튼만 흐린다(`:98` 꼴).
2. **§3 — 예외 하나.** `vBoR.P5i.E6FreeMode`(`P5iOperatorPanelTests.cpp:172-173`)의 Expected에 `FoldSeat` 하나만 더한다.
   - `UnfoldSeat`는 더하지 않는다. 펼 시트가 없어 꺼져 있기 때문이다.
   - 다른 단언·fixture는 그대로 둔다. 제안하신 대로 시트를 지우거나 순회를 줄이는 우회는 하지 않는다.
3. **D3 — 칸 이름을 "Seat folding"으로 바꿈.** "Rear seat"는 이미 2열 시점 버튼 이름이다(`VBoROperatorPanel.cpp:22`). 리뷰하면서 찾았다. 버튼 이름("Fold rear seat"/"Unfold rear seat")과 키 F·U는 그대로다.

**확인한 것.** 개정 뒤 단축키를 모두 도는 다른 시험도 소스로 대조했다.
- E7(판정 = 기록): 판정과 기록이 같은 길을 쓰므로 맞는다.
- R1 Snapshot: 창 상태와 실제 판정이 같아야 한다. 시트 명령은 `SeatAction`이 아니어서 `CommandViewpoint`가 스캔 수를 세기 전에 돌아간다(`VBoRScenarioOperator.cpp:50`). 그래서 "스캔 2회" 단언(`:495`)도 그대로다. 시트 명령을 `SeatAction`에 넣지 않는다.
- E21: 주 버튼은 하나다. 새 버튼은 Secondary다.
- E23(글 폭): 칸 전체 폭 `Pair`는 버튼마다 316이고, 시험이 새 버튼에 쓰는 폭도 같다.
- F·U 키는 소스 어디에도 묶여 있지 않다.

이 대조는 소스만 읽은 것이다. 빌드·실행 확인은 Codex의 최종 시험이다.

## Codex 보고 ①–⑤ — 개정 1 구현 완료 (2026-10-19)

브랜치 `codex/P4e-rear-seat-fold`, 구현 기준 `1c96cfa`. 커밋 제목 `feat: add rear seat folding controls`. Q1 답을 반영했고 새 질문은 없다. Claude 첫 구현 리뷰 대기.

① **바뀐 파일**
- `Source/vBoR/Interaction/VBoRFoldingSeat.h`, `.cpp`: Blueprintable 시트 부모, 고정 root·Y축 HingePivot·메시 원래 자세, 기본 80 deg·45 deg/s, 섬/접히는 중/접힘/펴지는 중, Fold/Unfold/ResetSeat.
- `Source/vBoR/P3Probe/P3VehicleMock.h`, `.cpp`: RearEye 기준 근사 방석·등받이 한 판, 편집 가능한 크기(cm), `Car.RearSeat`, 종료 정리. 기본 방석 중심 (-100,0,48) cm, 등받이 중심 (-128,0,78) cm·경첩 (-128,0,48) cm, 폭 177 cm.
- `Source/vBoR/Interaction/VBoROperatorPanel.h`, `.cpp`: enum·표 끝에 FoldSeat/UnfoldSeat, F/U, 영어 Seat folding 칸에 Secondary 버튼 둘.
- `Source/vBoR/Interaction/VBoRScenarioRunner.h`, `.cpp`: enum 끝 추가, 실제 게임 F/U 키 바인딩·공통 큐·전체 시트 명령, R 전체 시트 초기화·기록, CheckRouteArea에서 UpdateMarker.
- `Source/vBoR/Interaction/VBoRScenarioOperator.cpp`: 버튼 명령 매핑, 기존 시작 전·전환 중 검사 뒤 시트 판정, 없음 Rule·움직일 시트 없음 None.
- `Source/vBoR/P3Probe/P3FlowTests.cpp`: P4e 자동화 8개(E1–E10), 기하 방향·메시 원위치·배치·명령·버림·16 유지·R·영역 복구 화살표.
- `Source/vBoR/P3Probe/P5iOperatorPanelTests.cpp`: 개정 1이 허용한 E6 Expected FoldSeat 추가 한 곳만 변경.
- 이 리뷰 파일. git 무시 HANDOFF.md 결과 갱신. Content/Config/Plugins 변경 없음.

② **명세 §6 완료 조건별 결과**
1. §2의 1–5 구현 완료. D1–D7 반영, 16 규칙·에셋·시점·생성자 단계·TV 표는 그대로.
2. 최종 `vBoR.*` **406/406 Success, 실패 0**. 새 P4e 8/8 포함. P5i E2(실제 BindKey와 버튼 같은 큐)·E3·E5·E6·E14·R1 Snapshot·E21·E23, 기존 P4d E11 모두 통과. 시트 없음 Rule·no-op None(기록/계수 유지), 확인 잠금과 무관함, 안내/자유모드, 시작 전/시점·재정렬·레벨 재열기 암전, 지난 token·참가자 거부, 태그 없는 추가 시트까지 시험함.
3. non-unity(`-DisableUnity -Module=vBoR`)·unity **각 1회, 모두 Result: Succeeded, 프로세스 exit 0, vBoR 컴파일 경고·오류 0**. 전체 자동화 1회, discovery 406·completion 406, 시험 `EXIT CODE: 0`, 프로세스 exit 0. P0Buck 간헐 실패 없음, 재실행 없음.
4. 이 ①–⑤를 리뷰·HANDOFF·커밋 본문에 기록함.
5. Quest 3·VR·GUI·Blueprint·정식 T-04·T-08은 **not-tested** 유지. 합성 자동화는 기기·조작감 확인이 아님.

③ **명세와 다르게 한 것과 이유**
- 명세 이탈 없음. 구현 재량: 근사 폭 177 cm(기본 차폭 185 cm에서 양옆 4 cm), 운전석과 같은 eye 기준 방석/등받이 높이, 새 칸은 Free mode 바로 위. 움직일 시트 없음 이유는 `No seat ready for this action`. 참가자·TV·VR 정보 글에는 새 키를 추가하지 않음.
- Q1 개정 1대로 P5i E6에 FoldSeat만 추가함. E3/E5/E14와 나머지 기존 단언·fixture는 그대로. 새 P4e 시험은 기존 FP3World를 재사용하며 각 시나리오 단계를 Reset 후 지정함. D7 회귀는 별도 P4e E10으로 추가해 기존 P4d E11을 유지함.
- 하위 에이전트·자체 리뷰 루프·중간/red 빌드·MCP·에디터·에셋 작업 없음. push·PR·머지 없음.

④ **엔진·플러그인 API 확인(헤더 대조 + 실제 로컬 빌드, 추정 없음)**
엔진 경로는 `C:/Program Files/Epic Games/UE_5.7/Engine/Source/` 기준.
- `Runtime/Engine/Classes/Components/SceneComponent.h:443,456,729,1223,1287`: quaternion 상대 회전, 상대 transform, SetupAttachment, 위치·회전, Movable.
- `Runtime/Core/Public/Math/Quat.h:140`: Y축·radian quaternion 생성. `Runtime/Core/Public/Math/UnrealMathUtility.h:1483`: FInterpConstantTo.
- `Runtime/Core/Public/Math/TransformNonVectorized.h:481,525`: transform 합성·GetRelativeTransform(메시 rest를 경첩 상대 자세로 변환).
- `Runtime/Engine/Classes/Components/StaticMeshComponent.h:431`, `Components/PrimitiveComponent.h:1943`, `Components/ActorComponent.h:1305`: 메시 설정·충돌 끔·동적 방석 등록.
- `Runtime/Engine/Classes/Engine/World.h:3735`, `Runtime/Engine/Classes/GameFramework/Actor.h:2032,3116`, `Runtime/Engine/Public/EngineUtils.h:569`: deferred spawn·Actor 부착·FinishSpawning·TActorIterator.
- 시험 기하: `Runtime/Core/Public/Math/Box.h:511,610`(점 포함·변환).
- 새 ISDK/MetaXR API 호출·의존 추가 없음. 기존 common flow·진행자 UI 경로를 재사용함.

⑤ **로컬에서 확인할 것 / 검증 기록**
- 빌드 로그: `Saved/Logs/P4eNonUnityBuild.log`(27.98 s), `Saved/Logs/P4eUnityBuild.log`(26.25 s). 로컬 도구 플러그인은 기존 설정대로 빌드에 포함됨. vBoR warning/error 진단 없음.
- 자동화: `Saved/Logs/P4eAllAutomation.log`, 프로세스 출력 `Saved/Logs/P4eAutomationProcess.log`. 샘플·McpAutomationBridge·UnrealAgent 비활성, nullrhi·unattended·nosound, 따옴표로 감싼 절대 abslog 경로 사용. 기존 샘플 InfoCard CDO·XR 확장·fixture World 정리 로그는 Workflow §2.5에 따라 결과와 분리함. 406 성공·0 실패·시험/프로세스 exit 0.
- 시작·최종 점검에서 UnrealEditor 계열 프로세스 없음. 실제 GUI 에디터·MCP를 열지 않음.
- 사용자 GUI/Blueprint: Seat folding의 두 버튼·F/U 툴팁/흐림·마지막 명령 줄, 자식 BP에 CAD와 MeshRestFromRoot·경첩/각도/속도 편집값 적용을 확인할 것. 이번 에셋 작업 없음, GUI/Blueprint는 not-tested.
- 사용자 Quest 3 Link/VR Preview: 명세 §7의 F/U(게임·진행자 창 포커스), 움직이는 중 반복/반대 명령 무시, 속도·80 deg 자리값 적절성, 운전석·동승석·2열·차 뒤에서 접힘 모습, R 즉시 세움, 16 진행자 확인 유지. 계획서 T-04·T-08은 not-tested.
