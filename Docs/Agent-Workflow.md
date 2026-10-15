# 에이전트 작업 방식과 기록 체계

이 문서는 Codex·Claude·사용자가 이 리포에서 **어떻게 일하고, 알게 된 것을 어디에 모으는지** 적은 것이다. 지켜야 할 규칙은 [AGENTS.md](../AGENTS.md)에 있고, 이 문서는 그 규칙을 실제로 수행하는 절차와 근거를 담는다. §2·§3은 다른 Unreal 프로젝트에도 그대로 쓸 수 있게 썼다.

작성 2026-10-08 (Claude). 절차가 바뀌면 이 문서를 고친다.

---

## 1. 역할과 흐름

| 누가 | 하는 일 | 못 하는 일 |
|---|---|---|
| **Claude** (Claude Code, 로컬) | 작업 명세, 코드 리뷰, 조사, 문서·리포 설정, 빌드·자동화 시험 재현 | 요청이 없으면 제품 C++를 쓰지 않는다 |
| **Codex** (데스크톱 앱, 로컬 Windows) | 명세대로 구현, 로컬 빌드, 자동화 시험, 커밋 | 명세 밖 기능 추가, push·`main` 머지 |
| **사용자** | 결정, 에디터 작업(Blueprint·에셋·레벨·Project Settings), Quest 3 시험, push·머지 승인 | — |

두 에이전트는 **git과 파일로 연결된다.** 진입점은 리포 루트의 `HANDOFF.md`(git 무시)이고, 오래 남을 내용은 명세·리뷰 파일에 있다. 앱 채팅에 직접 작업을 보내는 연동은 확인되지 않았다(§1.1).

1. Claude가 `Docs/Specs/<명세>.md`를 쓰고 `main`에 넣는다.
2. Codex가 `main`에서 `codex/<명세>` 브랜치를 만들어 구현하고 커밋한다. 보고 ①~⑤는 커밋 본문에 쓴다.
3. Claude가 `git diff main...codex/<명세>`와 `git log`의 보고를 읽고 리뷰한다. 빌드와 시험을 직접 다시 돌려 보고 내용을 재현한다. **결과는 `Docs/Reviews/<명세>.md`에 회차로 남기고**, 명세를 고쳐야 하는 지적은 명세 파일을 직접 고친다. 둘 다 그 브랜치에 커밋한다.
4. Claude가 `HANDOFF.md`의 "지금 할 일"을 갱신하고, 사용자가 Codex 앱에 짧은 시작 문구를 보낸다(§1.1). Codex는 리뷰 파일의 `열림` 항목을 같은 브랜치에 **새 커밋**으로 반영하고 항목 상태를 `반영함 (<커밋>)`으로 바꾼다. amend·rebase로 리뷰한 커밋을 고쳐 쓰지 않는다.
5. 로컬 빌드 확인과 리뷰 통과 뒤 사용자 승인으로 PR·머지한다.

주의:

- **작업 폴더 하나를 둘이 같이 쓴다.** 한 번에 한쪽만 작업한다. Codex가 브랜치를 바꿔 두므로 작업 전에 `git branch --show-current`와 `git status`를 본다.
- **명세가 모호하면 구현자는 멈추고 묻는다.** 첫 명세(P0-a)에서 Codex가 흔들림 정의를 묻고 멈춘 것이 올바른 동작이었다.
- **사용자는 내용을 옮기지 않는다.** 명세·리뷰·질문은 모두 파일에 있다. 사용자가 보내는 것은 짧은 문구 두 개뿐이다: Codex 앱에 시작 문구, Claude에 리뷰 문구(§1.1). 사용자는 결정이 필요할 때만 질문을 받는다.
- **구현은 Codex 데스크톱 앱에서 한다(2026-10-09 사용자 결정).** 사용자가 앱에서 진행을 직접 보기 위해서다. 그 전 한때(2026-10-09, P0-a 리뷰 2회차) Claude가 `codex exec`로 실행했는데, 이 방식은 앱 화면에 보이지 않는다. 지금은 쓰지 않는다(§2.9).
- **역할을 바꿔 쓸 때가 있다.** Claude가 코드를 쓰면 리뷰는 Codex에 맡긴다(예: `8669f45`). 쓴 쪽이 자기 코드를 리뷰하면 독립 확인이 없다.

### 1.1 Codex 앱 인계와 연동 확인 (2026-10-09)

| 수단 | 확인 결과 |
|---|---|
| Codex 앱 채팅에 작업을 보내기 | **확인 안 됨.** CLI에 `codex queue --thread <세션> --message <글>`("기존 세션에 메시지를 대기열로")이 있다. 하지만 앱이 띄운 app-server에 실제로 닿는지는 보내 봐야 안다. 보내면 사용자 채팅에 메시지가 들어가므로 보내지 않았다. 시험하려면 사용자가 빈 채팅을 하나 만들고 허락한 뒤 한 번 보내 본다. |
| Codex 작업 완료 감지 | **가능하지만 비공식.** 앱 세션 기록 `~/.codex/sessions/<날짜>/rollout-*.jsonl`(originator `Codex Desktop`)에 `task_started`·`task_complete` 이벤트가 남는다(읽기만 해서 확인). 내부 형식이라 바뀔 수 있다. 믿을 신호는 Codex가 남기는 브랜치 커밋과 `HANDOFF.md`의 "Codex 결과" 절이다. Claude Code 세션이 열려 있으면 Claude가 백그라운드로 커밋을 감시하다가 자동으로 리뷰를 시작할 수 있다. 세션이 닫혀 있으면 사용자가 리뷰 문구를 보낸다. |
| 설치·설정이 필요한 것 (제안만 했다) | `codex remote-control start`(실험 기능, 공유 app-server 데몬). `codex queue`가 그 데몬을 통해 앱 채팅에 닿는지 볼 때 필요할 수 있다. OpenAI `codex-plugin-cc`(Claude Code 플러그인)도 있지만 CLI로 돌아서 앱 화면에는 보이지 않는다. |

사용자가 보내는 문구:

- Codex 앱에: `HANDOFF.md 읽고 '지금 할 일' 진행해.`
- Claude에: `Codex 끝났어. 리뷰해.`

---

## 2. Unreal 작업 절차 (프로젝트 공통)

### 2.1 시작 전

- 규칙 파일(AGENTS.md·CLAUDE.md), 계획, 조사 기록, 해당 명세를 읽는다.
- `git branch --show-current`, `git status` — 작업 폴더가 깨끗한지, 누가 브랜치를 바꿔 두었는지.
- Unreal Editor가 켜져 있는지: `tasklist /FI "IMAGENAME eq UnrealEditor.exe"`. **켜져 있으면 빌드·브랜치 전환·머지를 하지 않는다.** 에디터가 `.uasset`을 잠근다.
- 엔진 경로: `C:\Program Files\Epic Games\UE_5.7` (레지스트리에 없으므로 폴더로 찾는다).

### 2.2 API는 기억이 아니라 소스로 확인

- 플러그인 API는 `Plugins/` 아래 **헤더**를 읽고, 동작이 애매하면 **`.cpp` 구현까지** 따라간다. 엔진 API는 `UE_5.7/Engine/Source/` 헤더.
- 근거는 `경로:줄`로 남긴다. 명세·커밋 보고·리뷰 모두.
- 주석과 실제 타입이 어긋나는 경우가 있다. MetaXR 신뢰도 주석은 `[0,1]`인데 타입은 2단계 enum이었다(조사 기록 2026-09 §2).
- 이름도 믿지 않는다. MetaXR의 새끼손가락 키 문자열은 `Pinky`가 아니라 `Pink`다(`OculusXRInput.cpp:91`).
- 의심이 들면 끝까지 따라간다. OpenXR에서 본 이름이 엉뚱한 관절에 붙는지 의심했는데, 플러그인이 OpenXR 관절을 기존 OVR 순서로 다시 매핑하는 것(`OculusXRInputHandTrackingExtensionPlugin.cpp:848-850`)을 확인하고 해소했다.

### 2.3 빌드

에디터를 닫고 실행한다. 오래 걸리므로 백그라운드로 돌리고 끝나면 확인한다.

```
"C:\Program Files\Epic Games\UE_5.7\Engine\Build\BatchFiles\Build.bat" vBoREditor Win64 Development -Project="C:\Git\vBoR\vBoR.uproject" -WaitMutex -DisablePlugin=OculusInteractionSamples
```

- 통과 기준: `Result: Succeeded`, 로그에 `warning`·`error` 0.
- UBT에서 여러 플러그인을 끌 때는 `+`로 잇는다(`-DisablePlugin=A+B`, `Engine/Source/Programs/UnrealBuildTool/Configuration/TargetRules.cs:842`).
- **로컬 도구 플러그인(§2.9의 UnrealAgent, McpAutomationBridge)이 있는 PC에서는 함께 빌드된다.** 경고·오류 판정은 vBoR 모듈 기준이다. 도구 플러그인에서 나온 경고·오류는 따로 적고, 그 소스는 고치지 않는다. 리포에 없는 플러그인이라 다른 PC(회사 PC 포함)의 빌드와 무관하다.
- 새 플러그인이나 모듈을 추가한 뒤에는 플러그인 모듈까지 다시 컴파일된다(MetaXR+ISDK 처음 빌드는 약 1분).
- **non-unity 확인(빠진 include 찾기)은 vBoR 모듈로 한정한다:** 위 명령에 `-DisableUnity -Module=vBoR`를 붙인다. 모듈을 한정하지 않으면 로컬 도구 플러그인 약 1,200 파일까지 다시 컴파일한다(2026-10-11, P1 R4).
  - 이미 빌드된 파일은 다시 컴파일되지 않는다. 리뷰에서 재현할 때는 `Source/vBoR`의 `.cpp`·`.h` 수정 시각을 갱신한 뒤 non-unity, 그다음 unity 순서로 빌드한다(내용은 바뀌지 않는다).

### 2.4 자동화 시험

- 판정·측정 로직은 **엔진 없이 돌릴 수 있는 구조체로 분리**하고(예: `FP0ProbeMeasurement`), `WITH_DEV_AUTOMATION_TESTS` 시험을 붙인다. 손 추적처럼 기기가 있어야 하는 입력도 로직은 합성 입력으로 시험할 수 있다.
- headless 실행:

```
UnrealEditor-Cmd.exe "C:\Git\vBoR\vBoR.uproject" -ExecCmds="Automation RunTests vBoR.<접두어>;Quit" -unattended -nullrhi -nosplash -nosound -DisablePlugins=OculusInteractionSamples,McpAutomationBridge,UnrealAgent -log=<이름>.log
```

- 에디터 실행 인자에서는 쉼표로 잇는다(`-DisablePlugins=A,B`, `Engine/Source/Runtime/Projects/Private/PluginManager.cpp:1470, 1578`). 로컬 도구 플러그인을 끄는 이유: headless 시험 중에 MCP 서버(:3000)나 에이전트 백엔드가 뜨지 않게 하고, 도구가 없는 PC와 같은 조건으로 시험하기 위해서다.

- 결과: `Saved/Logs/<이름>.log`에서 `Found N automation tests`, 각 `Result={Success}`, `TEST COMPLETE. EXIT CODE: 0`.
- `-log=`에는 파일 이름만 준다(`Saved/Logs/` 아래에 생긴다). 다른 경로에 남기려면 `-abslog=<절대경로>`를 쓴다(`Engine/Source/Runtime/Core/Private/GenericPlatform/GenericPlatformOutputDevices.cpp:86`). `-log=`에 경로를 넣으면 그 이름으로 로그가 생기지 않는다(2026-10-15, P5b·P5c에서 두 번 겪음).
- **합성 시험 통과는 기기 시험 통과가 아니다.** 시험 상태는 Quest 3로 확인하기 전까지 `not-tested`.

### 2.5 맵·에셋 로드 확인

- **GUI 에디터로 맵을 연다.** `-game -nullrhi`로 맵을 띄우는 headless 방식은 맵을 읽기 전 종료 단계에서 크래시(EXCEPTION_ACCESS_VIOLATION)가 나서 판단할 수 없었다.

```
UnrealEditor.exe "C:\Git\vBoR\vBoR.uproject" /<플러그인 또는 Game>/<경로>/<맵> -log=<이름>.log
```

- 로그에 `Total Editor Startup Time`이 나오고, `registered with world '<맵>'`이 보이면 로드된 것이다. 그다음 `Error:`·`Warning:`을 찾는다.
- **알려진 잡음 — 무시해도 되는 것:**
  - `Failed to load 'aqProf.dll' / 'VtuneApi*.dll' / 'WinPixGpuCapturer.dll' / 'Wintab32.dll'` (프로파일러·태블릿 DLL)
  - `LogStreaming: Warning: Failed to read file ... .png` (엔진 Slate 아이콘)
  - `Asset has been saved with empty engine version` (MetaXR Content)
  - `Required extension XR_FB_passthrough / face_tracking2 / eye_tracking_social ... is not available`
  - `Unable to retrieve OculusXRHMD` (XR 시스템이 OpenXR일 때)
  - `LogHttp: Warning: HTTP request timed out` (오프라인 확인 요청)
  - 헤드셋 미연결 시 `xrGetSystem() failed` → OVRPlugin 실패 후 OpenXR로 넘어감
  - `CDO Constructor (IsdkInfoCardComponent): Failed to find /OculusInteractionSamples/...` (샘플 플러그인이 없는 PC. 쓰지 않는 컴포넌트)
  - OpenXR가 API 1.1로 인스턴스를 만들려다 실패한 뒤 `xrCreateInstance succeeded with XR_API_VERSION_1_0`로 정상 생성 (런타임이 1.0까지만 지원. 폴백이라 문제없음)
- 확인 뒤 에디터를 닫는다. **Meta XR 플러그인 설문 창이 모달로 떠서 정상 종료를 막는다.** 에이전트는 사용자 대신 답하지 않고 강제 종료한다. 닫은 뒤 `git status`로 바뀐 파일이 없는지 본다.
- 화면을 봐야 하면 스크린샷을 찍어 읽는다. 클릭으로 에디터를 조작하지는 않는다.

### 2.6 커밋·머지

- 브랜치에서 작업한다. 커밋 본문에 보고 ①~⑤(AGENTS.md).
- 플러그인 `Content/` 아래 `.uasset`이 diff에 있으면 의도한 변경인지 확인한다. LiftGateStudy에서 샘플 Pawn `.uasset`이 바뀐 채 커밋됐는데, 의도한 변경인지 아무도 몰랐다(§4 L8).
- push·PR·`main` 머지는 사용자 승인 후.

### 2.7 리뷰

- `git diff main...<브랜치>`. 보는 순서: 확정 제약·금지 → 명세의 범위·완료 조건 → 빌드 시점 오류 → 버그 → 주석 말투.
- **보고를 믿지 말고 재현한다:** 빌드와 자동화 시험을 직접 다시 돌린다.
- 명세 누락을 찾으면 구현자 탓으로 돌리지 않고 명세를 고친다(예: P0-a 신뢰도 복구 튐, `8669f45`).
- **항목 형식:** 문제(위치·조건·영향, 반드시 해결) / 제안(접근·API 경로:줄·주의점, 권장) / 확인할 시험 / 필요할 때만 스케치. 완성 코드는 주지 않는다 — Claude가 쓴 `8669f45`의 버그를 Codex의 독립 리뷰가 찾았듯이, 구현과 리뷰가 서로 다른 쪽이어야 확인이 된다.
- 비효율의 큰 원인은 명세의 빈틈이다. 명세에 지켜야 할 조건과 경계 상황·시험을 미리 적는다(CLAUDE.md).

### 2.8 에이전트가 못 하는 것 — 사용자 몫

- Quest 3 착용 시험, VR Preview 확인
- Blueprint·레벨·에셋 편집. `.uasset`은 바이너리라 고칠 수 없다. 참조 클래스 이름은 `grep -a`로 읽을 수 있다(예: 샘플 Pawn에 레이 인터랙터가 있는지).
- Project Settings, Meta XR Project Setup Tool, 대화상자 응답 등 에디터 GUI 조작

### 2.9 Unreal용 에이전트 도구

| 도구 | 위치 | 하는 일 | 비고 |
|---|---|---|---|
| 시작 훅 `unreal-context.sh` | `.claude/hooks/`, `.codex/hooks/` | 세션 시작 때 UE 프로젝트 사실(엔진 버전, 빌드 명령, 에디터 잠금 주의)을 컨텍스트에 넣는다 | Epic Games의 unreal-engine-skills 플러그인(MIT)을 GMTCK_MR을 거쳐 가져왔다. Claude는 `$CLAUDE_PROJECT_DIR`, Codex는 절대경로로 등록. **Codex의 명령형 훅은 로컬에서만 돌고**, 프로젝트를 신뢰로 지정하고 `/hooks`에서 승인해야 실행된다 |
| `ue-compile-check` 에이전트 | `.claude/agents/`, `.codex/agents/` | 빌드할 수 없는 환경에서 바뀐 C++의 UHT·include·모듈 의존·unity 빌드 충돌·Meta XR API를 검사한다 | 엔진이 있는 로컬에서는 이것 대신 실제 빌드(§2.3) |
| McpAutomationBridge (ChiR24 [Unreal_mcp](https://github.com/ChiR24/Unreal_mcp) 0.6 beta, MIT) | `Plugins/McpAutomationBridge` → `C:\Git\Unreal_mcp\plugins\McpAutomationBridge` (심볼릭 링크, `.gitignore`) | 열린 에디터 안에 MCP 서버를 연다: `http://127.0.0.1:3000/mcp`. 도구는 `unreal` 하나이고 `search`·`describe`·`execute`·`configure`로 약 400개 기능(액터 조회·배치, 레벨 저장, Blueprint, 스크린샷, 로그, PIE, Python)을 쓴다 | 토큰 `Saved/MCP/capability-token`(64자, 에디터를 다시 열어도 그대로). 에디터가 열려 있을 때만 동작. **2026-10-11 Claude Code(`unreal-engine`)·Codex(`unreal-vbor`)에 연결함**(§2.10) |
| UnrealAgent (사용자 프로젝트 `C:\Git\UnrealAgent`) | `Plugins/UnrealAgent` → 심볼릭 링크, 설정 `.unrealagent/settings.local.json`(`.gitignore`) | 에디터 안 AI 대화 창. Python 실행·스크린샷과 외부 MCP `unreal-bridge`(위 서버)를 쓴다. `search`·`describe`는 승인 없이, 그 밖은 승인을 받는다 | 사용자가 에디터에서 직접 쓰는 도구 |
| `codex exec` (Codex 앱 내장 CLI) | `%LOCALAPPDATA%\OpenAI\Codex\bin\<해시>\codex.exe` | Claude가 Codex에 작업을 넘긴다. 끝나면 마지막 응답이 파일로 돌아온다 | **지금은 쓰지 않는다**(구현은 앱에서, §1). 아래는 다시 쓸 때를 위한 기록 |

`codex exec` 실행 방법 (2026-10-09 확인, 지금은 쓰지 않음):

```
codex.exe exec -C "C:\Git\vBoR" --approve-for-me -o <마지막 응답 파일> "<지시>" < /dev/null
```

- **npm으로 설치한 `codex`(0.154.0)가 아니라 Codex 앱에 들어 있는 `codex.exe`(0.160.1)를 쓴다.** 설정의 기본 모델 `gpt-6.1-sol`을 npm 버전은 모른다("not supported when using Codex with a ChatGPT account"로 실패). 앱이 업데이트되면 `<해시>` 폴더가 바뀌므로 `bin\*\codex.exe`로 찾는다.
- `--approve-for-me`: 작업 폴더 쓰기 샌드박스에서 돌리고, 그 밖의 쓰기(git 커밋, UBT·DDC 쓰기 등) 승인 요청은 Codex 자동 검토가 처리한다. `--sandbox`와 함께 쓸 수 없다. 샌드박스를 끄는 `--dangerously-bypass-approvals-and-sandbox`는 쓰지 않는다.
- 시작 로그의 MCP 인증 오류(context7, GitHub Copilot)는 쓰지 않는 서버라 무시한다.
- 백그라운드로 돌리고 끝나면 커밋·리뷰 파일·마지막 응답을 읽는다. Codex가 도는 동안 Claude는 작업 폴더를 건드리지 않는다. `main` 문서는 별도 sparse worktree에서 고친다.

### 2.10 에디터 MCP — 연결과 쓰는 규칙 (2026-10-11, 사용자 요청으로 Claude가 설정)

**연결 방식: stdio proxy.** 클라이언트 → `unreal-engine-mcp-server` proxy(Node) → 에디터 안 서버 `http://127.0.0.1:3000/mcp`.
- proxy가 프로젝트의 토큰 파일을 직접 읽는다. 그래서 어떤 설정 파일에도 토큰을 적지 않았다.
- 에디터가 닫혀 있어도 proxy는 뜨고, 그동안의 호출은 `NOT_CONNECTED`를 돌려준다. 에디터가 다시 뜨면 다음 호출에서 붙는다. 세션 중에 에디터를 열고 닫는 이 리포의 흐름에 맞는다. 직접 HTTP 연결은 세션 시작 때 에디터가 닫혀 있으면 그 세션 내내 못 쓴다.
- 버전은 플러그인과 같은 `0.6.0-beta-e`로 고정한다(npm `beta` 태그와 같음, `latest`는 0.5.30이라 맞지 않음). 플러그인을 올리면 이 버전도 같이 바꾼다.

| 클라이언트 | 이름 | 등록 위치 | 명령 |
|---|---|---|---|
| Claude Code | `unreal-engine` | `~/.claude.json`의 vBoR 프로젝트(local scope) — **키 두 개 `C:/Git/vBoR`·`c:/Git/vBoR`에 같은 값** | `cmd /c npx -y unreal-engine-mcp-server@0.6.0-beta-e proxy`, env `UE_PROJECT_PATH=C:/Git/vBoR` |
| Codex | `unreal-vbor` | `~/.codex/config.toml`(전역 — 다른 프로젝트 세션에서도 뜨지만 vBoR 토큰이라 vBoR 에디터에만 붙는다) | 같음 |

- 다시 등록할 때: Git Bash에서는 `MSYS_NO_PATHCONV=1`을 먼저 둔다. 안 그러면 `/c`가 `C:/`로 바뀐다(2026-10-11에 한 번 겪음).
- **Claude Code local scope 키는 드라이브 문자의 대소문자를 구분한다.** VSCode 확장은 대화를 `c:\Git\vBoR`(소문자)로 시작하고, 터미널은 `C:\Git\vBoR`로 시작한다. 한쪽 키에만 등록하면 다른 쪽 대화에는 도구가 없다.
  - 2026-10-11 확인: `cmd`에서 `cd /d c:\Git\vBoR` 뒤 `claude mcp get unreal-engine` → "No MCP server". 대문자 경로에서는 "Connected"였다. 그래서 두 키에 다 등록했다.
  - 등록·버전 변경은 두 경로에서 각각 한다: `cmd /c "cd /d c:\Git\vBoR && claude mcp add unreal-engine -s local -e UE_PROJECT_PATH=C:/Git/vBoR -- cmd /c npx -y unreal-engine-mcp-server@<버전> proxy"`, 그리고 `C:\`로 한 번 더.
  - PowerShell `Set-Location`은 경로를 대문자로 바꾸므로 소문자 키 확인에 쓸 수 없다. `cmd`를 쓴다.
- **2026-10-11 확인한 것:**
  - 에디터 안 서버 직접 호출: `initialize`(서버 `unreal-mcp 0.6.0-beta-e`, 세션 ID 발급) → `tools/list`(`unreal` 하나) → `search` → `execute control_actor.list`(현재 레벨 액터 읽기) 성공.
  - proxy 직접 실행: 같은 결과(`[NativeProxy] connected`).
  - Codex `codex exec`: 서버가 뜨고 도구가 보였다. 다만 호출은 "MCP tool call requires approval, but approval policy is never"로 막혔다 — 아래 승인 참고.
  - Claude Code: 등록만 했다. 도구는 새 세션부터 보인다.
- 토큰은 비밀번호처럼 다룬다. 명세·리뷰·커밋·대화에 붙여 넣지 않는다(`Saved/`는 `.gitignore`).

**승인.** 이 서버는 읽기·쓰기를 모두 도구 하나(`unreal`)의 `operation`으로 나눈다. 그래서 클라이언트 설정으로 "읽기만 자동 승인"을 할 수 없다.
- Codex 앱은 호출마다 승인을 묻는다(기본값 그대로 둠). 사용자가 앱에서 보고 승인한다.
- `codex exec`처럼 승인할 수 없는 실행에서는 MCP를 못 쓴다.
- 서버 쪽에서도 삭제 등 62개 기능은 `describe`가 준 `consent`가 있어야 실행된다.

**에디터 상태와 순서.** 작업 폴더 하나를 에디터와 빌드가 같이 쓴다.
- 에디터가 열려 있으면 빌드·자동화 시험·브랜치 전환을 하지 않는다(§2.3).
- MCP는 에디터가 열려 있을 때만 쓸 수 있다.
- 그래서 한 작업 안에서 순서를 지킨다: 코드 단계(에디터 닫힘) → 에디터 단계(에디터 열림, MCP) → 다시 코드가 필요하면 에디터를 닫고 코드 단계로.

**Claude:**
- 읽기는 자유롭게 한다: 레벨 액터·설정 조회(GameMode Override, PlayerStart 높이, 시험대 배치), 출력 로그, 뷰포트 스크린샷, 에셋 존재 확인.
- 리뷰에서는 에디터 쪽 결과(Codex가 만든 레벨·Blueprint)를 MCP로 읽어 확인하고, 그 뒤 사용자에게 에디터를 닫아 달라고 해 빌드·시험을 재현한다.
- 쓰기는 사용자가 그 작업을 요청했을 때만 한다.

**Codex (2026-10-11 사용자 결정: MCP로 개발 가능):**
1. **명세가 에디터 작업을 맡길 때만 쓴다.** 명세의 "만들 것"에 레벨·Blueprint·액터 배치가 Codex 몫으로 적혀 있어야 한다. 그렇지 않으면 MCP는 읽기(확인용)만 한다.
2. **에디터 단계 순서:**
   - C++ 빌드·시험을 먼저 통과시킨다.
   - 에디터를 연다: `UnrealEditor.exe "C:\Git\vBoR\vBoR.uproject"` 백그라운드. `NOT_CONNECTED`가 사라질 때까지 기다린다(이 PC에서 :3000이 뜨기까지 약 20초).
   - MCP로 작업하고 `control_editor.save_all` 또는 `manage_level.save`로 저장한다.
   - 결과를 스크린샷·액터 조회로 확인한다.
   - **에디터를 닫는다.** MCP에는 종료 기능이 없다(`restart_editor`뿐이고 destructive). 저장한 뒤 `taskkill /PID <에디터>`로 닫고, Meta 설문 창 등으로 안 닫히면 `/F`로 닫는다 — 저장했으므로 잃을 것이 없다.
3. **시작할 때 사용자 에디터가 이미 열려 있으면** 빌드하지 않고, MCP로 쓰지도 않는다. 사용자가 저장하지 않은 작업과 부딪힐 수 있다. 그렇게 보고하고 멈춘다.
4. **하지 않는 것:**
   - 에셋 삭제·이름 바꾸기
   - 사용자가 만든 기존 레벨·에셋 수정(명세가 허락한 경우 빼고)
   - 프로젝트 설정(`Config/*.ini`) 변경
   - 삭제용 `consent` 전달
   - MCP의 Python 실행·콘솔 명령으로 위 금지를 우회하는 것
5. **보고:**
   - 보고 ①에 바뀐 `.umap`·`.uasset` 경로를 적는다.
   - ③에 MCP로 한 작업(capability 이름)을 적는다.
   - ⑤에 확인 스크린샷 경로와 사용자가 에디터에서 볼 것을 적는다.
   - `.umap`·`.uasset`은 바이너리라 diff 리뷰가 안 된다. Claude는 MCP로 읽어 확인하고, 사용자가 에디터에서 확인한다.

**공통 — MCP로 할 수 없는 것:** VR·손 추적 시험. PIE 합성 입력은 키보드·마우스뿐이다. 시험 상태 `not-tested` 규칙은 그대로다.

**사용자(UnrealAgent):** 에디터 안 반복 작업(레벨 만들기, GameMode Override 설정 등)에 쓴다. 만든 레벨·에셋은 지금처럼 Claude가 확인하고 커밋한다.

---

## 3. 기록 체계 — 알게 된 것을 어디에 모으나

**세션 대화는 남지 않는다.** claude.ai나 다른 세션에서 한 설명은 다음 세션이 볼 수 없다. 이 리포의 계획서도 원본 엑셀·메일·대화가 리포 밖에 있어서, 지금은 계획서에 요약된 내용만 남아 있다. 그래서 결정·실측·근거는 **그 자리에서 파일로** 남긴다.

| 무엇 | 어디에 | 예 |
|---|---|---|
| 매번 지킬 규칙 | `AGENTS.md` (Codex·Claude 공통 원본, `CLAUDE.md`가 가져온다) | 확정 제약, 작업 방식, 주석 말투 |
| 무엇을 만드는가 | `Docs/Dev-Plan-50pct-VR.md` | 범위, 시나리오 19개, 단계, 시험 T-ID |
| 결정의 근거·조사·실측 | `Docs/Research-Log.md` — 날짜별 절 | 플러그인 헤더 확인, 채택 안 한 대안, 로컬 빌드·에디터 확인 결과 |
| 작업 단위 | `Docs/Specs/<단계>-<이름>.md` | 목적, 범위·범위 밖, API 근거, 완료 조건, 개정 이력 |
| 작업 보고 | 커밋 메시지 본문 ①~⑤ | `git log`로 읽는다 |
| 일하는 방식 | 이 문서 | |
| 사용자 선호(세션 넘어) | Claude 메모리 `~/.claude/projects/<프로젝트>/memory/` | "항상 critical·honest", "Claude는 명세·리뷰" |

원칙:

- **같은 사실을 두 곳에 쓰지 않는다.** 규칙은 AGENTS.md 하나, 근거는 Research-Log 하나. 한쪽이 바뀌면 다른 쪽이 낡는다.
- **Codex와 Claude의 메모리는 공유되지 않는다.** 둘 다 알아야 하는 것은 리포 파일에 쓴다.
- **원본 자료의 위치를 적는다.** 파일을 리포에 넣지 못하더라도 어디에 있는지, 언제 읽었는지를 계획서·조사 기록의 근거 표에 남긴다.
- **실측은 날짜·환경·기기 상태와 함께 적는다.** 예: "헤드셋 미연결 상태에서 확인".

다른 프로젝트의 기록 방식 — 같은 사용자의 프로젝트라 참고할 수 있다:

- **LiftGateStudy** (`C:\UnrealEngineProject\LiftGateStudy\Docs`): `decisions/ADR_NNN_*.md`(결정마다 하나, 선택지 표 + 결과), `lessons_learned/phase*_retrospective.md`(증상·원인·우회·예방·관련 ADR), `phase_kickoff/`(단계별 시작 명세와 체크리스트).
- **GMTCK_MR** (`C:\Git\GMTCK_MR\Docs`): 운영 가이드, 헤드셋 시험 체크리스트, 에디터 후속 작업 목록.

vBoR은 ADR 대신 Research-Log의 날짜별 절에 결정과 근거를 모으고 있다. 결정이 많아지면 ADR로 나누는 것을 검토한다.

---

## 4. LiftGateStudy에서 가져올 교훈

같은 장비 계열(MetaXR + ISDK)로 먼저 진행한 프로젝트다. vBoR에 걸리는 것만 골랐다.

| 교훈 | 내용 | vBoR에서 |
|---|---|---|
| L7 / ADR-004 | 샘플 Pawn(`IsdkSamplePawn`)을 부모로 하는 child Blueprint로 손 리그 이식 단계를 통째로 없앴다 | 검토했으나 vBoR은 ISDK 위 C++ 직접 조립을 택했다(§5.3). "단계 시작 전 샘플 점검이 가치 크다"는 교훈은 그대로 쓴다 |
| L8 | 플러그인 `Content/`의 샘플 Pawn `.uasset`이 바뀐 채 커밋됐는데 의도한 변경인지 불분명했다. 예방: 플러그인 Content는 고치지 않고, 필요하면 프로젝트 `Content/`로 옮겨서 고친다 | §2.6 커밋 전 확인 |
| L2 / ADR-007 | ISDK pinch로 확인 입력을 받았더니 자연스러운 손 동작 중 오인식이 잦았다(특히 신뢰도가 낮을 때). 모든 확인을 button poke로 바꿨다 | P0의 "확정을 poke로 할지 pinch로 할지" 실측에 대한 **선행 근거**. poke 쪽으로 기운다. 다만 vBoR은 마이크로제스처를 허용했으므로 그 오인식은 따로 잰다 |
| L5 | C++ 클래스를 먼저 지우면 그걸 부모로 쓰는 Blueprint가 로드되지 않는다 | P0 끝나고 `AP0ProbePawn`을 지울 때, 그걸 부모로 한 Blueprint가 없는지 먼저 확인 |
| L1 | 대소문자만 다른 경로 변경이 Windows에서 실제 파일 삭제로 이어졌다 | 클라우드(Linux) 세션이 파일 이름을 바꿀 때 주의 |
| L4 | 에디터 C++ 마법사는 모듈 루트에 바로 파일을 만든다 | 폴더 배치는 명세가 정한다(P0-a는 `Source/vBoR/P0Probe/`) |

---

## 5. OculusInteractionSamples 판단 (2026-10-08, 결정 2026-10-09)

사용자가 `Plugins/OculusInteractionSamples/`(Meta ISDK 샘플, 42 MB, 파일 326개)를 추가했다. **결정: 참고 자료와 HandPoseStudio 녹화 도구로만 쓰고, vBoR 부품은 ISDK 위에 C++로 직접 만든다.** 샘플 폴더는 `.gitignore`로 리포에서 뺐다. 필요한 PC에 각자 설치한다.

### 5.1 확인한 것

- 샘플 C++ 모듈이 ISDK 1.205와 함께 빌드된다.
- `PokeExamples`·`TransformerExamples` 맵이 GUI 에디터에서 오류 없이 로드된다.
- 전에 나던 `IsdkInfoCardComponent` CDO 오류(`InfoCardWidget` 누락)가 이 플러그인으로 사라진다.
- 샘플 에셋은 UE 5.6 형식으로 저장돼 있다. ISDK 1.205 에셋도 같다. `VersionName`이 "1.0"뿐이라 1.205용 배포본인지는 확정할 수 없다. **ISDK를 올릴 때 샘플도 함께 바꾼다.**

### 5.2 쓸 것과 쓰지 않을 것

| 샘플 | vBoR에서 | 판단 |
|---|---|---|
| `PokeExamples`, `IsdkPokeInteractable`·`IsdkPokeButtonVisual`·`IsdkInteractableColorVisual`, `PokeFlatButton`·`BigRedButton` | P0 실측: 허공 버튼 맞추기, poke vs pinch, 접촉 피드백(조사 기록 2026-09 §10의 1·2·5) | **가장 쓸모 있다.** 단 "가장 가까운 하나만 활성 + 히스테리시스"(U-04)는 샘플에 없다 |
| `TransformerExamples`의 `IsdkOneGrabRotateTransformer`·`IsdkOneGrabTranslateTransformer` 물체 | P2 문 경첩, 12번 뚜껑·서랍, 16번 폴딩 | 참고 |
| `IsdkSampleRigModifierPokeLimiter` | poke limiter (U-06 필수) | 참고. ISDK `UIsdkPokeLimiterRigModifier`를 상속만 한 빈 Blueprint라 ISDK 클래스를 바로 쓴다 |
| `HandPoseStudio`, Mug·Key·Torch의 GrabPoses | 골프백 손잡이·도어 핸들 잡기 포즈 | **녹화 도구로 쓴다.** 저장 위치는 반드시 vBoR `Content/` 아래. 다이얼 돌리기 같은 동작은 이 도구로 만들 수 없다(정지된 손 모양만 저장) |
| `IsdkResetTransforms`, `IsdkRepositionOnDrop` | P5 자유모드 초기화 | 낮음 |
| `IsdkInstructionDialog` | P3 안내 매체 | 낮음 (안내 매체 미정) |
| `RayExamples`, `DistanceGrabExamples`, `ThrowExamples`, `DistanceGrabbables` | — | **금지** (레이·원거리 잡기·던지기) |
| `IsdkGrabFreeTransformer` (TransformerExamples·DistanceGrabExamples에 섞여 있음) | — | **금지** (자유 6DoF) |
| `IsdkSamplePoseRecognizerComponent`, `IsdkSamplePinchGrabPoseRecognizerComponent`, `HandDetectProfile_ThumbsUp` | — | **금지** (자체 포즈 인식기) |

### 5.3 샘플 Pawn — 그대로 쓰지 않는다

`IsdkSamplePawn`에는 `IsdkRayInteractor`, `IsdkRayGrabDetector`, `IsdkDistanceGrabDetector`, 컨트롤러 리그(`IsdkControllerRigComponent*`)가 들어 있다. 그대로 쓰면 13번 도달성·05번 자세 부담 평가가 무너진다. ISDK 리그(`UIsdkRigComponent`)는 `RayInteraction` 하위 컴포넌트를 기본으로 만든다(`Plugins/MetaXRInteraction/Source/OculusInteractionPrebuilts/Public/Rig/IsdkRigComponent.h:394`). 끄는 방법은 아직 확인하지 않았다.

검토한 방법은 세 가지였고, **C를 택했다(2026-10-09 사용자 결정).**

| 방법 | 장점 | 단점 | |
|---|---|---|---|
| A. 샘플 BP를 부모로 하는 child Blueprint (LiftGateStudy ADR-004 방식) | 가장 빠르다. ISDK 갱신이 자동 반영된다 | 상속받은 레이 컴포넌트를 **지울 수 없고 끄기만** 된다. 샘플이 실행 의존이 된다. Blueprint라 Codex가 고칠 수 없다 | 기각 |
| B. 샘플 BP를 vBoR `Content/`로 복제 | 레이 컴포넌트를 지울 수 있다 | 갱신을 직접 따라가야 한다. 역시 Blueprint | 기각 |
| **C. ISDK 위에 C++로 직접 조립** | 필요한 기능이 전부 ISDK 본체 C++에 있다. 샘플 의존이 없다. Codex가 구현하고 diff로 리뷰할 수 있다 | 샘플 BP 그래프(경계 클리퍼 설정, 상태별 색 전환 등)를 C++로 다시 짜는 비용 | **채택** |

C를 택한 이유: 이 리포는 Codex가 구현하므로 핵심 부품이 `.uasset` 바이너리가 되면 명세·리뷰 흐름에서 빠진다. LiftGateStudy가 A를 택한 이유(ADR-012의 "ISDK C++ API 학습·재작성 부담")는 구현자가 사람일 때의 비용이다.

C++로 만들 때의 조건:

- 레이·레이 잡기·원거리 잡기·컨트롤러 인터랙션을 만들지 않는다. ISDK 리그가 기본으로 만드는 `RayInteraction`을 끄는 방법은 P0-b에서 헤더로 확인한다.
- **Quest 3에서 레이·원거리로 아무것도 선택되지 않는지 확인한다.** 끈 것과 실제로 안 되는 것은 다르다.
- 메시 배치처럼 눈으로 맞추는 조정은 C++ 클래스를 부모로 한 vBoR Blueprint에서 사용자가 한다.
- 부품은 vBoR 모듈 안 한 폴더(예: `Source/vBoR/Interaction/`)에 두고 ISDK·MetaXR에만 의존하게 한다. vBoR 시나리오 코드를 참조하지 않는다. 두 번째 프로젝트가 같은 부품을 필요로 할 때 플러그인으로 옮긴다.

> **정정 기록:** 2026-10-08에 Codex에 "샘플 경로 직접 참조 금지"를 전달했다가, LiftGateStudy ADR-004를 보고 A·B를 열어 두는 것으로 고쳤었다. 최종 결정은 C이므로 결과적으로 샘플을 실행 의존으로 쓰지 않는다.

### 5.4 규칙

- 샘플 플러그인은 커밋하지 않는다(`.gitignore`). 리포의 어떤 코드·에셋도 `/OculusInteractionSamples/` 경로나 `OculusInteractionSamples` 모듈을 참조하지 않는다. 샘플이 없는 PC에서도 vBoR이 빌드·실행돼야 한다.
- `vBoR.Build.cs`에 `OculusInteractionSamples` 의존을 추가하지 않는다.
- 샘플 에셋을 vBoR 레벨에 배치하거나 고치지 않는다. 참고할 때는 샘플 맵을 열어 보기만 한다.
- HandPoseStudio로 녹화한 포즈는 vBoR `Content/` 아래에 저장한다. 포즈 에셋은 ISDK 클래스(`UIsdkHandPoseData`)라 샘플 없이 쓸 수 있다.
- 샘플을 빼면 `IsdkInfoCardComponent` CDO 오류가 로그에 다시 나온다. 쓰지 않는 컴포넌트라 무시한다(§2.5 잡음 목록).
- P0-b(poke limiter·눌리는 버튼·레이 없는 손 리그) 명세는 Claude가 쓴다.
