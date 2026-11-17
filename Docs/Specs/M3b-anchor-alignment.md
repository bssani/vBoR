# M3b 앵커로 정렬 지키기 — 헤드셋이 잠들었다 깨도 차가 제자리

| 항목 | 값 |
|---|---|
| 결정 근거 | 사용자 2026-11-18: "anchor 하는것도 작업해놔. 당장 이번에 안쓰더라도 기능 구현은 해놔." 문제는 M3a와 같다. 헤드셋이 잠들었다 깨면 Stage 원점이 옮겨져 차가 어긋난다 |
| 전제 | **M3a 기기 결과(A1–A4) 없이 만든다.** Link에서 앵커가 되는지 아직 모른다. 그래서 **기본은 꺼짐**이다. 켜도 앵커를 못 찾으면 지금 동작으로 돌아간다 |
| 베이스 / 작업 브랜치 | `main` / `codex/M3b-anchor-alignment` |
| 에디터 작업 | 없음 |

## 0. 지금 코드와 쓸 API

- 정렬 저장값(version 2)은 실물 점 4개의 **Stage 좌표**다(`FVBoRBuckSavedAlignment::Points`, `VBoRBuckAlignment.h:39-48`). 읽으면 점을 맞춰 Pawn을 정한다(`InitializeSetup`, `VBoRVehicleAlignmentComponent.cpp:116-149`).
- 저장하는 곳은 두 군데다. 마커 `SaveMarkerPoints`(`VBoRBuckProfiles.cpp:139-165`), 손끝 점 `SavePoints`(`VBoRPointAlignment.cpp:166-175`).
- 원점이 옮겨진 것은 모른다. 플레이 영역 크기와 원점 종류만 비교한다(`CheckEnvironment`, `:129`, Tick `:162-167`).
- 앵커 API는 M3a 명세 §0에 있다(`OculusXRAnchors.h:139,297,326`, `OculusXRAnchorTypes.h:356`). 실제 호출 순서와 Link 자세 처리는 **M3a 스파이크 코드**를 따른다(`Source/vBoR/M1Probe/M3aAnchorSpike.cpp`).
- Link 설정: Meta Horizon Link 앱 → Settings → Beta → "Spatial Data over Meta Horizon Link"(M3a 결과).

## 1. 동작

- **D1. 스위치.** 콘솔 변수 `vBoR.AnchorAlignment`, 기본 0. 0이면 지금과 완전히 같다(앵커를 만들지도 찾지도 않는다).
- **D2. 저장할 때(1일 때).** 정렬 파일 저장이 성공하면 앵커를 하나 만들어 기기에 저장한다.
  - 자리는 저장한 점들의 가운데(Stage), 방향은 Stage Yaw 0, 기울기 없음.
  - 앵커 저장이 성공하면 같은 정렬 파일에 `anchor` 항목을 더해 다시 저장한다. 담는 값은 UUID, 만든 순간의 앵커 Stage 위치(cm), Yaw(deg)다.
  - 실패하면 정렬 파일은 앵커 없이 그대로 두고, 결과 코드를 로그와 진행자 창 한 줄로 남긴다. 정렬 저장 자체는 성공이다.
- **D3. 읽을 때(1일 때, 파일에 `anchor`가 있을 때).** `BeginPlay`에서 그 UUID로 앵커를 찾는다.
  - 찾는 동안 상태는 `NeedsCheck`, 이유는 "Finding anchor". 시나리오 자동 시작이 기다리게 한다.
  - **찾으면:** 저장한 점마다 `점_지금 = 앵커_지금 × 앵커_저장⁻¹ × 점_저장`(Stage)으로 옮긴 뒤 지금처럼 맞춰 Pawn을 놓는다.
    - 원점이 옮겨진 것은 앵커가 바로잡으므로 `CheckEnvironment`의 플레이 영역 비교로는 막지 않는다. 다른 점이 있으면 알림만 남긴다. 원점 종류가 Stage가 아니면 지금처럼 막는다.
    - 상태는 `Applied`, 설명에 "anchor"를 붙인다. Tick의 플레이 영역 재검사(`:162-167`)도 이 경우 하지 않는다.
  - **10 s 안에 못 찾으면:** 지금 동작(Stage 값 + `CheckEnvironment`)으로 간다. 로그에 "anchor not found"와 결과 코드를 남긴다.
- **D4. 파일 형식.** `anchor` 항목은 선택이다. 없는 옛 파일도 그대로 읽힌다. version 번호는 바꾸지 않는다.
- **D5. 그대로 두는 것.** Recenter 처리, 진행자 `A`(다시 적용), 좌석 전환, Buck 전환, 눈 위치 정렬(version 1)은 바꾸지 않는다. `A`는 이미 찾은 앵커가 있으면 그 값을 쓰고, 다시 찾지는 않는다.

## 2. 하지 말 것

- 기본값을 1로 하기. 앵커 공유·클라우드. MRUK 장면. 프로젝트 설정 변경.
- 기존 시험 단언 수정. M3a 스파이크 코드 수정(참고만).
- 이 명세 밖의 정리·일반화.

## 3. 시험 `vBoR.M3b.*` (기기 없이)

앵커 찾기는 시험용 훅으로 결과를 넣는다(찾음 + 자세 / 못 찾음).

- E1: 스위치 0 → 앵커 요청 없음. 결과가 지금과 같다.
- E2: 파일 왕복 — `anchor` 있는 파일과 없는 옛 파일이 둘 다 읽힌다.
- E3: 찾음(앵커가 Stage에서 (30, -20, 0) cm, Yaw 15° 옮겨짐) → Pawn이 같은 만큼 옮겨진 자리. 플레이 영역 크기가 달라도 `Applied`.
- E4: 못 찾음(시간 초과) → 지금 동작과 같은 Pawn·상태. 찾는 동안은 `NeedsCheck`라 시작하지 않는다.

## 4. 완료 조건

E1–E4 통과, 두 빌드 성공, `vBoR.*` 전체 통과(기존 단언 변경 0개). 보고 ①–⑤.

## 5. 사용자가 Quest 3에서 (`not-tested`)

1. Link 앱에서 "Spatial Data over Meta Horizon Link"를 켠다. `~` 콘솔에서 `vBoR.AnchorAlignment 1`을 입력한다(계속 쓰려면 `Config/UserEngine.ini`의 `[ConsoleVariables]`에 넣는다).
2. 마커 정렬을 저장(M → S)한다. 로그에 앵커 저장 성공이 나오는지 본다.
3. 헤드셋을 벗어 화면이 꺼질 때까지 둔다. 다시 쓰고 `L_Main`을 Play한다. "anchor"가 붙은 `Applied`가 나오고 차가 실물 좌석에 맞는지 본다.
4. M3a A1–A4를 아직 안 했다면 같이 한다. 결과가 나쁘면 스위치를 0으로 둔다.
