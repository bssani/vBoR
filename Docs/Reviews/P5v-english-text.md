# P5v 영어 전환 — 리뷰

명세: [P5v-english-text.md](../Specs/P5v-english-text.md)

## 1회차 (2026-10-22, Claude) — 통과 (문제 1개는 Claude가 같은 브랜치에서 고침, 참고 3개)

- 리뷰한 커밋: `333a7b8` chore: translate code and asset text to English. 베이스 `main@5cdbef2`.
- Codex 토큰: 9.16M.

### 확인한 것

| 항목 | 결과 |
|---|---|
| 한글 0 (§7-1) | `Source/vBoR` 한글·자모 검사 0개 파일. 에셋은 Claude의 직렬화 문자열 검사로 `Content/` 전체 0 |
| 식별자 그대로 (D2·§7-2) | 135개 파일 모두 주석·문자열 리터럴을 뺀 코드가 `main`과 같다(공백 무시 비교, Claude 스크립트). 파일 줄 수도 같다. 바뀐 줄마다 printf 서식 지정자의 순서·종류가 같다. `KoreanGuidance`·`EnglishReason` 그대로 |
| 접두어 분기 (D4) | `"Viewpoint command"`: 만드는 곳 `VBoRScenarioOperator.cpp:137`·`VBoRScenarioRunner.cpp:464`, 읽는 곳 `VBoRScenarioOperator.cpp:190`. `"No saved value"`: 만드는 곳 `VBoRVehicleAlignmentComponent.cpp:22`(P5u)·`:90`, 읽는 곳 `:229`. `"Recenter"`: `:119` ↔ `:230`. Codex가 찾은 `"Pawn"` 접두어(`:99` ↔ `:231`)도 짝이 맞다 |
| 참가자 안내문 (D3·D6) | `DA_Main_Scenario` 안의 문장이 §6 표 18개와 정확히 같다. 표에 없는 문장 2개("Pick up the key from the table.", "Turn the exterior lights on and off with the key.")는 `main`에도 있던 영어 문장이라 바뀌지 않았다. C++ 기본 시나리오 14단계(고유 문장 13개)도 §6과 같다. `L_Main` TextRender 문구가 D6 문장이다. 바뀐 에셋은 이 둘뿐 |
| "없어야 한다" 시험이 헛통과하지 않음 | 시험이 `Contains`·`Count`로 찾는 문자열 중 제품 코드에 그대로 없는 것 18개를 따로 봤다. 전부 printf로 조립한 문자열이다. "0번 기록"을 보는 시험(`P3FlowTests.cpp:888-1017`의 `"Start position |"`)은 같은 파일 `:893`의 "1번 기록" 시험이 같은 문자열을 실제로 확인한다 |
| 빌드·시험 재현 | `333a7b8`: non-unity·unity Succeeded(링크만 함. Codex 로그 `P5vNonUnityBuild.log` 96개 컴파일, vBoR 경고 0). `vBoR.*` 547/547 통과(`Saved/Logs/P5vReviewAll.log`) |
| 주석 말투 (D5) | 표본(`VBoRDoorMotion.cpp`, `VBoRScenarioRunner.cpp`, `Spectator/`)에서 짧은 조각문, 근거 ID·단위가 남아 있다 |

### 문제

#### R1-1 화면 구분자 ` · `가 ` ,  `로 바뀜 — 반영함 (Claude, 같은 브랜치)

- **문제:** 진행자 창 요약(`VBoRScenarioOperator.cpp:246·252·258·260·261`), 참가자 안내판 단계 줄(`VBoRScenarioRunner.cpp:653`, 예: `02 ,  2/3`), 관전 TV 부품 목록(`Spectator/VBoRSpectatorModel.cpp:69`)의 구분자 ` · `가 ` ,  `(쉼표 앞 공백 하나, 뒤 공백 둘)로 바뀌었다. 시험 문자열 5곳도 같이 바뀌어서 시험은 통과했다. 원인은 명세다. §4가 "가운뎃점 `·`은 `, `로 바꾼다"고 해서 한글이 아닌 화면 구분자까지 바꾸게 했다.
- **반영:** 12곳 모두 ` · `로 되돌렸다(`main`과 같은 구분자). 명세 §4 개정 1. 고친 뒤 코드 대조 스크립트를 다시 돌렸고, 빌드·전체 시험은 아래 "재확인"에 적는다.

### 참고 (고치지 않아도 됨)

1. **Codex 보고 ⑤:** 마지막 `"Pawn"` 접두어 한 줄은 전체 시험 뒤에 고쳤고, 관련 시험(P3Flow 19개)만 다시 돌렸다. 그 상태(`333a7b8`)로 Claude가 전체 547개를 재현해 통과했다.
2. **영어 문구가 두 갈래로 남았다:** `Reason`(이제 영어)과 `EnglishReason()`(진행자 창용 영어)이 같은 내용을 다른 영어로 말한다. 명세 범위 밖(합치지 않음)이라 그대로 둔다. 나중에 정리하려면 따로 명세를 쓴다.
3. **화면 확인은 사용자 몫:** 진행자 창(P5i)·VR 안내판 영어 문장 길이, `L_Main` TextRender. 글꼴·크기는 그대로다. Quest 3 가독성은 `not-tested`.

### 재확인 (R1-1 반영 뒤)

- non-unity Succeeded(6개 컴파일), unity Succeeded(3개 컴파일), vBoR 경고 0(`Saved/Logs/P5vR1NonUnity.log`, `P5vR1Unity.log`).
- `vBoR.*` 547/547 통과, EXIT CODE 0(`Saved/Logs/P5vR1All.log`).
- 주석·문자열 밖 코드 대조 다시 통과(135개 파일 `main`과 같음, 서식 지정자 같음). 문자열 안 ` ,  ` 0곳.
