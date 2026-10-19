# P4f 본 Pawn·GameMode — `AVBoRPawn`(손 메시·재질 교체)·`AVBoRGameMode`, 첫 산출물 Blueprint 둘

| 항목 | 값 |
|---|---|
| 단계 | P4 50% 통합 — 산출물 구조의 첫 단계. 이어서 L_Main(P4g), L_Main에서 정렬 저장(P4h)이 온다 |
| 결정 근거 | **사용자 요청(2026-10-19):** P0–P3 구조를 L_Main으로 묶고, GameMode는 `vBoRGameMode`처럼 이름을 붙인다. `vBoRPawn`을 만들고, 다른 프로젝트(AI_Driving)의 Pawn처럼 **손 메시를 바꿀 수 있어야 한다.** 손 메시는 사용자가 준비할 수 있다 |
| 베이스 브랜치 | `main` @ `9ff47b1`(PR #38 정렬 높이) + 이 명세 커밋 |
| 작업 브랜치 | `codex/P4f-main-pawn-gamemode` |
| 리뷰 파일 | `Docs/Reviews/P4f-main-pawn-gamemode.md` (첫 리뷰 때 생김) |
| 상태 | 명세 — 구현 전 |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **`AVBoRPawn` — 지금 P3 Pawn을 본 클래스로 올림** | 새 클래스 `AVBoRPawn : AVBoRHandRigPawn`(`Source/vBoR/Interaction/VBoRPawn.h/.cpp`).<br>- 동작은 지금 `AP3PalmGrabPawn`(`P3Probe/P3FlowGameMode.h:9-23`)과 같다. 생성자에서 `bAllowPalmGrab = true`, `PreInitializeComponents`에서 두 손 Grabber에 Pinch를 허용한다.<br>- `AP3PalmGrabPawn`은 이름을 그대로 두고 **부모만 `AVBoRPawn`으로 바꾼다**(본문 없음). `L_P3_Flow`·기존 시험·`AP3FlowGameMode`가 이 이름을 쓴다.<br>- `AVBoRHandRigPawn`의 기본값은 바꾸지 않는다(`bAllowPalmGrab` 거짓 — P0·P1·P2 Pawn이 쓴다) |
| D2 | **손 메시·재질 교체 — 비워 두면 기본** | `AVBoRPawn`에 프로퍼티 넷을 둔다(`EditAnywhere`, 카테고리 `손 표시`). AI_Driving(`AAI_DrivingPawn`의 `LeftGripHandMesh` 등)처럼 **비워 두면 지금과 같고, 넣으면 바뀐다.**<br>- `LeftHandMesh`·`RightHandMesh`(`USkeletalMesh`): 비면 ISDK 기본 OpenXR 손이다(`IsdkContentAssetPaths.h:155-157`).<br>- `LeftHandMaterial`·`RightHandMaterial`(`UMaterialInterface`): 비면 메시 자신의 재질이다. 넣으면 그 손 메시의 **모든 재질 칸**에 넣는다.<br>- **두 표시 모두에 넣는다:** 각 손 리그의 `GetHandVisuals()`(`IsdkHandRigComponent.h:90`)의 `TrackedHandVisual`과 `SyntheticHandVisual`(`IsdkHandVisualsRigComponent.h:108-114`)이다. 보이는 것은 합성 손(poke limiter가 멈춘 손)이고, 추적 손은 기본으로 숨어 있다(`IsdkHandVisualsRigComponent.cpp:47`). 둘이 다르면 손이 바뀌는 순간 모양이 튄다.<br>- 넣는 때: 리그 초기화 전(`AVBoRHandRigPawn::PreInitializeComponents`, `Interaction/VBoRHandRigPawn.cpp:96-113`). ISDK는 메시가 바뀌면 매핑을 지우고 다시 시도한다(`IsdkHandMeshComponent.cpp:370-383`) |
| D3 | **본 이름이 안 맞으면 기본 손으로 돌아감** | ISDK 손은 본 이름으로 관절을 매핑하는 Poseable 메시다. 기본 이름은 `XRHand_Palm`·`XRHand_Wrist`… OpenXR 손 관절 이름이다(`IsdkHandMeshComponent.cpp:306-`). **그래서 아무 메시나 되지 않는다** — 같은 본 이름을 가진 메시만 그대로 움직인다.<br>- 매핑이 실패하면(`EIsdkSkeletonMappingState::Invalid`, `IsdkHandMeshComponent.h:47-55`, 조회 `:147-150`) 그 손의 두 표시를 ISDK 기본 메시·기본 재질로 되돌린다. Warning을 한 번 남긴다. 예: `BP_vBoRPawn_C_0: Left hand mesh SK_Glove bones do not match XRHand_* — using default hand`.<br>- 본 이름을 따로 맞추는 기능(`MappedBoneNames` 덮어쓰기)은 **이 명세에서 만들지 않는다.** 필요해지면 따로 정한다 |
| D4 | **`AVBoRGameMode`** | 새 클래스 `AVBoRGameMode : AGameModeBase`(`Source/vBoR/Interaction/VBoRGameMode.h`, 필요하면 `.cpp`). `DefaultPawnClass = AVBoRPawn`. 다른 것은 넣지 않는다.<br>- `AP3FlowGameMode`(`P3Probe/P3FlowGameMode.h:25-31`)는 그대로다 |
| D5 | **첫 산출물 Blueprint 둘 — 에디터 MCP로 만듦** | Codex가 `unreal-vbor` MCP로 만든다(AGENTS.md "에디터 MCP" — 이 명세가 맡김).<br>- `/Game/Main/Core/BP_vBoRPawn`: 부모 `AVBoRPawn`. 손 프로퍼티는 비워 둔다.<br>- `/Game/Main/Core/BP_vBoRGameMode`: 부모 `AVBoRGameMode`. `DefaultPawnClass` = `BP_vBoRPawn`.<br>- **폴더:** 산출물은 `Content/Main/` 아래에 모은다(Claude 제안 — 개발 시험대 `P0`–`P3` 폴더와 나눔). 이 명세는 `Main/Core/`만 만든다.<br>- 레벨(`L_P3_Flow` 등)·프로젝트 설정(기본 GameMode·맵)은 **바꾸지 않는다.** L_Main은 다음 명세(P4g)다.<br>- 끝나면 저장하고 에디터를 닫는다(Agent-Workflow §2.10) |

## 1. 왜 만드는가

- 지금 Pawn·GameMode는 개발 단계 이름(`AP3PalmGrabPawn`, `AP3FlowGameMode`)이다. 산출물 레벨(L_Main)이 쓸 본 클래스와 그 Blueprint가 필요하다.
- 평가에서 손 모양(장갑, 피부색, 다른 손 에셋)을 바꿔야 할 수 있다. 코드를 고치지 않고 Blueprint 값만으로 바꾸게 한다.

## 2. 범위

### 만들 것

1. `AVBoRPawn`(D1·D2·D3)과 `AP3PalmGrabPawn`의 부모 바꾸기
2. `AVBoRGameMode`(D4)
3. Blueprint 둘(D5)
4. 자동화 시험 §4

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| L_Main, 차량 Blueprint, 부품 Blueprint | 다음 명세 P4g |
| 진행자 창에서 정렬 저장 | P4h |
| 본 이름 매핑 덮어쓰기, AI_Driving식 "휠 잡을 때 붙는 손" | D3, 사용자가 아직 요청하지 않음 |
| `AVBoRHandRigPawn`·P0–P2 Pawn·`AP3FlowGameMode` 동작 바꾸기 | D1·D4 |
| 프로젝트 기본 GameMode·맵, 레벨 수정 | D5 |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- **`AP3PalmGrabPawn`의 동작은 그대로다.** 손바닥과 Pinch를 허용하고, 거리·레이 잡기는 막는다. 이 이름을 쓰는 레벨·GameMode·시험은 그대로 돈다.
- **`AVBoRHandRigPawn` 기본값은 그대로다.** `bAllowPalmGrab`는 거짓, poke 전용이다.
- 손 프로퍼티가 비어 있으면 손 메시·재질·보임은 지금과 같다. 추적 손은 숨고 합성 손이 보인다.
- poke limiter와 손끝 원본 읽기(`ReadRawIndexTip`·`ReadRawPalmAndTips`)는 그대로다. 도달 판정은 추적된 실제 손끝으로 한다(AGENTS.md).
- **기존 단언은 바꾸지 않는다. 예외 없음.** Claude가 Pawn을 쓰는 시험을 모두 대조했다. 아래는 그대로 통과해야 한다.
  - `vBoR.P3Flow`(`P3Probe/P3FlowTests.cpp:445-452`): `AP3FlowGameMode`의 Pawn 클래스가 `AP3PalmGrabPawn`이다. 그 Pawn의 손바닥 리그가 켜지고 레이·거리가 꺼진다. `AVBoRHandRigPawn` 기본은 손바닥 꺼짐이다. → 이름과 동작이 그대로라 같다.
  - `vBoR.P4a.Key.E1PinchAndP3Rig`(`P3Probe/P4aKeyTableTests.cpp:125`), P5e 잡기(`P3Probe/P5eRearviewMirrorTests.cpp:173`): `AP3PalmGrabPawn`을 띄운다. → 같다.
  - P0·P1 시험의 `AVBoRHandRigPawn`(`P0ButtonBenchTests.cpp:198`, `P1BoxLoadTests.cpp:24`, `P1CarryContactTests.cpp:99`·`:369`, `P1CarryWorldTests.cpp:174`·`:280`·`:356`, `P1cCarrySettleTests.cpp:361`): 부모 클래스를 바꾸지 않으므로 같다.
  - 이 목록 밖에서 실패하는 단언이 있으면 고치지 말고 리뷰 파일 "질문"에 적고 멈춘다.

## 4. 경계 상황과 그 시험

| # | 상황 | 기대 동작 | 자동화 시험 |
|---|---|---|---|
| E1 | `AVBoRPawn` 기본 | 두 손 Grabber에서 손바닥·Pinch 허용, 거리·레이 막힘, 손바닥 리그 켜짐. `AP3PalmGrabPawn`은 `AVBoRPawn`의 자식이고 같은 값. `AVBoRHandRigPawn`은 손바닥 꺼짐 | Pawn 설정 |
| E2 | 손 프로퍼티 비움 | 각 손의 두 표시가 ISDK 기본 OpenXR 손(왼손은 Left, 오른손은 Right)이고, 재질은 메시 기본. 추적 손 숨김·합성 손 보임 | 메시·재질·보임 |
| E3 | 메시만 넣음(본 이름 맞음) | 그 손의 두 표시가 넣은 메시. 다른 손은 기본. 매핑이 `Invalid`가 아니다 | 메시·매핑 상태(예: 다른 손의 OpenXR 메시처럼 `XRHand_*` 본을 가진 메시) |
| E4 | 메시 넣음(본 이름 안 맞음) | 매핑 실패 → 그 손의 두 표시가 기본 메시·기본 재질로 돌아감. Warning 한 번(다시 시도해도 반복하지 않음). 다른 손은 영향 없음 | 대체(예: 엔진 `SkeletalCube`) |
| E5 | 재질만 넣음 / 메시와 재질 둘 다 | 그 손 메시의 모든 재질 칸이 넣은 재질(두 표시 모두). E4로 돌아가면 재질도 기본 | 재질 칸 |
| E6 | `AVBoRGameMode` | `DefaultPawnClass == AVBoRPawn`. `AP3FlowGameMode`는 그대로 `AP3PalmGrabPawn` | 기본값 |
| E7 | Blueprint 둘 | `/Game/Main/Core/BP_vBoRPawn`의 부모가 `AVBoRPawn`, 손 프로퍼티 비어 있음. `BP_vBoRGameMode`의 부모가 `AVBoRGameMode`, Pawn 클래스가 `BP_vBoRPawn` | 에셋 로드 |
| E8 | 기존 시험 | §3 목록 모두 그대로 통과. `vBoR.*` 전체 통과 | 전체 시험 |

## 5. 구현자가 정할 것

- 매핑 실패를 확인하는 시점과 방법(ISDK가 매핑을 시도한 뒤 — 첫 Tick, 타이머, 컴포넌트 이벤트 등). 합성 시험 월드에서 관절 데이터 소스가 없어 매핑이 시도되지 않으면, 대체 경로를 직접 부르는 시험 접근자를 둬도 된다(E3·E4).
- `AVBoRGameMode`에 `.cpp`가 필요한지.
- MCP로 Blueprint를 만들고 값을 넣는 도구와 순서.

## 6. 완료 조건

1. §2의 1–4가 들어 있다.
2. §4 시험이 자동화로 통과한다. 기존 `vBoR.*`도 통과한다(§3 — 예외 없음).
3. non-unity(`-DisableUnity -Module=vBoR`)·unity 빌드에서 vBoR 경고·오류 0. 전체 시험은 최종 트리로 한 번 돌린다.
4. AGENTS.md 보고 ①–⑤를 커밋 본문과 `HANDOFF.md`에 남긴다. MCP 작업은 Agent-Workflow §2.10대로 보고한다(만든 에셋 경로, 저장, 에디터 닫음).
5. 시험 상태는 `not-tested`다.

## 7. 로컬에서 확인할 것 — 사용자

1. **에디터:** `BP_vBoRPawn`을 열어 Details의 `손 표시` 네 칸을 본다. 비워 둔 채 `L_P3_Flow`의 World Settings에서 GameMode를 `BP_vBoRGameMode`로 잠깐 바꿔 VR Preview를 한다. **저장하지 않는다.** 손이 지금과 같고, 키 pinch·문 손바닥 잡기·버튼 누르기가 같은지 본다.
2. 준비한 손 메시를 `LeftHandMesh`·`RightHandMesh`에 넣고 본다. 손가락이 따라 움직이면 본 이름이 맞는 것이다. 기본 손으로 돌아가고 Output Log에 Warning이 뜨면 본 이름이 다른 것이다(D3).
3. 바꾼 손으로 버튼을 눌러 손이 버튼 면에 멈춰 보이는지(poke limiter) 본다.

## 8. 확인한 API 근거

경로: `Source/vBoR/` 아래(`main` @ `9ff47b1`). ISDK는 `Plugins/MetaXRInteraction/Source/` 아래.

| 쓸 것 | 위치 | 쓰임 |
|---|---|---|
| 지금 Pawn·GameMode | `P3Probe/P3FlowGameMode.h:9-31`, `Interaction/VBoRHandRigPawn.h:33-58`, `.cpp:96-113` | D1·D4 |
| 손 리그 → 표시 | `OculusInteractionPrebuilts/Public/Rig/IsdkHandRigComponent.h:90-98`(`GetHandVisuals`), `Rig/IsdkHandVisualsRigComponent.h:104-114`(`TrackedHandVisual`·`SyntheticHandVisual`) | D2 |
| 기본 손 메시·숨김·본 이름 기본값 | `OculusInteractionPrebuilts/Private/Rig/IsdkHandVisualsRigComponent.cpp:38-47`, `:111-124`, `OculusInteraction/Public/IsdkContentAssetPaths.h:155-157` | D2·E2 |
| 매핑 | `OculusInteraction/Public/IsdkHandMeshComponent.h:47-55`(상태), `:107-112`(`MappedBoneNames`), `:147-150`(조회), `Private/IsdkHandMeshComponent.cpp:306-`(기본 이름 `XRHand_*`), `:370-383`(메시가 바뀌면 다시 매핑) | D3·E3·E4 |
| 참고 — 다른 프로젝트 | `C:/Git/AI_Driving/Source/AI_Driving/AI_DrivingPawn.h:140-175`(비우면 없음, 넣으면 교체하는 손 프로퍼티) | D2 방식만 참고. 그 프로젝트의 grip hand는 휠에 붙는 별도 손이라 여기와 다르다 |

## 개정 이력

| 날짜 | 개정 | 내용 |
|---|---|---|
| 2026-10-19 | 처음 | 작성(사용자 요청: L_Main 통합 준비, `vBoRGameMode`, `vBoRPawn` 손 메시 교체). 기존 Pawn 시험을 모두 대조함 — 바뀌는 단언 없음 |
