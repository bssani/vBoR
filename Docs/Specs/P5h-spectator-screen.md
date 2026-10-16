# P5h 관전 화면 — TV에 참가자 시야 + 아래 띠(영어 미션·진행 막대·조작 중인 부품·자유모드)

| 항목 | 값 |
|---|---|
| 단계 | P5 자유모드·리허설(운영). 시험 T-08과 리허설 |
| 결정 근거 | **사용자 요청(2026-10-16):** TV에 연결해 사람들이 관전하므로 진행 상황을 보인다 — 지금 할 미션, 자유모드일 때는 자유모드 표시, **영어로**. **사용자 문답(2026-10-16):** 화면 = 참가자 시야 + 아래 띠. 띠 내용 = 진행 막대(몇 단계 중 몇) + **지금 조작하는 부품 이름**(예: 콘솔 박스를 조작하면 "Console Box"). 좌석·진행자 조작 표시·경과 시간은 넣지 않는다. **사용자 관찰(2026-10-16): TV에서는 VR에서 보이는 것보다 잘려 보인다. 비율을 4:3으로 맞춰야 VR에서 보는 범위가 보인다**(D3). 방식은 사용자의 다른 프로젝트 AI_Driving의 관전 UI(`C:\Git\AI_Driving\Source\AI_Driving\VR\VRSpectatorUISubsystem.*`)를 따른다 |
| 베이스 브랜치 | `main` @ P5g 머지(PR #21, `84be709`) 뒤 이 명세 갱신 커밋 |
| 작업 브랜치 | `codex/P5h-spectator-screen` |
| 리뷰 파일 | `Docs/Reviews/P5h-spectator-screen.md` (첫 리뷰 때 생김) |
| 상태 | 명세 — 구현 전 |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **관전 화면 경로 — AI_Driving 방식** | Slate 위젯을 렌더 타깃에 그리고(`FWidgetRenderer`, 엔진 `Runtime/UMG/Public/Slate/WidgetRenderer.h:49` `CreateTargetFor`, `:53-75` `DrawWidget`), 그 텍스처를 헤드셋의 **스펙테이터 화면**에 눈 영상과 함께 넘긴다. 눈 영상은 D3의 4:3 영역에, UI 텍스처는 화면 전체에 알파로 겹친다(`ISpectatorScreenController`, 엔진 `Runtime/HeadMountedDisplay/Public/ISpectatorScreenController.h:34`·`:42`·`:58`, 모드 `TexturePlusEye` `HeadMountedDisplayTypes.h:153`, 배치 구조체 `:158`). AI_Driving 구현 순서: `VRSpectatorUISubsystem.cpp:69-95`(위젯 → 렌더 타깃 → 배치 → 텍스처 → 모드), `:144-193`(준비될 때까지 0.5 s마다 30 s까지 다시 시도), `:195-226`(정해진 빈도로만 다시 그림), `:100-114`·`:228-233`(정리 때 모드를 눈 영상으로 되돌림). **AI_Driving 실측 두 가지를 지킨다:** ① 뷰포트에 올린 위젯(`AddToViewport`)은 헤드셋에만 가고 모니터에는 안 간다(AI_Driving `README.md:104-112`) — 그래서 뷰포트에 올리지 않는다. ② `BeginPlay` 때는 헤드셋·플레이어 컨트롤러가 아직 없어 대개 실패한다(`README.md:131-133`) — 그래서 다시 시도한다. 헤드셋이 없으면 아무것도 하지 않는다. 위젯은 C++ Slate로 만든다(에셋 없음, 러너의 패널처럼). 그리는 크기 1920 × 1080, 다시 그리는 빈도는 프로퍼티(예: 초당 15회 — 바뀌는 것이 문구·막대뿐) |
| D2 | **참가자 헤드셋 화면은 바뀌지 않는다** | 관전 UI는 헤드셋으로 가는 장면에 들어가지 않는다(D1 경로). 참가자는 지금과 똑같이 본다 |
| D3 | **시야는 4:3, 띠는 그 아래** | **왜 잘리나:** 엔진 스펙테이터는 눈 영상을 놓을 영역의 비율에 맞춰 가운데를 기준으로 잘라 채운다. 지금 기본 모드는 창 전체 비율(16:9 TV)로 자른다(엔진 `Engine/Plugins/Runtime/XRBase/Source/XRBase/Private/DefaultSpectatorScreenController.cpp:343-349` `GetEyeCroppedToFitRect`). 겹침 모드(`TexturePlusEye`)도 눈 영역 비율로 똑같이 자른다(`:268-275`). 눈 영상은 정사각형에 가까워서, 16:9로 자르면 위아래가 잘린다. **그래서 눈 영역을 4:3으로 잡는다**(사용자 관찰). 화면 위쪽 `1 − 띠 높이`(예: 띠 0.22 → 0.78) 안에 4:3 영역을 가운데 맞춰 둔다. 창 비율 A(= 가로/세로)에서 눈 영역 가로 비율은 `높이 × (4/3) / A`이고, 1을 넘으면 가로를 1로 하고 높이를 줄인다. 배치는 0–1 비율이라(`HeadMountedDisplayTypes.h:158`, 범위 검사 `:180-200`), **창 크기가 바뀌면(게임 창을 TV로 옮겨 전체 화면) 다시 계산해 넣는다.** 눈 영역 밖은 검게 지운다(`bClearBlack`). 비율 4:3과 띠 높이는 프로퍼티다. **띠는 눈 영역 아래 칸에 둔다** — 시야를 가리지 않는다. 띠 안: 왼쪽 위 큰 꼬리표(상태), 그 아래 큰 미션 문구, 오른쪽에 조작 중인 부품 줄, 맨 아래 진행 막대. TV에서 몇 m 떨어져 읽을 수 있게 크게 쓴다(1080 기준 예: 꼬리표 40 px, 미션 56 px). 색·글꼴 크기는 프로퍼티. 16:9 창 예: 눈 영역 x 0.2075–0.7925, y 0–0.78, 띠 y 0.78–1 |
| D4 | **상태별 내용** | 아래 표. 러너(`AVBoRScenarioRunner`)의 상태를 읽는다. 러너가 없는 레벨에서는 띄우지 않는다 |
| D5 | **영어 미션 문구** | 시나리오 단계에 영어 문구 칸을 더한다(`FVBoRScenarioStep`, `Interaction/VBoRScenarioAsset.h:13-25`, 예: `SpectatorText`). **비어 있으면 단계 ID로 코드의 기본표(§0-1)를 찾는다.** 칸을 배열에만 넣으면, 이미 저장된 `Content/P3/DA_P3_Scenario.uasset`을 불러올 때 새 칸이 비어 나오기 때문이다(구조체 기본값). 표에도 없는 ID면 "Mission in progress"를 쓴다. **TV에 한국어를 내지 않는다** |
| D6 | **지금 조작하는 부품** | 부품 부모들이 공통 인터페이스로 ① 관전용 영어 이름(인스턴스마다 정하는 FText 프로퍼티, 기본 비어 있음) ② "지금 쓰는 중"을 알린다. 쓰는 중의 뜻은 아래 §0-2 표. 관전 쪽은 **매 frame** 모든 부품을 보고(그리기는 D1 빈도), 쓰는 중인 부품을 **놓은 뒤 1.0 s 더** 보인다(버튼처럼 순간인 조작도 읽히게 — 프로퍼티). 이름이 같으면 하나만, 최대 둘(가장 최근 것부터), "Interacting: Steering Wheel · Gear Lever" 꼴. 없으면 그 줄을 숨긴다. **이름이 빈 부품은 보이지 않는다** — 러너의 확인·좌석 버튼처럼 차 부품이 아닌 것은 이름을 주지 않는다 |
| D7 | **시험할 수 있게 나눈다** | 상태 → 띠 문구를 만드는 부분과, 부품 → 이름 줄을 모으는 부분(1 s 남김·둘까지·중복 제거)을 헤드셋·렌더러 없이 시험할 수 있게 뺀다. 렌더 타깃·스펙테이터 화면 경로는 VR에서만 본다(§7) |

### 0-1. 상태별 띠 내용

| 러너 상태 | 꼬리표 | 미션 문구 | 진행 막대 |
|---|---|---|---|
| 시작 전(정렬 대기 등) | `GETTING READY` | (없음) | 0 / N |
| 순차 안내 k번째 단계 | `MISSION k / N` | 그 단계의 영어 문구(D5) | k−1 / N 끝남 |
| 자유모드 — 마지막 단계를 마치고 들어옴 | `FREE MODE` | `All missions complete. Explore the vehicle freely.` | N / N |
| 자유모드 — 진행자 `End`로 들어옴 | `FREE MODE` | `Explore the vehicle freely.` | 숨김 |

N은 그 시나리오의 단계 수(P3 기본 14)다. 원본 ID(01, 07…)는 관객에게 보이지 않는다(번호가 건너뛰어 헷갈림). 문구는 모두 프로퍼티로 바꿀 수 있다.

**영어 기본표(단계 ID → 문구):**

| ID | 문구 | ID | 문구 |
|---|---|---|---|
| 01 | Approach the vehicle with the key. | 11 | Close the door. |
| 02 | Check the exterior lights as you approach. | 12 | Look around the interior. |
| 03 | Walk around the vehicle. | 13 | Reach for the farthest button. |
| 04 | Open the liftgate. | 14 | Check the view from the passenger seat. |
| 05 | Load the boxes into the trunk. | 15 | Check the view from the rear seat. |
| 06 | Close the liftgate. | 16 | Check the seat folding angle. |
| 07 | Operate the door handle. | 18 | Open the door to get out. |
| 08 | Open the door. | 19 | Get out of the vehicle. |
| 09 | Get into the vehicle. | 20 | Close the door. |
| 10 | Sit in the seat. | | |

01·02는 지금 P3 기본 단계의 한국어 문구(`VBoRScenarioAsset.cpp:10`, `:12`)에 맞췄다. 계획서의 U-10 문구("테이블의 키를 집으세요" 등)로 단계가 바뀌면 그때 에셋의 영어 칸을 채운다.

### 0-2. "쓰는 중"과 근사 차 이름

| 부모 | 쓰는 중 | 근사 차 인스턴스 → 이름 |
|---|---|---|
| `AVBoRAxisPart` | 잡은 손이 있음(`VBoRAxisPart.h:128` `HoldingGrabber`) | 문 손잡이 둘(`OutsideAxisHandle`·`InsideAxisHandle`) → `Driver Door`, `GloveboxHandle` → `Glovebox`, `GearLever` → `Gear Lever`, `TurnSignalLever` → `Turn Signal Lever` |
| `AVBoRHingedPanel`(`AVBoRHingedDoor` 포함) | 미는 손이 있거나 손잡이를 잡음(`VBoRHingedPanel.h:61` 동작 상태, `:112`·`:119`) | `Door` → `Driver Door`, `ConsoleLid` → `Console Box`, `Glovebox` → `Glovebox` |
| `AVBoRSteeringWheel` | 잡은 손이 하나 이상(`VBoRSteeringWheel.h:85`) | `Steering Wheel` |
| `AVBoRTwoAxisPart` | 잡거나 미는 중(`VBoRTwoAxisMotion.h:13` `GrabHand`·`PushHand`) | `Rearview Mirror` |
| `AVBoRKnob` | 잡은 손이 있음(`VBoRKnobMotion.h:11`) | `Console Knob` |
| `AVBoRPokeButton` | 눌린 동안(ISDK 선택 중) | `GearButton` → `Gear Button`, `ReachButton` → `Button`. 러너의 확인·좌석 버튼은 이름 없음 |
| `AVBoRKeyFob` | 쥔 손이 있음(`VBoRKeyFob.h:46` `GetHoldingHand`) | 키 → `Key` |

이름은 근사 차(`AP3VehicleMock`)가 만들 때 넣는다. 부모 기본값은 비어 있다(보이지 않음). 박스(`AVBoRCarryObject`)·리프트게이트(`AVBoRPoweredPanel`)는 아직 흐름 레벨에 없어 이번에 넣지 않는다(P4 통합 때).

## 1. 왜 만드는가

- 평가를 TV로 관전한다(사용자). 지금 TV(데스크톱 창)에는 참가자 시야만 나와, 관객이 무엇을 하는 중인지 모른다.
- 한국어 안내 패널은 헤드셋 안에만 있다. 관객용은 영어다.

## 2. 범위

### 만들 것

**1. 관전 화면** — D1·D3·D4·D7. AI_Driving처럼 월드 서브시스템으로 두는 것을 권장한다(레벨을 다시 열면 새 월드에서 다시 시도함, P5g D8).
**2. 영어 미션 문구** — D5(단계 칸 + 기본표).
**3. 조작 중인 부품** — D6, §0-2의 부모 7개에 인터페이스·이름 프로퍼티, 근사 차 이름.
**4. 자동화 시험** — §4 전부.

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| 좌석 표시·진행자 조작 표시·경과 시간 | 사용자 문답(경과 시간은 빠르기 경쟁처럼 보일 수 있음) |
| 헤드셋 안 UI 바꾸기(한국어 패널·진행자 줄) | D2 |
| 뷰포트에 위젯 올리기(`AddToViewport`) | D1 — 헤드셋으로 감 |
| UMG 위젯 에셋·프로젝트 설정 화면 | C++ Slate로 충분하다. 꾸미기는 나중에 |
| 박스·리프트게이트 이름 | 흐름 레벨에 없음(§0-2) |
| 부품 동작 바꾸기 | 읽기 함수만 더한다 |
| 진행자 버튼 창 | 다음 명세 P5i |
| 레벨·에셋 저장(`DA_P3_Scenario` 포함) | 기본표로 충분하다(D5) |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- 기존 `vBoR.*`가 모두 그대로 통과한다. 기존 단언은 바꾸지 않는다.
- 부품 동작·소리·이벤트는 그대로다. 더하는 것은 이름 프로퍼티와 읽기 함수뿐이다.
- 헤드셋 화면에 관전 UI가 나오지 않는다(D2).
- 헤드셋이 없거나 자동화 시험(`-nullrhi`)에서는 관전 화면을 만들지 않고, 경고·오류 없이 지나간다.
- 시나리오 ID는 원본 그대로다(17번 없음).

## 4. 경계 상황과 그 시험

| # | 상황 | 기대 동작 | 자동화 시험 |
|---|---|---|---|
| E1 | 헤드셋 없음(자동화·평면 PIE) | 관전 화면을 만들지 않음. 뷰포트에 아무것도 올리지 않음. 경고·오류 없음 | 상태·로그 |
| E2 | 시작 전 | `GETTING READY`, 미션 없음, 0 / N | 띠 문구 |
| E3 | 3번째 단계(08) 진행 중 | `MISSION 3 / 14`, `Open the door.`, 막대 2 / 14 | 띠 문구 |
| E4 | 에셋 단계에 영어 칸이 있음 / 비어 있음 / 표에 없는 ID | 그 칸 / 기본표 / `Mission in progress` | 띠 문구 |
| E5 | 진행자 이전(←)·건너뛰기(→)·처음부터(F5) | 꼬리표·문구·막대가 그 단계로 바뀜 | 띠 문구 |
| E6 | 마지막 단계를 마침 / 진행자 `End` | `FREE MODE` + 완료 문구 + N / N / `FREE MODE` + 탐색 문구, 막대 숨김 | 띠 문구 |
| E7 | 노브를 잡음 → 놓음 → 0.5 s / 1.1 s 뒤 | `Interacting: Console Knob` → 그대로 → 숨김 | 이름 줄·시간 |
| E8 | 문 밀기 / 문 바깥 손잡이 잡기(손잡이·문 둘 다 쓰는 중) | `Driver Door` / `Driver Door` 한 번만 | 이름 줄 |
| E9 | 두 손: 휠 다음 기어 / 세 부품을 차례로 | `Gear Lever · Steering Wheel`(최근 먼저) / 가장 최근 둘 | 이름 줄 |
| E10 | 도달 버튼을 1 frame만 누름 | `Button`이 1.0 s 보임(매 frame 봄) | 이름 줄·시간 |
| E11 | 러너의 확인·좌석 버튼을 누름 | 이름 줄 없음 | 이름 줄 |
| E12 | 룸미러 밀기 / 글러브박스 손잡이 잡기 / 콘솔 뚜껑 손잡이 / 키 쥐기 / 기어 버튼 | `Rearview Mirror` / `Glovebox` / `Console Box` / `Key` / `Gear Button` | 부모별 쓰는 중 |
| E13 | 근사 차의 모든 이름 | §0-2 표와 같음 | 근사 차 |
| E14 | 기존 시험 | 모두 통과. 기존 단언은 바꾸지 않는다 | 전체 시험 |
| E15 | 눈 영역 계산: 16:9 / 4:3 / 21:9 / 1:1 창, 띠 0.22 | x 0.2075–0.7925·y 0–0.78 / x 0.11–0.89·y 0–0.78 / x 약 0.277–0.723·y 0–0.78 / 가로 0–1·높이 0.75. 모두 0–1 범위 안이고 엔진 배치 검사(`IsValid`)를 통과 | 배치 계산(헤드셋 없이) |
| E16 | 창 크기가 바뀜 | 다음 그리기 때 배치를 다시 계산해 넣음. 같으면 다시 넣지 않음 | 배치 갱신 |

부모별 "쓰는 중"은 그 부모의 기존 시험 방식(합성 잡기·밀기)으로 넣는다. E7·E10의 시간은 관전 쪽에 시간을 넣어 시험한다(실제 시계를 기다리지 않음).

## 5. 구현자가 정할 것

- 인터페이스·프로퍼티·서브시스템 이름(예: `IVBoRSpectatorPart`, `SpectatorName`, `UVBoRSpectatorSubsystem`), 띠 안 배치·색·글꼴 크기.
- 러너 상태를 읽는 방법(공개 읽기 함수 더하기). P5g의 `IsFreeMode()`와 진입 방식(완료·`End`)을 구분해 읽을 수 있어야 한다.
- 부품을 모으는 방법(월드의 Actor 순회를 처음 한 번 하고 생성·파괴에 맞춰 갱신하거나, 매 frame 순회 — 부품이 수십 개라 어느 쪽이든 된다).

## 6. 완료 조건

1. §2의 1–4가 들어 있다.
2. §4 시험이 모두 자동화로 통과한다. 기존 `vBoR.*`도 통과한다.
3. non-unity(`-Module=vBoR`)·unity 빌드에서 vBoR 경고·오류가 0이다. 전체 시험은 최종 트리로 한 번 돌린다.
4. AGENTS.md 보고 ①–⑤를 커밋 본문과 `HANDOFF.md`에 남긴다.
5. 시험 상태는 `not-tested`다.

## 7. 로컬에서 확인할 것 — 사용자

### Quest 3 (Link, VR Preview 또는 Standalone, `L_P3_Flow`) + TV
1. **먼저 볼 위험:** vBoR은 Native OpenXR이라 엔진 기본 스펙테이터 컨트롤러를 쓴다(`Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.cpp:2370-2383`). AI_Driving은 `XrApi` 설정이 없어 MetaXR의 OVRPlugin 경로로 돌았을 가능성이 크다(HANDOFF "XR API" 줄). 인터페이스는 같지만 실측은 다른 경로였다. 모니터(TV)에 띠가 나오고 **헤드셋에는 안 나오는지** 가장 먼저 본다.
2. TV의 4:3 시야와 헤드셋에서 보이는 범위가 비슷한지 본다(창은 16:9 그대로 두고, TV를 4:3으로 바꾸지 않아도 됨). 아직 잘리거나 너무 넓으면 비율 프로퍼티만 바꾼다.
3. TV 몇 m 뒤에서 꼬리표·미션·막대·부품 이름이 읽히는지 본다.
4. 단계를 진행하며 미션·막대가 바뀌는지, 자유모드에서 `FREE MODE`가 나오는지 본다.
5. 콘솔 뚜껑·노브·휠·문을 조작하며 이름이 나오고 놓은 뒤 1초쯤 남는지 본다.
6. F6으로 레벨을 다시 연 뒤에도 띠가 다시 나오는지 본다.
7. TV 연결: Windows 디스플레이를 **확장**으로 두고 게임 창을 TV 쪽에 둔다(복제로 두면 진행자 창 P5i도 TV에 나옴).

## 8. 확인한 API 근거

경로: `Source/vBoR/` 아래(`main` @ P5g 머지 `84be709`), 엔진은 `C:/Program Files/Epic Games/UE_5.7/Engine/Source/` 아래, AI_Driving은 `C:/Git/AI_Driving/` 아래.

| 쓸 것 | 위치 | 쓰임 |
|---|---|---|
| 위젯 → 렌더 타깃 | 엔진 `Runtime/UMG/Public/Slate/WidgetRenderer.h:49`, `:53-75` | D1 |
| 스펙테이터 화면 | 엔진 `Runtime/HeadMountedDisplay/Public/ISpectatorScreenController.h:34`·`:42`·`:58`, `HeadMountedDisplayTypes.h:153`·`:158` | D1 |
| OpenXR 기본 스펙테이터 | 엔진 `Engine/Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.cpp:2370-2383` | §7-1 |
| 눈 영상을 영역 비율로 자름 | 엔진 `Engine/Plugins/Runtime/XRBase/Source/XRBase/Private/DefaultSpectatorScreenController.cpp:268-275`(겹침 모드), `:343-349`(기본 모드) | D3 |
| 실측된 구현 | AI_Driving `Source/AI_Driving/VR/VRSpectatorUISubsystem.h:28-114`, `.cpp:43-233`, `README.md:104-112`·`:131-133` | D1 |
| 모듈 | `vBoR.Build.cs:13`(UMG·Slate·SlateCore·HeadMountedDisplay 이미 있음) | D1 |
| 러너 상태 | `Interaction/VBoRScenarioRunner.h:38`(`Scenario`), `:51-58`(읽기 함수 — `IsFreeMode()` `:54`. 자유모드이면서 `IsFinished()` 참 = 완료로 들어옴, 거짓 = `End`) | D4 |
| 단계 구조체·기본 단계 | `Interaction/VBoRScenarioAsset.h:13-25`, `Interaction/VBoRScenarioAsset.cpp:3-26` | D5 |
| 부품 상태 | §0-2 표의 줄 | D6 |
| 근사 차 부품 | `P3Probe/P3VehicleMock.h`(공개 부품 프로퍼티) | D6 |

## 개정 이력

| 날짜 | 개정 | 내용 |
|---|---|---|
| 2026-10-16 | 처음 | 작성(사용자 요청·문답: 시야 + 아래 띠, 진행 막대, 조작 중인 부품 이름, 영어) |
| 2026-10-16 | 베이스 갱신 | P5g 머지(`84be709`) 뒤 러너 줄 번호 맞춤, 자유모드 진입 방식 읽는 법(D4) |
| 2026-10-16 | 구현 전 개정 | 사용자 관찰(TV가 VR보다 잘려 보임, 4:3이면 맞음): D3 시야를 4:3 영역에 두고 띠는 그 아래로(시야를 가리지 않음), 창 크기 바뀌면 다시 계산. E15·E16 추가 |
