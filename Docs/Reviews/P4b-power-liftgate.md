# P4b 리뷰 기록

명세: [Docs/Specs/P4b-power-liftgate.md](../Specs/P4b-power-liftgate.md) · 브랜치: `codex/P4b-power-liftgate`

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
| 1 | 2026-10-15 | `b44dc3c` 전동 판 부모·끼임 반전·버튼·키보드·P1 시험대 게이트 | Claude | **열림 1건(R1-1).** 나머지는 명세대로다. 빌드·시험 재현: non-unity·unity 성공, vBoR 경고·오류 0(Codex 첫 non-unity 로그에서 새 파일 컴파일·경고 0 확인), `vBoR.*` **171/171**, EXIT CODE 0. **Codex 토큰 4.45M(캐시 95%), 약 12분, 하위 에이전트 0, exec 33.** 기존 시험 두 개가 기본 닫힌 게이트와 부딪쳐 실패했고, 고친 뒤 빌드·전체 시험을 한 번 더 했다(실제 실패 때문이라 규칙 안) |

### 1회차에 확인한 것

- **D1 명령 전용·차 원점:** 손 입력 경로가 없다. Actor 원점은 시험대(입구) 원점이고, `HingePivot`을 경첩 자리로, 메시는 `MeshRestFromRoot`로 원래 자리를 지킨다(E10).
- **D2 상태·시간:**
  - 상태는 닫힘 → 열기 대기 → 열리는 중 → 열림 → 닫기 대기 → 닫히는 중이다. 일정 각속도 21.25 deg/s, 85°, 4 s, 대기 1 s(근사 프로퍼티)다.
  - 대기를 쓰고 남은 시간이 같은 Tick의 이동으로 넘어가서, 큰 Tick에서도 총 시간이 같고 목표 각에서 멈춘다(E8·E9).
- **D3 멈춰 있을 때만:**
  - `FVBoRPoweredMotion::Request`가 닫힘에서 열기, 열림에서 닫기만 받는다. 받으면 그 자리에서 대기 상태로 바꾼다. 그래서 같은 프레임의 두 번째 명령(버튼 + 키보드)은 무시된다(E5).
  - 이동·소리·상태 이벤트는 Tick에서만 처리한다(E11, 실제 SDK 포인터 delegate로 확인).
- **D4 경고음:** 명령 다음 Tick에 한 번 울리고, 대기·이동 동안 0.5 s마다 반복한다. 열림·닫힘에 도착하면 멈추고, 끼임 반전 중에는 이어진다.
- **D5 끼임:**
  - 닫히는 중에만 1° 이하 구간마다 판 OBB를 겹침 검사한다. 이때 그 구간의 회전을 덮는 여유(반지름 × 반각)를 더한다. 그래서 큰 Tick에서도 관통하지 않는다.
  - 박스(`AVBoRCarryObject::Mesh`)에 닿으면 마지막 안전 각에서 멈추고 대기 없이 열린다. 여러 박스 중 어느 것이든, 손에 든 박스도 본다(E6·E12).
  - 박스를 밀지 않는다. 시험은 끼임 뒤 박스 자세가 그대로이고 `IsPoseClear`인 것을 본다.
- **D6 버튼:** 판이 버튼 둘을 만들어 `HingePivot`에 붙인다. 버튼 이벤트에서는 명령만 받는다.
- **D7 키:** PageUp·PageDown이다. P1 `R`·`B`, P3 진행자 키와 겹치지 않는다.
- **D8 P1 시험대:** 트렁크 치수(바닥 75 cm + 안쪽 높이 70 cm)로 축·판·버튼 자리를 맞춘다. 기본 닫힘이고, 닫힌 게이트가 든 박스를 막는다(E12). `R`·`B`에서 시작 상태로 돌아간다.
- **D9 이벤트:** 상태 변경·열림·닫힘·끼임(박스 넘김) 넷이다.
- **범위:** 기존 시험 두 개(`ContactBlockageState`, `CylinderHullEntrance`)는 열린 입구를 재는 시험이라 게이트를 연 채 시작하게 했다. 단언은 그대로다. 짐 헤더에는 시험 friend 한 줄만 더했다. 명세 밖으로 커진 것이 없다.

## 항목

### R1-1 `B`에서 끼임 횟수가 0이 되고, 시험대 로그에 게이트가 없음 — 반영함 (fix: retain liftgate counts between trials)

**문제**
- `B`(다음 시도)는 `Gate->ResetPanel()`을 부른다(`P1LoadBench.cpp:157`). `ResetPanel`은 자세와 함께 `ObstructionCount`·`WarningRequests`도 0으로 만든다(`VBoRPoweredPanel.cpp:104-109`).
- 그런데 P1 시험대의 `B`는 "자리와 누적을 유지"한다(`P1LoadBench.cpp:155` 위 주석, P1 R1-6). 판넬의 다른 Total 값은 `B` 뒤에도 이어지는데, 게이트 끼임 수만 시도마다 사라진다. 06 "간섭 시 미완료"를 재는 값이다.
- 명세 §2-4는 "판넬·로그에 게이트 상태·끼임 횟수를 더한다"이다. 판넬에는 들어갔다. 하지만 시험대 로그(`RecordSnapshot`, `:416` — 시도 요약 `Trial N summary` 포함)에는 게이트가 없다. 게이트 자체 로그(`LogPoweredPanel`)에만 있어서, 시도별로 맞춰 보려면 두 로그를 시간으로 이어 붙여야 한다.

**제안**
- 자세·상태 되돌리기와 횟수 지우기를 나눈다(예: `ResetPanel(bool bClearCounts)`). `B`는 자세만, `R`은 둘 다 되돌린다(`R`은 원래 누적을 지운다).
- `RecordSnapshot` 한 줄에 게이트 상태·각도·끼임 수를 더한다. 그러면 시도 요약에도 들어간다.
- 판넬 "Recent events"에 끼임을 남기고 싶으면 `OnObstructed`를 묶어 `RecordEvent`로 한 줄 쓴다(선택). 진행자가 VR 판넬에서 바로 본다.

**확인할 시험**
- `BenchHeldBoxBlockingAndReset` 고침:
  - 끼임 1 → `B` → 게이트 닫힘, 끼임 수 1 그대로.
  - `R` → 0.
- 시도 요약 로그에 게이트 끼임 수가 들어가는지 확인한다(로그 수집은 기존 `P4aKeyTableTests`의 `FOutputDevice` 방식처럼 하면 된다).

## 참고 (항목 아님 — VR에서 볼 것)

- **열린 상태의 닫기 버튼:** 버튼은 닫힌 게이트 바깥 면 아래쪽(높이 약 85 cm)에 바깥을 향해 붙어 있다(명세 D6). 85°로 열리면 위를 향한 채 높이 약 140 cm, 입구 밖 약 60 cm로 간다. 위에서 아래로 눌러야 한다.
  - 실제 차처럼 열린 상태에서 뒤를 향하게 하려면 닫기 버튼을 판 아래 모서리 면에 둔다(닫힌 상태에서는 아래를 향함). 사용자 VR 의견을 듣고 정한다. 바꿀 값은 `CloseButtonFromHinge`이고 코드 구조는 그대로다.
- **입구에 바짝 붙은 박스:** 끼임 여유가 마지막 1° 구간에서 약 1 cm다. 그래서 박스 바깥면이 개구부 면에서 약 1 cm 안쪽까지 와 있으면 닫히지 않고 다시 열린다. 근사 형상에서는 안전한 쪽이다.
- **기본 닫힘:** `R`·`B` 뒤마다 게이트를 먼저 열어야(버튼 또는 PageUp) 박스를 넣을 수 있다. P1 시험 흐름이 바뀐 것을 VR 때 알고 시작한다.
- 명세 §7 순서대로 본다: 열기(1 s 대기·경고음) → 움직이는 중 입력 무시 → 박스 넣고 닫기 → 입구에 걸친 박스로 끼임 반전 → PageUp·PageDown → 버튼 자리·크기·각도·속도.

## Codex R1-1 반영 기록 (2026-10-15)

① 바뀐 파일
- Source/vBoR/Interaction/VBoRPoweredPanel.h/.cpp: ResetPanel(bool bClearCounts = true) 로 자세·상태 복원과 누적 삭제 분리.
- Source/vBoR/P1Probe/P1LoadBench.cpp: B 는 ResetPanel(false), R 은 기본 true. RecordSnapshot 에 게이트 상태·각도(deg)·끼임 누적 수 추가(시도 요약 포함).
- Source/vBoR/P1Probe/P4bPoweredPanelTests.cpp: BenchHeldBoxBlockingAndReset 에 B 누적 유지·닫힌 자세·소리 정지, R 누적 삭제, 실제 시도 요약/다음 시도 snapshot 로그 단언 추가.
- Docs/Reviews/P4b-power-liftgate.md: R1-1 반영 상태와 검증 기록. HANDOFF.md 는 git 무시 파일에 결과 기록.

② 명세 §6 완료 조건별 결과
1. §2의 1–5 유지. R1-1 의 B 누적 유지와 시험대 로그 누락 수정 완료.
2. §4 E1–E12 포함 P4b 12개 및 기존 vBoR 전체 171/171 Success, Fail 0, 발견 171개·완료 171개, TEST COMPLETE. EXIT CODE: 0. 실행 프로세스 exit 0.
3. 최종 트리 non-unity(-DisableUnity -Module=vBoR)·unity 각 1회 성공(Result: Succeeded, exit 0). vBoR 컴파일 경고·오류 0.
4. 보고 ①–⑤를 커밋 본문·HANDOFF.md 에 기록함.
5. Quest 3 시험 상태 not-tested 유지.

③ 명세와 다르게 한 것과 이유
- 없음. 리뷰 제안대로 초기화 bool 옵션을 추가했으며 기존 ResetPanel() 호출은 누적을 지우는 동작 유지. B 에서 끼임 수와 경고음 요청 누적을 보존하고, 대기·이동·경고음 재생 상태는 초기화함.
- 선택 사항인 Recent events 끼임 delegate 연결은 추가하지 않음. 필수 수정과 시험만 반영함.
- 하위 에이전트·자체 리뷰 루프·의도적 red·반복/스트레스 실행 없음. 최종 빌드 두 종류와 전체 시험 각 1회. 에디터/MCP 작업·레벨·에셋·설정 저장 없음. push·PR·머지 없음.

④ 엔진·플러그인 API 헤더 확인 (추정 아님)
- UE 경로 기준: C:/Program Files/Epic Games/UE_5.7/Engine/Source/.
- Runtime/Core/Public/Misc/OutputDevice.h:145 Serialize override.
- Runtime/Core/Public/Misc/OutputDeviceRedirector.h:70,77,88 AddOutputDevice/RemoveOutputDevice/FlushThreadedLogs. LogP1Probe 만 수집해 같은 시도 요약 한 줄에 상태·각도·끼임 수가 함께 있는지 검증함.
- Runtime/Engine/Classes/Components/AudioComponent.h:583,597 Stop/IsPlaying. 자세 복원 때 소리 정지 동작 유지·시험.
- 신규·변경 ISDK/MetaXR 호출 없음. ResetPanel 은 프로젝트 API 이며 선언/구현/호출을 함께 변경, UHT 및 실제 non-unity·unity 빌드로 확인함.

⑤ 로컬에서 확인할 것 / 검증 기록
- 빌드: Saved/Logs/P4bR1NonUnityBuild.log, P4bR1UnityBuild.log. non-unity 53 compile action, unity vBoR 4개 module compile. 두 빌드 모두 vBoR 경고·오류 0. 로컬 McpAutomationBridge/StructUtils 폐기 예정 의존 안내 각 2줄은 별도 환경 잡음으로 기록함.
- 자동화: Saved/Logs/P4bR1AllAutomation.log, 콘솔 P4bR1AutomationConsole.log. 샘플·McpAutomationBridge·UnrealAgent 비활성화. 171/171 Success, Fail 0, exit 0. 알려진 XR 확장·샘플 InfoCard CDO·합성 World 정리 잡음은 시험 판정과 구분함.
- 실제 로그: Trial 1 summary 에 gate state=Opening | gate angle=68.0 deg | gate obstructions=1. B 뒤 Trial 2 snapshot 에 Closed | 0.0 deg | 1. R 뒤 끼임·경고음 요청 수 0 을 자동화로 확인함.
- 에디터는 시작·종료 시 닫힘. Blueprint/GUI 동작·자식 값 적용은 not-tested. 기존 L_P1_GolfBag 에서 B 뒤 누적 수 유지와 R 삭제, 시도 요약을 확인할 것.
- Quest 3 Link/VR Preview: 명세 §7 열기→대기/경고음→움직임 중 입력 무시→닫기→박스 끼임 반전→PageUp/PageDown, 버튼 자리·크기·각도·속도와 B/R 반복 확인. 계획서 T-02·T-04·T-09 는 not-tested. 코드·합성 시험 통과를 기기/평가 동작 증거로 취급하지 않음.
- 브랜치 codex/P4b-power-liftgate. R1-1 반영 완료, 질문 없음. Claude 2회차 리뷰 대기.

## 질문

(없음)
