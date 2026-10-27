# P5zd 키 메시 크기 — 실제 메시는 넣은 크기 그대로

| 항목 | 값 |
|---|---|
| 단계 | P5 리허설 준비 — mid review(2026-10-28) 당일 수정(사용자 요청). 시험 T-10(키) |
| 결정 근거 | **사용자(2026-10-28):** "내 key mesh 넣으니까 사이즈가 이상하고 button도 이상해" → "지금 키만 따로 수정해"<br>**원인:** `AVBoRKeyFob::OnConstruction`이 어떤 메시든 경계 상자를 `KeySizeCm`(6×3×1.5 cm)에 맞춰 축마다 늘였다(`VBoRKeyFob.cpp:58-59`). 근사 큐브용 기능이 실제 메시에도 걸렸다. P4a 명세가 메시 교체를 다루지 않은 빈틈(Claude) |
| 작업 브랜치 | `claude/P5zd-key-mesh-size` |
| 구현 | **Claude가 직접**(사용자 요청, mid review 당일). Codex 구현·Claude 리뷰의 독립 확인은 없다 — 아래 §4 |
| 에디터 작업 | 없음 |

## 0. 결정

| # | 결정 | 내용 |
|---|---|---|
| D1 | **`KeySizeCm`은 근사 큐브에만** | 키 메시가 `/Engine/BasicShapes/Cube`일 때만 `KeySizeCm`으로 크기를 맞춘다. 다른 메시면 배율을 건드리지 않는다(BP·배치의 배율 그대로, 보통 1). 툴팁에 적는다 |
| D2 | **로그는 실제 크기** | BeginPlay 로그를 `key size=<경계 × 배율> cm`로 바꾼다(전에는 `KeySizeCm`을 찍었다) |

- 그대로: 버튼 자리(`UnlockButtonCm`·`LockButtonCm`, 키 루트에서 cm, 키 배율 무시), 버튼 크기(`ButtonSizeCm`), 버튼 클래스(P5zc), 잡기·물리·테이블 복귀.

## 1. 사용법

1. 키 메시 피벗을 키 가운데에 둔다.
2. `BP_KeyFob_*`의 `KeyMesh`에 메시를 넣는다. 크기는 메시 그대로다.
3. 버튼 자리 `UnlockButtonCm`·`LockButtonCm` = 키 피벗에서 버튼 윗면 가운데까지(cm, 메시 축). 버튼 모양은 `ButtonSizeCm` 또는 버튼 클래스(P5zc).

## 2. 시험

| ID | 확인 |
|---|---|
| E1 | 근사 큐브 키는 배율 = `KeySizeCm`/100(지금과 같음) |
| E2 | 다른 메시(엔진 Cylinder) 키는 배율 1, 버튼이 키 루트 + 회전 × cm 오프셋에 생김 |

- 기존 키 시험(`P4aKeyTableTests`, `P5zcSpawnedButtonClassTests`, `P4iCarBlueprintTests`)은 모두 큐브를 쓰므로 바뀌지 않는다. 파일 수정 없음.

## 3. 결과

- unity 빌드 Succeeded. `vBoR.*` **605/605**(기존 603 + 2), EXIT CODE 0(`Saved/Logs/P5zdAll.log`).
- 첫 빌드는 Claude가 에디터가 열린 채로 돌렸다(실수, 확인 결과를 보고도 멈추지 않음). 사용자가 에디터를 닫은 뒤 다시 빌드·전체 시험했다.

## 4. 남은 것

- 독립 리뷰가 없었다. mid review 뒤 Codex 또는 다음 리뷰 때 diff를 다시 본다.
- 숫자로 자리·크기를 정하는 구조 전체(키 버튼 자리, 리프트게이트 버튼, 경첩 등)는 mid review 뒤 전수 조사·공사(HANDOFF "미룬 것").
- Quest 3(T-10, `not-tested`): 실제 키 메시로 크기·버튼 자리·누름.
