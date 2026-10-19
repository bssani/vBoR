# P5n 손 조작 조정 — 구현 전 질문

명세: [P5n-hand-tuning.md](../Specs/P5n-hand-tuning.md) · 브랜치: `codex/P5n-hand-tuning` · 베이스: `main@b044d62`

상태: **중단 — Q1 답·명세 개정 대기 (2026-10-19, Codex 앱). 제품 코드·기존 시험 변경 없음.**

## 질문

### Q1 — 위·아래 중앙 밀기의 가장자리 후퇴 단언도 수정 허용에 포함하는가?

- 상태: **열림**
- **문제:** `Source/vBoR/P3Probe/P5eRearviewMirrorTests.cpp:224-234`의 `vBoR.P5e.E10VerticalEdgesAndCorner`는 `Y=0`, `Z=±3.5 cm`인 위·아래 중앙 밀기도 시험한다. `:232`는 Pitch 변화를, `:234`는 회전한 가장자리 `(-4,0,±3.5).X > -4`를 요구한다. 명세 D3·E8대로 수평 풀이에서 중앙 밀기의 Yaw를 0으로 유지하고 Pitch도 바꾸지 않으면, 해당 점의 X는 정확히 -4여서 `:234`는 거짓이다. `:233`도 이 경우 Yaw가 0에 가까워야 한다고 요구한다. 하지만 명세 §3 예외 2(`Docs/Specs/P5n-hand-tuning.md:56`)는 `:232`, `:362-367`만 열거하며 `:234`는 빠져 있다. 통과를 위해 중앙 밀기에 임의의 미세 Yaw를 넣는 것은 조작 근거가 없다. 이는 실행으로 확인한 실패가 아니라 기존 단언과 명세를 대조해 발견한 문제다.
- **제안:** Claude가 §3 예외 2에 `:234`도 명시한다. `Y=0`에서는 Pitch 유지·Yaw 0.5 deg 이내와 가장자리 X 유지, `Y=10`인 모서리에서는 Pitch 유지·Yaw 회전과 가장자리 후퇴를 확인하도록 기대값을 나눈다. 제품 구현은 D3의 수평 풀이만 한다. 새 엔진·플러그인 API는 필요 없다.
- **확인할 시험:** `vBoR.P5e.E10VerticalEdgesAndCorner`의 위·아래 중앙/모서리 네 경우. 새 P5n E7·E8은 앞·뒤 두 면의 좌우 밀기와 위·아래 중앙 밀기를 확인하고, 기존 잡기 시험은 유지한다.
- **필요한 결정:** `:234`를 중앙/모서리별 D3 기대값으로 바꾸도록 수정 허용 목록을 확장할지 명세에 답을 남겨야 한다. 답이 오기 전에는 제품 구현·시험 수정·빌드·자동화를 진행하지 않는다(AGENTS.md 작업 방식, 명세 §3).

## Codex 보고 ①–⑤ (중단)

1. **바뀐 파일:** 이 리뷰 파일만 추가. `HANDOFF.md`의 Codex 결과 갱신(git 무시). 제품 코드·시험·Content·Config 변경 없음.
2. **완료 조건:** §6-1 구현, §6-2 자동화, §6-3 non-unity·unity는 미수행. §6-4 보고는 이 파일·HANDOFF·커밋 본문에 기록. §6-5 기기 시험 상태 `not-tested` 유지.
3. **명세와 다른 점:** 구현하지 않고 Q1에서 중단. 명세 §3 예외 밖의 단언을 임의로 바꾸지 않기 위한 절차다. 하위 에이전트·자체 리뷰 루프·에디터·MCP 작업 없음.
4. **API 근거:** 새 엔진·플러그인 API 사용 없음. 기존 `FVBoRTwoAxisMotion::Rotation()`의 Z축 Yaw·Y축 Pitch 구성(`Source/vBoR/Interaction/VBoRTwoAxisMotion.cpp:3-5`)과 위 시험 `:224-234`를 소스로 대조. 새 API 헤더 확인 대상 없음.
5. **로컬에서 확인할 것:** Q1 해결·구현 뒤 최종 non-unity(`-DisableUnity -Module=vBoR`)·unity·`vBoR.*` 필요. Blueprint/Details·Quest 3 Link/VR Preview 및 T-02·T-04·T-05·T-08은 `not-tested`. 시작 시 UnrealEditor 계열 프로세스 없음. push·PR·머지 없음.
