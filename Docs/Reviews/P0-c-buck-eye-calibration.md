# P0-c 리뷰 기록

명세: [Docs/Specs/P0-c-buck-eye-calibration.md](../Specs/P0-c-buck-eye-calibration.md) · 브랜치: `codex/P0-c-buck-eye-calibration`

**Codex가 할 일:**
- 상태가 `열림`인 항목을 위에서부터 반영한다.
- 항목의 세 칸은 이렇게 다룬다:
  - **문제**는 반드시 해결한다.
  - **제안**은 권장이다. 다르게 하면 보고 ③에 이유를 적는다.
  - **확인할 시험**은 새로 넣거나 고칠 자동화 시험이다.
- 같은 브랜치에 새 커밋으로 한다. 반영한 항목의 상태를 `반영함 (<커밋 제목>)`으로 바꿔 같은 커밋에 넣는다. 커밋 본문에 보고 ①~⑤를 쓴다.
- 판단이 필요하면 맨 아래 "질문"에 적고 멈춘다.
- push·`main` 머지는 하지 않는다. 끝나면 `HANDOFF.md`의 "Codex 결과"를 채운다.

## 회차

| 회차 | 날짜 | 리뷰 대상 | 리뷰어 | 결과 |
|---|---|---|---|---|
| 1 | 2026-10-12 | `0068eee`·`6b68d45`·`332925f` + 레벨(커밋 전) | Claude | **통과, 열린 항목 없음.** 빌드·시험 재현: non-unity(`.cpp` 27개)·unity 경고·오류 0, `vBoR.*` 69/69. 레벨은 Codex MCP 기록·스크린샷·해시로 확인하고 Claude가 커밋. Codex는 레벨 커밋·보고 직전에 멈췄다 |

### 1회차에 확인한 것

- **빌드·시험 재현.** 에디터를 닫고, vBoR 소스 수정 시각을 갱신한 뒤 빌드했다.
  - non-unity(`-DisableUnity -Module=vBoR`): `.cpp` 27개와 gen 파일을 하나씩 컴파일했다. 경고·오류 0.
  - unity: 경고·오류 0.
  - 시험: `vBoR.*` **69/69 Success**(P0·P1 기존 55 + P0-c 14), `TEST COMPLETE. EXIT CODE: 0`. 로그: `Saved/Logs/P0cR1ClaudeAutomation.log`.
  - Warning은 알려진 잡음뿐이다(Agent-Workflow §2.5).
  - 시험이 실제 저장 파일 `Saved/Calibration/`을 만들지 않았다. 시험 픽스처 파일도 남지 않았다.
- **확정 제약·명세 §3:**
  - 추적 공간을 바꾸는 호출이 소스 전체에 없다(§3-2). `ResetOrientationAndPosition`·`SetBase*`·`SetTrackingOrigin`을 grep으로 확인했다.
  - 헤드셋 자세는 `GetCurrentPose`(VALID)와 `IsTracking`(TRACKED)을 둘 다 본다(`P0BuckBench.cpp:131-132`, §3-3). 창 안에 TRACKED가 아닌 frame이 하나라도 있으면 캡처 전체를 취소한다(`VBoRBuckAlignment.cpp:75-80`).
  - Pawn 변환은 저장값(Stage 위치·Yaw)과 eye point 월드 자세로만 계산한다. 이전 Pawn 변환을 읽지 않으므로 누적이 없다(`VBoRBuckAlignment.cpp:21-26`, §3-4).
    - 식 확인: Pawn 위치 + R·P = eye 위치, Pawn Yaw = eye Yaw − 캡처 Yaw, Pitch·Roll 0.
    - 전제도 확인했다: 카메라가 `VROrigin`에 오프셋 없이 붙어 있다(`VBoRHandRigPawn.cpp:63-66`).
  - `bUseControllerRotationYaw`가 true면 적용하지 않는다(§3-6).
  - 재중심 처리: `VRHeadsetRecenter`를 받으면 캡처를 취소하고 "확인 필요"만 표시한다. Pawn은 그대로다. EndPlay에서 delegate·입력 바인딩을 해제한다(§3-8, E11, E15).
- **캡처 시간:** 캡처 창(C 2 s, M 0.5 s)은 초 단위다(명세 §5 개정).
  - 시간은 명령 경계부터 `FPlatformTime::Seconds`로 잰다. Tick dt에는 키를 누르기 전 시간이 섞이기 때문이다(Codex 자체 지적 I1).
  - 시험이 15/90 fps를 비교한다.
- **저장 형식:** 사람이 읽을 수 있는 JSON 버전 1(`Saved/Calibration/BuckEye.json`). 읽을 때 형식·값을 검사하고, 파일이 없거나 깨지면 "저장값 없음"으로 둔다(E8).
- **레벨 `Content/P0/L_P0c_BuckEye.umap`(§2-5).** Codex가 MCP로 만들고 저장했지만 커밋하기 전에 멈췄다. Claude가 아래 셋으로 확인하고 커밋했다.
  1. MCP 기록(`Saved/Logs/P0c-work/mcp-final-receipts.json`):
     - `create_level`(World Partition 끔)
     - `set_world_settings` gameMode = `/Script/vBoR.P0BuckBenchGameMode`
     - 액터: 시험대 `[0,0,0]` Yaw 0, PlayerStart, 바닥(Cube 20×20×1, 중심 Z −50 → 윗면 Z 0), 조명·하늘
     - PlayerStart를 `[-15,80,0]`(방석 밖, Y로 55 cm 여유)으로 옮김
     - 저장 뒤 미저장 패키지 0
  2. PIE 확인(Codex): 저장값이 없을 때 Pawn 위치 `[-15,80,0]`, controller Yaw false. **P1 레벨의 92 cm 바닥 어긋남이 없다.**
  3. 스크린샷 `P0cBuckEye-final-placement.png`·`-final-oblique.png`: eye 화살표, PlayerStart, 격자, 실행 중 생기는 좌석·핸들·대시보드. 커밋한 파일의 SHA-256이 Codex 보고(`2D7576A6…F07D`)와 같다.
- 범위·주석 말투·`Config`/`Plugins`/기존 레벨 변경 없음 — 맞다. 새 모듈 의존은 `Json` 하나다(저장 형식).

### 받아들인 것 (P0 전용이라 정리 때 지운다)

명세 "구현자가 정할 것" 안이지만 무겁게 간 것이다. 다음 명세부터는 AGENTS.md 원칙("최소 실험", "범용 프레임워크부터 만들지 않는다")대로 더 단순하게 한다.

- **격자를 렌더 프록시로 직접 그린다**(`P0BuckGroundGridComponent`, 약 130줄). 원인은 Claude 명세다. 격자를 레벨 바닥 윗면(Z 0)에 두라고 해서 바닥과 같은 평면이 됐고, 깊이 싸움(z-fighting)을 피하려고 렌더링용 depth bias를 쓴 것이다. 기준 높이(Z 0)는 그대로이고 렌더 스레드에서 UObject를 읽지 않는다.
- **저장 파일을 Win32 `MoveFileExW`로 원자 교체한다**(`VBoRBuckAlignment.cpp:220-236`). 이전 저장값을 지키려는 것이다. Codex가 "전체 시험 한 번에서 이 교체가 실패했고 원인을 모른다(M1)"고 보고했다. 이번 재현에서는 실패하지 않았다.
  - 다시 나오면 로그에 OS 오류 번호가 남는다. 백신·검색 색인이 파일을 잡고 있는 경우가 흔하다.
  - 진행자는 C를 다시 누르면 된다. 이전 저장값은 그대로 남는다.
- **캡처 시계를 주입할 수 있게 했다**(`CaptureClock`). 시험 전용 주입 지점이다.

### VR에서 볼 것 (명세 §7에 추가함)

- **시작 직후나 헤드셋을 벗어 둔 동안 플레이 영역 크기를 못 얻으면 "확인 필요(비교 불가)"가 남는다.** 명세상 이 상태는 새로 저장해야만 지워진다(§3-8·§3-9).
  - Link에서 Stage 경계가 언제 나오는지는 모른다.
  - VR Preview를 다시 켤 때마다 이 상태가 뜨면, "못 얻음"을 경고만 하고 확인 필요로 두지 않게 명세를 다시 정한다.
- 저장(C)이 "candidate replacement failed (OS error N)"로 실패하면 N을 알려준다(M1).

## 열린 항목

없음.

## 질문

없음.
