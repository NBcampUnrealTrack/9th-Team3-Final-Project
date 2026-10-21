잔향 V2 · 연결된 날개 기류 후보 — 2026-10-19

프로젝트: D:\test\AnimalRT\AnimalRT.uproject
에셋 폴더: /Game/Knight/Creatures/Janhyang/VFX

현재 저장 상태
BP_Janhyang_Resonance와 L_Janhyang_ResonancePreview에 연결된 바람 외피를 적용했다.
날개당 하나씩, 총 두 개의 연결된 곡면이 원래 날개 뼈를 따라 변형된다.
표면의 연속 UV를 따라 바람 무늬가 흐르고, 가장자리와 접촉 부분은 투명하게 사라진다.
이번 후보는 Skeletal Mesh + Material 방식이다. Niagara 입자 발생 방식이나 실제 유체 시뮬레이션이 아니다.
기존 12개 Niagara 조각 발생기는 메인 BP에서 비활성화하고 숨겼다. 삭제하지 않았다.
사용자에게 검토를 위해 제시하는 후보이며, 외형 만족 여부까지 확정된 것으로 기록하지 않는다.

몸 머티리얼
두 비교 캐릭터의 몸/날개에는 기존 M_Janhyang_V2를 그대로 사용한다.
M_Janhyang_Resonance, MI_Janhyang_Normal, MI_Janhyang_Charged는 보관만 하며 적용하지 않는다.
원래 메시 슬롯, 원본 머티리얼/텍스처/애니메이션을 변경하지 않았다.
가슴 앞 와이파이 형태, 불꽃, 스파크는 추가하지 않았다.

열어 볼 곳
VFX/Maps/L_Janhyang_ResonancePreview: 왼쪽 공명(1), 오른쪽 기본(0). Play로 기류와 날갯짓 확인.
VFX/Blueprints/BP_Janhyang_Resonance: 재사용용 비교 캐릭터 BP.
VFX/Meshes/SK_Janhyang_WindShell: 두 날개를 감싸는 별도 효과용 메시.
VFX/Meshes/SKEL_Janhyang_WindShell: 효과 메시 전용 스켈레톤 복제본.
VFX/Materials/M_Janhyang_ContinuousWind: 연속 기류 전용 머티리얼.
VFX/Textures/T_Janhyang_WindVeil: 기류 무늬 텍스처.
VFX/Archive/BP_Janhyang_FragmentWind: 직전 12개 조각 기류 비교용 BP.
이전 NS_Janhyang_WingEnvelope, NS_Janhyang_WingFlow 및 해당 머티리얼은 보존했다.

설정과 동작
ResonanceStrength 0: 회청색 기본 기류. 1: 파란 공명 기류.
SetResonanceStrength(0~1)로 실행 중 부드럽게 전환한다. TransitionSpeed 기본값은 3.5다.
새 WingShell 컴포넌트의 Custom Primitive Data 0번이 기류의 공명 강도를 전달한다.
WingShell은 원래 SkeletalMeshComponent0을 Leader Pose로 사용한다. 상대 위치/회전은 0, 크기는 1이다.
게임 AI의 공명해방 상태에서 SetResonanceStrength를 호출하는 연결은 아직 별도 작업이다.
표준 UE Material/Blueprint/스킨 메시 기능을 사용한다. 실행용 C++/Python/Custom HLSL 의존성은 없다.
Python은 제작 및 검증 자동화에만 사용했다.

검증한 범위
Checks/continuous_shell_geometry.json: 연결 곡면 2개, 정점 5856개, 사각면 5760개, 뼈 84개, 정점당 최대 8개 웨이트.
Checks/continuous_shell_import.json: 원래 메시와 효과 메시의 실제 기준 자세를 비교했다. 위치 오차 0cm, 회전 오차 약 0.0000062도 이내.
Checks/continuous_shell_blueprint.json: BP 함수 그래프 컴파일과 비교 맵의 새 컴포넌트 배치 확인.
Checks/continuous_shell_runtime.json: 저장 후 새 Unreal 프로세스의 PIE에서 0→1 전환, 6개 날개 뼈 추종, 기존 조각 비활성, 원래 몸 머티리얼 확인.
재생 중 측정한 본 위치 추종 오차 0cm. 날개 끝 움직임 약 220cm 확인.
원본 에셋 54개 SHA-256 동일. Blender 원본 파일도 SHA-256 동일.
기본 비행 대기 동작에서 검증했다. 전체 48개 애니메이션의 모든 메시 변형이나 패키징까지 검증한 것은 아니다.
원본 M_Janhyang_V2의 SkeletalMesh usage 플래그 경고는 원본 변경 없이 남겨 두었다. 패키징 전 별도 점검 대상이다.

실제 엔진 화면
Previews/09_Continuous_Wind.png: 기본/공명 정면 비교.
Previews/10_Continuous_Wind_Detail.png: 근접 사선 비교.
Previews/11_Continuous_Wind_Motion.gif: 실제 Unreal 화면 60장을 연결한 약 3fps 샘플. 부드러운 원래 움직임은 비교 맵의 Play에서 확인한다.
초기 연속 캡처의 애니메이션 정지 문제를 발견해 캡처 간격과 재생 상태를 보정했다. 최종 캡처는 게임 시간 진행과 날개 끝 이동 약 172cm를 확인했다.
07~08은 이전 조각 기류, 01~06은 더 이전 머티리얼/리본 시험 화면으로 현재 상태와 구분한다.

보존과 다시 보기
Archive_Before_ContinuousShell: 직전 VFX 폴더 전체 보관본.
Archive_Before_WingEnvelope: 더 이전 리본/BP/맵 보관본.
Archive_Before_FlowVisibility: 초기 리본 보완 전 보관본.
Source/Janhyang_ContinuousWind.blend: 효과 외피 제작용 별도 Blender 파일.
Source/SK_Janhyang_WindShell.fbx: 효과 외피만 내보낸 파일.
기존 Janhyang_V2_Animations.blend는 읽기만 했고 저장하지 않았다.
이미 열려 있는 AnimalRT에는 이전 에셋이 남을 수 있다. 재생을 멈추고 변경 에셋을 다시 로드하거나 프로젝트를 다시 열어 확인한다.
현재 제작 진입점: tools/build_continuous_shell_blender.py → tools/create_continuous_shell.py.
현재 검증 진입점: tools/verify_continuous_shell.py. 이전 build_blueprint.py를 단독 실행하면 예전 리본 구성이 복원되므로 실행하지 않는다.
