# M3a 앵커 스파이크 — 헤드셋을 껐다 켜도 같은 자리를 다시 찾는가

| 항목 | 값 |
|---|---|
| 성격 | **스파이크(버려도 되는 코드).** 사용자 2026-11-16: 헤드셋이 잠들었다 깨면 Stage 원점이 옮겨져 차가 어긋나고, 매번 다시 정렬해야 한다. 공간 앵커로 이걸 없앨 수 있는지 먼저 확인한다. 결과가 좋으면 정식 명세(정렬 저장을 앵커 기준으로)를 쓴다 |
| 베이스 / 작업 브랜치 | `main` / `codex/M3a-anchor-spike` |
| 리뷰 | 가볍게(범위·금지 사항·빌드만) |
| 에디터 작업 | 있음 — 빈 레벨 하나(MCP) |

## 0. 배경

- 정렬 저장값은 Stage(가디언 원점) 기준 좌표다(`VBoRBuckAlignment.cpp` `stagePositionCm`). 원점이 옮겨지면 파일은 그대로인데 차가 어긋난다.
- 지금 코드는 플레이 영역 **크기**만 비교한다(`VBoRVehicleAlignmentComponent.cpp:162-167`). 원점이 옮겨진 것은 모른다.
- 공간 앵커는 Quest가 방 모습으로 다시 찾는 점이다. 앵커 기준으로 저장하면 원점이 옮겨져도 같은 물리 위치로 돌아온다 — **PC(Link) 실행에서 되는지, 오차가 얼마인지가 이 스파이크의 질문이다.**

### 확인한 API (MetaXR 1.205.0)

- 모듈 `OculusXRAnchors` — 이미 `vBoR.Build.cs:15`에 의존성이 있다.
- 만들기: `FOculusXRAnchors::CreateSpatialAnchor(WorldTransform, TargetActor, Callback, OutResult)` (`Plugins/MetaXR/Source/OculusXRAnchors/Public/OculusXRAnchors.h:139`). 콜백은 `UOculusXRAnchorComponent*`를 준다(`:15`).
- 저장: `SaveAnchors(TArray<UOculusXRAnchorComponent*>, …)` (`:297`). UUID는 `UOculusXRAnchorComponent::GetUUID()` (`Public/OculusXRAnchorComponent.h:30`).
- 다시 찾기: `DiscoverAnchors(FOculusXRSpaceDiscoveryInfo, ResultsCallback, CompleteCallback, …)` (`OculusXRAnchors.h:326`), 필터 `UOculusXRSpaceDiscoveryIdsFilter::Uuids` (`Public/OculusXRAnchorTypes.h:356`).
- 앵커 컴포넌트는 매 Tick 주인 Actor를 앵커의 **월드** 자세로 옮긴다(`Private/OculusXRAnchorComponent.cpp:136-176`). 월드 자세에는 Pawn 변환이 들어 있다 → Stage로는 Pawn 역변환으로 옮긴다(M1a 개정 2와 같음).
- **Link 전용 분기가 있다:** 에디터 빌드에서는 앵커 자세를 머리 기준으로 바꿔 계산한다(`OculusXRAnchorComponent.cpp:152-164`, `Private/OculusXRAnchorFunctionsOVR.cpp:98-118`, `#if WITH_EDITOR` "Link only head space"). 즉 Link에서도 동작을 의도했지만 경로가 다르다 — 오차가 여기서 생길 수 있다.
- Quest 기기 설정 "Enhanced Spatial Services"는 **공유** 앵커 문서의 전제 조건이다(Meta 문서 `unreal-spatial-anchors-sharing`). 로컬 저장에도 필요한지는 모른다 — 실패하면 결과 코드를 로그에 남기고 사용자가 켜 본다.

## 1. 할 것

1. `Source/vBoR/M1Probe/`에 Actor `AM3aAnchorSpike` 하나. 키(키보드, PC 창):
   - **`N` 새 앵커:** 왼손 검지 끝 1 s 평균(원본, `AVBoRHandRigPawn::ReadRawIndexTip` — M1a `T`와 같은 방식) 위치에, **Yaw만 Pawn과 같고 기울기 없는** 자세로 앵커를 만들고 저장한다. 성공하면 `Saved/Calibration/AnchorSpike.json`에 UUID와 **만든 순간의 앵커 Stage 자세**(위치 cm, Yaw deg)를 쓴다. 이전 앵커가 있으면 바꾼다(지우기는 선택).
   - **BeginPlay:** 파일이 있으면 그 UUID로 `DiscoverAnchors`. 찾으면 로그 `found`와 걸린 시간(s). 못 찾으면 결과 코드. 10 s 안에 못 찾으면 `not found`.
   - **`T` 확인점:** 왼손 검지 끝 1 s 평균을 Stage로 구해 로그. 앵커가 있으면 **앵커 기준 좌표**(앵커 자세 역변환, cm)도 같은 줄에 쓴다.
   - **1초 요약 로그:** 앵커 찾음 여부, 앵커 Stage 위치·Yaw, 저장할 때와의 차이(XY cm, Z cm, Yaw deg), Pawn 위치·Yaw.
2. 로그 카테고리 `LogM3a`. 줄마다 따로(사용자가 그대로 Claude에 넘긴다).
3. VR 표시: 앵커 자리에 작은 축(또는 구) 하나. 확인용.
4. 레벨 `Content/Development/M1/L_M3a_AnchorSpike`: 빈 레벨 + 바닥 평면 + `AM3aAnchorSpike`, GameMode Override `/Game/Main/Core/BP_vBoRGameMode`. 차·Runner 없음.
5. 자동 시험은 **하나만**: `vBoR.M3a.AnchorRelative` — 앵커 Stage 자세 두 가지(하나는 Yaw 30°)에 대해, Stage 점 → 앵커 기준 → Stage 왕복이 0.1 mm 안으로 돌아오는지. 기기 없이.

## 2. 하지 말 것

- `Source/vBoR/Interaction/`·`L_Main`·`Content/Main`·`BuckEye.json`·`Bucks/` 수정, 정렬 적용(Pawn 이동).
- 앵커 공유·클라우드 저장, MRUK 장면(Scene) 사용, 패스스루·카메라.
- 프로젝트 설정 변경. 앵커에 필요한 설정이 따로 있으면 **바꾸지 말고** 결과에 적고 멈춘다.
- 다듬기·일반화. 막히면 원인 로그를 남기고 `HANDOFF.md` "Codex 결과"에 적고 멈춘다. **"Link에서 안 된다"도 정상 결과다.**

## 3. 끝낼 때

- 빌드 non-unity(`-Module=vBoR`)·unity 각 한 번, 위 시험 1개, `vBoR.*` 전체 한 번(기존 시험 그대로 통과).
- 커밋 본문·`HANDOFF.md` "Codex 결과"에 AGENTS.md 보고 ①–⑤.

## 4. 사용자가 Quest 3에서 (케이블 Link, VR Preview)

확인점은 **실물에서 위치가 분명한 한 점**으로 정해 매번 같은 곳을 짚는다(예: Buck 스티어링 휠 중앙, 책상 모서리).

| ID | 할 일 | 볼 것 |
|---|---|---|
| A1 | Play → 확인점 근처에서 `N` → 확인점에 검지 → `T` 3회 | 앵커 만들기·저장 성공 로그. 실패면 결과 코드 |
| A2 | Play 끝 → **헤드셋을 쓴 채** 다시 Play → 찾을 때까지 기다림 → `T` 3회 | `found`·걸린 시간. Stage 차이 ≈ 0이어야 정상(기준 측정) |
| A3 | Play 끝 → 헤드셋을 벗고 **화면이 꺼질 때까지** 둠 → 다시 쓰고 Play → `T` 3회. 3번 반복 | Stage 차이(원점이 옮겨졌는가)와 **앵커 기준 `T`가 A1과 같은가** |
| A4 | 헤드셋 재부팅 뒤 같은 것 1회. 가능하면 다음 날 1회 | 같음 |

`Saved/Logs/vBoR.log`의 `LogM3a` 줄을 Claude에 넘긴다.

**판정(Claude):** A3·A4에서 Stage 차이는 있는데 앵커 기준 `T`가 A1과 손 추적 잡음(약 1–2 cm) 안에서 같으면 → 앵커 기준 저장으로 정식 명세. 앵커를 못 찾거나 앵커도 같이 밀리면 → 대안(진행자 키 하나로 하는 빠른 재정렬, 또는 확인점 경고). 시험 상태는 `not-tested`(스파이크).
