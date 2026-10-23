# P5y BP Pawn 반복 Play 잡기 — 질문

## 1회차 (2026-10-23, Claude) — 통과 (문제 없음, 참고 3개)

- 리뷰한 커밋: `bef15f3` fix: BP Pawn 잡기 인식기 인스턴스 분리. 베이스 `main@cf384dc`.
- Codex 토큰: 5.40M.

### 확인한 것

| 항목 | 결과 |
|---|---|
| D1 재현(red) | 고치기 전 BP E1·E2·E3 실패, C++ E1·E5 통과. 실패 단언이 공유 객체를 바로 보여 준다: `Hand0.Rig.PalmGrabRecognizer`·`PinchGrabRecognizer` → `/Game/Main/Core/BP_vBoRPawn.Default__BP_vBoRPawn_C:LeftHandRig...`(BP 클래스 기본 객체의 rig 것). 조건 그룹은 공유되지 않았다. 사용자 증상(BP만·두 번째 Play부터·hover는 됨)과 맞는다 |
| D2 수정 | `AVBoRHandRigPawn::PreRegisterAllComponents`(`Interaction/VBoRHandRigPawn.cpp`)에서 rig의 두 인식기가 이 rig 것이 아니거나 템플릿이면 이 rig를 outer로 새로 만들고 편집 가능 설정만 복사한다(delegate·손 visual·네이티브 상태는 복사 안 함 — 손 visual은 rig가 Play 때 다시 넣는다, `IsdkHandRigComponent.cpp:183`). 리플렉션으로 바꿔 ISDK 헤더·`Plugins/`는 그대로. C++ Pawn은 이미 자기 것이라 건너뛴다. ISDK가 쓰기 전(등록·InitializeComponent·BeginPlay 전)이다. `AVBoRPawn::PreRegisterAllComponents`는 `Super`를 먼저 불러 순서가 맞다(P4f D5 visual 수정 그대로) |
| 범위 | 제품 코드는 `VBoRHandRigPawn.h/.cpp`뿐. 에셋·GameMode·`Plugins/` 변경 없음 |
| 시험 | 새 `vBoR.P5y.*` 5개(E1 BP·C++, E2, E3 실제 손바닥 overlap → 선택·놓기, E5 C++ 반복). E4는 Q1대로 제외. 기존 시험 수정 없음 |
| 빌드·시험 재현 | non-unity·unity Succeeded(링크만 함. Codex 로그 `P5yFinalNonUnityBuild.log` 62개·`P5yFinalUnityBuild.log` 7개 컴파일, 경고 0). `vBoR.*` 562/562 통과(557 + 5, `Saved/Logs/P5yReviewAll.log`) |

### 문제

없음.

### 참고 (고치지 않아도 됨)

1. **복사 기준이 기본 객체의 인식기다.** 그 객체는 첫 Play에서 실제로 쓰여 설정 값이 바뀌었을 수 있다(지금은 Play 중에 바꾸는 편집 설정이 없어 영향 없음). 나중에 인식기 설정을 Play 중에 바꾸는 코드가 생기면 클래스 기본값(`GetClass()->GetDefaultObject()`)에서 복사하는 쪽이 안전하다.
2. **사용자 확인(`not-tested`):** L_Main `BP_vBoRPawn`으로 세 번 연달아 Play, 쥔 채 Play 끝내고 다시 Play, 진행자 레벨 다시 열기 뒤 — 매번 박스·문 손잡이·레버·휠이 잡히는지(T-02·T-03·T-08).
3. **임시 우회를 썼다면 되돌린다:** `BP_vBoRGameMode`의 Pawn을 C++로 바꿔 두었으면 `BP_vBoRPawn`으로 되돌린다(P4f E7).

## 질문

### Q1 — E4 제외 기록 (명세 §4의 허용 조건 적용)

- 상태: **답함 (Claude 2026-10-23) — E4 제외 유지.** 명세 §4가 허용한 경우다. 인식기를 진짜로 활성으로 만들 공개 경로가 없고, 가짜 대입은 증거가 못 된다는 판단이 맞다. 쥔 채 끝낸 상태가 남지 않는지는 사용자 Quest 3 반복 Play로 본다(`not-tested`).
- (원래 상태: E4 제외, Claude 확인 대기) 명세가 E4를 뺄 수 있다고 명시했으므로 E1·E2·E3·E5와 구현은 진행함.
- `UIsdkHandPalmGrabRecognizer`는 활성 상태를 직접 설정하는 공개 함수가 없다. `IsActive()`는 내부 네이티브 인식기를 읽고, `UpdateState()`는 유효한 손 visual/API frame·joint data가 있어야 활성 상태를 갱신한다 (`Plugins/MetaXRInteraction/Source/OculusInteraction/Public/HandPoseDetection/IsdkHandPalmGrabRecognizer.h:56-65`, `Private/HandPoseDetection/IsdkHandPalmGrabRecognizer.cpp:83-148`). Pinch도 공개 활성 setter가 없다 (`Public/HandPoseDetection/IsdkHandFingerPinchGrabRecognizer.h:58-70`).
- HMD 없는 이번 시험에서는 활성 상태를 거짓으로 대입하거나 `PalmGrabStarted.Broadcast()`를 인식기 활성화 증거로 간주하지 않았다. E4만 제외했다. 인식기 생성 직후·두 번째 Play 후 비활성 상태는 E2/E5로 검사한다.
- 확인 요청: 명세의 허용대로 E4 제외를 유지해도 되는가? 실제로 쥔 채 Play를 끝내는 상태 잔류 확인은 사용자 Quest 3 반복 Play 확인에 남아 있다(T-02·T-03·T-08, `not-tested`).

리뷰 회차·리뷰 지적은 아직 없음. Codex가 작성한 것은 이 질문 절뿐이다.
