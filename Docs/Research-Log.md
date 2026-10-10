# 조사·결정 기록

문서 셋에서 이 파일의 자리:

- **[Dev-Plan-50pct-VR.md](Dev-Plan-50pct-VR.md)** — 무엇을 만들 것인가. 범위·결정 상태·시나리오 표·개발 단계·시험 계획.
- **[../AGENTS.md](../AGENTS.md)** — 코드를 쓸 때 매번 필요한 규칙과 작업 방식. Codex·Claude 공통 원본이며 `CLAUDE.md`는 이것을 가져온다. 2026-10-06 이전 기록의 "CLAUDE.md"는 이 규칙을 가리킨다.
- **[Specs/](Specs/)** — 구현 단위 명세. Claude가 쓰고 Codex가 구현한다.
- **이 파일** — 그 결정들이 **무엇을 근거로** 내려졌는가. 확인한 플러그인 헤더·버전·외부 사실과, 검토했지만 채택하지 않은 것들.

결정을 나중에 재검토할 때 같은 조사를 다시 하지 않기 위한 기록이다. 근거가 바뀌면(플러그인 버전 교체 등) 해당 항목을 갱신한다.

---

## 2026-10-10 — P0 첫 VR 실행 (참고용, 약 15 fps)

사용자가 Quest 3 + Link, 에디터 VR Preview로 `L_P0a_HandSignals`(P0-a 프로브)와 `L_P0b_ButtonBench`(P0-b 시험대, `fe53a8d`)를 한 번씩 실행했다. 숫자는 `Saved/Logs/vBoR.log`의 `LogP0Probe`에서 Claude가 뽑았다. **명세 §5의 정식 측정(자세별 반복)이 아니라 첫 동작 확인이다.** 시험 상태는 계속 `not-tested`.

### 환경

- PC: NVIDIA GeForce RTX 3060, 드라이버 591.86. OpenXR 런타임 Oculus 1.208.0. XR API Epic Native OpenXR(`107539f`), OVRPlugin 로드 없음.
- **프레임 약 15 fps**(로그 프레임 번호로 계산, 두 레벨 모두). 렌더링이 데스크톱 템플릿 그대로(Lumen GI·반사, 레이트레이싱, VSM)이고, 에디터 VR Preview는 HMD 전체 해상도를 쓴다("OpenXR ignores max viewport resolution in editor"). **이 상태의 관통 깊이·시간은 화면 지연이 섞여 있어 설계 근거로 쓰지 않는다.**

### 확인된 것 (프레임과 무관)

- Native OpenXR에서 손 추적이 된다. 양손 위치 유효·신뢰도 High로 시작(P0-a). ISDK 데이터 소스는 `IsdkFromOpenXRHandDataSource`(P0-b).
- **pinch 키가 Native OpenXR에서 들어온다.** 검지 pinch 때 `ThumbPinch`와 `IndexPinch`가 같은 프레임에 함께 눌린다.
- P0-b 손 리그: 레이 `active=0, enabled=0`(양손), `IsdkPokeLimiterRigModifier` 활성, 컨트롤러 리그 0. 원거리 잡기 감지기 객체는 손마다 하나 있다(ISDK가 항상 만든다 — 잡기 컴포넌트가 꺼져 있어 동작은 안 함, P0-b 리뷰 R2-1).
- 먼 버튼(약 2 m) hover·select 0. 원본 손끝 추적 때문에 취소된 누름 0.

### 숫자 (15 fps, 참고용) — 오른손, 누름 15회

| 항목 | 값 |
|---|---|
| 실제 손끝이 버튼 앞면을 지나간 최대 깊이 | 11.4~20.9 mm(0 한 번 제외 14회). 단독 10 mm 20.2·20.9, 15 mm 15.3, 20 mm 20.9 |
| select 순간 원본 깊이(눌림 깊이 3 mm라 같으면 3 근처) | **−1.9~+1.4 mm**. ISDK poke 지점이 엔진 IndexTip보다 약 1.6~4.9 mm 손가락 끝 쪽(더 깊이)에 있다는 뜻. 관통 깊이를 읽을 때 이만큼 보정이 필요하다 |
| hover → select 시간 | 0.05~0.87 s |
| 몰린 묶음(12 mm·간격 15 mm) 동시 활성 최대 | **1**(누름 10회, 2 이상 0회) — ISDK 기본이 이 배치에서는 한 번에 하나만 활성 |
| 몰린 묶음 한 누름 동안 hover 대상 전환 | 0~4회. 목표로 가는 동안 이웃을 차례로 지나간다 |
| hover 뒤 select 없이 끝남 | 22회(대부분 몰린 묶음 지나가기 — 단독 15 mm에서 2회는 실제로 못 누른 시도). 나누어 세도록 P0-b 리뷰 R2-2 |

### 아직 안 한 것

- P0-a: 마이크로제스처·PC 키보드 입력·pinch 강도 값·다이얼 각도(로그에 시도 기록 없음), 흔들림·시야 경계·가림 측정.
- P0-b: 왼손, 크기별 10회 반복, `R` 다시 놓기, **먼 버튼을 손가락으로 가리켜 보기**(이번에는 가리키지 않아 hover 0이 레이 차단의 증거는 아니다).

### 사용자 체감 (2026-10-10)

- 손가락이 버튼 면에서 멈춰 보였다(poke limiter 동작). 버튼이 들어갔다가 돌아왔다. 색과 소리가 났다. 패널 가독성과 시험대 위치는 괜찮았다.
- **"버튼이 눌린 느낌은 손가락이 버튼을 통과하지 않아서인 것 같다."** 촉감이 없는 VR에서 limiter가 접촉 단서 역할을 한다는 첫 근거다(U-06 필수 결정을 뒷받침). 한 사람·한 번의 체감이므로 정식 측정에서 다시 묻는다.

### Buck 정렬 — eye position (2026-10-10, 방식 확인 중)

사용자 결정: Buck 정렬은 **차량의 eye position**(설계 눈 위치) 기준으로 한다. 차량 eye position 값에 맞춰 Pawn(VR 원점)을 옮긴다. 컨트롤러는 쓰지 않는다(열린 결정 #9).

Claude가 짚은 점 — **참가자마다 맞추면 평가가 무너진다:**

- 참가자의 실제 눈 위치를 설계 eye position에 맞추면, 키가 다른 참가자마다 가상 차가 위아래·앞뒤로 움직인다. 실물 좌석과 가상 좌석이 어긋나고, 13번 도달성·05번 자세 부담에서 재려던 **체격 차이가 지워진다.**
- 그래서 **Buck 설치 때 한 번 맞추는 기준점**으로 쓰는 안을 권했다: 실물 Buck의 설계 eye position 자리(지그나 표시)에 헤드셋을 두고 진행자가 키를 누르면, 그 순간 헤드셋 위치·정면이 가상 차 eye position과 같아지도록 오프셋을 계산해 저장한다. 이후 참가자는 누가 앉든 가상 차가 실물 Buck에 고정된다. Quest 가디언을 다시 그리지 않는 한 저장한 값을 다시 쓴다(UE 5.7 OpenXR 기본 추적 공간은 가디언 기준 Stage).
- 정면(Yaw) 정렬이 한 점으로는 불안정할 수 있다. 두 번째 기준점(예: 핸들 중심)을 쓸지는 실측으로 정한다.
- LiftGateStudy의 `Calibration/`(HMD 높이 + 바닥 레이캐스트 검증)이 참고 대상이다(2026-10 §3).
- **사용자 확인 대기:** 참가자마다인지, 설치 때 한 번인지. 확인되면 P0-c 명세를 쓴다.

### 결정 (2026-10-10 사용자)

- **렌더링 설정은 바꾸지 않는다.** 15 fps는 이 PC(RTX 3060)의 한계이고, 정식 측정은 그래픽 카드가 좋은 **회사 PC**에서 한다. 이 PC의 측정은 동작 확인용이다.

---

## 2026-10-09 — poke limiter 필수, 다이얼 손가락 회전, ISDK 샘플

### 1. 결정과 요구

| ID | 내용 | 기록 위치 |
|---|---|---|
| U-06 | **poke limiter 필수.** 버튼을 누를 때 손 모델이 표면에 멈춰 보이게 한다. 13번 도달 판정은 추적된 실제 손끝으로 한다(limiter는 손 모델의 위치만 옮긴다) | 계획서 §2·§5, AGENTS.md |
| U-06 | **다이얼을 엄지·검지로 돌리는 동작 요구.** pinch로 잡은 뒤 손가락으로 굴려 돌리는 것. 지금은 pinch·grab·poke만 된다 | 계획서 §2·§5, AGENTS.md |

열린 결정 갱신(2026-10 §5, 2026-09 §9):

- **#6 poke 관통 방지** — 해결. 버튼 poke에 한해 채택. 잡기·문 표면에 손을 고정하는 것은 여전히 평가 담당자와 협의할 항목이다.
- **2026-09 #3 노브 방식** — 사용자 요구는 (A) 손가락 굴림. 가능 여부는 P0 실측 뒤 정한다. 실패하면 (B) 손목 회전(ISDK 기본), (C) 단계만으로 내려간다.

### 2. 다이얼 손가락 회전 — 확인한 것과 방법

- **HandPoseStudio로는 안 된다.** 이 도구는 지금 손 모양을 `UIsdkHandPoseData` 에셋으로 저장하는 녹화 도구다([Meta 문서](https://developers.meta.com/horizon/documentation/unreal/unreal-isdk-hand-grab-poses/)). 저장한 포즈는 (a) 물체를 잡을 때 보이는 손 모양(`UIsdkHandGrabPose`)이나 (b) 손 모양 인식(`UIsdkHandPoseSubsystem::CreateHandPoseDetectionProfileFromMesh` → `RegisterHandPoseDetection`, `OculusInteraction/Public/Subsystem/IsdkHandPoseSubsystem.h`)에 쓴다. (a)는 허용, (b)는 U-03의 자체 포즈 인식기라 금지. 어느 쪽도 "돌리는 동작"은 아니다.
- **ISDK 변환기는 손가락을 보지 않는다.** 모든 transformer가 잡은 지점 기준이라 pinch로 잡고 돌리면 손목 회전이 된다(2026-09 §4). 놓으면 원위치로 돌아가는 기능·detent도 없다(`OculusInteraction/Public/Interaction/Grabbable/` 전체 확인).
- **방법(제안):** 잡은 동안 엄지 끝→검지 끝 벡터를 다이얼 축에 수직인 면에 투영하고, 그 벡터가 축 둘레로 돈 각도만큼 다이얼을 돌린다. 손가락으로 굴리면 두 손끝이 반대 방향으로 움직여 이 벡터가 돌고, 손목을 비틀어도 같은 벡터가 돈다. 그래서 (A)와 (B)를 한 계산으로 받는다. 각도는 unwrap해서 누적하고(2026-09 §6), 추적이 끊겼다 돌아오면 그 순간 벡터를 새 기준으로 잡는다(AGENTS.md 추적 손실 원칙).
- **위험:**
  - pinch 중에는 엄지와 검지가 서로를 가린다. 손가락별 신뢰도(`GetFingerTrackingConfidence`, `OculusXRInputFunctionLibrary.h:226`)가 High일 때만 갱신한다.
  - 다이얼을 잡은 손끝 사이 거리는 1~3 cm라 손끝 흔들림 몇 mm가 각도로는 크게 나온다. 떨림 저감 필터(허용 조작)와 dead band가 필요할 수 있다.
  - Quest 3 광학 추적이 손가락 굴림 같은 작은 움직임을 따라오는지 모른다.
- **실측으로 볼 것:** pinch로 정지했을 때 엄지→검지 벡터 각도의 1초 흔들림(deg), pinch 중 엄지·검지 손가락별 신뢰도, 손가락으로 굴렸을 때 각도가 따라오는지. P0-a 프로브에 붙이는 것을 제안했다.
- **평가 타당성:** 다이얼의 조작력·detent 느낌·질감은 VR로 평가할 수 없다. 위치·시인성·라벨 판독·회전 방향 직관성까지가 한계다(2026-09 §6).
- **시나리오 배치:** 원본 19개 항목에 다이얼 조작이 없다. 12번에 넣을지 자유모드로 보낼지는 2026-09 #5 그대로 열려 있다.

### 3. OculusInteractionSamples (사용자가 로컬에 추가, 미커밋)

- 샘플 C++ 모듈이 ISDK 1.205와 함께 빌드되고, `PokeExamples`·`TransformerExamples` 맵이 오류 없이 로드된다. 2026-10 §3의 `IsdkInfoCardComponent` 오류가 사라진다. 에셋은 UE 5.6 형식으로 ISDK 1.205와 같다. `VersionName` "1.0"이라 짝 버전은 미확정.
- **필요한 기능은 샘플이 아니라 ISDK 본체에 있다.** poke limiter `UIsdkPokeLimiterRigModifier`(샘플 BP는 상속만 한 빈 껍데기), 눌리는 버튼 `IsdkPokeInteractable`·`IsdkPointablePlane`·`IsdkClippedPlaneSurface`·`IsdkPokeButtonVisual`·`IsdkPointerEventAudioPlayer`, 1축 회전 `IsdkOneGrabRotateTransformer`, 잡기 포즈 `IsdkHandGrabPose`, 손 리그 `UIsdkHandRigComponentLeft/Right`. 샘플은 이것들을 Blueprint로 조립한 것이다.
- `IsdkSamplePawn`에는 레이 인터랙터·레이 잡기·원거리 잡기·컨트롤러 리그가 들어 있어 그대로 쓰면 U-03·U-04 위반이다.
- **결정(2026-10-09 사용자):** 샘플을 실행 의존으로 쓰지 않고, vBoR 부품을 ISDK 위에 C++로 직접 만든다. 샘플은 참고 자료와 HandPoseStudio 녹화 도구로만 쓰고 `.gitignore`로 리포에서 뺐다. 녹화한 포즈는 vBoR `Content/` 아래에 저장한다. LiftGateStudy는 반대로 샘플 BP를 부모로 쓰는 쪽을 택했다(ADR-004·012). vBoR은 구현자가 Codex라 C++가 맞다고 판단했다. 검토한 선택지와 규칙은 [Agent-Workflow.md](Agent-Workflow.md) §5.

---

## 2026-10 — ISDK 1.205.0 포함·문 개폐 모델

로컬 세션(Windows)에서 리포를 직접 확인했다. 2026-09 기록 중 바뀐 항목은 그 자리에도 표시했다.

### 1. 확정된 결정

| 결정 | 내용 | 기록 위치 |
|---|---|---|
| U-05 | Meta Interaction SDK **1.205.0**을 `Plugins/MetaXRInteraction/`에 포함(`d103db0`). 2026-09 §9의 열린 결정 #1 해결 | 계획서 §2, §5 / CLAUDE.md |
| U-05 | **문 개폐 모델:** 문은 닫힌 상태로 시작하고, 닫혀 있으면 밀어도 당겨도 움직이지 않는다. 손잡이를 조작해야 풀리고, 풀린 뒤에는 밀기·당기기를 둘 다 받는다. 다시 닫히면 잠긴다 | 계획서 §2, §5 |

문 모델에 따른 단계별 동작:

| 단계 | 위치 | 동작 |
|---|---|---|
| 07 → 08 | 밖 | 바깥 손잡이로 해제 → 당겨서 열기 |
| 11 | 안 | 당겨서 닫기. 안에서 밀면 여는 방향이다 |
| 18 | 안 | 안쪽 손잡이로 해제 → 밀어서 열기 |
| 20 | 밖 | 밀어서 닫기. 잡고 돌려 닫기도 가능 |

계획서 §4의 11번 구현 항목이 "손 밀기"였던 것을 이 모델에 맞춰 고쳤다.

### 2. ISDK 1.205.0에서 확인한 것

경로는 `Plugins/MetaXRInteraction/Source/` 기준.

| 항목 | 사실 | 위치 |
|---|---|---|
| 버전 | `VersionName 1.205.0`, `EngineVersion 5.7.0`, `IsExperimentalVersion: true`. MetaXR 코어와 같은 버전이라 2026-09 §5의 205+207 조합 우려는 해당 없음 | `../OculusInteraction.uplugin` |
| 클래스 | 2026-09 §4(1.78.0 기준)의 쓸 것·안 쓸 것 클래스가 모두 있다 | `OculusInteraction/Public/` |
| 손 리그 | C++ 컴포넌트 `UIsdkHandRigComponentLeft` / `Right` | `OculusInteractionPrebuilts/Public/Rig/IsdkHandRigComponent.h` |
| 커스텀 변환 | `IIsdkITransformer`(`BeginTransform`/`UpdateTransform`/`EndTransform`), 기반 `UIsdkTransformer`. 골프백·문의 제약 이동을 얹을 자리 | `OculusInteraction/Public/Interaction/Grabbable/IsdkITransformer.h`, `IsdkTransformer.h` |
| 포인터 이벤트 | `EIsdkPointerEventType`에 `Cancel`이 있다 | `OculusInteraction/Public/StructTypes.h:102` |
| 백엔드별 데이터 소스 | `IsdkDataSourcesMetaXR` / `IsdkDataSourcesOpenXR` 모듈, 둘 다 `LoadingPhase: None` | `../OculusInteraction.uplugin` |
| 프리빌트 | `ThirdParty/OculusInteractionLibrary/Lib/{Win64,arm64-v8a}`, `ThirdParty/EngineTelemetry/lib/Win64`. 철자가 MetaXR의 `Thirdparty`와 다르다 | `.gitignore` 예외 |

#### 함정 1 — "관통 방지"는 손을 표면에 고정하는 것이다

- `IsdkPokeLimiterVisual`은 **v79부터 deprecated**이고, 헤더 설명이 "사용자가 얼마나 더 밀어넣든 손 비주얼을 눌린 표면에 스냅한다"이다 (`OculusInteraction/Public/Interaction/IsdkPokeLimiterVisual.h:45-57`).
- 후속 `IsdkPokeLimiterRigModifier`(`OculusInteractionPrebuilts/Public/Rig/`)도 pose override로 같은 일을 한다.
- 2026-09 §4에서 협의 항목으로 둔 `IsdkSyntheticHand`와 같은 범주인데, 같은 절에서 "쓸 것"으로 분류돼 있었다. 13번에서 손가락이 버튼 면에 멈춰 보이면 참가자가 닿았다고 인지할 수 있다. **분류 재검토 필요 — 사용자 결정 대기.**

#### 함정 2 — grab은 추적 신뢰도를 보지 않는다

- `OculusInteraction/Private/Interaction/IsdkGrabberComponent.cpp`에 신뢰도 처리가 없다. 잡은 채 추적이 끊겼다 돌아올 때의 거동은 데이터 소스 층에 달려 있고, 실측 전에는 모른다.
- → CLAUDE.md의 "추적 손실은 보류, 복구 시 누적 이동 미적용"은 직접 얹어야 한다. P0·P1 실측 항목.

#### 네트워크 복제가 없다

- ISDK 소스 전체에 `GetLifetimeReplicatedProps`·`ReplicatedUsing`이 0건이다. 손 상호작용은 그 손이 연결된 PC에서만 계산된다. 멀티를 재검토할 때의 전제다(§4).

### 3. 프로젝트 상태 (2026-10-03)

- `Config/DefaultEngine.ini`는 데스크톱 템플릿 값 그대로다: DX12 SM6, Lumen GI·반사, 레이트레이싱, VSM, Substrate. XR 설정이 없고 기본 맵은 `/Engine/Maps/Templates/OpenWorld`, `Content/`는 비어 있다. 실행 방식을 정한 직후 정리할 대상이다.
- git-lfs는 설치돼 있으나 `.gitattributes`에 LFS 규칙이 없다. ISDK는 가장 큰 파일이 6.3 MB라 일반 git으로 넣었다. 차량 에셋을 처음 커밋하기 전에 LFS 사용 여부를 정해야 한다.
- 로컬 LiftGateStudy 사본(UE 5.5)에는 2026-09 §8이 다룬 Liftgate 외에 `Calibration/`(HMD 높이 + 바닥 레이캐스트 검증)과 `Session/`(GameInstanceSubsystem 진행 상태기계)이 있다. Buck 정렬(T-01)과 P3 단계 진행의 참고 대상. GitHub 원격과 같은 버전인지는 확인하지 않았다.

#### 로컬 빌드·에디터 확인 (2026-10-06, 헤드셋 미연결)

| 항목 | 결과 |
|---|---|
| 빌드 | `vBoREditor Win64 Development` 성공, 경고·오류 0. VS 2022 14.44, Windows SDK 10.0.22621. MetaXR 모듈 18개와 ISDK 모듈 6개가 처음으로 함께 컴파일됐다 |
| 에디터 | 18초 만에 열림. `OculusXR`·`OculusInteraction`이 프로젝트 플러그인으로 마운트됨. `.uproject`에 명시하지 않아도 프로젝트 플러그인은 기본으로 켜진다 |
| PC OpenXR 런타임 | Oculus **1.207.0**. 지원 목록에 `XR_EXT_hand_tracking`, `XR_FB_hand_tracking_aim`, `XR_EXT_hand_tracking_data_source`, **`XR_META_hand_tracking_microgestures`**가 있다. 없는 것: `XR_META_hand_tracking_wide_motion_mode`(1·2), `unextrapolated_poses`, `frequency_hint`. 목록에 있다는 것이고 Link에서 실제로 들어오는지는 P0-a 실측 대상이다 |
| XR 백엔드 | OVRPlugin을 먼저 시도해 `xrGetSystem() failed`(헤드셋 미연결)로 실패한 뒤 Epic OpenXR로 넘어갔다. **XR API 설정이 아직 기본값**이므로 헤드셋을 연결하면 OVRPlugin으로 뜰 수 있다. ISDK가 요구하는 Epic Native OpenXR로 바꾸는 일은 P0-a 명세 §5에 있다 |
| ISDK 오류 | `IsdkInfoCardComponent`의 CDO 생성자가 리포에 없는 `/OculusInteractionSamples/Widgets/InfoCardWidget`을 찾다 실패한다. 이 컴포넌트는 쓰지 않는다 |
| 첫 실행 창 | Meta XR 플러그인 설문 창이 모달로 떠서 에디터 정상 종료를 막는다 |
| 확인 못 한 것 | VR Preview, 핸드 트래킹 입력 — 헤드셋 연결 후 P0-a에서 |

### 4. 검토했지만 지금 하지 않는 것 — 멀티

| 형태 | 확인한 것 | 판단 |
|---|---|---|
| 진행자 PC ↔ 참가자 Quest | PCVR이면 앱이 진행자 PC에서 돌아 필요 없다. 독립 실행일 때만 의미가 있다 | 실행 방식 결정에 종속 |
| 1명 조작 + N명 관전, 조작자 전환 | 기술적으로 가능: 진행자 PC가 서버, 조작자 PC가 손 조작을 계산해 서버로 보낸다. 전환 순간의 잡기 취소는 추적 손실 처리와 같게. 위험: 순차 시나리오 중간 전환은 사람별 19단계 수행을 깨고, 관전자는 찾기·읽기 평가가 오염되며, 같은 공간에서 서로의 몸이 보이지 않는다 | **지금 하지 않음(제안).** 50% 범위 밖. 1인용으로 진행하되 상태를 값으로 두어 나중에 옮기는 비용만 낮춘다. 재검토는 P5 이후 |
| 같은 차량 공동 조작 | ISDK에 복제가 없어 잡기 소유권 중재를 직접 만들어야 한다. Buck이 1개다 | 비용이 크고 평가 의미가 바뀐다 |

- MetaXR에 `OculusXRColocation`, `OculusXRAnchors`(공유 앵커) 모듈이 있다. 이번 VR 구성에서 쓸 수 있는지는 확인하지 않았다. 헤드셋마다 같은 Buck에 정렬하면 공유 앵커 없이도 서로 맞는다.

### 5. 열린 결정 (2026-09 §9 갱신)

| # | 결정할 것 | 상태 |
|---|---|---|
| 1 | ISDK 도입 여부 | **해결** — 1.205.0 포함 (U-05) |
| 4 | PCVR vs 독립 실행 | 개발은 Link(에디터 VR Preview)로 제안. 최종 실행 방식의 판단 축: 진행자 키보드·화면(Link 유리), 차량 데이터 최적화 부담(Link 유리), 01–06 보행 중 케이블(독립 실행 또는 Air Link 안정성). 2026-09의 "독립 실행 유력"은 글로브 제외만 근거였다 |
| 6 | poke 관통 방지(§2 함정 1)를 협의 항목으로 옮길지 | 사용자 결정 대기 |
| 7 | 순차 모드에서 현재 단계 대상만 활성화할지 | 사용자 결정 대기. 문 모델(닫힘 = 잠김)로 12·13번에서 문이 밀리는 문제는 줄었지만, 문이 열려 있는 08–11·18–20 구간에는 남는다 |
| 8 | 문 재잠금 조건(닫힘 근처에서 스냅할지), 손잡이를 잡은 손이 다시 잡지 않고 그대로 문을 당기게 할지 | P2 실험으로 결정 |
| 9 | Buck 정렬 방법 — 진행자 정렬 단계에 컨트롤러를 써도 되는지 | 사용자 결정 대기 |
| 10 | git LFS 사용 여부 | 차량 에셋 첫 커밋 전 |

### 6. 이 기간의 커밋

브랜치 `philip/add-isdk-1.205`.

| 커밋 | 내용 |
|---|---|
| `d103db0` | ISDK 1.205.0 포함 + `.gitignore` 프리빌트 예외 |
| `c9c1a57` | 이 기록, 계획서 §2·§4·§5, CLAUDE.md 갱신 |
| `70c4eec` | Codex 로컬 설정(`.codex/`) |
| `26b0b02` | AGENTS.md를 규칙 원본으로, 명세·리뷰 작업 방식, P0-a 명세 |
| 다음 커밋 | §3의 로컬 빌드·에디터 확인 결과 |

---

## 2026-09 — 인터랙션 어휘·장비 확정

### 1. 확정된 결정

| 결정 | 내용 | 기록 위치 |
|---|---|---|
| U-03 | 핸드 트래킹 조작을 **1축 제약까지** 허용. 자유 6DoF·손가락 물리·자체 제스처 인식기·던지기는 계속 제외 | 계획서 §2, §5 / CLAUDE.md |
| U-03 | 장비는 **Meta Quest 3 단독**. 데이터 글로브·외부 트래커 없음 | 계획서 §2 / CLAUDE.md |
| U-03 | **VR이다. 실물은 좌석뿐이고 나머지는 허공.** 손은 가상 형상을 통과한다 | CLAUDE.md / 계획서 §5 |
| U-04 | 몰린 버튼에서 평가하는 것은 **찾기·읽기·도달성**. 오조작 빈도·간격 적정성은 평가 대상 아님 → **조준 보조 허용** | 계획서 §2, §5 |
| U-04 | 아이 트래킹을 위한 장비 전환(Varjo XR-4 등) 하지 않음 | 계획서 §2 |

핵심 원칙 두 개:

- **도달은 참가자가, 조준은 시스템이 돕는다.** 도달성은 평가 대상이므로 손을 실제로 뻗어야 하지만, "어느 버튼인지"는 보조해도 평가가 훼손되지 않는다.
- **촉감이 없으므로 시각·청각 피드백이 접촉을 대신한다.** 참가자는 닿았는지 스스로 알 수 없다.

### 2. 현재 프로젝트에 들어 있는 것 — MetaXR 1.205.0

`Plugins/MetaXR/`에서 직접 확인했다. 경로는 `Plugins/MetaXR/Source/OculusXRInput/` 기준.

#### 쓸 수 있는 신호

| 기능 | 심볼 | 위치 |
|---|---|---|
| 손 추적 신뢰도 | `GetTrackingConfidence(EOculusXRHandType, int32)` | `Public/OculusXRInputFunctionLibrary.h:216` |
| 손가락별 신뢰도 | `GetFingerTrackingConfidence(EOculusXRHandType, EOculusXRFinger, int32)` | 같은 파일 `:226` |
| 반환 타입 | `EOculusXRTrackingConfidence` — **`Low` / `High` 2단계뿐** | 같은 파일 `:31` |
| 본 회전 | `GetBoneRotation(...)` — **회전만. 위치를 주는 함수는 없다** | 같은 파일 `:189` |
| 본 이름 | `GetBoneName(EOculusXRBone)` → `FString` | 같은 파일 `:275` |
| 손끝 본 | `EOculusXRBone::Thumb_Tip`, `Index_Tip` | 같은 파일 `:74`, `:76` |
| 손가락별 pinch 5종 | `OculusHand_{Left,Right}_{Thumb,Index,Middle,Ring,Pinky}Pinch` | `Private/OculusXRInputState.h:159` 이하 |
| pinch 강도 10종 (0~1) | `..._{Finger}PinchStrength` | 같은 파일 `:174` 이하 |
| 마이크로제스처 5종 | `..._MicrogestureSwipe{Left,Right,Forward,Back}`, `..._MicrogestureTapThumb` | 같은 파일 `:186`–`:196` |

pinch·마이크로제스처는 **입력 키/축**이므로 Enhanced Input에서 바로 쓸 수 있다. 별도 플러그인이 필요 없다.

#### 손끝 **위치**를 얻는 경로

공개 API에 본 위치 함수가 없지만 우회로가 있다:

- `UOculusXRHandComponent`는 **`UPoseableMeshComponent` 상속** (`Public/OculusXRHandComponent.h:28`)
- → 표준 UE `GetBoneLocation(BoneName, EBoneSpaces::WorldSpace)` 사용 가능
- 본 이름은 `GetBoneName(EOculusXRBone::Thumb_Tip)` 등으로 얻는다

#### 함정 — 주석이 타입과 어긋난다

`Private/OculusXRInputHandTrackingTypes.h:220-227`의 주석은 신뢰도가 `Range [0,1]`이라고 적혀 있지만, 실제 필드 타입은 위와 같은 **2단계 enum**이다. 주석을 믿지 말 것. "신뢰도가 떨어질수록 서서히 약화" 같은 연속 처리는 이 값만으로 안 된다.

#### 아이 트래킹 — 모듈은 있으나 Quest 3에서 안 된다

`Plugins/MetaXR/Source/OculusXREyeTracker/`가 존재해서 오해하기 쉽다. 플러그인이 Quest Pro까지 커버하기 때문이고, 코드 자체가 런타임에 확인한다:

- `OculusXRMovement::IsEyeTrackingSupported()` 로 분기 (`Private/OculusXREyeTracker.cpp:86`)
- 미지원이면 `return FVector::ZeroVector; // Not supported` (`:161`)

**Quest 3 / 3S에는 아이 트래킹 하드웨어가 없다.** 아이 트래킹은 Quest Pro에만 있다. Quest 3에서 "gaze"라고 부를 수 있는 것은 **머리 방향**뿐이며, 이는 U-04에서 금지했다(손을 뻗지 않고 선택하면 13번 도달성 평가가 사라진다).

### 3. 없는 것 — 인터랙션 레이어

MetaXR가 주는 것은 **신호**이고, 다음은 직접 만들어야 한다:

- hover → select → release 상태 기계
- 잡기·누르기 후보 선택 (여러 대상 중 하나, 무관한 손 접근 배제)
- 1축 제약 변환 (각도 clamp, 축 투영)
- 추적 손실·복구 처리 — CLAUDE.md의 "누적 이동을 한 번에 적용하지 않는다"를 직접 구현
- 손가락이 대상을 관통해 들어가는 것을 막는 시각 보조
- 손 떨림 저감 필터

이 레이어를 직접 만드는 것은 정의상 프레임워크이므로 **"범용 프레임워크부터 만들지 않는다"에 어긋난다.** 그래서 검증된 인터랙션 플러그인 도입을 검토했다(아래 §5).

### 4. ISDK(MetaXRInteraction)에 있는 것

근거: `bssani/LiftgateStudy`(UE 5.5, ISDK **1.78.0**)를 클론해 헤더를 읽었다. **경로와 API는 그 버전 기준이며 207.0에서 달라질 수 있다.** 실제 도입 후 재확인이 필요하다.

> 2026-10 갱신: 리포에 들어온 1.205.0에 아래 클래스가 모두 있다. 단 `IsdkPokeLimiterVisual`은 deprecated이고 손을 표면에 고정하는 기능이다 — 2026-10 §2 함정 1.

#### 쓸 것 (U-03 허용 범위 안)

| 용도 | 클래스 |
|---|---|
| poke 버튼 | `IsdkPokeInteractor`, `IsdkPokeInteractable`, `IsdkPokeButtonVisual`, `IsdkPokeLimiterVisual`(관통 방지) |
| 1축 회전 (콘솔 뚜껑·노브) | `IsdkGrabbableComponent` + `IsdkOneGrabRotateTransformer` |
| 1축 직선 (서랍) | `IsdkOneGrabTranslateTransformer` |
| 손바닥 잡기 (문·손잡이) | `IsdkHandPalmGrabRecognizer` |
| pinch 잡기 | `IsdkHandFingerPinchGrabRecognizer` |
| 떨림 저감 | `IsdkOneEuroFilterDataModifier` |
| 물체별 잡기 포즈 | `IsdkHandGrabPose`, `IsdkGrabPoseCollection` |
| 위젯 직접 조작 | `IsdkPointableWidget`, `IsdkPointablePlane`, `IsdkPointableBox` |

1축 회전 제약의 실제 구조 (`Interaction/Grabbable/IsdkOneGrabRotateTransformer.h`):

- `FOneGrabRotationConstraint` — `RotationAxis`(`EAxis::X/Y/Z`), `AxisAngleRange`(`FIsdkAxisConstraints`: `bUseConstraint`/`Min`/`Max`), `MaxAngleFromAxis`, `bUseAngleFromAxisConstraint`

#### 안 쓸 것

| 제외 | 이유 |
|---|---|
| `IsdkRayInteractor` (레이 포인팅) | 13번 도달성 평가를 무의미하게 만든다 |
| `IsdkDistanceGrabDetector` (원거리 잡기) | 05번의 팔 뻗음·자세 부담이 사라진다 |
| `IsdkGrabFreeTransformer` (자유 6DoF·양손) | U-03 제외 범위 |
| `IsdkThrowable` (던지기) | "추적 복구 시 짐이 던져지지 않아야 한다"와 충돌 |
| `IsdkSyntheticHand` (손을 표면에 고정) | 손이 가상 형상을 관통하는 것을 **시각적으로 숨긴다**. 관통 문제의 정석 해법이기도 해서 **평가 담당자와 협의할 항목**으로 남긴다 |
| 포즈 인식기 4종 (`IsdkHandPoseDetectionProfile`, `HandDigitRecognizer`, `HandFingerRecognizer`, `HandThumbRecognizer`) | false positive, 평가자 학습 부담, 손 크기별 일관성 결여 |

#### ISDK에 **없는** 것

- **detent(단계 클릭) 기능이 없다.** `detent`/`step`/`notch` 전부 검색했으나 없고, 회전은 연속값 + 각도 min/max뿐이다. 에어컨 팬 1-2-3-4단 같은 단계 스냅은 직접 얹어야 한다.
- **손가락 구동 회전이 없다.** 모든 transformer가 grab point 기반이라, pinch로 잡고 돌리면 실질적으로 **손목 회전**이 된다. 실제 노브를 손가락으로 굴리는 동작과 다르다 (아래 §6 노브 항목).

#### 외부 손 데이터 입력 지점

`DataSources/IsdkExternalHandDataSource.h`:

- `SetHandData(const UIsdkHandData* InHandData, const FTransform& WorldTransform)` (`:71`)
- `SetHandJointMappings(ThumbJointMappings, FingerJointMappings)` (`:83`)

손목 포즈와 손가락 조인트를 **분리해서** 받는 구조다. 글로브를 도입했다면 이것이 통합 지점이었을 것이다. Quest 3 단독으로 결정했으므로 현재는 사용하지 않는다.

### 5. 버전·호환성

| 항목 | 현재 | 최신 | 비고 |
|---|---|---|---|
| UE | 5.7 (`vBoR.uproject`) | — | `Plugins/MetaXR/OculusXR.uplugin`의 `"EngineVersion": "5.7.0"` |
| MetaXR | **1.205.0** | **207.0** (2026-09-23) | 교체 검토 필요 |
| MetaXRInteraction (ISDK) | **1.205.0** (2026-10 포함, 당시에는 없음) | **207.0** (2026-09-22) | UE **5.7·5.8 모두 지원**, `.uasset`은 5.7 형식 직렬화 |

- ISDK 207.0은 **UE 5.7을 지원하므로 이 프로젝트에 도입 가능하다.**
- MetaXR와 ISDK가 **하루 차이로 함께 릴리스**됐다. `205 코어 + 207 ISDK`는 Meta가 시험한 조합이 아니므로 MetaXR도 함께 올리는 것이 안전하다. → 2026-10: 실제로는 ISDK 1.205.0이 들어와 코어와 버전이 같으므로 해당 없음.
- 도입 시 설정: **`Project Settings > Plugins > MetaXR > XR API = Epic Native OpenXR`** (ISDK 기본 샘플이 요구).
- 알려진 이슈(207.0): UE 5.7+에서 Fixed Foveated Rendering 런타임 변경 불가(프로젝트 설정으로 대체), Dynamic Resolution은 엔진 GitHub 포크에서만.

**미확인 (다운로드 페이지 직접 확인 필요):** `developers.meta.com`이 클라우드 세션 프록시에 막혀 페이지를 열지 못했다. ① MetaXR 최신 버전 번호가 정말 `207.0`인지 ② `VersionName: 1.<NNN>.0` ↔ 패키지 `<NNN>.0` 대응이 맞는지 ③ ISDK 207.0의 MetaXR 최소 버전 요구사항.

### 6. 미해결 설계 문제

#### 노브 (라디오·에어컨)

1축 회전 자체는 U-03 범위 안이지만 세 가지가 남는다:

- **조작 방식 미정** — (A) 손가락 굴림: 실제에 가깝지만 굴릴 실물이 없어 동작 유도가 어렵다. (B) 손목 회전(ISDK 기본): 큰 다이얼에는 자연스러우나 작은 노브엔 부적합하고 한 번에 돌릴 범위가 좁다. (C) 회전 포기, 단계만: 정확한 회전량을 요구하지 않아 **VR 전제에서 가장 현실적**이다. (D) 실물 노브 병용: Quest 3 결정과 무관하게 가능하다.
- **각도 wrap** — 노브는 여러 바퀴 돌 수 있고 180°를 넘으면 부호가 뒤집혀 시계 회전이 반시계로 읽힌다. 누적 회전각을 unwrap해서 따로 들고 있어야 한다.
- **추적 손실 복구** — 다른 각도에서 재인식되면 노브가 확 돌아간다. 재인식 시점의 손 각도를 새 기준으로 다시 잡아야(rebase) 한다. 문보다 증상이 눈에 띈다.

**평가 타당성:** 노브의 조작력·detent 느낌·표면 질감은 VR로 평가할 수 없다. 위치·시인성·라벨 판독·회전 방향 직관성까지가 한계다.

**시나리오 배치:** 원본 19개 항목에 노브 조작이 없다(12번은 관찰, 13번은 버튼 도달, 16번은 시트 폴딩). 17번을 만들 수 없으므로 12번에 포함하거나 **자유모드**로 보내야 한다. 자유모드가 적절해 보인다 — 순차 안내에 없는 조작을 12번에 끼우면 단계 완료 판정이 모호해진다.

#### 몰린 버튼 — 방식은 정해졌고 세부는 P0에서

정해진 것: 가장 가까운 것 하나만 활성 + 히스테리시스 + 시각 표시 + 판정 영역은 키우되 보이는 크기는 실제 그대로 + 13번은 판정 영역 확대 없음.

P0에서 정할 것: 확정을 **poke로 할지 pinch로 할지**. pinch 확정은 정확한 깊이까지 손을 밀어넣지 않아도 되므로 깊이 판단 부담이 없다.

### 7. 검토했지만 채택하지 않은 것

| 검토 대상 | 확인한 사실 | 결론 |
|---|---|---|
| **Manus Metagloves Pro Haptic** | 25 DoF EMF 손가락 추적(가림·드리프트 없음), 손가락당 LRA 진동 5개(120 Hz·256단계), 9DOF IMU는 **방향만** — 손목 위치는 외부 트래커 필요. **Quest에서 PC 필수, standalone 미지원.** 공식 Quest 3 경로는 **Quest 컨트롤러를 글로브에 장착**하는 것 | 미채택. PCVR 강제 + 스택 4개 + 참가자별 착용·보정·위생. 진동은 접촉 단서일 뿐 **힘 피드백이 아니라** 조작력 평가는 여전히 불가. 결정적으로 **VR이라 가릴 실물이 없어 글로브의 최대 장점인 "가림 없음"이 거의 무의미** |
| **Varjo XR-4** | 아이 트래킹 200 Hz 내장, 핸드 트래킹 내장(`XR_EXT_hand_tracking` 26 랜드마크, `XR_EXT_hand_interaction`의 aim·pinch·grasp·poke), **테더드 Windows PC 필수** | 미채택. 평가 목적이 찾기·읽기·도달성으로 정해지면서 **아이 트래킹이 필요 없어졌다.** 오히려 시선 조준은 도달성 평가를 없애므로 있어도 쓰면 안 된다. 전환하면 MetaXR·ISDK·U-03 조작 어휘 결정이 전부 무효가 되고 테더가 고정된다 |
| **아이 트래킹 / gaze 조준** | Quest 3·3S에 하드웨어 없음(Quest Pro 전용). ISDK 207.0에 gaze 기능이 추가됐으나 Quest 3에서는 머리 방향 | 미채택 + **금지로 확정.** 손을 뻗지 않고 선택하면 13번 도달성 평가가 사라진다 |
| **MR 전환** | XR-4로 가면 실물 대시보드를 만지며 조작할 수 있어 촉감·깊이 판단 문제가 근본적으로 해결된다 | 이번 범위 밖(CLAUDE.md 확정 제약). 다만 **오조작·간격 적정성을 평가하려면 VR 단독으로는 타당성이 약하므로**, 그 평가가 필요해지면 별도 범위 안건으로 올릴 것 |

### 8. LiftgateStudy에서 가져올 것

`bssani/LiftgateStudy` (public, UE 5.5, MetaXR 1.110.0 + ISDK 1.78.0).

- **C++에 ISDK 의존이 전혀 없다.** `LiftGateStudy.Build.cs`의 private 의존성은 비어 있고(주석만), `Liftgate.h:25`의 ISDK 언급은 주석이다. ISDK는 전부 Blueprint에 배선돼 있다. → **리프트게이트 C++ 이식은 ISDK 도입 여부와 무관하게 가능하다.**
- 대상: `Liftgate.h`(276줄) / `Liftgate.cpp`(407줄) / `LiftgateTypes.h`(54줄) / `HeightZone.h`(66줄) = 803줄
- **`.uasset`은 이식 대상이 아니다** — UE 5.5 형식이다. 에셋은 로컬에서 다시 만들어야 한다.
- `ELiftgateMode` 4종(Manual-Full / Manual-Assist / Power-Auto / Power-Hybrid)은 그 프로젝트의 **모드 비교 평가** 설계다. vBoR 04·06은 "간단한 조작에 의한 개방"이라 4종이 다 필요해 보이지 않는다. **이식 범위 미정.**
- `HeightZone`은 vBoR에 대응 항목이 없어 제외 예정.
- 참고할 만한 결정 문서: `Docs/decisions/ADR_001`(ISDK 기반 grab), `ADR_007`(confirm = button poke, 자체 제스처 금지). 후자는 "ISDK pinch detection은 false positive가 잦다"를 기록하고 있다.

### 9. 아직 열린 결정

| # | 결정할 것 | 막고 있는 것 |
|---|---|---|
| 1 | ISDK 도입 여부 | **해결(2026-10):** 1.205.0을 `Plugins/MetaXRInteraction/`에 포함 |
| 2 | 리프트게이트 이식 범위 | 모드 4종 전부 vs Manual-Assist 하나 |
| 3 | 노브 방식 (A/B/C/D) | P0 실측 후 판단이 나을 수 있다 |
| 4 | PCVR(Link) vs Quest 독립 실행(Android) | P0. 글로브를 뺐으므로 독립 실행이 다시 유력하다 → 2026-10 §5에서 판단 축 보강 |
| 5 | 노브 조작을 12번에 넣을지 자유모드로 보낼지 | 자유모드 쪽이 적절해 보인다 |

### 10. P0에서 실측할 것

VR 전제가 확정되면서 항목이 바뀌었다. 실물이 없으므로 "실물에 가려지는지"는 확인 대상이 아니다.

1. 허공에서 **가상 버튼·노브에 손을 맞출 수 있는지** (깊이 판단)
2. 확정을 **poke로 할 때와 pinch로 할 때** 어느 쪽이 참가자에게 자연스러운지
3. 히스테리시스 없이 가장 가까운 대상이 안정적으로 유지되는지
4. 시야 경계(무릎 근처·좌석 옆·등 뒤)와 **실물 좌석에 의한** 가림에서 트래킹 거동
5. 접촉 피드백을 어떤 방식으로 줄 때 참가자가 "닿았다"고 인지하는지
6. 마이크로제스처·pinch 강도 입력 키가 **Epic Native OpenXR 백엔드에서도** 동작하는지

### 11. 이 기간의 커밋

브랜치 `philip/peaceful-brahmagupta-9syieh`. 전부 문서 변경이며 **C++은 아직 작성하지 않았다.**

| 커밋 | 내용 |
|---|---|
| `aa035b2` | 핸드 트래킹 조작 범위를 1축 제약까지로 확정 (U-03), 조작 어휘 8종 표 |
| `e150e73` | 장비를 Meta Quest 3 단독으로 확정 |
| `b71a22d` | VR 전제 명시 + 잘못된 가림 근거 정정 (패스스루를 전제한 오류였다) |
| `609fbf0` | 몰린 버튼 선택 방식 (U-04), XR-4 미전환 |
| `0f903a0` | 클라우드 세션 테스트 파일 삭제 |

### 12. 조사 방법 메모

클라우드 세션(claude.ai/code)에서 진행했다. 다음 세션에서 같은 조사를 할 때 참고:

- **플러그인 API는 기억이 아니라 `Plugins/` 아래 헤더를 직접 읽어 확인했다.** CLAUDE.md의 지시대로다. 주석이 타입과 어긋나는 사례가 실제로 있었다(§2 함정).
- ISDK API는 리포에 없어서 `bssani/LiftgateStudy`를 클론해 읽었다. `git clone --depth 1` 로 받는다.
- 외부 사실(하드웨어 스펙·플러그인 버전)은 웹 검색으로 확인했다. **`developers.meta.com`, `manus-meta.com`은 이 환경의 프록시에 막혀 직접 열리지 않는다.** 검색 결과 스니펫으로 대체했으므로, 중요한 수치는 사용자가 원문에서 대조해야 한다.
