# P5w 문 밀기 반대 면 막기 — 리뷰

명세: [P5w-door-push-side.md](../Specs/P5w-door-push-side.md)

## 1회차 (2026-10-22, Claude) — 통과 (문제 없음, 참고 2개)

- 리뷰한 커밋: `48bc269` fix: retain pushed door face during withdrawal. 베이스 `main@148fcca`.
- Codex 토큰: 2.52M.

### 확인한 것

| 항목 | 결과 |
|---|---|
| 범위 (D5) | 제품 코드는 `VBoRDoorMotion.h/.cpp`뿐이다. 판 Actor·룸미러·레버 변경 없음. 새 수치·새 무시 이유 없음(D4). 가장자리 여유 5 cm는 기존 상수를 파일 안에서 같이 쓰도록 옮겼을 뿐 값이 같다 |
| D1 | 기억은 밀기가 받아들여진 곳(`PushHand = H; PushFace = Face;`)에서만 정한다. 무시된 접촉은 정하지 않는다 |
| D2 | 면 선택 순서: 지금 밀고 있으면 `PushFace` → 기억한 면 → 기존 규칙 |
| D3 | 지우는 확인은 추적이 끊긴 손의 `continue` 뒤에 있어서, 끊긴 frame에는 지우지 않는다. 점 5개가 모두 X·Z 범위 밖이거나 기억한 면 앞이어야 지운다. `QueryFace`가 실패하면 상자 면 Y로 돌아간다(질의가 출력값을 바꿔도). NaN 점은 비교가 거짓이라 기억을 유지한다(보수적). `Reset()`·`BeginGrab()`은 두 손 모두 지운다 |
| 시험 E1–E9 | 모두 있다. 각 시험이 "기억이 남으면 막힘 / 지워지면 받아들여짐"을 구분한다. Codex가 더한 경우: 점 하나만 문 뒤에 남으면 유지(E2·E3), 추적이 끊긴 동안 남은 좌표로 지우지 않음(E5), 질의 면과 상자 면이 다른 위치(E9) |
| 기존 시험 | 바뀐 시험 없음. 명세 §3 감사(Claude 임시 코드 547/547)와 같다 |
| 빌드·시험 재현 | non-unity·unity Succeeded(링크만 함. Codex 로그 `P5wFinalNonUnityBuild.log` 39개·`P5wFinalUnityBuild.log` 8개 컴파일, 경고 0). `vBoR.*` 556/556 통과(547 + P5w 9, `Saved/Logs/P5wReviewAll.log`) |

### 문제

없음.

### 참고 (고치지 않아도 됨)

1. **보이는 손은 여전히 문을 통과한다.** 사용자 결정(로직만)대로다. Quest 3에서 문을 끝까지 연 뒤 손을 넣었다 빼기, 닫고 잠근 뒤 손을 넣었다 빼기, 콘솔·글러브박스 같은 동작을 본다(T-03·T-08, `not-tested`).
2. **룸미러·레버의 밀기는 그대로다.** 같은 증상이 보이면 따로 명세로 한다(범위 밖).
