# P0-b poke 버튼 시험대

| 항목 | 값 |
|---|---|
| 단계 | P0 환경·기준 확인 (계획서 §6) |
| 대상 | 조사 기록 2026-09 §10의 1·3·5번 실측(허공 버튼 맞추기, 몰린 버튼에서 하나만 활성되는지, "닿았다" 피드백), U-06 poke limiter 필수, U-03·U-04의 레이·원거리 금지 |
| 베이스 브랜치 | `main` |
| 작업 브랜치 | `codex/P0-b-poke-button-bench` |
| 리뷰 파일 | [Docs/Reviews/P0-b-poke-button-bench.md](../Reviews/P0-b-poke-button-bench.md) (첫 리뷰 때 생김) |
| 상태 | **초안 — 사용자 결정 대기(§0).** 시험 상태 `not-tested` |

## 0. 사용자 결정 (구현 전에 확정)

| # | 결정할 것 | 명세의 현재 가정 |
|---|---|---|
| D1 | 확정을 **poke만** 시험할지, **pinch 확정도 같이** 시험할지 | poke만. LiftGateStudy에서 ISDK pinch 확정이 자연스러운 손 동작 중 오인식이 잦아 poke로 바꿨다(LiftGateStudy ADR-007, Agent-Workflow §4 L2). poke limiter(U-06)도 poke 쪽 결정이다. pinch를 넣으면 "손끝으로 겨누고 pinch로 확정"하는 선택 로직을 새로 만들어야 한다 |
| D2 | 버튼 크기·간격 실제 자료가 있는지 | 없음. §2의 크기·간격은 **시험용 자리값**이며 차량 사양도 합격 기준도 아니다. 자료가 있으면 그 값으로 바꾼다 |

## 1. 왜 만드는가

- **허공에서 버튼에 손을 맞출 수 있는가.** VR이라 촉감이 없다. 참가자는 닿았는지 모르고, 실제 손끝은 버튼 면을 지나 더 들어간다(AGENTS.md). 얼마나 지나가는지(mm)가 눌림 깊이·limiter·판정 영역 설계의 근거다.
- **몰린 버튼에서 ISDK 기본 동작이 하나만 활성하는가.** U-04는 "손끝에서 가장 가까운 하나만 활성 + 히스테리시스"를 요구한다. ISDK poke가 이미 그렇게 하는지 모른다(선택 로직이 프리빌트 라이브러리 안에 있어 읽을 수 없다). 그렇지 않으면 선택 층을 직접 얹어야 한다(조사 기록 2026-09 §10의 3).
- **"닿았다"고 느끼게 하는 피드백이 무엇인가.** poke limiter(손이 표면에 멈춰 보임, U-06), 버튼 눌림, 상태 색, 누름 소리를 함께 보여 주고 참가자에게 묻는다(조사 기록 2026-09 §10의 5).
- **레이·원거리 잡기가 실제로 꺼져 있는가.** 손을 뻗지 않고 고를 수 있으면 13번 도달성과 05번 자세 부담 평가가 사라진다(U-03·U-04). 컴포넌트를 끈 것과 실제로 안 되는 것은 다르다(Agent-Workflow §5.3).

이 시험대는 P0 측정용이다. 단 손 리그 Pawn과 버튼(§2 "다시 쓰는 부품")은 P1 이후에도 쓴다.

## 2. 범위

### 만들 것

**다시 쓰는 부품** — `Source/vBoR/Interaction/`. ISDK·MetaXR에만 의존하고 vBoR 시나리오 코드를 참조하지 않는다(Agent-Workflow §5.3). ISDK 샘플 플러그인은 참조하지 않는다.

1. **손 리그 Pawn** (`AVBoRHandRigPawn`)
   - HMD 카메라, ISDK 손 리그 좌·우(`UIsdkHandRigComponentLeft/Right` — `Plugins/MetaXRInteraction/Source/OculusInteractionPrebuilts/Public/Rig/IsdkHandRigComponent.h:142` 부근).
   - **레이 인터랙션을 끈다.** 리그는 `RayInteraction`을 기본으로 만들고(`IsdkRigComponent.h:394`), 레이 리그는 "컴포넌트가 활성일 때만 켜짐" 조건을 갖는다(`IsdkRayInteractionRigComponent.h:147-151`). 끄는 방법은 헤더·구현으로 확인해 고른다.
   - **원거리 잡기·컨트롤러 리그를 만들지 않는다.** 손 리그만 쓴다.
   - **poke limiter를 붙인다(U-06):** `RigModifiersToSpawn`(`IsdkRigComponent.h` 같은 파일)에 `UIsdkPokeLimiterRigModifier`(`OculusInteractionPrebuilts/Public/Rig/IsdkPokeLimiterRigModifier.h`)를 넣는다.
   - 데이터 소스: ISDK는 MetaXR·OpenXR 데이터 소스 모듈을 둘 다 올린다(`OculusInteractionPrebuilts/Private/OculusInteractionPrebuiltsModule.cpp:32-59`). XR API가 Epic Native OpenXR일 때 어느 쪽이 쓰이는지 확인해 보고한다.
2. **poke 버튼** (`AVBoRPokeButton`)
   - ISDK 조립: `IsdkPokeInteractable` + `IsdkPointablePlane` + `IsdkClippedPlaneSurface`(경계 클리퍼 포함) + `IsdkPokeButtonVisual`(눌림 표현) + `IsdkPointerEventAudioPlayer`(누름 소리). 같은 구성의 샘플 BP(BigRedButton)는 참고만 한다.
   - 보이는 버튼: 교체 가능한 Static Mesh. 기본은 엔진 Cube를 크기에 맞춰 줄인 것.
   - 프로퍼티: 보이는 크기(가로·세로 mm), 판정 영역 배율(기본 1.0 — U-04는 키워도 되지만 13번은 1.0), 상태 색(평상·hover·select).
   - **활성 상태를 시각으로 표시**한다(U-04 — 촉감이 없으므로 이것이 접촉을 대신한다). hover·select 색이 달라야 한다.
   - 누름 소리는 ISDK Content의 poke 소리 에셋(`/OculusInteraction/Audio/Interaction_BasicPoke_ButtonPress`)을 쓴다.
   - 눌렸을 때 알리는 이벤트(delegate) 하나. 그 뒤 기능 명령 연결은 P3 범위다.

**P0 전용 시험대** — `Source/vBoR/P0Probe/` (P0이 끝나면 지운다)

3. **시험대 Actor** (`AP0ButtonBench`) — 버튼 배치, 측정, 결과 표시.
   - 배치(전부 프로퍼티. 기본값은 시험용 자리값, D2):
     - **단독 버튼 3개:** 보이는 크기 10·15·20 mm 정사각형, 손이 닿는 거리(정면 약 45 cm)에 서로 10 cm 간격.
     - **몰린 버튼 묶음:** 3×4 격자, 버튼 12 mm, 중심 간격 15 mm.
     - **먼 버튼 1개:** 크기 20 mm, 정면 2 m. 팔이 닿지 않는 거리라 **선택되면 안 된다**(레이·원거리 확인용).
   - 결과 표시는 P0-a처럼 레벨 공간에 고정한 텍스트 패널과 `LogP0Probe`.
4. **GameMode** (`AP0ButtonBenchGameMode`) — `DefaultPawnClass = AVBoRHandRigPawn`. 시작 위치 앞의 설정된 위치에 시험대를 생성한다(사용자가 레벨에 직접 배치하지 않아도 되게).

### 측정·표시할 것

| 항목 | 설명 |
|---|---|
| 버튼별 상태 이벤트 | normal·hover·select·unselect 전환을 시각과 함께 로그. 패널에는 최근 10개 |
| 누름 기록 | 한 번의 hover→select→unselect마다: hover에서 select까지 시간(s), **실제 손끝이 버튼 앞면을 지나 들어간 최대 깊이(mm)**, 어느 손 |
| 실제 손끝 | **poke limiter가 옮기지 않은 추적 원본 손끝**으로 잰다. limiter는 손 모델 루트를 옮기므로(`IsdkPokeLimiterRigModifier.h` `ApplyRootPokeLimiting`) 손 모델이나 거기 붙은 인터랙터 위치를 쓰면 깊이가 0 근처로 나온다. 원본을 어디서 읽는지 헤더·구현으로 확인해 보고한다. 원본을 얻을 수 없으면 멈추고 리뷰 파일 "질문"에 적는다. 13번 도달 판정 원칙과 같다(AGENTS.md U-06) |
| 동시 hover 수 | 몰린 묶음에서 같은 순간 hover 상태인 버튼 수. 현재 값, 최대값, 2 이상이 된 횟수. **1을 넘으면 ISDK 기본이 "하나만 활성"이 아니라는 뜻**이다 |
| 대상 전환 | 몰린 묶음에서 한 번의 누름 동안 hover 대상이 바뀐 횟수(떨림으로 대상이 번갈아 바뀌는지 — 히스테리시스 필요 근거) |
| 먼 버튼 | hover·select 횟수. **0이어야 한다** |
| 리그 상태 | 시작할 때 한 번 로그: 레이 인터랙션 활성 여부, 원거리 잡기·컨트롤러 리그 유무, 활성 rig modifier 목록(poke limiter가 있어야 함), 쓰인 데이터 소스 |

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| pinch 확정·손끝 겨누기 선택 로직 | D1 결정 대기. 결정이 바뀌면 명세를 고친다 |
| "가장 가까운 하나만 활성 + 히스테리시스" 선택 층 | 이 시험대가 그것이 필요한지 재는 것이다. 필요하면 다음 명세 |
| 기능 명령 연결, 단계 진행, 13번 도달 판정 | P3·P4 범위 |
| 차량 메시·실제 버튼 사양 | D2. 자리값만 쓴다 |
| 진동·촉감 | 장비가 Quest 3 단독이다 |
| 파일 저장·CSV, OK/NG 판정 | AGENTS.md(자동 기록 시스템·합격 수치 금지) |
| `Config/*.ini`·`.uasset`/`.umap` 생성 | XR 설정과 레벨은 사용자가 에디터에서 한다(§5) |
| ISDK 샘플 플러그인 참조 | Agent-Workflow §5.4 |

## 3. 구현자가 정할 것

- 레이 인터랙션을 끄는 방법(컴포넌트 비활성, 생성 안 함 등)과 그 근거(헤더 경로:줄).
- 추적 원본 손끝을 읽는 곳(ISDK 데이터 소스, MetaXR 본 등)과 근거.
- `IsdkPokeButtonVisual`·`IsdkPokeInteractable` 설정값(눌림 깊이·판정 영역 오프셋)을 C++에서 거는 방법. Blueprint에서만 되는 설정이 있으면 멈추고 질문.
- `vBoR.Build.cs`의 의존 모듈(`OculusInteraction`, `OculusInteractionPrebuilts` 등).
- 판정·측정 로직을 엔진 없이 시험할 수 있는 구조체로 나누는 방식(P0-a의 `FP0ProbeMeasurement`처럼).

## 4. 완료 조건 — 구현자가 확인할 것

1. §2의 측정 항목이 모두 패널과 로그에 나온다.
2. 손 리그에 레이·원거리 잡기·컨트롤러 인터랙션이 없거나 꺼져 있고, 시작 로그에 그 상태가 찍힌다. poke limiter가 활성 rig modifier에 있다.
3. 관통 깊이는 추적 원본 손끝 기준이다. limiter가 옮긴 손 모델·인터랙터 위치로 계산하지 않는다.
4. 자동화 시험(합성 입력):
   - 관통 깊이: 손끝이 앞면 앞이면 0, 지나가면 지나간 거리(mm), 한 누름 안의 최대값만 남는다.
   - 동시 hover 수와 2 이상 횟수: 전환 때만 센다.
   - 한 누름 동안의 hover 대상 전환 횟수.
   - hover→select 시간.
5. 샘플 플러그인 참조가 없고, 샘플 플러그인을 끈 상태로 빌드·시험한다(`-DisablePlugin=OculusInteractionSamples` / `-DisablePlugins=OculusInteractionSamples`).
6. 모든 ISDK·MetaXR 호출을 `Plugins/` 헤더와 대조했고 보고에 경로:줄을 적었다.
7. 로컬 빌드 경고·오류 0, `vBoR.*` 자동화 시험 전체 통과(기존 P0-a 시험 포함).
8. 보고에 "로컬에서 확인할 것"이 있다.

## 5. 로컬에서 확인할 것 — 사용자

### 에디터

1. XR API **Epic Native OpenXR**, 핸드 트래킹 켬(P0-a와 같은 설정).
2. 빈 레벨과 바닥을 만들고 World Settings의 GameMode Override를 `AP0ButtonBenchGameMode`로 둔다. P0-a 레벨과 따로 둔다.

### Quest 3 (Link, VR Preview)

P0 실측이며 T-ID 시험이 아니다. 아래는 측정 방법이고 합격 기준이 아니다.

| 측정 | 방법 | 기록할 것 |
|---|---|---|
| 손 리그 | 양손이 실제 손을 따라오는지, 손가락이 버튼 앞면에서 멈춰 보이는지(limiter) | 이상 여부 |
| 단독 버튼 | 크기별로 10번씩 누른다 | hover→select 시간, 관통 깊이(mm) 범위, 놓친 횟수 |
| 몰린 버튼 | 정해 둔 한 버튼을 10번 누른다. 위치를 바꿔 3곳 | 동시 hover 최대값, 2 이상 횟수, 대상 전환 횟수, 엉뚱한 버튼이 눌린 횟수 |
| 먼 버튼 | 손가락으로 먼 버튼을 가리키거나 손바닥을 향해 본다 | hover·select가 0인지 |
| 피드백 | 누를 때 "닿았다"고 느꼈는지, 무엇 때문인지(멈춘 손·눌림·색·소리) | 참가자 말 그대로 |

> ⚠️ 몰린 버튼에서 엉뚱한 버튼이 눌린 횟수는 **설계 판단용**이다. 평가 결과(오조작 빈도·간격 적정성)로 보고하지 않는다(AGENTS.md "하지 말 것").

결과는 [조사 기록](../Research-Log.md)에 "P0-b 실측" 절로 남긴다. 이 결과로 U-04 선택 층(가장 가까운 하나 + 히스테리시스)이 필요한지, 판정 영역 배율, 확정 방식(D1)을 정한다.
