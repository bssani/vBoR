# K1 VRCalibrationKit — 마커·앵커 정렬을 재사용 플러그인으로 복사

| 항목 | 값 |
|---|---|
| 결정 근거 | 사용자 2026-11-23·24. vBoR은 그대로 두고, 검증된 기능만 복사해 별도 VR 템플릿을 만든다. 첫 대상은 Calibration이다. 이름은 `VRCalibrationKit`이고, 새 리포에 새 UE 프로젝트를 만든다. 구현은 Codex가 한다 |
| 출처 | vBoR `main@6b030ec`(PR #148 M3c 머지)의 정렬·마커·앵커 코드. 기기 근거는 Research-Log 2026-11-23: Link를 다시 연결한 뒤 앵커로 복원해 마커와 비교하면 0.3 cm 안이었다 |
| 작업 위치 | 새 리포 `https://github.com/bssani/VRCalibrationKit`(**public**, 사용자 2026-11-24), 로컬 `C:\Git\VRCalibrationKit`. 브랜치 `codex/K1-vrcalibrationkit-extract`, 베이스는 `main@66ab2b1`(README와 `.gitignore`만 있음)이다 |
| 시험 상태 | Quest 3 `not-tested` |

## 1. 목적

차도, 시나리오도, 손 조작도 없는 작은 UE 5.7 프로젝트에서 다음 흐름이 vBoR 코드 없이 동작하게 한다.

> 마커로 정렬 → 저장(앵커 포함) → Play 다시 시작, 또는 Link 다시 연결 → 앵커로 복원 → 마커로 다시 재서 2 cm 안

정렬 결과는 **차 기준**으로 낸다. Pawn을 차 Actor에 붙여서, 차가 가만히 있으면(static) 지금 vBoR과 같은 결과가 나오고, 차가 움직이면(VR 주행) Pawn이 함께 따라간다. 주행 기능 자체는 이 명세에 없다.

## 2. 지켜야 할 조건 (바꾸지 않는 것)

- **vBoR 리포는 건드리지 않는다.** 읽기와 복사만 한다. 리다이렉트를 넣거나 vBoR 안에서 코드를 쪼개지 않는다.
- **검증된 계산과 기준을 그대로 옮긴다.**
  - 점 맞추기 계산: `FVBoRBuckAlignment::FitPoints`. XY 맞추기와 바닥 기준 높이(U-17)를 그대로 둔다.
  - 오차 기준: 기본값 `PointFitLimitCm` 2, `PointMinSpanCm` 20.
  - 마커 측정 기준: `MarkerSizeMm` 60, `MarkerSpreadLimitMm` 20, `MarkerYawLimitDeg` 30. 사전 ArUco `DICT_4X4_50`.
  - 앵커 M3c 동작:
    - D1: 1초·0.5 cm·0.5° 동안 멈춰 있을 때만 쓴다.
    - D2: 찾았을 때 drift와 흔들림 횟수를 기록한다.
    - D3: Tracking(Stage) 좌표로 읽고, 저장할 때도 실제로 만들어진 자세를 적는다.
    - 10초 안에 못 찾으면 저장 좌표로 돌아간다.
- **저장 파일 형식은 version 2를 그대로 쓴다.**
  - 내용: 측정점(Stage, cm), 추적 원점, 플레이 영역, 선택적 `anchor` UUID와 Stage 자세.
  - vBoR이 저장한 `BuckEye.json`이나 `Bucks/<Name>.json`을 이 키트가 읽을 수 있어야 한다. 반대로 vBoR이 이 키트의 파일을 읽을 필요는 없다.
- **카메라는 정렬할 때만 쓴다.** 마커 레벨에서만 카메라를 켜고, 영상은 PC 밖으로 보내지 않는다(vBoR AGENTS.md M1 예외와 같다).
- **public 리포다. Meta XR 플러그인은 커밋하지 않는다.** vBoR `.gitignore`의 주석에 따르면 Meta XR 일부가 Oculus SDK License라 vBoR은 private이어야 한다.
  - `Plugins/MetaXR/`는 `.gitignore`에 들어 있다. 로컬에는 vBoR에서 복사해 빌드에 쓰되 커밋하지 않는다.
  - 설치 방법은 README에 적혀 있다.
  - Meta XR 헤더나 소스를 이 키트의 코드로 옮겨 적지 않는다. include만 한다.
  - 커밋 전에 `git status`로 `Plugins/MetaXR`가 빠졌는지 확인한다.
- **코드와 에셋은 영어로 쓴다.** 주석은 짧은 조각문으로 쓴다(vBoR AGENTS.md "코드·에셋은 영어만"과 같다).
  - 회사 이름, 차종, 프로그램 이름, 계획서 내용은 넣지 않는다.
  - 주석의 근거 ID는 이 키트 기준(`K1 §n`)으로 쓴다. vBoR ID는 "출처" 한 줄에만 남긴다.

## 3. 범위

### 3.1 프로젝트
- UE 5.7 C++ 프로젝트 `VRCalibrationKit`를 만들고, 그 안에 플러그인 `Plugins/VRCalibrationKit`을 둔다.
- Meta XR 1.205.0을 vBoR에서 복사해 로컬 `Plugins/MetaXR/`에 둔다. 빌드용이고, 커밋하지 않는다(2절).
  - ISDK(`MetaXRInteraction`)는 복사하지 않는다. 손 조작이 없다.
- 프로젝트 설정은 vBoR에서 정렬에 필요한 것만 옮긴다.
  - OpenXR, 추적 원점 Stage, `WorldToMeters` 100.
  - Link에서 앵커와 패스스루 카메라를 쓰는 데 필요한 Meta XR 설정.
  - 무엇을 옮겼는지 보고 ④에 적는다.
- `.gitignore`는 첫 커밋에 이미 있다. 필요한 줄만 더한다.
- 라이선스 파일은 넣지 않는다. 사용자가 아직 정하지 않았다.

### 3.2 모듈
| 모듈 | 형태 | 들어갈 것 | 의존 |
|---|---|---|---|
| `VRCalibrationCore` | Runtime | 정렬점 컴포넌트, Buck 프로필, `FitPoints`, 저장·읽기(v2), 앵커 저장·복원(M3c), 정렬 컴포넌트(아래 3.3), 상태·이유·기록 이벤트 | Core, CoreUObject, Engine, Json, HeadMountedDisplay, XRBase, OculusXRAnchors |
| `VRCalibrationMarker` | Runtime, **Win64 전용** | 카메라 입력, ArUco 검출, solvePnP, 측정 세션, 미리보기(G/H), 저장(S), 화면 기록 | Core 모듈, MRUtilityKit, OpenCVHelper, OpenCV, RHI, RenderCore |

- Marker 모듈이 Core를 쓰고, 반대 방향 의존은 없다.
- Marker는 Win64에서만 빌드한다. vBoR `vBoR.Build.cs`의 Win64 분기와 같다.

### 3.3 정렬 컴포넌트 (차 Actor에 붙임)
vBoR `UVBoRVehicleAlignmentComponent`에서 **정렬에 해당하는 부분만** 옮긴다.
- BeginPlay에서 저장값을 읽고(`LoadSetupAlignment(true)`), 앵커를 찾아 Pawn을 놓는다.
- 진행자 다시 적용: vBoR의 A. 키는 샘플 쪽에 둔다.
- Recenter를 감지하면 NeedsCheck로 바꾼다.
- 플레이 영역 비교로 NeedsCheck를 판정한다.
- Buck 프로필 선택(`Bucks`, `FirstMarkerId`)을 그대로 옮긴다.
- 상태(`Applied`·`NoSavedValue`·`NeedsCheck`), 이유 문자열, 기록 이벤트(`OnPointRecord`에 해당)를 낸다. BP에서 읽을 수 있게 한다.

**차 기준 정렬 (새로 넣는 것):**
- 맞추기 계산을 월드 좌표가 아니라 **차 Actor 기준 좌표**로 한다. 정렬점 위치와 바닥 높이를 차 기준으로 넣는다.
- 결과로 나온 Pawn의 차 기준 위치를 Pawn에 적용하고, **Pawn을 차 Actor에 붙인다**(Attach, 상대 위치 유지).
- 차가 원점에 가만히 있으면 vBoR과 같은 월드 위치가 나와야 한다(시험 K1-E2).
- 차가 움직이면 Pawn이 따라간다. 컴포넌트가 따로 Tick에서 따라가게 하지 않고, 붙인 결과로 따라가게 한다.

### 3.4 마커 측정 Actor
vBoR `AM1MarkerAlignProbe`와 `M1MarkerCamera`를 옮긴다. 키는 M(측정·취소), G(미리보기), H(되돌리기), S(저장)다.
- MRUK World Lock을 끄는 동작을 그대로 둔다.
- `Marker fit vs pawn` 기록을 그대로 둔다. 이것이 판정 기준이다.
- 손끝 비교(T)는 넣지 않는다. 정렬은 이제 마커로만 한다. 그래서 `VBoRHandRigPawn` 의존이 사라진다.

### 3.5 샘플
- `Content/Samples/` 아래에 다음을 둔다.
  - `BP_SampleCar`: 근사 차. 박스 메시, 정렬점 4개, 정렬 컴포넌트.
  - `L_Calibrate`: 마커 측정 Actor와 `BP_SampleCar`.
  - `L_Sample`: `BP_SampleCar`, 저장값 복원.
- 샘플 Pawn은 카메라만 있는 가장 단순한 VR Pawn으로 한다.
- `BP_SampleCar`를 앞뒤로 움직이는 디버그 키(예: 화살표 키, 기본 꺼짐)를 둔다. 붙인 Pawn이 따라가는지 볼 수 있게 하기 위해서다.
- 마커 인쇄본은 vBoR `Docs/Markers/vBoR_markers_aruco4x4_ID1-4.pdf`를 복사하되, 파일 이름에서 `vBoR`을 뺀다.

### 3.6 자동 시험
- 옮긴 기능에 해당하는 vBoR 시험을 옮긴다. 시험 경로는 `VRCalibrationKit.*`로 바꾼다.
  - 마커 계산: `M1MarkerAlignTests.cpp` 중 손끝 비교(T)를 뺀 것.
  - 프로필과 저장: `M2BuckProfileTests.cpp`.
  - 앵커: `M3bAnchorAlignmentTests.cpp`의 M3b·M3c.
- 옮기지 않는 시험과 그 이유를 보고 ③에 적는다.
- 아래 4절의 K1-E 시험을 새로 만든다.

## 4. 경계 상황과 시험

| ID | 상황 | 기대 | 시험 |
|---|---|---|---|
| K1-E1 | vBoR이 저장한 v2 파일(앵커 있음·없음) | 그대로 읽고 같은 Pawn 위치 | 새 시험: vBoR 시험의 고정값 파일로 왕복 |
| K1-E2 | 차가 원점·회전 0에 가만히 있음 | 월드 Pawn 위치가 vBoR `BasePawn` 결과와 같음(0.1 mm 안) | 새 시험 |
| K1-E3 | 차가 놓인 자리가 원점이 아님(위치·Yaw), 정렬 뒤 차를 옮김 | 정렬 결과(차 기준)는 같고, Pawn이 차 이동만큼 따라감 | 새 시험 |
| K1-E4 | 앵커를 찾는 중에 차가 움직임 | 찾은 뒤 결과가 차의 현재 위치 기준 | 새 시험 |
| K1-E5 | 앵커 스위치 꺼짐(기본값) | M3b E1과 같음. 앵커 요청 없음 | 옮긴 시험 |
| K1-E6 | 사용자 `Config/UserEngine.ini`에 앵커 스위치가 켜져 있음 | 시험 결과가 달라지지 않음. 시험 안에서 스위치를 직접 정한다 | 새 시험 또는 고정 방법 보고. vBoR에서는 이 파일 때문에 `M2.E9MarkerSave`가 실패했다(M3c §4) |
| K1-E7 | Marker 모듈 없이 Core만 씀(비 Win64 상황) | Core만으로 빌드·저장값 복원 가능 | Core만 의존하는 시험 모듈 또는 보고 |

## 5. 확인한 근거 (vBoR main 기준, 경로:줄)

- 맞추기 계산과 바닥 높이: `Source/vBoR/Interaction/VBoRBuckAlignment.cpp:15-46`. 월드 정렬점 위치를 넣는 곳은 `VBoRPointAlignment.cpp:60-67`, Pawn을 내는 곳은 `:70-91`(앵커 다시 매핑 `:78-82`).
- 저장: `VBoRBuckProfiles.cpp:139-167`(`SaveMarkerPoints` → `Store::Save` → `SaveAlignmentAnchor`).
- 앵커: `VBoRAnchorAlignment.cpp` 전체(M3c D1–D3). Tracking 읽기는 `Plugins/MetaXR/Source/OculusXRAnchors/Public/OculusXRAnchorBPFunctionLibrary.h:40`, OpenXR 구현은 `Private/openxr/OculusXRAnchorsXR.cpp:529-562`.
- 정렬 컴포넌트에 섞여 있는 것(옮기지 않음):
  - 좌석 전환: `SwitchViewpoint`·`ReturnToBase`·`AdvanceSwitch`, `VBoRVehicleAlignmentComponent.h:34-38`.
  - 눈 정렬(P0-c): `BaseEye`·`UpdateEyeAlignment`.
  - HomeShift.
  - 손끝 점 캡처: `CapturePoint`·`SavePoints`·`ReadPointTip`, `VBoRPointAlignment.cpp:93-98`의 `AVBoRHandRigPawn` 캐스팅.
- 마커: `M1Probe/M1MarkerCamera.cpp:29`(solvePnP), `:87`(ArUco). `M1MarkerAlignProbe.cpp:70-110`(World Lock 끄기, 컴포넌트 찾기), `:430-560`(맞추기·비교·저장).
- MRUK World Lock: `Plugins/MetaXR/Source/MRUtilityKit/Private/MRUtilityKitSubsystem.cpp:1081-1097`.

## 6. 범위 밖

- vBoR 수정과 vBoR이 이 플러그인을 쓰게 바꾸는 일.
- 손끝 점 캡처, 눈 정렬(P0-c), 좌석 전환과 암전, HomeShift, 시나리오, 진행자 창.
- VR 주행: 차량 물리, 주행 입력, 흔들림 거르기, 멀미 대책.
- 실행 중 계속 보정(앵커를 계속 지켜보다 다시 맞추기). 이 명세 뒤 따로 다룬다.
- 같은 레벨 안의 정렬 모드. 이번에는 vBoR처럼 정렬 레벨을 따로 둔다.
- Android(Quest 독립 실행), 패키징, 마켓플레이스 배포.
- 앵커를 찾는 시간 개선(중간값, 허용 범위). M3c 동작을 그대로 옮긴다.

## 7. 구현자가 정할 것

- 클래스 접두어와 이름. 예: `UVRCalAlignmentComponent`, `AVRCalMarkerCalibrator`. 리포 안에서 한 가지로 통일한다.
- 앵커 스위치를 콘솔 변수(`VRCal.AnchorAlignment`)로 둘지, 컴포넌트 프로퍼티로 둘지. 기본값은 꺼짐.
- 저장 폴더 이름. 기본은 `Saved/Calibration/`이라 vBoR과 같다.
- 샘플 Pawn과 디버그 키를 만드는 방법.

## 8. 완료 조건

1. 새 리포가 vBoR 코드·에셋·경로를 참조하지 않는다. `grep -ri vbor`로 확인한다. 출처 한 줄과 마커 PDF의 출처 표기는 예외다.
2. non-unity 빌드(`-DisableUnity -Module=VRCalibrationCore`, `VRCalibrationMarker` 각각)와 unity 빌드가 경고 없이 성공한다.
3. `VRCalibrationKit.*` 전체 시험이 통과한다. K1-E1~E7을 포함한다.
4. `L_Calibrate`와 `L_Sample`이 에디터에서 오류 없이 열린다. 맵 로드 확인 방법은 vBoR Agent-Workflow를 따른다.
5. 보고와 커밋 본문에 vBoR AGENTS.md 형식의 ①~⑤를 쓴다.

## 9. 로컬에서 확인할 것 (Quest 3, `not-tested`)

1. `Config/UserEngine.ini`에 앵커 스위치를 켠다. 시험이 끝나면 지운다.
2. `L_Calibrate`에서 M → S. `anchor saved … created vs requested`가 나와야 한다.
3. `L_Sample`을 Play한다. `anchor found … drift ≈ 0`이 나오고, 차 박스가 실물 마커 자리에 와야 한다.
4. Link를 끊었다 다시 연결한 뒤 `L_Calibrate`를 Play하고 **M만** 누른다. `Marker fit vs pawn` dX·dY가 2 cm 안이어야 한다.
5. `L_Sample`에서 디버그 키로 차를 옮긴다. 시점이 차와 함께 움직여야 한다(차 기준 정렬 확인).
