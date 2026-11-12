# M1a 카메라 스파이크 — Link에서 마커 1장의 위치를 카메라와 손끝으로 비교

| 항목 | 값 |
|---|---|
| 성격 | **스파이크(버려도 되는 코드).** 사용자 결정 2026-11-11: M1 전체 전에, 전제(DX12 + Link에서 카메라 영상이 오는가, 카메라 좌표 = Stage인가)부터 확인한다. 결과로 [M1](M1-marker-alignment-probe.md)을 개정한다 |
| 베이스 / 작업 브랜치 | `main` / `codex/M1a-camera-spike` |
| 리뷰 | 가볍게(범위·금지 사항·빌드만). 리뷰 파일은 Claude가 필요할 때만 만든다 |
| 에디터 작업 | 있음 — 빈 레벨 하나(MCP) |

## 1. 할 것

1. 의존성: `vBoR.Build.cs`에 `MRUtilityKit`(private), Win64일 때만 `OpenCVHelper`·`OpenCV`. `vBoR.uproject`에 `OpenCV` 플러그인 켬. OpenCV 코드는 `WITH_OPENCV`로 감싼다.
2. `Source/vBoR/M1Probe/`에 Actor `AM1aCameraSpike` 하나:
   - BeginPlay: `UMRUKSubsystem::EnableWorldLock = false`(런타임 값, EndPlay에 되돌림 — 이유는 M1 D12).
   - **`M`:** 카메라 켜기·끄기. `UMRUKPassthroughCameraAccess`·`…Texture`를 `NewObject`로, 왼쪽 눈, 4:3 해상도(1280×960이 있으면 그것). 켜져 있는 동안 **새 영상(시각이 바뀐 것)마다** 픽셀을 읽고(M1 D3 — `ReadPixels` 아님, `GetTexture2DRHI` + `ReadSurfaceData`), ArUco `DICT_4X4_50`(`FOpenCVHelper::IdentifyArucoMarkers`)로 마커를 찾고, 한 변 `MarkerSizeMm`(기본 60)으로 마커 중심의 Stage 위치(cm)를 구한다. 방법은 자유(M1 D5·D6, §5 참고) — 단 **그 프레임의 재투영 오차(px)를 MRUK `WorldToViewportPoint`로 계산해** 같이 낸다.
   - **`T`:** 1 s 동안 왼손 검지 끝 원본(`AVBoRHandRigPawn::ReadRawIndexTip`)을 모아 Pawn 역변환으로 Stage 평균을 구하고, 가장 최근 마커 중심과의 차이(XY·Z·3D, mm)를 낸다. 추적이 끊기면 취소.
   - 화면: `GEngine` 메시지(PC 창). VR: 마커 중심에 작은 구.
3. **로그(`LogM1a`, 줄마다 따로):** 카메라 시작 결과·해상도·`GetCameraIntrinsics` 값 전부, 1초마다 요약(받은 영상 수, 인식 수, 마커 중심 Stage cm, 재투영 오차 px, 카메라 Stage 위치), `T` 결과. 이것으로 Claude가 축·부호를 판단한다.
4. 레벨 `Content/Development/M1/L_M1a_CameraSpike`: 빈 레벨 + 바닥 평면(`Mock`) + `AM1aCameraSpike`, GameMode Override `/Game/Main/Core/BP_vBoRGameMode`. 차·Runner 없음.
5. 자동 시험은 **하나만**: `vBoR.M1a.PoseFromCorners` — 알려진 카메라 자세로 정사각형 모서리 4개를 투영해 넣으면 중심이 1 mm 안으로 돌아오는지(자세 2가지, 하나는 Roll 포함). 이미지·카메라 없이.

## 2. 하지 말 것

- `Source/vBoR/Interaction/`·`L_Main`·`Content/Main` 수정, `BuckEye.json` 쓰기, 정렬 적용(Pawn 이동).
- 패스스루 화면 표시, 프로젝트 설정·RHI 변경, 영상 파일 저장.
- M1의 나머지(4점 맞추기, `G`/`H`, 확인점, 세션 창). 다듬기·일반화.
- 막히면 오래 붙잡지 않는다 — 원인 로그를 남기고 `HANDOFF.md` "Codex 결과"에 적고 멈춘다(예: `Play`가 false, 영상이 검정, `ReadSurfaceData` 실패). **"안 된다"도 이 스파이크의 정상 결과다.**

## 3. 끝낼 때

- 빌드 non-unity(`-Module=vBoR`)·unity 각 한 번, 위 시험 1개, `vBoR.*` 전체 한 번(기존 시험 그대로 통과).
- 커밋 본문·`HANDOFF.md` "Codex 결과"에 AGENTS.md 보고 ①–⑤.

## 4. 사용자가 Quest 3에서 (케이블 Link, VR Preview)

| ID | 할 일 | 볼 것 |
|---|---|---|
| Q1 | Play → `M` | 카메라 시작 로그·해상도, 마커를 보면 인식 수가 올라가는지, 60 s 동안 Pawn 그대로 |
| Q2 | 바닥이나 책상에 60 mm 마커 1장 → 0.5 m·1.0 m에서 본 뒤 검지로 중심을 누르고 `T`, 각 5회 | 카메라 − 손끝 차이. 일정한 방향으로 쏠리면 좌표계 차이(Claude가 로그로 판단), 1 cm 안팎이면 M1로 |
| Q3 | Q2 1회를 Air Link(Puppis)로 | 카메라가 켜지는지 |

결과 로그(`Saved/Logs/vBoR.log`의 `LogM1a`)를 Claude에 넘기면 M1을 개정한다. 시험 상태는 `not-tested`(스파이크).

## 개정 1 (Claude 2026-11-12) — Q1·Q2 1차 결과: MRUK 카메라 자세가 Stage가 아니다

**사용자 Quest 3 결과(케이블 Link, 모니터에 띄운 마커, 약 20 cm, 2회 Play, `LogM1a`):**
- 카메라 시작 성공(1280×960, intrinsics 정상). 마커를 볼 때 영상마다 인식. 재투영 오차는 머리를 멈췄을 때 0.18–0.76 px, 움직일 때 3–6 px.
- 계산한 카메라–마커 거리(약 21–23 cm)가 실제(약 20 cm)와 맞는다 → 크기·PnP는 맞다.
- **`T` 3회 모두 마커 − 손끝 ≈ X −8.15 m, Y −5.1~−5.2 m, Z +6.1~+6.3 m.** 손끝 Stage는 (33–35, 17–19, 72–74) cm로 정상. 그런데 "camera Stage cm"는 (−770, −515, 688) 근처로, 높이 6.9 m라 Stage일 수 없다.
- 원인: `UMRUKPassthroughCameraAccessSubsystem::GetCameraPose`는 `MRUKShared::GetHeadsetPoseAtTime`의 자세에 렌즈 오프셋을 더한다(`Plugins/MetaXR/Source/MRUtilityKit/Private/MRUKPassthroughCameraAccess.cpp:229-240`). 이 자세는 **MRUK 런타임 공간**이지 UE OpenXR Stage가 아니다. 스파이크는 이를 Stage로 가정했다(`M1aCameraSpike.cpp:285`, `SolveMarkerCorners(…, FTransform(Orientation, CameraStageCm), …)`).
  - 재투영 오차가 작게 나온 건 같은 MRUK 공간 안에서 왕복했기 때문이다(`WorldToViewportPoint`도 같은 자세를 씀). 축과 투영은 맞다.
  - 두 번의 Play에서 차이가 거의 같았다 → 기준 공간의 고정 차이일 가능성이 크다. 이번 개정으로 확인한다.
- 그 밖: 방 안의 무언가가 **ID 17**로 잘못 인식됐다(0.18–0.84 px). 손가락이 마커를 가리는 순간 마커 Z가 27 cm 튄 프레임이 하나 있었다(1.23 px).

### 할 것 (같은 브랜치 `codex/M1a-camera-spike`, 새 커밋)

1. **Stage 변환.** 마커는 카메라 기준 상대 위치로만 MRUK 값을 쓰고, Stage 위치는 UE 헤드셋 자세로 만든다.
   - `MarkerCam = CamPose_MRUK⁻¹ · Marker_MRUK`(지금 풀이 결과를 카메라 기준으로).
   - `CamPose_Stage = HmdPose_Stage · LensOffset`. `HmdPose_Stage`는 `GEngine->XRSystem->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, …)`(`Engine/Source/Runtime/HeadMountedDisplay/Public/IXRTrackingSystem.h:137`, 이 프로젝트에서 이미 Stage로 쓰는 값 — `VBoRVehicleAlignmentComponent.cpp:317`). `LensOffset`은 intrinsics의 `LensOffsetPosition`·`LensOffsetOrientation`(MRUK가 `GetCameraPose`에서 붙이는 방식과 같게).
   - `Marker_Stage = CamPose_Stage · MarkerCam`. `T` 비교와 VR 구는 이 값을 쓴다.
   - 영상 시각과 "지금" 자세 사이의 지연이 있다. 측정은 머리를 멈추고 하므로 이번에는 보정하지 않는다.
2. **기준 차이 로그(1초 요약에 추가).** `D = HmdPose_Stage · HmdPose_MRUK⁻¹`. `HmdPose_MRUK`는 `CamPose_MRUK · LensOffset⁻¹`. 위치(cm)와 Yaw·Pitch·Roll(deg)을 쓴다. Pawn 위치·Yaw도 같은 줄에 쓴다(리뷰 참고 2).
3. **ID 거르기.** ID 1–4만 받는다. 그 밖의 ID는 1초 요약에 "ignored id N" 개수로만 쓴다.
4. 하지 말 것은 §2 그대로다. 시험은 `vBoR.M1a.PoseFromCorners` 그대로 두고, 새 자동 시험은 만들지 않는다. 빌드(non-unity·unity)와 이 시험 하나만 한 번 돌린다. 전체 `vBoR.*`는 돌리지 않는다(Interaction 코드 변경 없음).

### 사용자가 다시 (Q2′)

- 같은 방법(모니터 마커, `Marker Size Mm` = 잰 값). 20 cm와 50 cm에서 각각 `T` 5회. 머리를 멈추고 한다.
- 그 사이 머리를 천천히 좌우로 돌리며 10초 동안 마커를 본다 → `D`가 일정한지 본다.
- 판정(Claude): `D`가 1 cm·0.5° 안에서 일정하고 `T` 차이가 1 cm 안팎이면 M1을 이 방식으로 개정한다. `D`가 머리 움직임에 따라 흔들리면 시각 보정(영상 시각의 UE 자세)이 따로 필요하다.
