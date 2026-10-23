# P5y BP Pawn 반복 Play 잡기 — 질문

## 질문

### Q1 — E4 제외 기록 (명세 §4의 허용 조건 적용)

- 상태: **E4 제외, Claude 확인 대기**. 명세가 E4를 뺄 수 있다고 명시했으므로 E1·E2·E3·E5와 구현은 진행함.
- `UIsdkHandPalmGrabRecognizer`는 활성 상태를 직접 설정하는 공개 함수가 없다. `IsActive()`는 내부 네이티브 인식기를 읽고, `UpdateState()`는 유효한 손 visual/API frame·joint data가 있어야 활성 상태를 갱신한다 (`Plugins/MetaXRInteraction/Source/OculusInteraction/Public/HandPoseDetection/IsdkHandPalmGrabRecognizer.h:56-65`, `Private/HandPoseDetection/IsdkHandPalmGrabRecognizer.cpp:83-148`). Pinch도 공개 활성 setter가 없다 (`Public/HandPoseDetection/IsdkHandFingerPinchGrabRecognizer.h:58-70`).
- HMD 없는 이번 시험에서는 활성 상태를 거짓으로 대입하거나 `PalmGrabStarted.Broadcast()`를 인식기 활성화 증거로 간주하지 않았다. E4만 제외했다. 인식기 생성 직후·두 번째 Play 후 비활성 상태는 E2/E5로 검사한다.
- 확인 요청: 명세의 허용대로 E4 제외를 유지해도 되는가? 실제로 쥔 채 Play를 끝내는 상태 잔류 확인은 사용자 Quest 3 반복 Play 확인에 남아 있다(T-02·T-03·T-08, `not-tested`).

리뷰 회차·리뷰 지적은 아직 없음. Codex가 작성한 것은 이 질문 절뿐이다.
