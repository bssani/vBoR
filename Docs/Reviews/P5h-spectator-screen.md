# P5h 리뷰 기록

명세: [Docs/Specs/P5h-spectator-screen.md](../Specs/P5h-spectator-screen.md) · 브랜치: `codex/P5h-spectator-screen`

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
| 1 | 2026-10-16 | `11c21aa` 관전 화면·영어 미션·조작 중 부품 | Claude | **열림 2건(R1-1·R1-2).** 둘 다 원인의 일부는 명세에 있다(각 항목에 적음).<br>빌드·시험 재현: 바뀐 파일의 수정 시각을 갱신해 다시 컴파일했다(non-unity 37개 파일). non-unity·unity 모두 성공, vBoR 경고·오류 0. `vBoR.*` **289/289**(P5h 13/13), EXIT CODE 0. 시험 뒤 `UnrealEditor` 프로세스 0개.<br>**Codex 토큰 6.53M(캐시 97%), 약 12분, 하위 에이전트 0, 도구 호출 42.** 첫 전체 시험이 287/289(새 시험의 준비 오류 2개)여서 빌드와 전체 시험을 한 번 더 돌렸다 |

### 1회차에 확인한 것

- **D1 경로:** AI_Driving 순서 그대로다.
  - 렌더 타깃 → 배치 → 텍스처 → 모드 순으로 넣는다(`VBoRSpectatorSubsystem.cpp:60-79`). 뷰포트에는 올리지 않는다.
  - 0.5 s마다 다시 시도하고 30 s에 멈춘다(`:93-106`).
  - `-nullrhi`에서는 대기 자체를 켜지 않는다(`:56`, `FApp::CanEverRender`).
  - 정리 때 텍스처를 비우고 눈 영상 모드로 되돌린다. renderer는 지연 정리한다(`:182-196`).
- **AI_Driving과 다른 점(문제 없음):** 매 frame 헤드셋이 꺼졌는지 보고, 꺼지면 멈춘 뒤 다시 시도하지 않는다(`:109`). 그래서 OpenXR에서 언제 꺼지는지 엔진 코드로 확인했다.
  - 참가자가 헤드셋을 벗어 세션이 `STOPPING`이 되면 세션만 멈춘다. 스테레오는 켜진 채다(엔진 `Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.cpp:3961-3969`). `IsHMDEnabled`는 늘 참이다(`:619-622`).
  - 스테레오가 꺼지는 것은 `LOSS_PENDING`·`EXITING`(Link 끊김)뿐이다(`:3971-3985`, `DestroySession` 안 `:2502`). 그때는 VR 자체가 끝난다.
  - 그래서 참가자를 바꾸느라 헤드셋을 벗겨도 띠는 남는다.
- **D3 배치:**
  - 계산식은 명세와 같다(`VBoRSpectatorModel.cpp:73-86`). 네 가지 창 비율이 E15를 통과하고 엔진 `IsValid`도 통과한다.
  - 창 크기는 뷰포트 크기가 아니라 창의 client 크기다(`VBoRSpectatorSubsystem.cpp:35-45`). 맞는 선택이다. 에디터 VR Preview에서는 엔진이 뷰포트 크기를 눈 렌더 크기로 고정한다(엔진 `OpenXRHMD.cpp:1086` `SetIndependentViewportSize`).
  - 배치는 다시 그릴 때만 다시 계산하고, 같으면 다시 넣지 않는다(`:117-118`, E16).
- **D4·D5:**
  - 상태표는 `VBoRSpectatorModel.cpp:25-45`에 있다. 자유모드에 어떻게 들어왔는지는 `IsFinished()`로 가른다.
  - 영어 기본표는 원본 ID 19개다(17 없음, `:4-23`). 에셋 칸 → 기본표 → "Mission in progress" 순으로 찾는다.
- **D6:**
  - 부모 7개 모두 인터페이스를 단다. "쓰는 중"은 각 부모가 이미 가진 상태를 읽는다(`VBoRAxisPart.h:75`, `VBoRHingedPanel.h:59`, `VBoRSteeringWheel.h:58`, `VBoRTwoAxisPart.h:53`, `VBoRKnob.h:56`, `VBoRPokeButton.cpp:124`, `VBoRKeyFob.h:45`).
  - 휠은 실제로 잡은 손의 목록(`HandOrder`)을 읽는다. 길이 2로 미리 만든 배열을 읽지 않으므로 맞다(Codex 보고 ③).
  - 수집은 그리기 빈도와 관계없이 매 frame 한다(`VBoRSpectatorSubsystem.cpp:111`).
  - 이름 줄(`VBoRSpectatorModel.cpp:47-71`):
    - 같은 이름은 하나만 보인다.
    - 다시 잡으면 가장 최근이 된다.
    - 놓은 뒤에는 유지 시간만큼 남는다.
    - 빈 이름은 무시한다.
- **근사 차 이름:** 15개가 §0-2 표와 같다(E13). 키의 자식 버튼과 러너 버튼은 이름이 없다.
- **범위:**
  - 러너·부품 동작은 바뀌지 않았다. 바뀐 것은 읽기 함수와 이름 칸뿐이다.
  - 저장한 에셋·레벨·설정은 없다.
  - `RenderCore` 의존은 `BeginCleanup` 때문에 필요하다(엔진 `Runtime/RenderCore/Public/RenderDeferredCleanup.h:32`).
  - 기존 시험 단언은 바뀌지 않았다(289 = 276 + 13). 주석은 사용자 말투다.
- **명세 실수 — E3:** 명세는 "3번째 단계(08)"라고 했다. 하지만 실제 기본 순서는 01·02·07·08이라 08은 4번째다(`VBoRScenarioAsset.cpp:10-13`).
  - Codex는 시험 시나리오에서만 순서를 바꿔(`P5hSpectatorTests.cpp:53`) 명세 문구대로 시험했다. 계산 경로가 같으므로 시험은 고치지 않아도 된다.
  - 명세 E3을 바로잡았다.
- **참고 — 고치지 않아도 됨:** UI는 1920×1080으로 그려 창 전체에 늘인다(`VBoRSpectatorSubsystem.cpp:70`).
  - 창이 16:9가 아니면(VR Preview 창을 마음대로 늘린 경우) 글자가 가로로 늘거나 줄어든다.
  - 띠 높이 비율은 그대로라 시야와 겹치지는 않는다. TV는 16:9라 문제없다.
- **참고:** 이름이 셋 이상이면 최근 둘만 보인다. 그래서 놓은 지 1 s가 안 된 부품이 아직 잡고 있는 부품보다 앞에 설 수 있다. 명세("가장 최근 것부터")대로다.

## 항목

### R1-1 관전 화면 값(비율·띠 높이·글꼴·문구)을 바꿀 곳이 없음 — 반영함 (fix(p5h): configure and fit spectator banner)

**문제:**
- 값은 `UVBoRSpectatorSubsystem`의 `EditAnywhere` 프로퍼티다(`VBoRSpectatorSubsystem.h:18-34`).
- 하지만 월드 서브시스템은 에디터 Details에 나오지 않는다. 클래스가 `Config`도 아니고(`:13` `UCLASS()`) Blueprint 자식도 만들 수 없다. 그래서 값을 바꾸려면 C++ 기본값을 고쳐 다시 빌드해야 한다.
- 명세 §7-2는 VR 확인 때 사용자에게 "비율 프로퍼티만 바꾼다"고 한다. §7-3은 TV 거리에서 글꼴 크기를 맞추는 확인이다. 지금은 둘 다 사용자가 혼자 할 수 없다.
- **원인 일부는 명세다.** "프로퍼티"라고만 쓰고 어디서 바꾸는지 적지 않았다. 서브시스템도 명세가 권장했다(§2-1).

**제안:**
- 가장 단순하게 `UCLASS(Config=Game)`로 하고, 값 프로퍼티에 `Config`를 단다.
  - 사용자는 `Config/DefaultGame.ini`의 `[/Script/vBoR.VBoRSpectatorSubsystem]` 절에 한 줄 넣고 다시 실행한다.
  - 새 서브시스템 인스턴스는 CDO 값을 받으므로 지금 코드 경로는 그대로다.
- 설정 화면(`UDeveloperSettings`)이나 콘솔 명령은 만들지 않는다(명세 §2 범위 밖 "프로젝트 설정 화면"). ini 파일도 고치지 않는다. 기본값은 코드에 둔다.
- 플래그: 엔진 `Runtime/CoreUObject/Public/UObject/ObjectMacros.h:207` `CLASS_Config`, `:433` `CPF_Config`.

**확인할 시험:**
- 클래스에 `CLASS_Config`가 있다. 비율·띠 높이·이름 유지 시간·다시 그리는 빈도·글꼴 크기·문구·색 프로퍼티에 `CPF_Config`가 있다.
- CDO의 `EyeAspect`·`BandHeight`를 바꾸면 새로 만든 게임 월드의 서브시스템이 그 값을 쓴다. 시험 끝에 CDO 값을 되돌린다.

### R1-2 자유모드 완료 문구가 두 줄로 넘어가 진행 막대를 덮음 — 반영함 (fix(p5h): configure and fit spectator banner)

**문제:**
- 띠는 1080 기준 237.6 px이고, 위아래 여백 12를 빼면 213.6 px다(`VBoRSpectatorSubsystem.cpp:125-130`).
- 미션 칸은 가로의 70%라 1275 px다(`:136`, 좌우 여백 32와 오른쪽 24를 뺌).
- 엔진 Roboto로 쟀다(Slate는 96 DPI로 그림 — 엔진 `Runtime/SlateCore/Public/Fonts/SlateFontInfo.h:15` `RenderDPI`):
  - 자유모드 완료 문구 "All missions complete. Explore the vehicle freely."는 50 pt에서 **1463 px**다. 칸을 넘으므로 두 줄로 접힌다(`:147` `AutoWrapText`).
  - 줄 높이는 꼬리표 40 pt가 62.5 px, 미션 50 pt가 78.1 px, 막대 줄 24 pt가 37.5 px다. 미션이 두 줄이면 합이 **260 px**로 213.6 px를 넘는다.
  - 미션 칸은 남는 높이만 받는다(`:145` `FillHeight`). 그래서 둘째 줄이 아래 진행 막대와 "14 / 14" 위에 겹친다.
  - "Check the exterior lights as you approach."도 1262 px로, 칸 폭에서 13 px만 남는다.
- 자유모드는 참가자가 가장 오래 머무는 상태다. 그래서 관객은 겹친 화면을 가장 오래 보게 된다.
- **원인 일부는 명세다.** D3에 "오른쪽에 부품 줄"과 큰 글꼴(미션 56 px 예)을 함께 적으면서 문구 폭을 재지 않았다. 단위도 px라고 썼지만 Slate 글꼴 크기는 pt다.

**제안:**
- 부품 줄을 꼬리표와 같은 줄의 오른쪽으로 옮기고, 미션 문구가 띠 가로 전체(1856 px)를 쓰게 한다.
  - 가장 긴 꼬리표 "MISSION 14 / 14"는 40 pt Bold로 397 px다. 그 옆에 가장 긴 부품 줄 "Interacting: Turn Signal Lever · Rearview Mirror"(32 pt, 907 px)가 한 줄로 들어간다.
  - 미션이 한 줄이면 세로 합이 182 px라 213.6 px 안에 들어간다.
- 미션은 한 줄로 두고, 넘치면 말줄임으로 끝낸다. 에셋에 긴 영어 문구를 넣어도 막대를 덮지 않게 하기 위해서다. 부품 줄도 같다.
  - 엔진 `Runtime/Slate/Public/Widgets/Text/STextBlock.h:148` `OverflowPolicy`, `Runtime/SlateCore/Public/Styling/SlateTypes.h:78-84` `ETextOverflowPolicy::Ellipsis`.
- 글꼴 크기는 지금 값(40·50·32)을 그대로 둬도 된다.

**확인할 시험:**
- Slate 글꼴 측정은 `-nullrhi` 자동화에서도 동작한다. 이번 시험 로그에 `LogSlate: SlateFontServices - WITH_FREETYPE: 1`이 있다.
  - 엔진 `Runtime/SlateCore/Public/Rendering/SlateRenderer.h:87` `GetFontMeasureService`로 측정기를 얻는다.
  - `Fonts/FontMeasure.h:39` `Measure`로 폭을, `:172` `GetMaxCharacterHeight`로 줄 높이를 잰다.
- 원본 ID 19개의 기본 문구와 완료·탐색·"Mission in progress"가 기본 글꼴로 미션 줄 폭 안에 한 줄로 들어간다.
- 가장 긴 꼬리표와, 근사 차 이름 두 개로 만든 가장 긴 부품 줄이 꼬리표 줄에 함께 들어간다.
- 세 줄 높이(꼬리표 줄·미션·막대 줄)와 여백의 합이 띠 높이 이하다.
- 폭·높이·여백은 위젯이 쓰는 값에서 읽는다. 위젯을 바꾸면 시험도 따라가게 하기 위해서다.

### Codex 반영 결과 — 2026-10-16

**완료.** R1-1·R1-2 반영, 질문 없음. 커밋 제목: `fix(p5h): configure and fit spectator banner`.

① **바뀐 파일**
- `Source/vBoR/Spectator/VBoRSpectatorSubsystem.h`: `Config=Game`와 설정 프로퍼티 17개의 `Config`. 위젯과 시험이 같은 렌더 크기를 읽도록 `DrawSize`를 클래스 안으로 옮김.
- `Source/vBoR/Spectator/VBoRSpectatorSubsystem.cpp`: 꼬리표·부품 이름 / 전체 폭 미션 / 진행 막대의 세 줄. 미션·부품 이름은 한 줄·말줄임.
- `Source/vBoR/P3Probe/P5hSpectatorTests.cpp`: E17 글꼴·실제 위젯 배치 측정, E18 Config 플래그·CDO 상속 추가. 기존 13개 시험·단언 유지.
- `Docs/Reviews/P5h-spectator-screen.md`: 두 항목 상태와 이 보고. `HANDOFF.md`는 결과만 갱신(git 제외).

② **명세 §6 완료 조건**
1. §2의 1–4: 기존 관전 경로·영어 미션·부품 수집을 유지하며 R1-1·R1-2와 시험 추가 완료.
2. E17·E18 포함 P5h **15/15**, 기존 시험 포함 전체 **291/291** 통과. E17은 원본 19개 기본 문구·완료·탐색·fallback, 가장 넓은 꼬리표·부품 두 이름, 긴 에셋 문구를 실제 `MakeWidget`에 넣음. 위젯의 글꼴·할당 폭·행 높이·border 여백을 읽어 세 줄이 띠 안에 들어가는지 확인. E18은 설정 17개의 플래그와 변경한 CDO 값이 새 게임 월드 서브시스템에 상속되는지 확인한 뒤 복구.
3. 최종 트리 non-unity(`-DisableUnity -Module=vBoR`)·unity 모두 `Result: Succeeded`, 프로세스 exit 0, vBoR 컴파일 경고·오류 0. 로컬 도구 플러그인의 StructUtils 폐기 안내는 별도 환경 잡음.
4. 이 보고를 커밋 본문·HANDOFF에 기록.
5. Quest 3·TV·VR 기능 시험 상태 **not-tested** 유지.

**최종 검증 로그**(`Saved/Logs/`, git 제외):
- `P5hR1-FinalNonUnity.log`: `Result: Succeeded`, exit 0.
- `P5hR1-FinalUnity.log`: `Result: Succeeded`, exit 0.
- `P5hR1-Full.log`: 발견 291, 완료 291, Success 291, Fail 0, P5h 15/15, `TEST COMPLETE. EXIT CODE: 0`, 프로세스 exit 0. 전체 시험 1회. P0Buck.E8Persistence도 성공하여 재실행 없음.
- 자동화 종료 뒤 `UnrealEditor*` 프로세스 0개.
- 중간 확인: `P5hR1-Focused.log` P5h 15/15, exit 0. 이때 임시 E3 수정이 들어 있었으며, 최종 전체 시험에는 원래 E3 시험으로 복구됨.

③ **명세와 다르게 한 것과 이유**
- 제품 구현은 제안대로 반영. ini·설정 화면·콘솔 명령·에셋 변경 없음. 관전 경로·4:3 계산·러너·부품 동작 변경 없음. MCP·하위 에이전트·자체 리뷰 없음.
- 절차상 추가 빌드 발생: 명세 E3 표에 맞춰 기존 시험을 바꿨다가, 개정 이력의 기존 시험 유지 지시를 늦게 확인해 취소함. 중간 트리 non-unity·unity와 P5h 확인 후, 복구한 최종 C++ 트리로 non-unity·unity를 다시 확인함. 전체 시험은 최종 트리에서 한 번만 실행. 초기 로그는 `P5hR1-NonUnity.log`·`P5hR1-Unity.log`.

④ **API 헤더 확인**
엔진 기준 경로: `C:/Program Files/Epic Games/UE_5.7/Engine/Source/`.
- `Runtime/CoreUObject/Public/UObject/ObjectMacros.h:207` CLASS_Config, `:433` CPF_Config.
- `Runtime/Slate/Public/Widgets/Text/STextBlock.h:115` AutoWrapText, `:148` OverflowPolicy, `:180` GetText, `:274` GetFont.
- `Runtime/SlateCore/Public/Styling/SlateTypes.h:78` ETextOverflowPolicy(Ellipsis).
- `Runtime/SlateCore/Public/Rendering/SlateRenderer.h:87` GetFontMeasureService.
- `Runtime/SlateCore/Public/Fonts/FontMeasure.h:39` Measure, `:172` GetMaxCharacterHeight.
- `Runtime/SlateCore/Public/Widgets/SWidget.h:669` SlatePrepass, `:849` ArrangeChildren.
- `Runtime/SlateCore/Public/Layout/Geometry.h:197` MakeRoot.
새 플러그인 API 사용 없음. 위 API는 헤더와 실제 로컬 빌드·시험으로 확인했으며 추정에 의존하지 않음.

⑤ **로컬에서 확인할 것**
- 빌드·headless 자동화는 위 로그로 확인 완료.
- Blueprint·에셋·레벨 작업 없음. ini 값 변경 실사용은 명세 §7의 DefaultGame.ini 절을 사용자가 넣고 다시 실행해 확인(이번에는 ini를 쓰지 않고 CDO 상속만 자동화).
- Quest 3 Link + TV, 명세 §7 전체: 띠가 TV에만 보이는지, 4:3 시야, 몇 m 거리 글꼴 가독성·말줄임, 미션·진행 막대·FREE MODE 전환, 부품 이름과 해제 후 유지 시간, F6 재진입, 확장 디스플레이 배치. 계획서 **T-08 및 리허설 — not-tested**.

## 질문

(없음)
