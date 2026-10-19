# 회사 PC 작업 안내 — 이 프로젝트를 처음 여는 AI에게

작성 2026-10-19 (Claude, 집 PC).

이 문서는 회사 PC에서 이 리포를 처음 여는 AI 에이전트가 맨 먼저 읽는다. 회사 PC는 고사양이고 실제 차 CAD가 있다.
- 리포 루트의 `HANDOFF.md`는 git에 올리지 않는다. 그래서 회사 PC에는 없다. 지금 상태는 이 문서 §2에 적었다.
- 상태가 바뀌면 이 문서를 고친다.

## 0. 읽는 순서

1. 이 문서
2. [AGENTS.md](../AGENTS.md): 지켜야 할 규칙(확정 제약·금지·작업 방식). 반드시 읽는다.
3. [Dev-Plan-50pct-VR.md](Dev-Plan-50pct-VR.md): 범위, 결정(U-01…), 시나리오 19개, 개발 단계, 시험 계획
4. [Research-Log.md](Research-Log.md): 결정의 근거. 최신 항목이 위에 있다. 정렬은 2026-10-19 항목을 본다.
5. [Agent-Workflow.md](Agent-Workflow.md): 빌드·시험 명령, 무시해도 되는 로그 잡음, 기록 위치
6. 맡은 작업의 [Specs/](Specs/)`<이름>.md`와 [Reviews/](Reviews/)`<이름>.md`

## 1. 프로젝트 한 줄

참가자가 번호 안내를 따라 가상 차를 둘러보고 간단히 조작하는 VR 평가 경험이다. 19단계(01–16, 18–20)를 마치면 자유모드로 넘어간다.
- **환경:** UE 5.7, Meta Quest 3 핸드 트래킹만 쓴다(컨트롤러 없음). PCVR(Link), Epic Native OpenXR.
- **평가 실행:** 에디터 VR Preview로 한다(2026-10-16 결정). 패키징 빌드는 지금 필요 없다.
- **실물:** 좌석뿐이다. 나머지는 모두 가상이다.

## 2. 지금 상태 (2026-10-19, `main` 기준)

- **들어가 있는 것:**
  - 손 입력: poke 버튼, 1축 부품(문·손잡이·레버·뚜껑), 스티어링, 룸미러, 노브, 박스 적재
  - 키·리프트게이트, 2열 폴딩, 19단계 안내 흐름과 바닥 경로, 좌석 시점 전환, 자유모드
  - 진행자 창, TV 관전 화면, 집 시험 도구
- **시험:** 자동화 시험 `vBoR.*` 424개는 집 PC에서 통과했다. Quest 3 실기 확인은 대부분 **`not-tested`**다. 집에서 일부를 보는 중이다.
- **레벨:**
  - `Content/P3/L_P3_Flow`: 통합 흐름. 근사 차 `AP3VehicleMock`을 쓴다.
  - `Content/P0/L_P0c_BuckEye`: Buck 정렬 저장
  - 나머지 P0–P2 레벨: 개발 시험대
- **근사 차는 실제 차가 아니다**(U-07). 부품 약 20개를 C++ `BeginPlay`에서 만들고 서로 잇는다(`Source/vBoR/P3Probe/P3VehicleMock.cpp`).
  - 자동화 시험 상당수가 근사 차로 돈다. 그래서 **지우지 않는다.**
  - 실제 차는 CAD 메시와 부품 Blueprint로 새 레벨(L_Main)에 만든다(§5).
- **집 시험 도구(콘솔 변수, 기본 꺼짐):**
  - `vBoR.HomeShift`: 키패드로 1 m씩 옮김
  - `vBoR.RouteIgnorePlayArea`: 플레이 영역 밖이어도 바닥 경로를 그림
  - **회사 평가에서는 켜지 않는다.**

## 3. 회사 PC에서 처음 설정

- **필요한 것:** UE 5.7, Visual Studio 2022 C++ 도구, Meta Quest Link
- **플러그인:**
  - Meta XR 1.205.0과 Interaction SDK 1.205.0은 리포의 `Plugins/` 안에 있다.
  - `Plugins/UnrealAgent`·`Plugins/McpAutomationBridge`는 집 PC 전용 도구라 리포에 없다. 없어도 빌드된다.
- **빌드:** [Agent-Workflow.md](Agent-Workflow.md) §2의 명령을 쓴다. non-unity(`-DisableUnity -Module=vBoR`)와 unity 둘 다 경고·오류 0이어야 한다.
- **시험:** `UnrealEditor-Cmd ... -ExecCmds="Automation RunTests vBoR.;Quit" -unattended -nullrhi -nosplash -nosound -DisablePlugins=OculusInteractionSamples,McpAutomationBridge,UnrealAgent`
- **빌드하는 동안 에디터를 열지 않는다.**
  - 2026-10-19 집에서 빌드 도중 에디터를 열었다. 그러자 엔진이 `-0001` DLL로 핫 리로드를 시도하다 크래시가 났다.
  - 크래시 보고 창이 떠 있으면 DLL을 잡아 다음 링크가 실패한다. 그 창을 닫고 다시 빌드한다.

## 4. CAD에서 확인할 값 — 회사 PC에서 처음 할 일

근사값은 모두 `Source/vBoR/P3Probe/P3VehicleMock.h`의 `자리값` 카테고리 프로퍼티(약 59개)에 있다. **그 목록이 곧 CAD에서 뽑을 값의 목록이다.** 아래는 묶음과 쓰는 곳이다.

| # | 묶음 | 뽑을 값 | 쓰는 곳 | 주의 |
|---|---|---|---|---|
| 1 | 좌표계 | CAD 원점, 축 방향, 단위, 설계 하중 상태의 지면 높이 | 모든 값의 변환 | 차량 좌표는 흔히 X가 뒤쪽이다. UE는 X 앞, Y 오른쪽, Z 위, cm다. **축부터 확인한다** |
| 2 | 좌석 기준점 | SgRP(설계 착좌 기준점, H-point): 운전석·동승석·2열 좌우. **SgRP를 정한 좌석 상태(슬라이드·등받이 각·높이 조절)** | 실물 좌석을 설계 위치에 두는 기준, **좌석 전환(14·15)** | 실물 좌석을 이 상태로 두고 정렬한다. 실물 좌석은 실제 차 높이에 맞춘다(U-17). SgRP는 공중 점이라 정렬에 직접 쓰지 않는다(U-18). 좌석 전환은 운전석 SgRP와 그 좌석 SgRP의 차이만큼 옮긴다(Research-Log 2026-10-20). 2열은 근사 차처럼 운전석 뒤 좌석이 기본 |
| 3 | 정렬용 실물 점 | **실물 좌석에서 손끝으로 짚을 수 있는 단단한 점 4개**와 그 CAD 좌표(U-18). 볼트 머리·모서리·레일 끝처럼 손끝이 한 자리에 멈추는 점 | 정렬(P4h 예정) | 스펀지·넓은 평면은 안 된다. 서로 멀리 떨어지고 한 줄로 놓이지 않게 고른다. 좌석 조절로 움직이지 않는 점(레일·프레임)이 좋다. 움직이는 점이면 그때 좌석 상태도 적는다. 위에서 눌러 짚을 수 있는 자리가 좋다 |
| 4 | 눈 위치 | 운전석·동승석·2열 설계 눈 위치 | `UVBoREyePointComponent`(근사 `DriverEyeCm` 등) | **선택.** 회사 차의 정렬·전환에는 쓰지 않는다(U-18 — 점 4개와 SgRP). 참고 표시와 대체용 눈 위치 저장에만 쓴다 |
| 5 | 차체 | 전장·전폭·전고, 바닥 윤곽 | `BodySizeCm`, `Car.Footprint`(경로가 차를 지나지 않게) | 경로 검사에 여유 20 cm |
| 6 | 문 4개 | 힌지 축(두 점), 최대 열림 각, 걸림 각, 안·밖 손잡이 회전축·각 | `AVBoRHingedDoor`, `AVBoRAxisPart` | 손잡이는 문에 붙어 같이 돈다(U-14) |
| 7 | 리프트게이트 | 힌지 축, 열림 각, 개구부 크기, 버튼 자리 | `AVBoRPoweredPanel`, `AVBoRPokeButton` | |
| 8 | 트렁크 | 적재 공간 바닥 크기·높이 | `AVBoRLoadTarget` | 박스 90° 맞춤 기준(P5n D4) |
| 9 | 2열 등받이 | 접힘 축, 접힘 각 | `AVBoRFoldingSeat` | |
| 10 | 스티어링 | 휠 중심, 축 방향(틸트), 지름, 림 두께, 최대 회전 각 | `AVBoRSteeringWheel` | |
| 11 | 레버 | 기어·방향지시 레버 회전축, 길이, 걸림 각 | `AVBoRAxisPart` | 휠 잡기 영역과 안 겹치게(P5n D1) |
| 12 | 룸미러 | 피벗, 거울면 크기, 좌우·위아래 한계 | `AVBoRTwoAxisPart` | |
| 13 | 노브 | 축, 지름, 칸 각 | `AVBoRKnob` | |
| 14 | 콘솔 뚜껑·글러브박스 | 힌지 축, 열림 각, 손잡이 | `AVBoRHingedPanel`, `AVBoRAxisPart` | |
| 15 | 버튼 | 시동·리프트게이트·도달(13번 대상) 버튼의 자리와 크기 | `AVBoRPokeButton` | 보이는 크기는 실제 그대로(AGENTS.md) |
| 16 | 메시 구성 | 움직이는 부품마다 같이 움직이는 메시 이름 목록, 피벗 상태 | 부품 Blueprint | 메시 피벗은 차 원점 하나다(U-09). 회전 중심은 부모의 축 컴포넌트로 정한다 |

**뽑은 값을 어디에 적을지:**
- 차량 수치를 이 리포에 적어도 되는지(회사 보안)는 **아직 정하지 않았다.**
- 정하기 전에는 회사 PC의 git 밖 파일에 적는다.
- 원래 CAD 값과 UE 축으로 바꾼 값을 둘 다 적는다.

## 5. 열린 결정

| 결정 | 지금 상태 |
|---|---|
| Buck 정렬 방법 | **정함.** 높이는 바닥 기준(U-17, `bHeightFromFloor`·`FloorWorldZCm`). 앞뒤·좌우·Yaw는 실물 좌석의 점 4개를 손끝으로 짚어 CAD 좌표에 맞춘다(U-18, Research-Log 2026-10-20). 구현은 P4h. 사람이 쓴 채 눈 위치를 저장하는 방식은 집 시험·대체용 |
| L_Main에서 정렬 저장 | 사용자 요청(2026-10-19). 지금은 `L_P0c_BuckEye`에서만 저장한다. P4h에서 진행자 창에 4점 정렬을 넣는다 |
| 실제 CAD 에셋을 리포에 올릴지 | **미정**(용량·보안). 정하기 전에는 올리지 않는다 |
| L_Main 구조 | 계획: CAD 메시는 그대로 두고, 부품 Blueprint를 레벨에 놓고, 태그로 잇는다(`Car.Footprint`, `Route.*`, 단계의 `MarkerTag`·`TargetTag`). 시나리오는 `DA_P3_Scenario`를 그대로 쓴다(두 벌이면 어긋남) — CAD 부품 Blueprint에 같은 태그를 붙인다. `L_Main`은 `L_P3_Flow`를 복제하고 GameMode만 `BP_vBoRGameMode`로 바꿔 시작한다(2026-10-20) |
| Pawn·GameMode | **만듦(P4f).** `AVBoRPawn`(손 메시·재질 교체 프로퍼티. ISDK 손은 본 이름 매핑이라 Meta 손 뼈대와 같은 본 이름이어야 그대로 움직임)과 `AVBoRGameMode`, `/Game/Main/Core/BP_vBoRPawn`·`BP_vBoRGameMode` |

## 6. 다음 작업 순서 (제안)

1. §4의 CAD 값 뽑기
2. 정렬 4점 방식(P4h, U-18)
3. ~~`AVBoRPawn`·`AVBoRGameMode`~~ — 끝(P4f)
4. L_Main(`L_P3_Flow` 복제)과 차량 Blueprint 틀
5. 부품 Blueprint 대조와 "Blueprint 만드는 법" 문서
   - 근사 차가 C++에서 이어 주던 연결을 레벨 Blueprint에서 어떻게 할지 부품별로 확인한다. 예: 박스 받침 → 적재 영역, 키 태그, 기어 끝 버튼.
   - 레벨에 놓은 Blueprint로 확인된 것은 문·손잡이(P2e)뿐이다.
6. 산출물:
   - 설치·정렬 절차서
   - 진행자 매뉴얼
   - 시험 기록(계획서 §7 T-ID)
   - P6 준비도 검토 자료

## 7. 지켜야 할 것 (요약 — 원문은 AGENTS.md)

- 회사 이름을 리포 문서에 쓰지 않는다. 계획서 내용(프로그램명·일정·회의)을 커밋·PR에 옮기지 않는다.
- 50% 범위만 만든다. 입력은 핸드 트래킹의 제약된 조작만 쓴다.
- 합격 수치는 만들지 않는다. Quest 3로 확인하기 전에는 시험 상태가 `not-tested`다.
- 역할: Claude가 명세와 리뷰, Codex가 구현이다(AGENTS.md "작업 방식"). 회사 PC에서 누가 무엇을 맡을지는 사용자가 정한다.
- 에디터가 열려 있으면 빌드하지 않는다. 빌드 중에는 에디터를 열지 않는다.
