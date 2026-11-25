# M3c 앵커 위치가 멈춘 뒤에 읽기 — L_Main 앵커 복원이 어긋나는 문제

| 항목 | 값 |
|---|---|
| 결정 근거 | 사용자 2026-11-23: 앵커를 켜면 `L_Main`에서 차가 맞지 않는다. 사용자가 Claude에게 직접 고치고 고친 내용을 문서로 남기라고 했다 |
| 베이스 / 작업 브랜치 | `main@1a17da9` / `claude/M3c-anchor-stable-read` |
| 구현 | **Claude 직접 구현. 독립 리뷰 없음**(P5zd와 같은 예외). 명세가 아니라 작업 기록이다 |
| 시험 상태 | 합성 시험 통과. Quest 3(집 PC, Link): 저장 → Link 재연결 → 복원 2 cm 안 1회 통과(§5 결과). 재부팅·다음 날은 `not-tested` |

## 1. 기기에서 본 것 (2026-11-23, 시험 PC `vBoR.log`)

| 시각 | 레벨 | 로그 | 해석 |
|---|---|---|---|
| 00:53:06 → 00:53:25 | 마커 | `anchor found` → `Marker fit vs pawn (Applied)` dX +0.42, dY +1.92 cm | 마커 레벨에서는 앵커로 복원해도 맞았다 |
| 00:55:44 | 마커 | 저장했지만 `anchor saved` 없음 | 스위치 0으로 저장해서 파일에서 앵커가 빠졌다 |
| 02:05:10 | 마커 | 앵커 없이 `Applied`, dX −29.17, dY +7.73 cm, dYaw +170.7° | 약 1시간 뒤 Stage 원점이 크게 옮겨졌다. **앵커가 필요하다는 증거** |
| 02:05:20 | 마커 | `anchor saved UUID=A89C…` | 새로 저장 |
| 02:06:32.027 → .055 | L_Main | `Finding anchor` → 0.03초 뒤 `Applied anchor` | 저장한 지 1분, 헤드셋이 쉬지 않았는데도 차가 어긋남 |
| 02:06:59 – 02:07:44 | L_Main | 진행자 A 6번, 매번 `Applied anchor` | A는 처음 읽은 값을 다시 쓴다(`VBoRVehicleAlignmentComponent.cpp:87`). 그래서 안 고쳐짐 |

**판단:** 저장한 지 1분이고 헤드셋이 쉬지 않았으면 앵커는 저장 때 위치와 같아야 한다. 그런데도 어긋났으니 `L_Main`이 읽은 앵커 위치가 틀린 것이다.

M3b는 "앵커 Actor가 처음 자리에서 한 번 움직이면 바로 읽는다"(`VBoRAnchorAlignment.cpp:160`)였다. `L_Main`은 시작 첫 프레임에 읽었다. Link가 첫 프레임에 아직 자리 잡지 않은 자세를 준다고 **추정**한다.

마커 레벨은 카메라를 켜느라 시작이 무거워서, 늦게 읽어 맞았을 수 있다(추정).

## 2. 바꾼 것

- **D1. 멈춘 뒤에 읽기.** 앵커 자세(Pawn 기준 Stage)가 처음 본 자세에서 `AnchorStableCm`(0.5 cm)·`AnchorStableDeg`(0.5°) 안에 `AnchorStableSeconds`(1 s) 동안 머물러야 쓴다.
  - 벗어나면 그 자세로 창을 다시 시작한다.
  - 10초 제한과, 시간이 지나면 저장 좌표로 돌아가는 동작은 그대로다.
  - 세 값은 `BP_Car_Core`의 Alignment 컴포넌트 "Anchor alignment" 칸에서 바꿀 수 있다.
- **D2. 옮겨진 양 기록.** 찾았을 때 로그와 진행자 기록에 다음을 한 줄로 남긴다.
  ```
  anchor found result=0 after <초> s, pose changes <횟수> | Stage drift since save XY <cm> Z <cm> Yaw <deg>
  ```
  - `Stage drift`는 저장 뒤 Stage 원점이 방에 대해 옮겨진 양이다. 헤드셋이 쉬지 않았으면 0에 가까워야 한다.
  - `pose changes`는 기다리는 동안 자세가 튄 횟수다.
  - 10초 안에 못 쓰면 `anchor not found result=…, pose changes <횟수>; using saved Stage points`가 나온다. 횟수가 0보다 크면 앵커는 보였지만 멈추지 않은 것이다.
- **D3. 앵커 자세를 Stage(tracking) 좌표로 직접 읽기 (2회차, 집 시험 뒤 추가).**
  - 불러올 때: 앵커 Actor의 World 자세를 Pawn 기준으로 바꾸던 것을 그만두고, `TryGetAnchorTransformByHandle(..., EOculusXRAnchorSpace::Tracking)`으로 Stage 자세를 바로 읽는다.
  - 저장할 때: 요청한 자세(점 중심) 대신, 만들어진 앵커의 Stage 자세를 읽어 `AnchorStage`로 적는다. 읽지 못하면 앵커 없이 저장한다(`anchor pose unavailable after save`).
  - 저장 로그에 `created vs requested XY … cm Z … cm Yaw … deg`를 단다. Link 생성 경로가 앵커를 얼마나 옮겨 만드는지 보는 값이다.
  - 근거:
    - 에디터 빌드의 Link 전용 경로는 앵커 Actor 자세를 카메라 관리자 자세와 현재 머리 자세를 섞어 만든다(`Plugins/MetaXR/Source/OculusXRAnchors/Private/OculusXRAnchorComponent.cpp:150-164`, `:188-202`).
    - 기본 World 값에는 이미 Pawn이 들어 있다(OpenXR `Private/openxr/OculusXRAnchorsXR.cpp:548-556`).
    - 그래서 읽은 값이 Pawn 자리와 머리 움직임에 따라 달라졌다고 본다(아래 표).
    - Tracking 값은 `xrLocateSpace`를 tracking(Stage) 공간 기준으로 부른 결과 그대로다(`:529`, `:558-562`).
    - 앵커를 만드는 쪽도 같은 방식으로 섞는다(`Private/OculusXRAnchorFunctionsOVR.cpp:98-118`). 그래서 요청한 자세를 그대로 믿지 않는다.
  - 시험(자동)에는 XR 세션이 없다. 그래서 `bAnchorLocationValidForTest`일 때만 예전처럼 Actor 자세로 대신한다. 기기 경로는 Quest에서만 확인된다.
- **바꾸지 않은 것:** 파일 형식, 진행자 A, 스위치 0일 때의 동작.

**집 시험 (2026-11-23 밤, D1·D2만 있을 때):**

| 시각 | 레벨 | 결과 |
|---|---|---|
| 23:06 | L_Main | 고개를 움직임 → `not found … pose changes 91` |
| 23:09 | L_Main | 고개를 멈춤 → `found after 4.43 s, pose changes 2`, drift XY 67.9 cm |
| 23:12 | 마커 | **같은 앵커** → drift XY 200 cm, Z +110 cm. `Marker fit vs pawn` dX −187 cm |
| 23:13 → 23:13:13 | 마커 저장 → L_Main | 저장 8초 뒤, 흔들림 0인데 drift 93 cm, 28° |

같은 앵커를 레벨마다 다르게 읽었다. 그래서 D1(멈춘 뒤 읽기)만으로는 부족하고, 읽는 좌표가 틀렸다고 판단했다(D3).

**현장에서 앵커가 틀렸을 때 되돌리는 법(기존 동작):** `~` 콘솔에 `vBoR.AnchorAlignment 0`을 넣고 진행자 A를 누른다. 저장 좌표만으로 다시 놓는다.

## 3. 바뀐 파일

- `Source/vBoR/Interaction/VBoRVehicleAlignmentComponent.h`: 설정 3개, 내부 상태 4개
- `Source/vBoR/Interaction/VBoRAnchorAlignment.cpp`: `BeginAnchorDiscovery`(창 초기화), `AdvanceAnchorDiscovery`(D1·D3), `FinishAnchorDiscovery`(D2), `SaveAlignmentAnchor` 저장 콜백(D3), `ReadTrackingStage`(D3)
- `Source/vBoR/M1Probe/M3bAnchorAlignmentTests.cpp`: 아래 시험

## 4. 시험

- **바꾼 단언 1개:** `vBoR.M3b.R1WaitForAnchorActorPose`의 "Updated Actor applies the anchor". 처음 움직인 자세에서는 `Finding anchor`로 기다리고, 창이 지난 뒤 적용되게 했다. 기대하는 보정 결과(E3과 같은 Pawn)는 그대로다.
- **새 시험 `vBoR.M3c.StableAnchorPose`:**
  - 3 cm 튄 자세 뒤에는 창이 다시 시작하고, Pawn이 그대로다.
  - 0.2 cm 흔들림은 창을 유지하지만, 시간이 지나기 전에는 기다린다.
  - 멈춘 뒤에는 E3과 같은 보정을 한다.
  - 기록 줄이 `anchor found result=0`으로 시작하고, `pose changes 1`과 `Yaw +15.00 deg`를 담는다.
- 나머지 M3b 단언은 바꾸지 않았다. D3은 시험을 바꾸지 않았다(기기 API라 자동 시험으로는 확인되지 않는다).
- **주의:** `Config/UserEngine.ini`에 `vBoR.AnchorAlignment=1`이 있으면 자동 시험에도 적용된다. 그러면 `vBoR.M2.E9MarkerSave`가 실패한다(저장 알림 문구가 앵커 쪽으로 바뀜). 전체 시험은 이 파일을 잠시 치우고 돌린다.

## 5. Quest 3에서 확인할 것 (`not-tested`)

시험 PC에서 이 브랜치를 받아 빌드한다. `Config/UserEngine.ini`에 `vBoR.AnchorAlignment=1`을 넣는다.

1. 마커 레벨에서 **M → S를 한 번만** 한다. `anchor saved … created vs requested …`가 나와야 한다. 이 값은 기록용이다.
2. 헤드셋을 쓴 채 `L_Main`을 Play한다.
   - 로그 `anchor found … after 1.xx s … Stage drift since save XY ≈ 0`이 나와야 한다.
   - 차가 실물 좌석에 맞아야 한다.
   - **판정:** drift가 0에 가까운데 차가 맞으면 해결이다. drift가 크게 나오면 읽은 값이 여전히 틀린 것이니 그 줄을 그대로 Claude에게 준다.
3. 헤드셋을 벗어 화면이 꺼질 때까지 둔 뒤, 다시 쓰고 `L_Main`을 Play한다. drift가 0이 아니어도 차가 맞아야 한다.
4. 마커 레벨을 Play해 **M만** 눌러 `Marker fit vs pawn` dX·dY를 본다(2 cm 안이면 좋음).
5. (가능하면) 재부팅 뒤와 다음 날에 3·4를 한다.

### 결과 (2026-11-23 밤, 집 PC + Link, D3 포함)

| 순서 | 결과 |
|---|---|
| 마커 레벨, 이전 앵커 복원 → M | drift 10.1 cm·3.2°를 보정, `Marker fit vs pawn` +0.04 / −0.16 cm |
| S (새 저장) | `created vs requested` 62.6 cm·30.5° |
| **Link 끊었다 다시 연결** → 마커 레벨 | `anchor found after 8.38 s, pose changes 24`, drift 44.5 cm·−17.3° |
| M | `Marker fit vs pawn` **+0.04 / +0.24 cm, +0.21°** |

- 판정: 1·2·4번 통과(1회). 3번(헤드셋 잠금)은 Link 재연결로 대신했다. 5번(재부팅·다음 날)은 아직 하지 않았다.
- 남은 것: 찾는 데 4–9초 걸린다. 10초 안에 못 찾은 적이 두 번 있다(pose changes 18·61).

로그는 다음 명령으로 뽑는다.

```powershell
Select-String -Path <프로젝트>\Saved\Logs\vBoR.log -Pattern "Alignment anchor","LogP3Flow: Alignment","Marker fit vs pawn"
```
