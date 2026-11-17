# M3b 리뷰 — 앵커로 정렬 지키기

## 1회차 (Claude 2026-11-18) — `4699340`, 문제 2개

`git diff main...codex/M3b-anchor-alignment`. 순서: AGENTS.md 제약 → 명세 범위·완료 조건 → 빌드 → 버그.

- **범위:** 명세 D1–D5 안이다. 기본 꺼짐, 꺼지면 요청 없음(E1). 파일 `anchor` 항목은 선택이고 옛 파일도 읽힌다(E2). 못 찾으면 지금 동작(E4). M3a 코드·설정·에셋 변경 없음.
- **좋은 점:** 새 저장이나 `EndPlay` 때 일련번호로 늦게 온 콜백을 버린다. 찾는 동안 `A`를 거부한다. Recenter가 오면 찾기를 멈춘다.
- **빌드·시험(Codex 보고):** non-unity·unity 성공, `vBoR.*` 792/792. 다시 돌리지 않았다.

### 문제 1 — 찾은 앵커의 자세를 앵커 컴포넌트가 옮기기 전에 읽을 수 있다 — 반영함 (M3b 앵커 자세 대기·마커 저장 알림)

- **문제:** `VBoRAnchorAlignment.cpp` `AdvanceAnchorDiscovery`.
  - 유효 여부는 핸들로 직접 묻는다(`TryGetAnchorTransformByHandle`, Flags). 그런데 Stage 계산은 **앵커 Actor의 현재 Transform**으로 한다(`LocatedAnchor->GetOwner()->GetActorTransform()`).
  - 그 Actor는 `SpawnActorWithAnchorHandle`이 **Transform 없이(원점)** 만든다(`OculusXRAnchorBPFunctionLibrary.cpp:30`). Actor를 앵커 자리로 옮기는 것은 앵커 컴포넌트의 `TickComponent` → `UpdateAnchorTransform`뿐이다(`OculusXRAnchorComponent.cpp:136-176`, private).
  - 찾기 결과 콜백에서 Actor를 만들고 같은 frame이나 다음 frame에 정렬 컴포넌트 Tick이 먼저 돌면, Flags는 유효한데 Actor는 아직 원점이다. 그러면 Stage = Pawn 역변환이 되어 **차가 수 m 엉뚱한 자리에 놓인다.** 새로 만든 컴포넌트의 Tick이 그 frame에 도는지는 보장되지 않는다. `AddTickPrerequisiteComponent`는 둘 다 같은 frame에 예약된 경우에만 순서를 정한다.
  - 시험은 훅으로 Stage를 바로 넣어서(`FinishAnchorDiscovery`) 이 경로를 지나지 않는다.
- **제안:**
  - Actor가 실제로 옮겨진 뒤에만 받는다. 가장 단순한 방법은 Actor를 만든 frame 번호를 기억하고, 그 뒤 frame에서 Actor가 만든 자리(원점)에서 **움직였을 때**만 Stage를 읽는 것이다.
  - 더 안전하게는 **연속 2 frame의 Stage가 1 mm·0.1° 안에서 같을 때** 받는다. Link 머리 기준 보정(`#if WITH_EDITOR`)이 첫 frame에 흔들리는 것도 함께 거른다.
  - 10 s 시간 초과는 그대로 둔다(그 안에 안정되지 않으면 못 찾음 처리).
- **확인할 시험:** 시험용 훅으로 "앵커 Actor를 찾았지만 아직 원점" 상태를 만들어 한 frame Tick → 적용 안 됨, 상태 `Finding anchor`. Actor를 앵커 자리로 옮기고 Tick → 적용, Pawn이 E3과 같은 자리.

### 문제 2 — 마커 레벨에서 앵커 저장 결과가 화면에 안 보인다 — 반영함 (M3b 앵커 자세 대기·마커 저장 알림)

- **문제:** 앵커 결과는 `AnchorResult` → 로그와 `OnPointRecord`로만 나간다. 진행자 창의 점 정렬 상태(`PointStatus`)에는 붙는다.
  - 하지만 실제로 저장하는 곳인 `L_M1_MarkerAlign`의 `AM1MarkerAlignProbe::SaveFit`(`M1MarkerAlignProbe.cpp:516-540`)는 `OnPointRecord`를 받지 않아 화면에 나오지 않는다.
  - 앵커 만들기·저장은 비동기라 몇 초 걸릴 수 있다. 그 전에 Play를 멈추면 `EndPlay`가 일련번호를 올려 결과를 버리고, 파일에 `anchor`가 남지 않는다. 사용자는 화면만 보고는 기다려야 하는지 알 수 없다.
- **제안:**
  - `AM1MarkerAlignProbe`가 정렬 컴포넌트의 `OnPointRecord`를 받아 `Publish`로 화면에 띄운다. `AnchorResult` 문구가 이미 영어 한 줄이라 그대로 쓴다.
  - 스위치가 1일 때는 S 직후 "Saving anchor — wait" 한 줄을 띄운다. 결과 줄("anchor saved …" 또는 실패)이 나오면 끝이다.
- **확인할 시험:** 마커 프로브 시험대(`FM2Fixture`)에서 스위치 1 + 훅으로 저장 요청 → 화면 문구에 "Saving anchor"가 나온다. 결과를 넣으면 "anchor saved" 줄이 나온다.

### 참고 (고치지 않아도 됨)

1. 저장할 때마다 새 앵커를 기기에 저장하고 옛 앵커는 지우지 않는다. 명세에 없었다. 쌓여도 동작에는 문제가 없다.

시험 상태: Quest 3 `not-tested`.
