# K1 리뷰 — VRCalibrationKit 추출

명세: [K1](../Specs/K1-vrcalibrationkit-extract.md). 대상 리포: `https://github.com/bssani/VRCalibrationKit`, PR #1.

## 1회차 (2026-11-24, `31e1496`)

**판정: 거의 통과. 아래 2개를 반영한 뒤 머지한다.** Quest 3 `not-tested`.

확인한 것:
- **public 제약:**
  - `Plugins/MetaXR`가 git에 0개 들어 있다.
  - 코드·설정에 한글과 회사·차종 정보가 없다.
  - `vbor` 참조는 README 출처 한 줄과 마커 PDF뿐이다(R1-2).
- **범위:**
  - 손끝 캡처, 눈 정렬, 좌석 전환, HomeShift, 시나리오를 뺐다.
  - Core→Marker 역의존이 없다. Marker는 `PlatformAllowList: Win64`다.
  - 앵커 코드는 vBoR `VBoRAnchorAlignment.cpp`(M3c D1–D3)와 이름·주석 ID만 다르다(diff로 확인).
- **차 기준 정렬:**
  - 맞추기 계산을 차 기준 좌표로 하고(`VRCalAlignmentComponent.cpp` `FitCaptured`), Pawn을 차에 붙인다(`InitializeSetup`).
  - 미리보기(G)와 되돌리기(H)도 차 기준이다. 원래 부모가 없으면 떼어 낸다.
  - K1-E2·E3·E4가 static 동등성, 이동 추종, 앵커를 찾는 중 차 이동을 확인한다.
- **재현 (Claude, 같은 PC):**
  - `VRCalibrationKitEditor` 빌드 성공.
  - `VRCalibrationKit.*` 45/45 통과, EXIT 0(`Saved/Logs/ClaudeK1Review.log`).
  - 맵 로드는 Codex 보고를 그대로 받았다. Claude는 재현하지 않았다.

### R1-1 차 루트에 크기(scale)가 있으면 Pawn도 같은 크기가 되고, 오류 이유가 엉뚱하다 — 반영함 (`0899e10`)

- **문제:**
  - `VRCalAlignmentComponent.cpp` `FitCaptured`는 정렬점을 `GetActorTransform().InverseTransformPosition`으로 차 기준 좌표로 바꾼다. 그래서 차 루트의 scale로 나눈 값이 된다.
  - `InitializeSetup`은 Pawn을 붙인 뒤 `SetActorRelativeTransform(RelativePawn)`을 한다. 그러면 Pawn의 월드 scale이 차 루트 scale이 된다.
  - 발생 조건: CAD를 가져오면서 차 BP 루트에 scale을 준 경우(예: mm→cm로 0.1).
  - 결과: 측정점 거리와 차 기준 거리가 맞지 않아 `Fit … over limit` 또는 `alignment points moved`로 거부된다. 진짜 원인(루트 scale)을 알 수 없다. 거부되지 않는 경우(scale이 1에 아주 가까움)에는 VR 카메라가 커지거나 작아진다.
  - 명세에 이 경계를 적지 않은 것은 Claude의 빈틈이다.
- **제안(가장 단순한 것):** 루트 scale이 1이 아니면(허용 1e-3) 정렬하지 않는다. `NeedsCheck`로 두고 이유를 `Vehicle root scale must be 1 (put scale on child meshes)`로 쓴다.
  - 검사 위치는 `InitializeSetup`과 `SaveMarkerPoints`(마커 저장 전 거부)다.
  - scale을 차 기준 계산에서 빼고 붙이는 방식(`FAttachmentTransformRules`의 scale 규칙 `KeepWorld`)도 되지만, 메시 scale은 자식에 두는 것이 UE 관례라 거부로 충분하다.
- **확인할 시험:** 새 시험 `VRCalibrationKit.K1.E8RootScale`.
  - 차 루트 scale (2,2,2) → `InitializeAlignment` 뒤 `NeedsCheck`, 이유에 `scale`, Pawn이 붙지 않고 그대로.
  - 같은 상태에서 `SaveMarkerPoints`가 실패하고 이유에 `scale`.

### R1-2 마커 PDF 본문에 "vBoR marker set"이 인쇄된다 — 반영함 (`0899e10`)

- **문제:** `Docs/Markers/markers_aruco4x4_ID1-4.pdf`의 각 쪽 머리글이 `vBoR marker set - ArUco DICT_4X4_50, 60 mm`다(PDF 본문 문자열). public 리포의 인쇄물에 private 프로젝트 이름이 찍힌다. 명세 §2의 "출처 표기 예외"는 출처 한 줄을 뜻했고, 인쇄 제목까지 뜻하지 않았다.
- **제안:**
  - 머리글을 `VRCalibrationKit marker set - …`로 바꿔 PDF를 다시 만든다.
  - 마커 무늬, 크기(60·80 mm), 여백, 눈금, 배율 확인 선은 그대로 둔다.
  - 원본은 vBoR에서 Python OpenCV로 만들었다(vBoR Research-Log 2026-11-11). 생성 스크립트를 `Scripts/`에 함께 두면 다음에 다시 만들 수 있다.
  - 다시 만든 뒤 인쇄 크기를 확인한다. 300 dpi로 다시 그려 ID 1–4가 인식되고 한 변이 60.0·80.0 mm여야 한다. vBoR 때와 같은 확인이다.
- **확인할 시험:** 자동 시험 없음. 보고 ⑤에 위 인쇄 크기 확인 결과를 적는다.

### R1-3 코드·리포에 vBoR에서 온 이름이 남아 있다 — 반영함 (`0899e10`) (사용자 2026-11-24 "기능은 같지만 이름은 달라야 한다")

- **문제:** public 리포에 원 프로젝트를 드러내는 이름이 남아 있다.
  - `README.md:11` 출처 줄. vBoR 링크다.
  - 저장 파일 이름 `BuckEye.json`: `VRCalAlignmentComponent.cpp:40`, `MarkerTests.cpp:520`, `ProfileTests.cpp:31`·`:393`. 옛 눈 정렬에서 온 이름이다.
  - 시험 클래스 이름 `FM3b*`·`FM3c*`(`AnchorTests.cpp:17-133`, `ProfileTests.cpp:398`)와 `M2Flags`.
  - 시험 문구 `"M1 fit"`(`ProfileTests.cpp:360-361`).
  - vBoR 리뷰 ID를 단 시험 경로: `Anchors.R1WaitForAnchorActorPose`, `Anchors.R1MarkerAnchorSaveNotice`.
  - 마커 PDF 머리글(R1-2).
- **제안:**
  - README 출처 줄을 지운다.
  - 기본 저장 파일을 `Saved/Calibration/Alignment.json`으로 바꾼다. Buck 프로필 경로 `Bucks/<Name>.json`과 `ActiveBuck.txt`는 그대로 둔다. "buck"은 실물 목업을 뜻하는 일반 용어다.
  - 시험 클래스와 경로를 기능 이름으로 바꾼다. 예: `FAnchorDisabledTest`, `VRCalibrationKit.Anchors.WaitForAnchorActorPose`.
  - **파일 형식(v2 필드)은 그대로 둔다.** 기존 파일은 이름만 바꾸면 읽힌다. 명세 §2의 호환은 형식 호환으로 본다.
  - 설명서 `Docs/Integration-Guide.md`(Claude 작성)가 이 이름(`Alignment.json`)과 R1-1(루트 scale 1)을 이미 쓴다. 이름을 다르게 정하면 설명서도 같이 고친다.
- **확인할 시험:**
  - `git grep -n -i -E "vbor|buckeye|M3b|M3c|M2Flags|M1 fit"` 결과가 0줄이어야 한다(PDF 포함, `git grep -a`).
  - 바꾼 시험 이름으로 전체 시험이 통과해야 한다.

### 질문
(없음)

### 참고 (지적 아님)
- README 출처 줄이 private 리포 `bssani/vBoR`를 링크한다. 다른 사람에게는 404가 뜨고 이름만 보인다. 남길지는 사용자가 정한다.
- 라이선스 파일이 없다. 명세대로다. 사용자가 정할 일이다.

## 2회차 (2026-11-24, `0899e10`)

**판정: 통과. PR #1을 머지한다.** Quest 3 `not-tested`.

- R1-1: `InitializeSetup`과 `SaveMarkerPoints`가 루트 scale을 축마다 1e-3 허용으로 검사한다. 거부하면 `NeedsCheck`와 이유를 남기고, Pawn을 붙이지 않으며 파일도 만들지 않는다.
  - `LoadSetupAlignment`는 저장값이 없을 때도 scale 이유를 덮어쓰지 않는다.
  - `K1.E8RootScale`이 2·0.1·비균일·음수·1.002를 거부하고 1.0005를 허용하는지 확인한다.
- R1-2: PDF 머리글을 바꿨다. 생성·검증 스크립트 `Scripts/generate_markers.py`가 있다. Codex 보고: 300 dpi에서 ID 1–4가 인식되고 한 변이 60.000·80.000 mm다. 실제 프린터 출력은 사용자가 확인한다.
- R1-3: `git grep -n -i -a -E "vbor|buckeye|M3b|M3c|M2Flags|M1 fit"` 결과 0줄(Claude 확인). 기본 파일 `Alignment.json`. README의 출처 줄을 빼고 설명서 링크를 넣었다. 설명서에 scale 규칙과 메시지를 반영했다.
- 재현(Claude): 에디터 빌드 성공. `VRCalibrationKit.*` 46/46 통과, EXIT 0(`Saved/Logs/ClaudeK1Review2.log`).
- 보고 ③: 이름 검색을 다시 하다가 빌드와 시험을 한 번 더 했다. 이유가 타당하다.
