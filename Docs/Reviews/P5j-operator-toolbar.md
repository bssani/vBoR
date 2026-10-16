# P5j 에디터 툴바 아이콘 — 리뷰

명세: [P5j-operator-toolbar.md](../Specs/P5j-operator-toolbar.md) · 브랜치: `codex/P5j-operator-toolbar`

## 리뷰 1회차 (2026-10-17, Claude — `6b4138b`)

**결과: 통과. 열린 항목 없음.** 명세 D1–D5대로 들어갔다. 바뀐 것은 새 에디터 모듈 `vBoREditorTools`와 아이콘 SVG, `vBoR.uproject`의 모듈 한 줄뿐이다. `vBoR`(런타임)·진행자 창(P5i)·관전 화면(P5k) 코드는 바뀌지 않았다. Codex 보고는 커밋 본문에 있다(첫 구현이라 리뷰 파일이 없어 Claude가 이 파일을 만듦).

**재현(에디터 닫힘 확인 뒤, 새 모듈 `.cpp`·`.h` 시각 갱신):**
- non-unity(`-DisableUnity -Module=vBoR -Module=vBoREditorTools`) `Result: Succeeded`. `VBoROperatorToolbar.cpp`·`P5jOperatorToolbarTests.cpp` 컴파일, `UnrealEditor-vBoREditorTools.dll` 링크. 경고·오류 0. 로그 `Saved/Logs/P5jReview-NonUnity.log`.
- unity `Result: Succeeded`. 경고·오류 0. 로그 `P5jReview-Unity.log`.
- `vBoR.*` **326/326**(P5j 5개 포함), `TEST COMPLETE. EXIT CODE: 0`. P5j 시험 구간 경고·오류 0. 로그 `P5jReview-Full.log`.

**Codex 토큰:** 3.41M(캐시 약 95%, 2026-10-16 15:04–15:11 UTC 약 8분). 빌드 non-unity 두 번(첫 번은 include 경로 실패 — 보고 ③), unity 한 번, 전체 시험 한 번. 규칙 안이다.

**확인한 것:**
- **D1 모듈:** `vBoR.uproject`에 `"Type": "Editor"`로 더했다. 새 모듈이 `vBoR`을 참조하고(`vBoREditorTools.Build.cs`), `vBoR.Build.cs`는 바뀌지 않았다. 게임 타깃에서 빠지는 것은 Codex가 UBT JsonExport로 확인했다(보고 ②-D1). `vBoR`에는 `Public` 폴더도 공개 include 경로도 없어서, 새 모듈 쪽에만 `Source/vBoR` 경로를 더했다(`vBoREditorTools.Build.cs`). 런타임 모듈을 건드리지 않는 가장 작은 방법이다.
- **D2 등록:** `LevelEditor.LevelEditorToolBar.PlayToolBar`에 새 묶음 하나와 버튼 하나(`Operator`). 등록 전에 같은 owner 항목을 먼저 치우므로(`Private/VBoROperatorToolbar.cpp:65-66`) 다시 등록해도 하나다. 내릴 때 시작 콜백·owner·스타일을 푼다(`:51-59`). 엔진 쪽도 안전하다: `UnregisterOwner`는 메뉴 시스템이 있을 때만 풀고(엔진 `Developer/ToolMenus/Public/ToolMenus.h:123-127`), 시작 콜백은 시스템이 이미 켜져 있으면 바로 실행된다(`Developer/ToolMenus/Private/ToolMenus.cpp:3534-3540`).
- **D3 누르면:** 누를 때마다 `GEditor->PlayWorld`에서 러너를 다시 찾아 `ShowOperatorWindow()`만 부른다(`:29-36`, `:75`). 툴바 쪽에 창 코드가 없다. Play를 멈추거나 레벨을 다시 연 뒤의 옛 러너를 붙잡지 않는다.
- **D4 흐림:** 판정은 월드를 받는 `Evaluate` 하나다(`:16-27`). 켜짐과 툴팁이 둘 다 이 함수를 쓴다. 문구는 명세 그대로다.
- **D5 아이콘:** 디자인 SVG(20 × 20, 흰 선 1.5)를 그대로 두고, 브러시 색 `#5b8cff`를 `FromSRGBColor`로 준다. 스타일 루트는 프로젝트 폴더 기준이다.
- **시험:** E1–E5가 `vBoR.P5j.`로 새 모듈 안에 있다. E2는 두 월드 중 Play 월드의 러너를 고르는지, 한 번 누르면 한 번 부르는지, 러너가 지워진 뒤 부르지 않는지 본다. E5는 두 번 등록, 시작 두 번, 종료 두 번, 다시 시작에서 항목이 늘 하나(또는 0)인지 본다.

### 참고 (반영할 것 없음 — 에디터에서 볼 것, 명세 §7)

- **E5는 실제 모듈의 메뉴·스타일을 내렸다가 다시 올린다.** 시험이 끝날 때 다시 등록된 상태로 돌아오므로 전체 시험 뒤 상태는 같다. 시험 중간에 실패하면 그 실행 안에서는 툴바 버튼이 사라질 수 있다(자동화 전용 실행이라 영향 없음).
- **실제 버튼의 클릭 lambda**(`:73-76`)는 시험이 직접 부르지 않는다. 시험은 같은 `Execute`에 `ShowOperatorWindow`를 부르는 lambda를 넣어 본다. 한 줄짜리라 받아들인다.
- 에디터 GUI에서 볼 것은 명세 §7 그대로다. Play 옆 아이콘이 흐리게 있는지, VR Preview 중 켜지는지, 최소화·가림·Alt+F4 뒤 누르면 창이 앞으로 오는지, Play를 멈추면 다시 흐려지는지. 시험 상태는 `not-tested`다.
