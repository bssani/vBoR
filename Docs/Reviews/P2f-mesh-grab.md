# P2f 메시 충돌로 잡기 — 리뷰

명세: [P2f-mesh-grab.md](../Specs/P2f-mesh-grab.md) · 브랜치: `codex/P2f-mesh-grab` · 베이스: `main@110be72`

상태: **1회차 리뷰 — 통과. 문제 없음, 열린 항목 없음. 머지 승인 대기.**

## 리뷰 1회차 (2026-10-21, Claude — `ffeaaeb`)

**결과: 통과.** D1–D6이 명세대로 들어갔고 범위 밖 변경은 없다.
- 에디터·에셋·Config·플러그인·근사 차는 바뀌지 않았다.
- 기존 시험 파일은 하나도 바뀌지 않았다(§3 감사 목록의 단언 그대로).
- `vBoR.Build.cs`에 엔진 모듈 `PhysicsCore`를 더했다. `UBodySetupCore::GetCollisionTraceFlag`가 `PHYSICSCORE_API`라서 필요하다(`PhysicsCore/Public/BodySetupCore.h:23`). 플러그인 수정이 아니다.
- Codex는 보고를 커밋 본문과 HANDOFF에만 썼고, 리뷰 파일을 만들지 않았다.

**재현(`ffeaaeb`).** 에디터가 닫힌 것을 확인했고, 단계마다 에디터가 다시 열렸는지 보며 돌렸다.
- non-unity(`-DisableUnity -Module=vBoR`): 성공, vBoR 경고·오류 0. 로그 `Saved/Logs/ReviewP2f-NonUnity.log`.
- unity: 성공, vBoR 경고·오류 0. 로그 `Saved/Logs/ReviewP2f-Unity.log`.
- `vBoR.*`: 459/459(P2f 10개 포함), `TEST COMPLETE. EXIT CODE: 0`. 로그 `Saved/Logs/ReviewP2f-All.log`.

**Codex 토큰:** 3.61M(캐시 96%), 2026-10-21 01:15–01:23 UTC(KST 10:15–10:23), 약 8분. 하위 에이전트 0, 빌드 실패·재실행 없음.

**확인한 것:**
- **공통 함수**(`Interaction/VBoRGrabCollider.cpp`의 `ConfigureVBoRGrabCollider`):
  - 판정은 simple 요소 수 > 0 또는 `CTF_UseComplexAsSimple`이다(D3). 짐의 `HasSimpleCollision`(`VBoRCarryObject.cpp:169-173`)에 complex-as-simple을 더한 것이다.
  - 쓰는 충돌만 `bGrabValid`일 때 켠다. 메시는 `QueryOnly`, `ECC_WorldDynamic`, 모든 채널 `Ignore`, `WorldDynamic`만 `Overlap`이다(D2). 상자는 반대로 끈다. 잘못된 설정이면 둘 다 꺼진다(D5).
  - 피벗 경고는 `MeshRestFromRoot` 위치 크기가 1 cm 이하일 때다(D4). E7이 0·1·1.01 cm로 경계를 본다.
- **네 부모의 호출 자리**는 명세가 짚은 줄(`AxisPart.cpp:192`·`SteeringWheel.cpp:85`·`TwoAxisPart.cpp:78`·`Knob.cpp:76`)이다.
  - 미러의 `GrabBox->SetVisibility(bValid,true)`는 상자에만 남았다. E8이 미러 메시가 계속 보이는지 본다.
  - 휠 반지름 검사, 레버 밀기(메시 bounds), overlay는 손대지 않았다(D6).
- **메시가 `WorldDynamic` 질의에 보이게 되는 영향.** 상자도 같은 응답이었으므로 새로 보이는 종류는 없다. 자리만 상자에서 메시로 바뀐다. 다른 질의도 다 걸러진다.
  - 짐의 쓸기·겹침은 모두 막힘만 센다: `VBoRCarryContact.cpp:32`·`:59`·`:101`, `VBoRCarryLanding.cpp:85`, `VBoRCarryObject.cpp:187`·`:193`·`:225`.
  - 리프트게이트 끼임 검사는 짐 메시만 고른다(`VBoRPoweredPanel.cpp:145-150`).
- **시험**은 실제 ISDK grabber로 hover와 `PalmGrab` 선택을 본다.
  - E3: 반지름 20 cm 구의 bounds 모서리 (18,18,98)는 표면에서 약 11 cm 떨어져 있다. 손바닥 구가 4 cm이므로 메시에 닿지 않는다.
  - E4: 원점 거리 4/6 cm에서 A/B를 고른다.
  - E6: 복제 Cube에 `CreatePhysicsMeshes`로 삼각형 충돌을 만들고, 실제 hover와 선택까지 통과한다. 로컬 확인으로 넘긴 항목이 없다.

### 참고 (반영 불필요 — 열린 항목 아님)

1. **메시가 없거나 설정이 잘못일 때 경고가 사실과 다르다.** 켬 상태에서 메시가 없으면 "No part mesh; grabbing disabled"와 함께 "grab mesh has no simple collision …; using GrabBox"가 찍힌다. 그런데 상자도 꺼져 있다. 메시는 있지만 걸림·한계 설정이 잘못되고 충돌도 없을 때도 같다.
   - 메시가 상자로 돌아가면 피벗 경고도 의미가 없다. 상자 원점은 bounds 가운데이기 때문이다.
   - 잡기는 바르게 꺼지고 경고 수도 시험과 맞는다. 문장만 오해를 부른다. 이 함수를 다음에 고칠 때 메시가 없거나 `bGrabValid`가 거짓이면 충돌 검사를 건너뛰고, 피벗 경고는 메시를 쓸 때만 내면 된다.
2. **Play 때 `PartMesh`(휠·미러·노브 메시도) 충돌을 항상 덮어쓴다.** 옵션이 꺼져 있으면 `NoCollision`, 켜져 있으면 D2 설정이다. 그래서 Blueprint에서 메시 Collision Presets를 바꿔도 Play에서는 쓰이지 않는다. D2의 뜻과 같다. 회사 Blueprint에서는 메시 충돌 설정은 두고 옵션만 켜면 된다. 머지 뒤 Blueprint-Guide에 적는다.
3. E2의 "Runtime option change" 단언은 `Tick`이 다시 설정하지 않는다는 것만 본다. D1은 `PreInitializeComponents`에서 한 번 읽는 것이라 이것으로 충분하다.
4. `VBoRGrabCollider.cpp`의 주석 "(P2f D2, UE cm)"에서 "UE cm"은 단위가 없는 줄이라 뜻이 없다. 사소하다.

### 질문

없음.
