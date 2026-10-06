# P0-a 손 입력 신호 프로브

| 항목 | 값 |
|---|---|
| 단계 | P0 환경·기준 확인 (계획서 §6) |
| 대상 | 조사 기록 2026-09 §10의 3·4·6번 실측, 2026-10 §2 함정 2(추적 손실·복구), 계획서 P0의 "손 입력·키보드 경로" |
| 베이스 브랜치 | `main` (ISDK 브랜치 머지 후) |
| 작업 브랜치 | `codex/P0-a-hand-signal-probe` |
| 상태 | 명세 — 구현 전. 시험 상태 `not-tested` |

## 1. 왜 만드는가

P1(골프백)과 P2(문)의 설계는 아직 모르는 값 몇 개에 달려 있다.

- **추적이 끊겼다 돌아올 때 손끝이 얼마나 튀는가.** AGENTS.md의 "추적 손실은 보류, 복구 시 누적 이동을 한 번에 적용하지 않는다"를 어떻게 구현할지가 이 값에 달려 있다. ISDK 잡기는 추적 신뢰도를 보지 않으므로(조사 기록 2026-10 §2 함정 2) 직접 얹어야 한다.
- **신뢰도가 언제 Low로 떨어지고, Low일 때도 위치가 오는가.** 시야 경계(무릎 근처·좌석 옆·등 뒤)와 실물 좌석 가림에서 확인한다. 이걸 알아야 보류를 Low에 걸지, 위치 무효에 걸지 정할 수 있다.
- **가만히 둔 손끝이 얼마나 흔들리는가.** 몰린 버튼의 히스테리시스 폭을 정할 근거다.
- **pinch·pinch 강도·마이크로제스처 키가 Epic Native OpenXR에서 실제로 들어오는가.** ISDK가 이 백엔드를 요구한다(조사 기록 2026-09 §5).
- **진행자 키보드 입력이 VR Preview 중 게임에 들어오는가.**

이 프로브는 위 값을 사람이 읽기 위한 **측정 도구**다. P1 이후 코드의 기반이 아니며, P0이 끝나면 지운다.

## 2. 범위

### 만들 것

전부 `Source/vBoR/P0Probe/` 아래에 둔다.

1. **`AP0ProbePawn`** — HMD 카메라, 좌·우 손 표시, 측정, 결과 표시.
2. **`AP0ProbeGameMode`** — `DefaultPawnClass = AP0ProbePawn`. 그 외 기능 없음.

### 손마다 측정·표시할 것

| 항목 | 출처 — 헤더로 확인할 것 |
|---|---|
| 손 위치 유효 여부 | `UOculusXRInputFunctionLibrary::IsHandPositionValid` — `Plugins/MetaXR/Source/OculusXRInput/Public/OculusXRInputFunctionLibrary.h:258` |
| 추적 신뢰도 (`Low`/`High` 2단계) | `GetTrackingConfidence` — 같은 파일 `:216`. 주석은 `[0,1]`이라 적혀 있지만 실제 타입은 2단계 enum이다(조사 기록 2026-09 §2) |
| 검지 끝·엄지 끝 월드 위치 | `UOculusXRHandComponent`(`Public/OculusXRHandComponent.h:28`, `UPoseableMeshComponent` 상속)의 `GetBoneLocation(..., EBoneSpaces::WorldSpace)`. 본 이름은 `GetBoneName(EOculusXRBone::Index_Tip / Thumb_Tip)` — `OculusXRInputFunctionLibrary.h:275` |
| 검지 끝 흔들림 | 최근 1초 동안 검지 끝 위치의 최대 편차(mm) |
| 신뢰도 하락·복구 | High→Low 시각, Low→High 시각, 지속 시간 |
| 위치 무효·복구 | 유효→무효 시각, 무효→유효 시각, 지속 시간, **마지막 유효 위치와 복구 첫 위치 사이 거리(mm)** |
| pinch 5손가락 눌림·뗌 | 키 `OculusHand_{Left,Right}_{Thumb,Index,Middle,Ring,Pinky}Pinch` |
| pinch 강도 (0~1) | 키 `OculusHand_{Left,Right}_{Finger}PinchStrength`. 손가락별 현재 값 |
| 마이크로제스처 5종 | 키 `OculusHand_{Left,Right}_MicrogestureSwipe{Left,Right,Forward,Back}`, `..._MicrogestureTapThumb` |

손과 무관하게 표시할 것:

| 항목 | 출처 |
|---|---|
| 키보드 | 아무 키 눌림 → 키 이름 |
| 활성 XR 시스템 | `GEngine->XRSystem`의 시스템 이름. OpenXR인지 OculusXR(OVRPlugin)인지 구분되게 |

키 이름에 관해: `FOculusKey`는 Private 헤더(`Plugins/MetaXR/Source/OculusXRInput/Private/OculusXRInputState.h:159` 이하)에 있어 include할 수 없다. 이름으로 `FKey`를 만들되, 문자열은 정의된 곳(`Private/OculusXRInput.cpp:88` 부근의 `const FKey FOculusKey::...` 정의)에서 그대로 옮긴다. 추측해서 쓰지 않는다. pinch 눌림 키는 `FKeyDetails::NotBlueprintBindableKey`로 등록돼 있다(`OculusXRInput.cpp:273`). 고른 바인딩 방식에서 이 키가 실제로 들어오는지 코드로 확인하고 보고한다.

### 손 표시

`UOculusXRHandComponent`의 `ConfidenceBehavior` 기본값은 `HideActor`다(`OculusXRHandComponent.h:47`). 프로브에서는 `None`으로 둔다. 신뢰도가 Low인 동안에도 본 위치가 갱신되는지가 측정 대상이기 때문이다. 이 설정이 본 갱신 자체를 바꾸는지는 구현을 읽고 보고한다.

### 결과 표시

- **레벨 공간에 고정한 텍스트 패널.** 헤드셋을 쓴 사람이 읽는다. 머리에 붙이지 않는다. 위치는 에디터에서 바꿀 수 있는 프로퍼티로 둔다.
- 패널에는 손별 현재 상태와 **최근 이벤트 10개**를 보인다.
- 같은 내용을 `UE_LOG`로 남긴다. 카테고리는 `LogP0Probe`. 이벤트(손실·복구, pinch, 마이크로제스처, 키보드)는 일어날 때 한 줄씩, 연속값은 1초에 한 줄 이하로 쓴다.

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| ISDK 의존(rig·interactor·grabbable·poke) | poke·pinch 시험대는 이 결과를 보고 다음 명세(P0-b)에서 정한다 |
| 필터·평활화·보류 처리 | 프로브는 **날것의 값**을 보여야 한다. 필터를 끼우면 측정할 대상이 사라진다 |
| 파일 저장·CSV·세션 기록 | AGENTS.md "자동 기록 시스템은 만들지 않는다". 로그로 충분하다 |
| OK/NG, 임계값 판정 표시 | 합격 수치는 만들지 않는다. 숫자만 보인다 |
| 공통 입력 명령·단계 진행 | P3 범위 |
| `Config/*.ini` 변경 | XR 설정은 사용자가 에디터의 Meta XR Project Setup Tool로 한다(§5) |
| `.uasset`/`.umap` 생성 | 필요한 에셋은 §5에 사용자 할 일로 적었다 |

## 3. 구현자가 정할 것

- **입력 바인딩 방식.** Enhanced Input 런타임 매핑과 `BindKey`/`GetInputAnalogKeyState` 중 고른다. 이 프로젝트의 기본 입력 컴포넌트는 Enhanced Input이다(`Config/DefaultInput.ini` 확인). 그 상태에서 동작하는 방식을 고르고 이유를 보고한다.
- **손 컴포넌트의 부모.** 모션 컨트롤러 아래인지 VR 원점 아래인지는 `OculusXRHandComponent`의 헤더와 구현을 읽고 정한다. 이 파일에 OVR용과 OpenXR용 루트 보정 회전이 따로 있다(`OculusXRHandComponent.h:22-24`).
- **`vBoR.Build.cs`의 의존 모듈.**

## 4. 완료 조건 — 구현자가 확인할 것

1. §2의 표에 있는 항목이 모두 패널과 로그에 나온다.
2. 복구 거리의 기준점은 **무효로 바뀌기 직전 프레임의 유효 위치**다. 무효 프레임에서 읽은 위치(0이거나 마지막 값에 고정된 값)로 계산하지 않는다.
3. 흔들림 1초 창에는 위치 유효·신뢰도 High인 프레임만 넣는다. 그렇지 않은 프레임이 끼면 창을 비우고 다시 채운다.
4. 신뢰도 하락과 위치 무효는 **별개의 이벤트**로 기록한다. 둘이 같은 순간에 일어난다고 가정하지 않는다.
5. 모든 OculusXR 호출을 `Plugins/MetaXR/` 헤더와 대조했고, 보고에 경로:줄을 적었다. 키 이름 문자열은 정의 위치를 적었다.
6. `ue-compile-check`를 통과했거나 로컬 빌드에 성공했다.
7. 보고에 "로컬에서 확인할 것"이 있다.

## 5. 로컬에서 확인할 것 — 사용자

### 에디터

1. Meta XR Project Setup Tool로 PC(Link) 기준 권장 설정을 적용한다. XR API는 **Epic Native OpenXR**, 핸드 트래킹을 켠다.
2. 빈 레벨과 바닥을 만들고 World Settings의 GameMode Override를 `AP0ProbeGameMode`로 둔다. 텍스트 패널 위치를 조정한다.
3. (선택) XR API를 OculusXR(OVRPlugin)로 바꿔 같은 측정을 한 번 더 한다. 두 백엔드의 신호 차이를 보기 위해서다.

### Quest 3 (Link, VR Preview)

이 측정은 P0 실측이며 계획서 7절의 T-01~T-09 시험이 아니다. 아래 절차는 측정 방법이고 합격 기준이 아니다.

| 측정 | 방법 | 기록할 것 |
|---|---|---|
| 흔들림 | 손을 허공에 10초 가만히 둔다. 팔을 뻗은 자세와 몸 가까이 둔 자세로 각각 3회 | 1초 창 최대 편차(mm)의 범위 |
| 시야 경계 | 손을 무릎 근처·좌석 옆·등 뒤로 천천히 옮긴다 | Low로 바뀌는 지점, 무효로 바뀌는 지점, Low인 동안 위치가 오는지 |
| 실물 좌석 가림 | Buck 좌석에 앉아 손을 좌석 옆으로 내린다 | 위와 같음 |
| 손실·복구 튐 | (a) 손을 시야 밖으로 빼서 움직인 뒤 다시 들인다 (b) 손을 그대로 두고 다른 손으로 가린다 | 지속 시간, 복구 거리 |
| 신호 백엔드 | 손가락별 pinch, pinch 강도, 마이크로제스처 5종을 각각 시도한다 | Native OpenXR에서 들어온 것과 안 들어온 것 |
| 키보드 | VR Preview 중 PC 키보드를 누른다 | 들어오는지, PIE 창 포커스가 필요한지 |

결과는 [조사 기록](../Research-Log.md)에 "P0-a 실측" 절로 남긴다. 이 결과로 P0-b(poke·pinch 확정 시험대)와 P1의 추적 손실 처리 방식을 정한다.
