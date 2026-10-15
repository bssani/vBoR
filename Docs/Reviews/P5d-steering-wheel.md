# P5d 리뷰 기록

명세: [Docs/Specs/P5d-steering-wheel.md](../Specs/P5d-steering-wheel.md) · 브랜치: `codex/P5d-steering-wheel`

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
| 1 | 2026-10-15 | `2b97cc1` 새 부모 `AVBoRSteeringWheel`, 근사 차 휠 | Claude | **열림 1건(R1-1, 작음).** 빌드·시험 재현: non-unity·unity 성공(다시 컴파일한 파일 0). Codex 첫 non-unity 로그에서 16개 파일을 컴파일했고, 진단은 모두 새 시험 파일(`auto*` 추론, 다음 빌드에서 고침), 나머지는 경고 0. `vBoR.*` **230/230**(P5d 18/18), EXIT CODE 0. **Codex 토큰 3.43M(캐시 95%), 약 10분, 하위 에이전트 0, exec 27** |

### 1회차에 확인한 것

- **D1:** 새 부모이고 ISDK·MetaXR에만 의존한다. 1축 부품·걸림 코드는 그대로다. 변환기는 두 점·`MultiGrab`·던지기 없음이고, 대상을 움직이지 않고 지금 축 자세를 돌려준다(`VBoRSteeringWheel.cpp:30-34`).
- **D2 각도:** 부호는 "+X를 바라보는 운전자 기준 +Z → +Y = 시계 방향"이다(`:175-176`). `ApplyAngle`이 `FQuat(X, −각도)`로 돌린다(`:190`). 계산해 보면 +각도에서 12시가 +Y로 간다. 변화량을 더한 뒤 ±한계로 자르므로 되돌리면 바로 줄어든다(E4).
- **D3·D4:** 투영은 쉬는 축 frame에서 한다(돌아간 축이 아님). 축 근처·90° 넘는 튐·계산 방식 변경(한 손 0/1, 두 손 2+첫 손)은 더하지 않고 기준만 옮긴다(`:182-184`). 한 손 고정·다른 손만 60° → +30(E7), 짧은 선분 넘나들기(E8), 손 추가·놓기·다시 잡기(E9)를 시험으로 본다.
- **D5:** 선택 때 쉬는 축 기준 투영 반지름이 [12, 24] cm 밖이면 받지 않고, SDK 선택 취소는 `Tick`에서 한다(E12, 콜백 안 취소 0). 잡은 뒤에는 반지름을 벗어나도 놓지 않는다.
- **D6:** 한 손이라도 추적이 끊기면 전체 보류하고, 복구 때 버린 각도를 기록한다(E10 한 손·두 손). SDK `Unselect`가 추적 손실 중에 오면 소유를 유지한다(E11). `ForceCancel`은 SDK 선택을 모두 끊지만 휠의 손 소유와 기준은 유지한다. `bCancelling`으로 그때 오는 `Unselect`를 무시한다.
- **`Unselect` 안의 마지막 자세 반영(E14):** 놓기 처리 안에서 축을 돌린다. ISDK 겹침 처리는 탐지기의 hover 집합에 넣고 빼기만 한다(`IsdkHandGrabDetector.cpp:302-388`). `Unselect`는 그 집합을 도는 중이 아니라 입력이 꺼질 때 따로 나간다(`IsdkGrabberComponent.cpp:765`). 그래서 재진입 위험이 없다고 보고 받아들인다. 사용자 이벤트·소리는 대기열에서 `Tick`으로 미룬다.
- **D7:** 한계 이벤트 ±1·딸깍 1번, 10° 벗어나면 다시 무장(`:191-197`, E4). hover·잡기 덮개 색(E15).
- **D8:** 허브 (35, −37, 87), 원판 36 × 3 cm, 12시 표시는 운전자 쪽 면. 핸들 판은 휠이 생겼을 때만 `HiddenInGame`이고, 에디터에서는 보인다(E16). 옮기고 돌린 차에서 대시보드·두 레버(세 걸림 × 세 걸림)와 겹치지 않는다. `EndPlay`에서 정리한다(E17).
- **범위:** 방향지시 자동 해제·복원력·림 버튼·키보드는 없다. P3 단계 데이터·레벨·에셋은 그대로다. 기존 시험 단언은 바뀌지 않았다. 주석은 사용자 말투다.

## 항목

### R1-1 설정 검사가 `OnConstruction`에만 있어 레벨에 놓은 휠에는 실행 때 적용되지 않음 — 반영함 (fix(p5d): validate wheel settings at runtime)

**문제**
- `bGrabValid`(잡기 허용 여부)는 `OnConstruction`에서만 계산한다(`VBoRSteeringWheel.cpp:69-71`). 그런데 이 변수는 저장되지 않는 일반 변수(기본 true, `VBoRSteeringWheel.h:95`)다. `PreInitializeComponents`의 Palm 허용(`:84`)과 `BeginHold`(`:125`)는 이 저장된 값을 그대로 쓴다.
- 레벨에 놓은 Actor는 패키지(쿠킹) 실행에서 레벨을 불러올 때 construction을 다시 돌리지 않는다. 다시 돌리는 분기는 에디터 빌드에만 있다(엔진 `Runtime/Engine/Private/Level.cpp:1880-1885`, `#if WITH_EDITOR`의 `RunConstructionScripts` / `#else Finalize`). PIE도 이미 construction을 돌린 레벨을 복제한다.
- 그래서 레벨에 놓은 휠(구조 원칙상 실제 휠은 자식 Blueprint로 레벨에 놓인다)은 실행 때 `bGrabValid`가 기본 true다. 메시가 없거나, 반지름 범위가 뒤집혔거나, 한계가 0인 잘못된 설정이어도 잡기가 꺼지지 않는다.
  - 1축 부품은 같은 이유로 쉬는 축과 Palm 허용을 `PreInitializeComponents`에서 다시 계산한다(`VBoRAxisPart.cpp:182`·`:193`). 걸림 검사도 거기서 한다(P5b D5).
  - 휠도 `RestPivot`은 `PreInitializeComponents`(`:83`)에서 다시 잡는다. 검사만 빠졌다.
- 덤으로, `OnConstruction`의 경고(`:77`)는 에디터에서 프로퍼티를 바꿀 때마다 반복된다.

**제안**
- 검사(메시·한계·반지름·최소 거리)와 경고·잡는 상자 충돌 끄기를 `PreInitializeComponents`로 옮긴다. 스폰마다 한 번 돈다.
- `OnConstruction`은 배치(메시·표시·잡는 상자 위치와 크기)만 한다. 잡는 상자 위치·크기는 컴포넌트 프로퍼티라 저장된다.
- 걸림 검사(`AVBoRAxisPart::DetentProblem`, P5b)와 같은 모양이면 된다.

**확인할 시험**
- 유효한 휠을 만든다. construction을 다시 돌리지 않고 프로퍼티를 잘못된 값으로 바꾼다(예: 최대 잡는 반지름 < 최소). 그다음 `PreInitializeComponents`만 부르면:
  - 경고가 1번 난다.
  - Palm 잡기가 거부되고 잡는 상자 충돌이 꺼진다.
  - 선택 이벤트를 넣어도 잡은 손이 0이다.
- 잘못된 설정으로 `OnConstruction`을 두 번 불러도 경고가 0번이다.
- 기존 시험은 그대로 통과한다.

## Codex R1 반영 기록 (2026-10-16)

① 바뀐 파일
- Source/vBoR/Interaction/VBoRSteeringWheel.cpp: 메시·한계·반지름·최소 거리
  검사, 경고, GrabBox 충돌 설정을 PreInitializeComponents로 이동.
  OnConstruction은 메시·표시·잡는 상자 배치를 유지.
- Source/vBoR/P3Probe/P5dSteeringWheelTests.cpp: R1 시험 2개 추가.
  construction 재호출 없이 반지름 범위를 뒤집고 PreInitializeComponents만
  호출하여 경고 1회·Palm 거부·충돌 꺼짐·선택 뒤 소유 손 0을 확인.
  잘못된 설정으로 OnConstruction 2회 호출하여 경고 0회 확인.
- Docs/Reviews/P5d-steering-wheel.md: R1-1 반영 상태와 검증 기록.
- HANDOFF.md: Codex 결과 갱신, 이전 결과 보존(git 무시 파일).

② 명세 §6 완료 조건별 결과
1. 충족: 기존 휠·근사 차 범위 유지, R1-1 실행 초기화 검사 반영.
2. 충족: 최종 vBoR.* 232개 발견·232개 완료·232 Success·0 Fail.
   P5d 20/20(기존 18 + R1 2), TEST COMPLETE EXIT CODE 0,
   프로세스 exit 0. 기존 시험 단언 변경 없음.
3. 충족: 최종 non-unity(-DisableUnity -Module=vBoR)·unity 모두
   Result: Succeeded, exit 0, 컴파일 warning/error 0.
4. 충족: 이 보고를 커밋 본문·리뷰 파일·HANDOFF.md에 기록.
5. 충족: Quest 3·VR Preview·Blueprint/Details는 not-tested 유지.

검증 기록(Saved/Logs/, git 무시)
- P5d-R1-nonunity-build-final.log: 성공, exit 0, 경고·오류 0.
- P5d-R1-unity-build-final.log: 성공, exit 0, 경고·오류 0.
- P5d-R1-full-tests-final.log: 232/232, P5d 20/20, exit 0.
- 첫 빌드 2개도 성공. 첫 전체 시험 P5d-R1-full-tests.log는 231/232,
  exit 255: 새 시험의 별도 FOutputDevice 경고 계수 단언만 실패.
  UE는 기대한 경고를 Verbose로 낮춤(Core/Private/Misc/AutomationTest.cpp:
  313-315). 경고 1회 검증은 공식 AddExpectedMessagePlain의 기본
  Occurrences=1로 확인하도록 중복 계수 단언을 제거함.
  시험 수정으로 최종 트리가 바뀌어 빌드 2개·전체 시험을 재실행함.
- 명령: Build.bat vBoREditor Win64 Development
  -Project=C:\Git\vBoR\vBoR.uproject -WaitMutex
  -DisablePlugin=OculusInteractionSamples
  (non-unity에 -DisableUnity -Module=vBoR 추가).
- 시험: UnrealEditor-Cmd.exe C:\Git\vBoR\vBoR.uproject
  "-ExecCmds=Automation RunTests vBoR;Quit" -unattended -nullrhi
  -nosplash -nosound
  "-DisablePlugins=OculusInteractionSamples,McpAutomationBridge,UnrealAgent"
  -log=P5d-R1-full-tests-final.log.
- 샘플 CDO·headless XR·임시 world 로그는 기존 환경 잡음으로 구분.
  최종 자동화 실패와 vBoR 컴파일 진단은 0.

③ 명세와 다르게 한 것과 이유
- 동작 차이 없음. R1-1 제안대로 검사·경고·충돌 처리를 실행 초기화로
  옮김. 경고 1회는 UE 공식 기대 메시지 계수로 검증하고,
  construction 경고 0회는 별도 로그 수집기로 확인함.
- 하위 에이전트·자체 리뷰 루프·에디터 MCP 사용 없음.
- 첫 시험 실패를 고친 뒤 재검증한 것 외 반복 빌드·전체 시험 없음.
  의도적인 red 실행은 하지 않음. push·PR·머지 없음.

④ 확인한 API 근거(헤더 직접 확인, 추정 없음)
엔진 경로: C:/Program Files/Epic Games/UE_5.7/Engine/Source/Runtime/.
- Engine/Classes/GameFramework/Actor.h:3123,3448:
  PreInitializeComponents 실행 초기화, OnConstruction 배치.
  Engine/Private/Level.cpp:1880-1885에서 WITH_EDITOR 밖은
  construction을 다시 실행하지 않고 Finalize로 진행함도 확인.
- Engine/Classes/Components/PrimitiveComponent.h:1943,2528:
  SetCollisionEnabled/GetCollisionEnabled.
- Core/Public/Misc/AutomationTest.h:1815:
  AddExpectedMessagePlain, 기본 Occurrences=1로 정확한 1회 확인.
- Core/Public/Misc/OutputDevice.h:145,
  Core/Public/Misc/OutputDeviceRedirector.h:70,77,88:
  Serialize, Add/RemoveOutputDevice, FlushThreadedLogs.
- Plugins/MetaXRInteraction/Source/OculusInteraction/Public/Interaction/
  IsdkGrabbableComponent.h:138,146:
  IsGrabInputMethodAllowed/SetGrabInputMethodAllowed.

⑤ 로컬에서 확인할 것
- 빌드·합성 자동화는 위 최종 로그로 확인함. 리뷰어 재현은
  Docs/Agent-Workflow.md §2.3·2.4 절차를 따름.
- 에디터: 잘못된 설정의 Blueprint 자식을 레벨에 놓고 실행 시 잡기가
  꺼지는지, 프로퍼티 편집마다 경고가 반복되지 않는지 확인 필요.
  Blueprint·Details·레벨 직접 확인은 not-tested.
- Quest 3 Link/VR Preview: 계획서 T-08, 명세 §7 전체(한 손·두 손,
  놓고 다시 잡기, 한계·딸깍, 교차 추적 손실, 레버 근처 오선택,
  허브 잡기 거부) not-tested. 합성 시험은 기기 검증을 대신하지 않음.
- 질문 없음. Claude 다음 리뷰 대기.

## 참고 (항목 아님 — VR에서 볼 것)

- **레버 근처 림을 잡으면 레버가 잡힐 수 있다**(명세 §7-6). 휠 잡는 상자는 X 32.5–37.5이고 레버 상자는 X 37–41이라 X 37–37.5에서 겹친다. ISDK는 겹친 것 중 상자 원점이 가까운 것을 고르는데(`IsdkHandGrabDetector.cpp:397-456`), 휠 원점은 허브다. 자주 그러면 레버 축 X를 2–3 cm 뒤로 옮긴다(자리값).
- **한 손을 놓으면 SDK 쪽 다른 손 선택도 끊긴다**(`ForceCancel`). 휠은 남은 손을 계속 소유하므로 동작은 맞다. 그 손을 다시 쥐었다 펴는 동안 ISDK의 손 표시(있다면)가 어색한지 본다.
- **두 손 교차 때 한 손이 가려지면 휠 전체가 멈춘다**(D6, P1 박스와 같은 정책). 거슬리면 "끊긴 손만 빼고 남은 손으로 계속"으로 바꿀 수 있다(명세 §7-5).
- 명세 §7 순서대로 본다.

## 질문

(없음)
