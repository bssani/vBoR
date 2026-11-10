# M1a 카메라 스파이크 — 리뷰

## 1회차 (2026-11-11, `549994e`) — 통과 (스파이크 기준)

**판정:** 메모 범위 그대로, 금지 사항 위반 없음. 그대로 Quest 3 Q1–Q3를 한다. 아래 참고는 **고치지 않아도 된다** — 기기 결과를 보고 필요할 때만 다음 작업에 넣는다.

**확인한 것:**
- 범위: `git diff main...codex/M1a-camera-spike`에 `Source/vBoR/Interaction/`·`Content/Main`·`Config` 변경 없음. `BuckEye.json` 쓰기·Pawn 이동 없음. 패스스루 표시 없음.
- 빌드·시험 로그: `M1a-non-unity-retry-UBT.log`·`M1a-unity-UBT.log` `Result: Succeeded`, `M1a-Automation.log` `TEST COMPLETE. EXIT CODE: 0`.
- 축 변환이 MRUK와 같다: OpenCV (오른쪽, 아래, 앞) → (앞, 오른쪽, 아래) = MRUK 투영의 (X, Y=u, Z=v)(`MRUKPassthroughCameraAccess.cpp:308-311`, `:341-343`). 영상 픽셀 기준 초점·주점을 뷰포트 (0,0)·(1,1) 광선으로 구하는 식도 맞다(잘라 낸 영역 포함, `:266-279`).
- IPPE 사각형 모서리 순서(`calib3d.hpp:951-963`)와 ArUco 모서리 순서(왼위·오위·오아래·왼아래)가 같다.
- 마커 0개면 `IdentifyArucoMarkers`가 false(`OpenCVHelper.cpp:394-397`) → `Markers[0]` 접근 안전. 모서리 subpixel 보정 켜짐(`:386`).
- `ReadRawIndexTip`은 Tracked가 아니면 false(`VBoRHandRigPawn.cpp:156`) → `T` 취소가 동작.
- 영상 읽기 뒤 `FlushRenderingCommands`로 같은 프레임의 자세와 맞춘다(프레임마다 멈춤 — 스파이크라 괜찮음, Q1에서 체감 확인).

### 참고 1 — 검정 영상 자동 정지가 이를 수 있음

- **문제:** `M1aCameraSpike.cpp:342-347` — 1초 요약 구간의 모든 영상이 검정이면 카메라를 끈다. Link 카메라가 켜진 직후 잠깐 검정 영상을 줄 수 있다면(미확인) 정상인데도 꺼진다.
- **제안:** Q1에서 `Camera returned only black frames`가 켜자마자 나오면 `M`을 한 번 더 누른다. 계속 나오면 실제 문제(권한·Link 설정)다. M1에서는 경고만 하고 몇 초 연속일 때 끄는 것으로.
- **확인할 시험:** 없음(스파이크).

### 참고 2 — Q1 "Pawn 그대로"가 로그에 없음

- **문제:** 1초 요약(`:334-341`)에 Pawn 위치가 없어, World Lock이 Pawn을 옮겼는지 로그로 볼 수 없다.
- **제안:** Q1은 눈으로 본다(바닥·구가 미끄러지지 않는지). M1에서는 요약에 Pawn 위치·Yaw를 넣는다.

### 참고 3 — 자동 시험은 축 확인을 못 한다

- 시험(`:404-446`)은 풀이와 같은 투영식으로 모서리를 만들어서 PnP 계산만 확인한다. **축·부호가 MRUK와 맞는지는 실행 중 재투영 오차(`WorldToViewportPoint`, `:313-317`)로만 보인다.** Q1–Q2에서 마커가 인식되는데 재투영 오차가 수 px 이상이면 축 문제다. 1 px 안팎이면 축은 맞다.
- 뷰포트 0·1은 픽셀 가장자리, ArUco 모서리는 픽셀 중심 기준이라 0.5 px 차이가 있다. 60 cm에서 약 0.3 mm — 무시.

### 참고 4 — OpenCV 플러그인이 에디터 첫 시작 때 pip 다운로드를 한다 (명세 빈틈, Claude)

- **사실:** `OpenCV.uplugin`이 `PythonFoundationPackages`(에디터 전용)와 `opencv-python==4.5.5.62`를 요구한다. 플러그인을 켠 뒤 첫 에디터 시작에서 `pip install --require-hashes -r Intermediate/PipInstall/merged_requirements.txt`가 PyPI에서 패키지를 받았다(`Saved/Logs/M1a-PoseFromCorners.log:1290-1308`). 해시 고정이고 받기만 한다(회사 데이터를 보내지 않음).
- **영향:** 회사 PC가 인터넷·프록시에 막혀 있으면 첫 에디터 시작이 느리거나 실패할 수 있다(미확인).
- **결정(Claude 권장):** 이 브랜치는 **50% 평가(11/19–20) 전에는 `main`에 머지하지 않는다.** 집 PC에서 이 브랜치로 Q1–Q3를 한다. M1을 `main`에 넣을 때 회사 PC 확인 항목(Company-PC-Guide)에 넣는다.

### 다음

- 사용자: 이 브랜치(`codex/M1a-camera-spike`, 지금 작업 폴더에 체크아웃됨)로 빌드된 에디터에서 `L_M1a_CameraSpike` VR Preview → 메모 §4 Q1–Q3. `Saved/Logs/vBoR.log`의 `LogM1a` 줄을 Claude에 넘긴다.
- Claude: 로그로 축·좌표계(카메라 − 손끝 차이)를 판단하고 M1 명세를 개정한다.
