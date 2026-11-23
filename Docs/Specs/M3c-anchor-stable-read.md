# M3c 앵커 위치가 멈춘 뒤에 읽기 — L_Main 앵커 복원이 어긋나는 문제

| 항목 | 값 |
|---|---|
| 결정 근거 | 사용자 2026-11-23: 앵커를 켜면 `L_Main`에서 차가 맞지 않는다. 사용자가 Claude에게 직접 고치고 고친 내용을 문서로 남기라고 했다 |
| 베이스 / 작업 브랜치 | `main@1a17da9` / `claude/M3c-anchor-stable-read` |
| 구현 | **Claude 직접 구현. 독립 리뷰 없음**(P5zd와 같은 예외). 명세가 아니라 작업 기록이다 |
| 시험 상태 | 합성 시험만 통과. Quest 3 `not-tested` |

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
- **바꾸지 않은 것:** 앵커 저장, 파일 형식, 진행자 A, 스위치 0일 때의 동작.

**현장에서 앵커가 틀렸을 때 되돌리는 법(기존 동작):** `~` 콘솔에 `vBoR.AnchorAlignment 0`을 넣고 진행자 A를 누른다. 저장 좌표만으로 다시 놓는다.

## 3. 바뀐 파일

- `Source/vBoR/Interaction/VBoRVehicleAlignmentComponent.h`: 설정 3개, 내부 상태 4개
- `Source/vBoR/Interaction/VBoRAnchorAlignment.cpp`: `BeginAnchorDiscovery`(창 초기화), `AdvanceAnchorDiscovery`(D1), `FinishAnchorDiscovery`(D2)
- `Source/vBoR/M1Probe/M3bAnchorAlignmentTests.cpp`: 아래 시험

## 4. 시험

- **바꾼 단언 1개:** `vBoR.M3b.R1WaitForAnchorActorPose`의 "Updated Actor applies the anchor". 처음 움직인 자세에서는 `Finding anchor`로 기다리고, 창이 지난 뒤 적용되게 했다. 기대하는 보정 결과(E3과 같은 Pawn)는 그대로다.
- **새 시험 `vBoR.M3c.StableAnchorPose`:**
  - 3 cm 튄 자세 뒤에는 창이 다시 시작하고, Pawn이 그대로다.
  - 0.2 cm 흔들림은 창을 유지하지만, 시간이 지나기 전에는 기다린다.
  - 멈춘 뒤에는 E3과 같은 보정을 한다.
  - 기록 줄이 `anchor found result=0`으로 시작하고, `pose changes 1`과 `Yaw +15.00 deg`를 담는다.
- 나머지 M3b 단언은 바꾸지 않았다.

## 5. Quest 3에서 확인할 것 (`not-tested`)

시험 PC에서 이 브랜치를 받아 빌드한다. `Config/UserEngine.ini`에 `vBoR.AnchorAlignment=1`을 넣는다.

1. 마커 레벨에서 **M → S를 한 번만** 한다. `anchor saved`가 나와야 한다.
2. 헤드셋을 쓴 채 `L_Main`을 Play한다.
   - 로그 `anchor found … after 1.xx s … Stage drift since save XY ≈ 0`이 나와야 한다.
   - 차가 실물 좌석에 맞아야 한다.
   - **판정:** drift가 0에 가까운데 차가 맞으면 해결이다. drift가 크게 나오면 읽은 값이 여전히 틀린 것이니 그 줄을 그대로 Claude에게 준다.
3. 헤드셋을 벗어 화면이 꺼질 때까지 둔 뒤, 다시 쓰고 `L_Main`을 Play한다. drift가 0이 아니어도 차가 맞아야 한다.
4. 마커 레벨을 Play해 **M만** 눌러 `Marker fit vs pawn` dX·dY를 본다(2 cm 안이면 좋음).
5. (가능하면) 재부팅 뒤와 다음 날에 3·4를 한다.

로그는 다음 명령으로 뽑는다.

```powershell
Select-String -Path <프로젝트>\Saved\Logs\vBoR.log -Pattern "Alignment anchor","LogP3Flow: Alignment","Marker fit vs pawn"
```
