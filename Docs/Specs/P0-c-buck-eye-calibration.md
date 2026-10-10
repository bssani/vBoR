# P0-c Buck eye position 정렬 (설치 때 한 번)

| 항목 | 값 |
|---|---|
| 단계 | P0 환경·기준 확인 — "Buck 정렬 확인 방법을 정한다", "물리 접촉 시험 전 정렬 확인"(계획서 §6) |
| 대상 | T-01 데이터·Buck 정렬(이 실험은 T-01 정식 시험이 아니다). 나중에 T-05(기준 정렬로 복귀)가 이 저장값을 쓴다 |
| 결정 근거 | U-07, 조사 기록 2026-10-10 "Buck 정렬 — eye position", AGENTS.md 확정 제약 |
| 베이스 브랜치 | **P1 머지 뒤의 `main`** (P1 다음 순서) |
| 작업 브랜치 | `codex/P0-c-buck-eye-calibration` |
| 리뷰 파일 | `Docs/Reviews/P0-c-buck-eye-calibration.md` (첫 리뷰 때 생김) |
| 환경 | PCVR(Link), Epic Native OpenXR, 추적 원점 Stage(UE 5.7 OpenXR가 Stage를 먼저 고른다, `OpenXRHMD.cpp:2332-2335`), WorldToMeters 100 |
| 상태 | 확정(2026-10-10) — **P1 다음**. 시험 상태 `not-tested` |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **Buck 설치 때 한 번**(U-07, 2026-10-10 사용자) | 참가자마다 다시 맞추지 않는다. 참가자의 눈에 차를 맞추면 13번 도달성·05번 자세 부담에서 재려는 체격 차이가 지워진다 |
| D2 | **기준은 지그에 둔 헤드셋 한 점 + 그 순간의 정면(Yaw)** | 실물 Buck의 설계 eye position 자리에 헤드셋을 고정하는 받침(지그)을 두고, 진행자가 PC 키로 저장한다. 두 번째 기준점은 이번 범위 밖 — 반복성 실측을 보고 정한다 |
| D3 | **저장값은 추적 공간(Stage) 기준** | 저장하는 것은 "Stage 공간에서 지그 위 헤드셋의 위치(cm)와 Yaw(deg)"다. 월드 좌표가 아니다. 레벨·차량 배치가 바뀌어도 같은 값을 다시 쓴다 |
| D4 | **Pawn(VR 원점)을 옮긴다. 차량은 움직이지 않는다** | 레벨의 차량·부품 배치를 그대로 둔다. 나중의 좌석 시점 전환(T-05)도 Pawn 이동 위에 얹힌다 |
| D5 | **Yaw와 위치만 맞춘다. Pitch·Roll은 0** | 지그 위 헤드셋이 조금 기울어 있어도 가상 차·바닥이 기울지 않게 한다 |
| D6 | **차량 eye position은 차량 쪽에 표시 부품으로 둔다** | 차량 Blueprint 안 설계 eye position 자리에 놓는 Scene Component. 지금은 차량 데이터가 없어 근사 형상과 자리값으로 시험한다(U-07) |
| D7 | **진행자 PC 키보드로 저장** | 헤드셋이 지그에 있으니 손 조작은 쓸 수 없다. 키 배치는 이 시험대 전용이다(실제 진행자 키 배치는 미정, AGENTS.md) |

## 1. 왜 만드는가

VR에서 실물은 좌석뿐이다. 가상 좌석이 실물 좌석과 어긋나면 참가자는 보이는 좌석과 다른 곳에 앉고, 손 도달(13번)·자세(05번) 관찰의 기준이 틀어진다. 아래를 확인한다.

- **반복성:** 같은 지그에 헤드셋을 다시 놓았을 때 저장값과 몇 mm·몇 deg 차이 나는가. 사람이 헤드셋을 놓는 오차까지 포함한 값이다.
- **지속성:** VR Preview 재시작, 에디터 재시작, Link 재연결, 헤드셋 절전·재부팅, 다음 날에도 저장값이 맞는가. UE 5.7 OpenXR의 Stage 원점은 Quest 가디언 기준이다. 가디언을 다시 그리지 않으면 원점이 그대로인지 실측으로 본다.
- **변화 감지:** 실행 중 재중심(Meta 버튼 길게 누르기)·가디언 변경을 앱이 알아채는가. OpenXR `XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING`이 오면 UE가 `FCoreDelegates::VRHeadsetRecenter`를 보낸다(`OpenXRHMD.cpp:4017-4029`). Link에서 실제로 오는지는 모른다.
- **실물 대조:** 정렬 뒤 손끝으로 실물 표시점을 짚었을 때 eye position 기준 좌표가 줄자로 잰 값과 맞는가. 헤드셋 안 시점 원점(OpenXR VIEW 공간, 두 눈 사이)과 지그가 헤드셋을 받치는 자리 사이의 오프셋도 이 대조에 함께 드러난다.

## 2. 범위

### 만들 것

**다시 쓰는 부품** — `Source/vBoR/Interaction/` (엔진·XRBase에만 의존)

1. **`UVBoREyePointComponent`** (Scene Component)
   - 차량 Actor 안 설계 eye position 자리를 표시한다. 컴포넌트의 +X가 차량 정면이다.
   - 에디터에서 위치·방향이 보이게 한다(방법은 구현자).
   - 레벨에 하나만 있어야 한다. 0개나 2개 이상이면 정렬을 적용하지 않고 이유를 표시한다.

2. **정렬 계산·저장** (이름은 구현자. 예: `FVBoRBuckAlignment` + 저장 클래스)
   - **캡처:** 헤드셋 자세 샘플 여러 개 → 평균 위치(cm, Stage 공간), 평균 Yaw(deg), 창 안 흩어짐(mm, deg), 사용한·버린 frame 수.
     - Yaw는 헤드셋 정면 벡터를 수평면에 투영해서 구한다. 수평 성분이 너무 짧으면(헤드셋이 거의 위·아래를 봄) 거부한다.
     - Yaw 평균은 각도를 그냥 더하지 않는다(±180 경계).
   - **Pawn 변환 계산:** 저장값과 eye point 월드 자세(Yaw·위치만)로 Pawn 변환을 구한다. 이때 "지그 위 헤드셋의 월드 위치 = eye point 위치", "헤드셋 정면 Yaw = eye point +X Yaw"가 되어야 한다. 결과의 Pitch·Roll은 0이다.
   - **저장·불러오기:** 이 PC의 `Saved/` 아래(git에 안 올라감, `.gitignore:68`)에 저장한다. 저장 항목:
     - 형식 버전, 저장 시각
     - 추적 원점 종류(`GetTrackingOrigin`)
     - Stage 공간 위치(cm)·Yaw(deg)
     - 캡처 창 흩어짐
     - Stage 플레이 영역 크기(`GetPlayAreaBounds(Stage)`, cm. 못 얻으면 0)
   - **엔진 없이 시험할 수 있게** 계산부를 순수 함수·구조체로 나눈다.

**P0-c 전용 시험대** — `Source/vBoR/P0Probe/`

3. **`AP0BuckBench`**
   - 레벨에 놓인 자리 그대로 **고정**된 근사 차량을 만든다. 머리 위치로 배치하지 않는다 — P0-b·P1 시험대와 다르다.
   - 근사 차량 구성(값은 모두 프로퍼티, 화면에 "근사 형상·자리값" 표시):
     - `UVBoREyePointComponent` 하나
     - eye point 표시(작은 십자 + 정면 화살표)
     - 좌석 방석·등받이 상자
     - 핸들 고리
     - 대시보드 판
     - 가상 지면 격자
   - 자리값(eye point 기준, cm): 방석 윗면 중심 (−15, 0, −65), 등받이 앞면 X −35, 핸들 중심 (+45, 0, −20)·지름 38, 대시보드 X +70. eye point는 가상 지면 위 Z 120이다. 실제 차량 값이 오면 바꾼다.
   - 시작 시 저장값을 불러와 적용한다. 상태는 아래 중 하나다:
     - **저장값 없음:** Pawn은 PlayerStart 그대로
     - **적용됨:** 저장 시각을 표시
     - **확인 필요:** 이유를 함께 표시
   - 키(프로퍼티):

     | 키 | 동작 |
     |---|---|
     | `C` 길게 누름 | 저장. 누르는 동안 자세를 모으고 끝까지 누르면 저장·적용한다. 중간에 떼면 취소 |
     | `M` | 측정만. 짧은 창으로 평균한 지금 헤드셋 자세와 저장값의 차이를 기록한다. 저장하지 않음 |
     | `T` | 손끝 대조. 양손 검지 끝(추적된 원래 위치, `AVBoRHandRigPawn::ReadRawIndexTip`)을 eye point 기준 좌표(mm, X 앞·Y 오른쪽·Z 위)로 기록한다 |

   - `VRHeadsetRecenter`를 받으면 상태를 "확인 필요(추적 공간 바뀜)"로 바꾸고 기록한다. Pawn을 자동으로 다시 옮기지 않는다.

4. **`AP0BuckBenchGameMode`** — 기본 Pawn은 `AVBoRHandRigPawn`이다. P0-b·P1 설정을 건드리지 않는다.

### 측정·표시할 것 (패널 + `LogP0Buck`)

**패널은 헤드셋을 지그에 둔 채 PC 모니터(VR Preview 미러 창)에서 읽을 수 있어야 한다.** 진행자는 그때 헤드셋을 쓰고 있지 않다.

| 항목 | 단위 | 언제 |
|---|---|---|
| 상태(저장값 없음·적용됨·확인 필요 + 이유) | — | 항상 |
| 추적 원점 종류, 헤드셋 TRACKED 여부 | — | 항상 |
| Stage 플레이 영역 크기: 지금 / 저장값 | cm | 항상 |
| 지금 헤드셋과 저장 eye point의 차이: 위치 X·Y·Z와 거리, Yaw | mm, deg | 항상(실시간) + `M` 때 평균값을 로그로 |
| 캡처 결과: 평균 위치·Yaw, 창 안 흩어짐, 사용·버린 frame 수, 이전 저장값과의 차이 | cm, mm, deg | `C` 저장 때 |
| 적용한 Pawn 변환, Pawn Z(물리 바닥과 가상 지면의 높이 차) | cm, deg | 적용 때 |
| 손끝 대조 좌표(손별, 추적 안 되면 "추적 안 됨") | mm | `T` 때 |
| `VRHeadsetRecenter` 받은 횟수·시각 | — | 받을 때 |

### 범위 밖 — 하지 말 것

- **참가자별 정렬.** 참가자의 머리 위치로 정렬을 계산하는 경로를 만들지 않는다(AGENTS.md 확정 제약).
- 두 번째 기준점·여러 점 맞춤.
- 좌석 시점 전환(P3, T-05). 저장값을 다시 적용하는 공용 API도 P3에서 필요할 때 만든다.
- P0-a·P0-b·P1 시험대에 정렬 적용.
- 실제 차량 데이터·최종 차량 Blueprint.
- Meta 공간 앵커(spatial anchor)로 물리 위치를 저장하는 방식. Stage 지속성이 실측에서 부족하면 그때 검토한다. Link에서 되는지 확인한 적 없다.
- 컨트롤러 사용, 정렬용 손 제스처.
- `Config/DefaultEngine.ini`의 XR 설정, 렌더링 설정, `Content/P0` 기존 레벨 변경.
- `AVBoRHandRigPawn` 클래스에 정렬 로직을 넣는 것. 정렬은 별도 클래스가 Pawn을 옮긴다. 정렬은 나중에 다른 Pawn·레벨에서도 쓴다.

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

1. **정렬값은 진행자의 저장 키로만 만들어진다.** 시작·재중심·추적 복귀 때 머리 자세로 정렬을 다시 계산하지 않는다.
2. **추적 공간을 바꾸는 엔진 호출을 하지 않는다.** `ResetOrientationAndPosition`, `SetBaseRotation`·`SetBaseOrientation`·`SetBasePosition`, `SetTrackingOrigin`이 그것이다. 이 호출들은 저장값이 기준으로 삼는 추적 공간 자체를 옮긴다(`OpenXRHMD.cpp:915-930`).
3. **캡처·측정(`C`·`M`)에는 TRACKED frame만 쓴다.** OpenXR `GetCurrentPose`는 VALID만으로도 true를 돌려준다(`OpenXRHMD.cpp:740-765`). 추적 여부는 `IsTracking(HMDDeviceId)`로 따로 본다(`:711-726`, 위치·방향 TRACKED 비트). 창 안에 TRACKED가 아닌 frame이 끼면 그 캡처를 취소한다. 버리고 계속 모으지 않는다 — 놓는 중에 흔들린 것일 수 있다.
4. **저장값은 Stage 공간 값이다.** 월드 좌표나 Pawn을 옮긴 뒤의 카메라 월드 위치를 저장하지 않는다. 다시 저장해도 이전 적용이 누적되지 않는다.
5. **Pawn의 Pitch·Roll은 0이다.** 적용 뒤 "캡처한 헤드셋 위치의 월드 좌표 = eye point 위치"(위치 오차 0.1 mm 미만), "캡처 Yaw의 월드 Yaw = eye point Yaw"(0.01 deg 미만)여야 한다. 이 허용치는 계산 검산용이고 합격 기준이 아니다.
6. **차량·eye point는 움직이지 않는다.** 정렬은 Pawn만 옮긴다. Pawn의 `bUseControllerRotationYaw`는 false다(`APawn` 기본값, `Pawn.cpp:96-98`). true면 컨트롤러 회전이 적용한 Yaw를 덮어쓴다.
7. **키 한 번으로 저장값을 덮어쓰지 않는다.** `C`를 끝까지 눌러야 저장하고, 중간에 떼면 아무것도 바꾸지 않는다. 덮어쓸 때는 이전 값과의 차이를 기록한다.
8. **"확인 필요"는 조용히 지워지지 않는다.** 새로 저장해야만 지워진다. 패널과 로그 둘 다에 남는다.
9. **추적 원점 종류가 저장값과 다르면 적용하지 않는다.** 플레이 영역 크기가 다르거나 못 얻은 경우에는 적용하되 "확인 필요"로 둔다. 크기 비교 허용치는 프로퍼티다.
10. P0-a·P0-b·P1의 동작과 자동화 시험 결과가 바뀌지 않는다.

## 4. 경계 상황과 그 시험

| # | 상황 | 기대 동작 | 시험 |
|---|---|---|---|
| E1 | `C`를 누르는 중 헤드셋 TRACKED가 꺼짐 | 캡처 취소, 저장값 그대로, 이유 기록 | 가짜 자세 입력으로 자동화 |
| E2 | `C`를 끝까지 누르기 전에 뗌 | 취소, 저장값 그대로 | 자동화 |
| E3 | 헤드셋 정면이 거의 수직(수평 성분 부족) | 거부, 이유 표시 | 자동화(순수 함수) |
| E4 | Yaw 샘플이 179°와 −179° 근처에 섞임 | 평균 ≈ ±180°(0°가 아님) | 자동화(순수 함수) |
| E5 | 헤드셋이 지그에서 Pitch 20°·Roll 10° 기운 채 캡처 | Pawn Pitch·Roll 0, 위치·Yaw 검산은 §3-5 허용치 안 | 자동화(순수 함수) |
| E6 | 차량(eye point)이 레벨에서 Yaw 90° 돌아가 있고 원점에서 떨어져 있음 | 같은 저장값으로 eye point에 맞춰짐 | 자동화(순수 함수) |
| E7 | 같은 자세로 두 번 저장 | 두 번째 Pawn 변환이 첫 번째와 같음(누적 없음) | 자동화 |
| E8 | 저장 파일 없음 / 깨짐 / 형식 버전 다름 | "저장값 없음", Pawn은 PlayerStart, 충돌 없음 | 자동화 |
| E9 | 저장값의 추적 원점 종류가 지금과 다름 | 적용 안 함, "확인 필요(추적 원점 다름)" | 자동화(순수 함수) |
| E10 | 플레이 영역 크기가 저장값과 허용치 넘게 다름 / 0(못 얻음) | 적용 + "확인 필요(가디언 변경 의심 / 비교 불가)" | 자동화(순수 함수) |
| E11 | 실행 중 `VRHeadsetRecenter` | "확인 필요(추적 공간 바뀜)", Pawn 그대로 | 자동화(delegate를 직접 broadcast) |
| E12 | eye point 0개 / 2개 이상 | 적용 안 함, 이유 표시, 충돌 없음 | 자동화 |
| E13 | `C`를 누르는 중 `M`·`T`·다시 `C` | 이중 캡처 없음. 무시하거나 취소하고, 어느 쪽인지 기록 | 자동화 |
| E14 | `T` 때 손 추적 안 됨 | "추적 안 됨" 기록, 마지막 위치를 쓰지 않음 | 자동화 |
| E15 | Actor EndPlay | `VRHeadsetRecenter`·입력 바인딩 해제 | 자동화 또는 리뷰로 확인 |

## 5. 구현자가 정할 것

- 저장 방식(SaveGame, ini, JSON 등)과 파일 위치(`Saved/` 아래). 사람이 열어 볼 수 있는 형식이면 좋다. 이유를 보고한다.
- 캡처 창 길이와 `C` 누름 시간의 기본값. 둘을 하나로 묶어도 된다.
- Yaw 평균 방법, 수평 성분 거부 기준, 플레이 영역 비교 허용치. 기본값과 이유를 보고한다.
- 패널을 PC 미러 창에서 읽히게 하는 방법. 예: eye point 앞 월드 공간 텍스트.
- 가짜 자세 입력을 넣는 방법(시험용 주입 지점).
- eye point의 에디터 표시 방법.

## 6. 완료 조건

1. §2의 측정 항목이 패널과 `LogP0Buck`에 나온다.
2. §3의 지켜야 할 조건을 모두 지킨다.
3. §4 표의 자동화 시험이 있다.
4. 엔진 API를 헤더와 대조했고 보고에 경로:줄을 적었다. 최소 범위:
   - `IXRTrackingSystem.h`: `:70` `HMDDeviceId`, `:137` `GetCurrentPose`, `:183` `GetTrackingOrigin`
   - `HeadMountedDisplayFunctionLibrary.h`(XRBase): `:403` `GetPlayAreaBounds`
   - `CoreDelegates.h:436` `VRHeadsetRecenter`
5. 샘플 플러그인을 끈 상태로 빌드 경고·오류 0, `vBoR.*` 전체 통과.
6. 보고에 "로컬에서 확인할 것"이 있다.

확인한 API 근거(엔진 `C:\Program Files\Epic Games\UE_5.7\Engine` 기준):

| API | 위치 | 확인한 것 |
|---|---|---|
| `IXRTrackingSystem::GetCurrentPose` | `Source/Runtime/HeadMountedDisplay/Public/IXRTrackingSystem.h:137` | 추적 공간 자세. OpenXR 구현은 VALID 비트만 보고 true(`Plugins/Runtime/OpenXR/Source/OpenXRHMD/Private/OpenXRHMD.cpp:740-765`) |
| `IXRTrackingSystem::IsTracking` | 같은 헤더, `HMDDeviceId` `:70` | OpenXR 구현은 위치·방향 TRACKED 비트(`OpenXRHMD.cpp:711-726`) |
| `GetTrackingOrigin` | `IXRTrackingSystem.h:183`, `HeadMountedDisplayFunctionLibrary.h:198` | Stage 확인용 |
| `GetPlayAreaBounds(Stage)` | `Plugins/Runtime/XRBase/Source/XRBase/Public/HeadMountedDisplayFunctionLibrary.h:403` | `xrGetReferenceSpaceBoundsRect`. 실패하면 경고 + 0(`OpenXRHMD.cpp:444-487`) |
| `FCoreDelegates::VRHeadsetRecenter` | `Source/Runtime/Core/Public/Misc/CoreDelegates.h:436` | 기준 공간 변경 이벤트 때 broadcast. Stage면 `OnPlayAreaChanged`도(`OpenXRHMD.cpp:4017-4029`) |
| `ResetOrientationAndPosition` 등 | `IXRTrackingSystem.h:228`, `OpenXRHMD.cpp:915-930` | BasePosition·BaseOrientation을 바꿔 추적 공간을 옮김 → 쓰지 않음 |
| `APawn` 기본값 | `Source/Runtime/Engine/Private/Pawn.cpp:96-98` | `bUseControllerRotationPitch/Yaw/Roll = false` |
| vBoR Pawn | `Source/vBoR/Interaction/VBoRHandRigPawn.cpp:47-50` | 루트 `VROrigin`, 카메라 `bLockToHmd` |

참고: LiftGateStudy `Source/LiftGateStudy/Public/Calibration/`은 HMD 높이 + 바닥 레이캐스트로 Pass/Fail만 판정한다. 오프셋 저장·적용은 없어 이 명세와는 "평가 시작 전 확인 단계" 정도만 겹친다.

## 7. 로컬에서 확인할 것 — 사용자

### 에디터

1. `Content/P0/L_P0c_BuckEye` 레벨(Basic + PlayerStart). GameMode Override = `P0BuckBenchGameMode`. `AP0BuckBench`를 하나 놓는다.

### Quest 3 (Link, VR Preview)

P0 실험이며 T-01 정식 시험이 아니다. 아래는 방법이고 합격 기준이 아니다. 렌더링 성능과는 관계없는 시험이라 이 PC에서도 할 수 있다. 실물 Buck이 없으면 의자 + 상자로 만든 임시 지그로 장치 동작만 본다.

**준비:**

- **헤드셋을 쓰지 않은 채 추적이 계속되는지 먼저 확인한다.** 머리에서 벗으면 근접 센서 때문에 헤드셋이 잠들 수 있다. Meta Quest Developer Hub의 근접 센서 설정으로 끄거나 센서를 가린다(메뉴 이름은 확인 필요).
- 키 입력이 가도록 VR Preview 창에 포커스를 둔다.
- 가디언 종류(고정·룸스케일)를 기록한다.

| 확인 | 방법 | 기록할 것 |
|---|---|---|
| 저장 | 지그에 헤드셋을 놓고 `C` 길게 | 창 안 흩어짐(mm, deg), Pawn Z |
| 반복성 | 헤드셋을 들었다 다시 놓기 × 10, 매번 `M` | 차이(mm, deg) 10개 |
| 눈으로 대조 | 헤드셋을 쓰고 지그를 봄 | 가상 eye point 십자가 지그 자리에 보이는지, 대략 몇 cm 어긋나는지 |
| 손끝 대조 | 실물 표시점 3곳(예: 좌석 앞 모서리 가운데, 등받이 위 가운데, 지그 옆 표시)을 검지로 짚고 `T` | eye point 기준 좌표 vs 줄자로 잰 값 |
| 지속성 | (a) VR Preview 재시작 (b) 에디터 재시작 (c) Link 재연결 (d) 헤드셋 절전·깨우기 (e) PC 재부팅 (f) 다음 날 — 매번 지그에 놓고 `M` | 차이(mm, deg), 상태 표시 |
| 재중심 | 실행 중 Meta 버튼 길게 눌러 재중심 | "확인 필요"로 바뀌는지, 그 뒤 `M` 차이 |
| 가디언 변경 | 가디언을 다시 그리고 시작 | 플레이 영역 크기 차이로 "확인 필요"가 뜨는지, `M` 차이 |

결과는 조사 기록 "P0-c 실측"으로 남긴다. 사용자가 말로 알려주면 Claude가 `Saved/Logs/vBoR.log`의 `LogP0Buck`과 함께 정리한다. 반복성·지속성 수치로 충분한지는 사용자(평가 담당자)가 정한다. 이 명세는 기준을 정하지 않는다.
