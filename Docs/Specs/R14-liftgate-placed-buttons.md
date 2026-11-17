# R14 리프트게이트 버튼을 에디터에서 놓기

| 항목 | 값 |
|---|---|
| 결정 근거 | 사용자 2026-11-17: "liftgate은 play되면 버튼이 보이는 식이라 좀 불편해. 그냥 다른것과 같이 editor에서도 볼 수 있게 하자." |
| 베이스 / 작업 브랜치 | `main` / `codex/R14-liftgate-placed-buttons` |
| 에디터 작업 | 없음 |

## 지금

`AVBoRPoweredPanel::BeginPlay`가 열기·닫기 버튼을 Play 때 Spawn한다(`VBoRPoweredPanel.cpp:77-108`). 에디터에서는 버튼이 안 보인다.

## 할 일

- 게이트 밑에 붙은 `AVBoRPokeButton` 중 Actor 태그가 `Gate.OpenButton` / `Gate.CloseButton`인 것을 찾는다. 범위는 문 손잡이 찾기(`ForEachAttachedActors`, `VBoRHingedPanel.cpp:173-185`)와 같다.
- 찾은 칸은 그 버튼을 쓰고 Spawn하지 않는다.
  - `OnButtonPressed`를 연결한다.
  - `HingePivot` 밑이 아니면 월드 자세를 지킨 채 `HingePivot`에 붙인다.
  - 버튼의 메시·크기·소리는 덮어쓰지 않는다.
- 못 찾은 칸은 지금 그대로 Spawn한다.
- `EndPlay`는 Spawn한 버튼만 없앤다.

## 하지 말 것

- 기존 Spawn 경로와 칸을 지우거나 바꾸기.
- 기존 시험 단언 수정.
- 그 밖의 기능 더하기. Check setup 문구도 더하지 않는다.

## 시험 `vBoR.R14.*`

- E1: 두 버튼을 놓고 Play → Spawn 없음. 누르면 열림·닫힘 요청. 열리는 동안 게이트와 함께 돈다.
- E2: 열기만 놓음 → 닫기는 지금처럼 Spawn.
- E3: `EndPlay` 뒤 놓은 버튼이 남아 있다.

## 완료 조건

E1–E3 통과, 두 빌드 성공, `vBoR.*` 전체 통과.

## 사용자 확인

- 게이트 BP에 poke 버튼 BP 두 개를 `HingePivot` 밑에 Child Actor로 넣고 태그를 단다.
- Play 때 버튼이 겹쳐 두 개씩 보이지 않는지 본다.
- Quest 3 `not-tested`.
