# P5zc 생성 버튼 클래스 — 리뷰

## 1회차 (2026-10-27, Claude) — 통과 (문제 없음, 참고 3개)

- 리뷰한 커밋: `2019c91` feat: 생성 버튼의 BP 클래스 선택 칸 추가. 베이스 `main@4db0fba`. 명세 처음 판.

### 확인한 것

| 항목 | 결과 |
|---|---|
| D1 | `AVBoRPoweredPanel`에 `OpenButtonClass`·`CloseButtonClass`(`Powered Panel\|Buttons`), `AVBoRKeyFob`에 `UnlockButtonClass`·`LockButtonClass`(`Key`). `TSubclassOf<AVBoRPokeButton>`, 기본 빈 값, 영어 표시 이름·툴팁 |
| D2 | 빈 칸이면 생성 클래스·크기 덮기·키 누름 깊이 1·키 색 재질이 지금 코드 그대로. Abstract·Deprecated·NewerVersionExists 클래스는 Warning(Actor·칸 이름) 뒤 기본 버튼 |
| D3 | 클래스를 고르면 크기·누름 깊이·색 재질을 덮지 않음. `HitAreaScale`·메시는 원래 건드리지 않음 |
| D4 | 자리(경첩 × `…FromHinge`, 키 루트 + 오프셋), 붙이기, 소리 복사, 키 `PressFilter`·`Poke` 끔, 열기·닫기 연결은 클래스와 상관없이 그대로 |
| D5 | 생성 뒤 `SpectatorName` 비움. 관전 화면은 이름을 매 frame 읽으므로(`Spectator/VBoRSpectatorSubsystem.cpp:117`) `FinishSpawning` 뒤에 비워도 된다 |
| D6 | 두 헤더에 자리 규칙 주석 |
| 범위 | 기어 버튼·시나리오 버튼·에셋·레벨 변경 없음. 시험 전용 클래스 `AP5zcTestButton`·`AP5zcAbstractButton`은 Probe 폴더(기존 `AP2bTrackedDoor`와 같은 방식) |
| 시험 | 새 `vBoR.P5zc.*` 7개(E1–E7). 기존 시험 파일 변경 0 |
| 빌드·시험 재현 | `2019c91`: unity 빌드 Succeeded, `vBoR.*` **603/603**, EXIT CODE 0(`Saved/Logs/P5zcReviewAll.log`) |

### 문제

없음.

### 참고 (고치지 않아도 됨)

1. **버튼 BP의 소리 칸은 쓰이지 않는다.** 판·키의 `ButtonPressedCue`·`ButtonReleasedCue`가 늘 덮어쓴다(D4, 판·키가 비어 있어도 덮음). 소리는 `BP_Liftgate`·`BP_KeyFob_*`에서 정한다. Blueprint-Guide에 적는다(Claude).
2. **키 버튼 생성 실패 처리:** 키의 `CreateButton`은 `SpawnActorDeferred`가 null일 때를 보지 않는다(`VBoRKeyFob.cpp`, 원래 코드부터). 리프트게이트는 본다. 정상 클래스에서는 실패하지 않으므로 그대로 둔다.
3. **Quest 3(T-02·T-10, `not-tested`):** BP 버튼의 자리 규칙(원점 = 누르는 면 가운데, +Z 바깥)을 지키지 않으면 버튼이 면에 묻히거나 뜬다. 바꾼 뒤 보이는 자리·누름 깊이·키를 든 때만 눌리는지 본다.

## 질문

(없음)
