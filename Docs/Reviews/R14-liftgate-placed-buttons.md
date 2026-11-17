# R14 리뷰 — 리프트게이트 버튼을 에디터에서 놓기

## 1회차 (Claude 2026-11-18) — `a6a4473`, 통과

`git diff main...codex/R14-liftgate-placed-buttons`. 순서: AGENTS.md 제약 → 명세 범위·완료 조건 → 빌드 → 버그.

- **범위:** `VBoRPoweredPanel.cpp/.h`와 새 시험 파일 하나. Check setup·Spawn 경로·클래스 칸·기존 시험 변경 없음. 명세대로다.
- **동작:**
  - 놓은 버튼은 `ForEachAttachedActors`로 찾는다. 같은 태그가 여럿이면 처음 것을 쓴다.
  - 못 찾은 칸은 기존 `CreateButton`을 그대로 쓴다.
  - `HingePivot` 밑이 아니면 월드 자세를 지킨 채 붙인다.
  - `EndPlay`는 바인딩을 풀고, Spawn한 버튼만 없앤다.
- **시험:** E1(Outliner 부착 + Child Actor, 자세·메시·소리 유지, 회전 추종, 열림·닫힘), E2(닫기만 Spawn, 종료 때 Spawn만 파괴), E3(놓은 버튼이 남고 바인딩이 풀림).
- **빌드·시험(Codex 보고):** non-unity·unity 성공, `vBoR.*` 787/787. 다시 돌리지 않았다.

### 문제

없음.

### 참고 (고치지 않아도 됨)

1. 한 버튼에 두 태그를 다 달면 열기·닫기 둘 다에 연결된다. 명세에서 뺀 경우이고, 태그를 하나씩만 달면 생기지 않는다.

시험 상태: Quest 3 `not-tested`. 사용자 확인 — 게이트 BP의 `HingePivot` 밑에 버튼 Child Actor 두 개를 넣고 태그를 단 뒤, Play에서 버튼이 겹치지 않는지, 게이트와 함께 도는지 본다.
