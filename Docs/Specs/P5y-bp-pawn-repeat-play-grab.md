# P5y BP Pawn 두 번째 Play부터 잡기가 안 됨

| 항목 | 값 |
|---|---|
| 단계 | P5 리허설 준비 — 버그(사용자 보고 2026-10-23). 시험 T-02(박스)·T-03(문 손 조작)·T-08(자유모드 잡는 부품) |
| 결정 근거 | **사용자 보고(2026-10-23, 직접 시험):** L_Main에서 `BP_vBoRPawn`으로 에디터를 켜고 첫 Play는 잡힘, 끄고 다시 Play하면 잡는 조작이 전부 안 됨(박스·문 손잡이·레버·휠). hover 색은 나오고 `LogAxisPart: ... grab hand=` 로그는 없음. 밀기·버튼은 됨. 새로 만든 BP Pawn(부모 `VBoRPawn`)도 첫 Play만 됨. C++ `VBoRPawn`은 몇 번을 다시 Play해도 됨. P1 레벨(C++ `P1PalmGrabPawn`)도 됨. `BP_vBoRPawn`·Pawn 코드는 10-19 이후 바뀌지 않음 |
| 베이스 브랜치 | `main` @ 이 명세 머지 뒤 |
| 작업 브랜치 | `codex/P5y-bp-pawn-repeat-play-grab` |
| 리뷰 파일 | `Docs/Reviews/P5y-bp-pawn-repeat-play-grab.md` (첫 리뷰 때 생김) |
| 상태 | 명세 — 구현 전. **원인 객체는 아직 모른다 — 재현으로 찾는 것이 이 작업의 첫 단계다** |
| 에디터 작업 | **없음**(에셋을 고치지 않는다) |
| 앞 명세 | [P5x-panel-handles-only.md](P5x-panel-handles-only.md) |

## 0. 지금 아는 것과 모르는 것

**확정(사용자 시험):** BP Pawn에서만, 같은 에디터 프로세스의 두 번째 Play부터, 손바닥 쥐기(그리고 아마 pinch) → grabber select 경로가 끊긴다. grabber의 hover(손바닥 구 overlap)는 살아 있다. 부품·환경 문제가 아니다.

**잡기 경로(ISDK 1.205.0, Claude 확인):**
1. `UIsdkHandRigComponent`의 `PalmGrabRecognizer`가 손 모양을 보고 `PalmGrabStarted`를 낸다. 연결은 Play마다 한다(`Plugins/MetaXRInteraction/Source/OculusInteractionPrebuilts/Private/Rig/IsdkHandRigComponent.cpp:183-186`, 생성 `:48-49`, 갱신 `:113-115`).
2. rig가 `OnPalmSelectDelegate`를 낸다(`:245`).
3. `UIsdkGrabInteractionRigComponent`가 BeginPlay마다 그 delegate에 연결하고(`IsdkGrabInteractionRigComponent.cpp:81-88`) grabber의 palm grab을 부른다.
4. grabber의 켜짐은 조건 그룹이 정한다. InitializeComponent·BindDataSources마다 조건을 **추가만** 한다(`:54-57`, `:147-151`). 지우는 곳이 없다.

**의심 객체(추정 — 확인 필요):** 컴포넌트 안에서 `CreateDefaultSubobject`로 만든 **UObject 하위 객체**인데 `UPROPERTY`에 `Instanced`가 없다:
- `UIsdkHandRigComponent::PalmGrabRecognizer`·`PinchGrabRecognizer` (`Public/Rig/IsdkHandRigComponent.h:107`, `:114`)
- `UIsdkGrabInteractionRigComponent::EnabledConditionalGroup`·`IsActiveConditional` (`Public/Rig/IsdkGrabInteractionRigComponent.h:108-116`)
- 우리 rig는 Pawn의 하위 객체다(`Interaction/VBoRHandRigPawn.cpp:90-91`). 즉 Pawn → rig 컴포넌트 → UObject, 두 단계 아래다.

BP 클래스에서 이런 객체가 인스턴스마다 새로 생기지 않고 BP 템플릿의 것을 공유하면 증상이 설명된다. 예: 인식기 상태(쥐는 중)가 다음 Play까지 남아 "시작" 이벤트가 다시 안 남, 또는 조건 그룹에 죽은 조건이 쌓여 값이 false. 이 Pawn은 같은 종류의 문제(손 visual이 낡은 템플릿 부모를 가리킴)를 이미 한 번 겪었고, 그때는 그 부분만 막았다(`Interaction/VBoRPawn.cpp:17-26`, P4f D5).

## 1. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **먼저 재현 시험으로 공유 객체를 찾는다** | `vBoR.P5y.*`에 시험을 만든다. HMD 없이 판정할 수 있는 것으로 한다.<br>ⓐ **같은 월드에 `BP_vBoRPawn` 두 개를 spawn**하고 손마다 위 의심 객체(와 rig·grab rig 밑의 다른 UObject 하위 객체)가 **서로 다른 객체이고, outer 사슬이 각자의 Pawn**인지 본다. C++ `AVBoRPawn` 두 개로도 같은 확인을 한다(기준).<br>ⓑ **월드 두 개를 차례로** 만들고 지우며(`P4fPawnTests.cpp`의 `FP4fWorld` 방식) 각 월드에서 BP Pawn을 spawn·Play·EndPlay한다. 두 번째 월드 Pawn의 의심 객체가 첫 번째와 같은 객체가 아닌지, 조건 그룹의 조건 수가 늘지 않는지, 인식기 상태가 초기값인지 본다.<br>ⓒ 두 번째 Pawn에서 rig의 `OnPalmSelectDelegate`를 직접 내거나(또는 인식기의 `PalmGrabStarted`) grabber까지 가는지 본다(grabber 쪽에서 관찰할 수 있는 값으로).<br>- **고치기 전에 이 시험이 지금 코드에서 실패하는 것을 한 번 확인한다(red).** 실패하는 단언과 그때 공유된 객체 이름을 보고 ③에 적는다. 실패하지 않으면(자동화로 재현이 안 되면) 고치지 말고 질문에 적고 멈춘다 |
| D2 | **우리 Pawn 초기화에서 막는다. ISDK는 고치지 않는다** | D1에서 찾은 객체만 다룬다. 방법은 구현자가 정하되 이 조건을 지킨다.<br>- `AVBoRHandRigPawn`(또는 `AVBoRPawn`)의 초기화 단계에서, 그 객체가 이 인스턴스 것이 아니면(outer가 다르거나 템플릿·다른 인스턴스와 같은 객체) 이 rig를 outer로 새로 만들고 설정을 복사해 넣는다. ISDK가 그 객체를 쓰기 전(조건 그룹은 `InitializeComponent`, 인식기 연결은 rig BeginPlay)이어야 한다.<br>- private 멤버면 리플렉션(`FObjectProperty`)으로 바꿔도 된다. 헤더·`Plugins/` 아래 파일은 고치지 않는다(AGENTS.md).<br>- C++ Pawn(`AVBoRPawn`, `AP1PalmGrabPawn` 등)은 이미 자기 객체를 갖고 있으므로 아무것도 바뀌지 않아야 한다 |
| D3 | **레벨 다시 열기도 확인한다** | 진행자 "레벨 다시 열기"(`OpenLevel`, `VBoRScenarioRunner.cpp:541`, P5g)는 같은 프로세스에서 Pawn을 새로 만든다. 패키지 빌드에서도 두 번째 생성이 생기는 경로다. D1 ⓑ가 이 경우를 대신한다(월드를 새로 만들고 BP Pawn을 다시 spawn). 따로 레벨 로드 시험은 만들지 않는다 |
| D4 | **에셋은 그대로 둔다** | `BP_vBoRPawn`·`BP_vBoRGameMode`를 고치지 않는다. GameMode가 BP Pawn을 쓰는 것은 P4f E7이 확인한다(`P3Probe/P4fPawnTests.cpp:163`) |

## 2. 범위

### 만들 것

- D1 시험(재현 → red 한 번 → 고친 뒤 green).
- D2 수정.

### 범위 밖 — 하지 말 것

| 하지 말 것 | 이유 |
|---|---|
| `Plugins/MetaXRInteraction/` 고치기 | AGENTS.md. 우리 Pawn에서 막는다 |
| D1에서 공유로 확인되지 않은 객체까지 다시 만들기 | 가장 단순한 수정. 확인된 것만 |
| GameMode를 C++ Pawn으로 바꾸기 | 임시 우회일 뿐이다(사용자 몫, 커밋하지 않음 — §6) |
| 잡기 규칙·부품 코드 바꾸기 | 부품 문제가 아니다(사용자 시험) |

## 3. 지켜야 할 조건 (바꾸면 안 되는 것)

- **C++ Pawn:** `AVBoRPawn`, `AVBoRHandRigPawn`, P1–P3 palm grab Pawn의 동작과 시험 결과가 그대로다.
- **P4f D5 수정(손 visual 부착, `VBoRPawn.cpp:17-26`)** 과 P4f E1–E7 결과가 그대로다. 특히 E7(BP Pawn visual이 자기 인스턴스에 붙음, Actor Tick 유지)과 손 메시·재질 교체(E2–E5).
- **잡기 설정:** 손바닥 잡기 켬·pinch 켬(P4f D1), distance·ray grab 꺼짐(U-03), poke limiter(U-06), ray 끔.
- **기존 시험 감사(Claude, 2026-10-23):** 손 rig·grabber를 만지는 시험은 `P3Probe/P4fPawnTests.cpp`(E1–E7), P1·P2·P3 palm grab Pawn 시험(`P1LoadBenchTests.cpp:163`, `P2DoorWorldTests.cpp:42`, `P3FlowTests.cpp:445`, GameMode 기본 Pawn 확인)이다. 이 수정은 "이 인스턴스 것이 아닐 때만" 바꾸므로 C++ Pawn 시험은 바뀌지 않아야 한다. 전체 실행에서 다른 시험이 바뀌면 고치지 말고 질문에 적고 멈춘다.

## 4. 시험 — `vBoR.P5y.*`

| ID | 상황 | 확인 |
|---|---|---|
| E1 | 같은 월드 BP Pawn 두 개 | 손마다 의심 객체가 두 Pawn 사이에 다르고 outer가 각자 rig. **C++ Pawn 두 개도 같은 확인**(기준, 지금도 통과해야 함) |
| E2 | 월드 두 개 차례로, BP Pawn spawn·Play·EndPlay | 두 번째 월드 Pawn: 의심 객체가 첫 번째와 다름, 조건 그룹 조건 수가 첫 번째와 같음, 인식기 상태 초기값 |
| E3 | E2의 두 번째 Pawn에서 palm select 신호 | grabber까지 전달됨(관찰 가능한 값으로) |
| E4 | 첫 번째 Play 중 "쥐는 중"으로 끝남 | E2에서 첫 번째 월드를 인식기 활성 상태로 끝내도 두 번째가 초기값(이전 Play 상태가 안 남음). 인식기를 직접 활성으로 만들 수 없으면 질문에 적고 E4는 뺀다 |
| E5 | C++ Pawn 회귀 | `AVBoRPawn`으로 E2와 같은 순서 — 바뀌는 것 없음 |

## 5. 구현자가 정할 것

- 의심 객체를 다시 만드는 방법과 단계(설정 복사 방법, 리플렉션 사용 여부).
- E3에서 grabber 쪽 관찰 값.

## 6. 완료 조건

1. D1 red 한 번 확인(보고에 실패 단언·공유 객체 이름), D2 뒤 `vBoR.P5y.*` 통과.
2. `vBoR.*` 전체 통과(기존 557 + P5y).
3. non-unity·unity 빌드 성공, vBoR 경고 0.
4. 주석 영어 짧은 조각문. 커밋 본문 보고 ①–⑤.

## 7. 로컬에서 확인할 것

- **빌드·시험:** red 한 번(명세 요구) + 끝에 한 번.
- **에디터:** 없음.
- **사용자(PIE, Quest 3 Link):** `BP_vBoRPawn`으로 L_Main을 **세 번 연달아 Play** — 매번 박스·문 손잡이·레버·휠이 잡히는지. 진행자 "레벨 다시 열기" 뒤에도 잡히는지(T-02·T-03·T-08, `not-tested`).
- **그동안 임시 우회:** `BP_vBoRGameMode`의 Default Pawn Class를 C++ `VBoRPawn`으로 바꾸면 반복 Play가 된다(`BP_vBoRPawn`에 따로 넣은 값이 없어 잃는 것 없음, P4f E7이 확인). **커밋하지 않는다** — P4f E7("GameMode Blueprint uses pawn Blueprint")이 실패한다. 이 명세 작업 전에 되돌린다.

## 개정 이력

| 날짜 | 내용 |
|---|---|
| 2026-10-23 | 처음 작성 |
