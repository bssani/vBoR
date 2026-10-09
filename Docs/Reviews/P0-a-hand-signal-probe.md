# P0-a 리뷰 기록

명세: [Docs/Specs/P0-a-hand-signal-probe.md](../Specs/P0-a-hand-signal-probe.md) · 브랜치: `codex/P0-a-hand-signal-probe`

**Codex가 할 일:** 아래 "열린 항목"을 위에서부터 반영한다. 같은 브랜치에 새 커밋으로 하고, 반영한 항목의 상태를 `반영함 (<커밋>)`으로 바꿔 같은 커밋에 넣는다. 커밋 본문에 보고 ①~⑤. 명세와 어긋나거나 판단이 필요하면 맨 아래 "질문"에 적고 멈춘다. push·`main` 머지는 하지 않는다.

## 회차

| 회차 | 날짜 | 리뷰 대상 | 리뷰어 | 결과 |
|---|---|---|---|---|
| 1 | 2026-10-07 | `b02f6ab` 손 신호 프로브 | Claude | 통과. 명세 누락 1건(신뢰도 Low→High 튐) → Claude가 `8669f45`로 구현 |
| 2 | 2026-10-09 | `9108972` 다이얼 신호 | Claude | **통과(머지 막는 문제 없음).** 개선 4건 — 3건은 아래 열린 항목, 1건은 명세 §5에 Claude가 반영 |
| 3 | 2026-10-09 | `1dc1777` 2회차 반영 | Claude | **통과. 열린 항목 없음.** Codex가 R2-4에서 찾은 `8669f45`의 버그(직전 High 프레임을 못 읽으면 더 오래된 High 위치로 튐 계산)는 실제 버그이고 수정이 명세 §4-4에 맞다 |

### 3회차에 재현한 것

- 바뀐 C++ 3개 강제 재컴파일(샘플 플러그인 끔), 경고·오류 0. `vBoR.P0Probe` 12개 Success(샘플 플러그인 끔), `TEST COMPLETE. EXIT CODE: 0`.
- R2-1: 전환은 `UE_LOG`로만 남고 패널 최근 이벤트를 밀어내지 않는다. 세 횟수는 검지 pinch에 0이 되고, 초기 프레임·같은 상태 반복·pinch 초기화 자체는 전환으로 세지 않는다(`Dial.Transitions`).
- R2-2: `Dial.FrameSwing`의 꺾임 축 (2,-1,0)은 다이얼 축 (1,2,3)에 수직이다. swing·twist 순서로 합친 회전에서 twist 성분이 정확히 30°로 나오는 것을 손으로 확인했다. 꺾임 분리 회귀를 잡는다.
- R2-3: `Dial.ProjectionBoundary`가 0.05 mm 보류, 0.15 mm 갱신과 복구 첫 프레임 rebase를 확인한다.
- Codex가 ⑤에 남긴 "OpenXR API 1.1/런타임 1.0 불일치"는 문제가 아니다. 로그에 `xrCreateInstance succeeded with XR_API_VERSION_1_0`이 있다. 1.1을 시도한 뒤 1.0으로 정상 생성된 것이다. Agent-Workflow §2.5 잡음 목록에 넣었다.
- 다음 단계는 Quest 3 실측(명세 §5, 사용자)이다. 시험 상태는 `not-tested`.

## 열린 항목

없음 — 3회차(2026-10-09) 통과.

## 2회차 항목 (모두 반영함)

### 2회차에 재현한 것

- 다이얼 소스 3개 강제 재컴파일(샘플 플러그인 끔), 경고·오류 0.
- `vBoR.P0Probe` 9개 Success(샘플 플러그인 끔). 구현 전 새 시험 5개가 Fail이었던 것을 `P0DialRed.log`에서 확인.
- 기존 측정(`FP0ProbeMeasurement`) 변경 없음. 다이얼은 같은 프레임의 손끝 위치를 기존 측정 뒤에 읽는다.
- 손목 swing-twist: 월드 기준 프레임 사이 회전 차이를 정규화한 월드 축으로 분해하고, W≥0 반구에서 2·atan2로 ±180°. 다이얼 각도와 같은 부호 규칙이다. 맞다.
- 투영 길이 하한 0.1 mm: 계산 안정용으로 적절하다(품질은 거르지 않음 — 해석 지침을 명세 §5에 넣었다).
- 샘플 플러그인 참조·범위 밖 기능 없음.

### R2-1 엄지·검지 신뢰도와 다이얼 보류 전환을 기록하고 횟수를 센다 — 반영함 (`1dc1777`)

- **중요도:** 중간~낮음. 명세 누락(Claude 몫)이었고 명세 §2 "결과 표시"를 고쳤다.
- **위치:** `Source/vBoR/P0Probe/P0ProbePawn.cpp:244-245` — 값만 갱신하고 전환을 남기지 않는다.
- **발생 조건:** 손등 가림·손 낮춤 측정에서 엄지·검지가 잠깐 Low로 떨어졌다 돌아올 때.
- **영향:** 명세 §5 "다이얼: 가림"이 기록하라는 Low 빈도를 셀 수 없다. 패널은 현재 상태만 보이고, 로그는 초당 한 줄이라 짧은 Low를 놓친다. updating↔held 전환도 남지 않는다.
- **할 일:** 명세 §2 "결과 표시"의 새 항목대로. 전환은 `UE_LOG`에 한 줄씩(패널 최근 이벤트 목록에는 넣지 않음), 패널 다이얼 줄에 엄지 High→Low 횟수·검지 High→Low 횟수·held 진입 횟수, 세 횟수는 검지 pinch 눌림에 0. 시험은 명세 §4-6의 마지막 항목.

### R2-2 프레임 사이 회전에 꺾임(swing)이 섞인 경우를 시험한다 — 반영함 (`1dc1777`)

- **중요도:** 낮음. 구현은 맞다고 판단한다. 회귀 보호용이다.
- **위치:** `Source/vBoR/P0Probe/P0ProbeDialTests.cpp:96-99` — 꺾임은 시작 자세에만 있고 프레임 사이 변화(`Turn`)는 순수 비틀림이다.
- **발생 조건:** 실제로 손가락을 굴릴 때 손목이 앞뒤로 꺾이는 경우.
- **영향:** 꺾임 분리가 나중에 깨져도 지금 시험은 잡지 못한다. 손목 비틀림은 이 측정의 핵심 대조값이다.
- **할 일:** 명세 §4-6의 새 항목. (a) 프레임 사이 회전이 축에 수직인 꺾임뿐이면 손목 비틀림 ≈ 0 (b) 꺾임 + 비틀림이면 비틀림만 누적.

### R2-3 투영 길이 하한의 경계를 시험한다 — 반영함 (`1dc1777`)

- **중요도:** 낮음.
- **위치:** `Source/vBoR/P0Probe/P0ProbeDialTests.cpp:33`은 길이 0만 확인한다. `Source/vBoR/P0Probe/P0ProbeDialMeasurement.h:14`의 값 자체(0 < 길이 < 0.1 mm)는 검증되지 않는다.
- **영향:** 하한 비교가 빠지거나 cm/mm 단위가 바뀌어도 0 나누기 방지만 남아 있으면 시험이 통과한다.
- **할 일:** 명세 §4-6의 새 항목. 0.05 mm 보류, 0.15 mm 갱신.

### R2-4 `8669f45` 독립 리뷰 — 반영함 (`1dc1777`) (리뷰어: Codex)

- **이유:** `8669f45`(신뢰도 Low→High 튐, Low 중 최대 이동)는 Claude가 구현했다. 쓴 쪽이 리뷰하면 독립 확인이 없다.
- **할 일:** `git show 8669f45`를 명세 §2·§4-4 기준으로 리뷰한다. 결과는 아래 "Codex 리뷰" 절에 파일:줄·발생 조건·영향으로 적는다. 명백한 버그는 시험과 함께 고치고 그 커밋을 적는다. 판단이 필요한 것은 적기만 한다.

## 닫힌 항목

| 항목 | 내용 | 반영 |
|---|---|---|
| R2-5 | 실측 해석 지침(부호 확인, 전후 차이로 읽기, 투영 길이 vs 손끝 흔들림) | Claude — 명세 §5 "다이얼 값을 읽는 법" (이 파일과 같은 커밋) |

## Codex 리뷰

### 2026-10-09 — `8669f45` 독립 리뷰 (R2-4)

- **발견·수정:** `Source/vBoR/P0Probe/P0ProbePawn.cpp:441` (`8669f45` 기준). High 상태에서 위치 무효 또는 본 미준비 frame 이 마지막에 끼고 Low→High가 발생하면, 그 frame 에서 `bHasLastHighPosition`을 지우지 않아 더 오래된 High 위치를 `:395`의 복구 거리 기준으로 쓴다. 명세 §2·§4-4의 **Low 직전 High frame** 기준에 어긋나며 패널·로그에 잘못된 jump가 나온다. 현재 `:493`에서 모든 High frame 의 기준점 가용 여부를 갱신하도록 수정했다. 직전 High 위치를 못 읽으면 jump는 `n/a`다. 수정 커밋: `1dc1777`.
- **확인:** Low 구간 위치는 High 기준점을 덮어쓰지 않는다. Low 중 최대 이동은 첫 유효·본 준비 위치 기준이며 무효·본 미준비 frame 을 제외한다(`8669f45`의 `:431-438`). 새로운 Low 구간마다 초기화하며 위치 복구와 신뢰도 복구 이벤트가 별개다. 이 부분에서 추가 버그는 찾지 못했다.
- **시험:** `ConfidenceRecovery`에 직전 High의 위치 무효/본 미준비 2개 재현을 추가했다. 수정 전 두 경우 실패(`Saved/Logs/P0ReviewR2Red.log`), 수정 후 통과했다. Low 시작·중간의 본 미준비도 제외되는지 추가 확인했다. 판단이 필요한 지적 없음.

### R2-1~R2-3 반영·검증

- R2-1: 전환은 발생 frame 마다 손 이름·시각과 함께 `LogP0Probe`에 기록한다. 최근 이벤트 목록에는 넣지 않는다. 다이얼 줄에 엄지/검지 High→Low 횟수와 held 진입 횟수를 표시하고 해당 손 검지 pinch에서 초기화한다. 초기 sample·같은 상태 반복·reset 자체는 전환으로 세지 않는다. `Dial.Transitions` 수정 전 실패, 수정 후 통과.
- R2-2: `Dial.FrameSwing` — 비정렬 월드 축, 순수 swing, swing+30 deg twist, 이어지는 swing-20 deg twist를 확인한다. 손목의 전체 회전을 twist로 잘못 세는 임시 변이에서 실패했다(`Saved/Logs/P0ReviewR2SwingMutation.log`). 임시 변이는 복원했다.
- R2-3: `Dial.ProjectionBoundary` — 0.05 mm 보류·창 비움, 0.15 mm 복구 첫 frame rebase 및 이후 누적을 확인한다. 하한 비교를 0 나누기 방지만 남기도록 바꾼 임시 변이에서 실패했다(`Saved/Logs/P0ReviewR2Mutation.log`). 임시 변이는 복원했다.
- 최종 로컬 빌드: `Build.bat vBoREditor Win64 Development -Project=C:/Git/vBoR/vBoR.uproject -WaitMutex -DisablePlugin=OculusInteractionSamples`. 변경 C++ 3개 모두 재컴파일, `Result: Succeeded`, 종료 0, 빌드 경고·오류 0. 기록: `Saved/Logs/P0ReviewR2Build.log`.
- 최종 전체 시험: `UnrealEditor-Cmd.exe C:/Git/vBoR/vBoR.uproject -ExecCmds="Automation RunTests vBoR.P0Probe;Quit" -unattended -nullrhi -nosplash -nosound -DisablePlugins=OculusInteractionSamples -abslog=C:/Git/vBoR/Saved/Logs/P0ReviewR2.log`. 12개 모두 Success, `TEST COMPLETE. EXIT CODE: 0`, 프로세스 종료 0. 샘플 플러그인 mount 없음. 초기화 로그에는 샘플 InfoCardWidget CDO 오류(Workflow §2.5)와 OpenXR API 1.1/런타임 1.0 불일치가 있지만 자동화 시험 실패는 없다. 기기 시험 상태는 `not-tested`.
- 커밋 표시는 처음에 커밋 제목(`:/fix: P0-a 리뷰 2회차 반영`)으로 적었다. 자기 해시는 같은 커밋 안에 넣을 수 없기 때문이다. 3회차 리뷰에서 Claude가 실제 해시 `1dc1777`로 바꿨다.

## 질문

없음 (2026-10-09, R2-1~R2-4).
