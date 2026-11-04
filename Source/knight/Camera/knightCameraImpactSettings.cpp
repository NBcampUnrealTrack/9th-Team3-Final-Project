#include "Camera/knightCameraImpactSettings.h"

#include "Camera/CameraShakeBase.h"

UknightCameraImpactSettings::UknightCameraImpactSettings()
{
	// 처음 만든 에셋이 바로 차이를 보이도록 순간마다 약한 기본값을 둔다. 실제 값은 튜닝 탭에서 맞춘다.
	PlayerHit.bEnabled = true;
	PlayerHit.FOVPunch = 1.5f;
	PlayerHit.Push = 6.f;
	PlayerHit.AttackTime = 0.03f;
	PlayerHit.HoldTime = 0.03f;
	PlayerHit.ReleaseTime = 0.18f;

	// 강공격 3~4단(그로기 피해 31~38, 양손이면 1.2배)부터 등 뒤로 당긴다. 반격·내려찍기(60 이상)는 최대.
	PlayerHeavyHit.bEnabled = true;
	PlayerHeavyHit.MinStrength = 32.f;
	PlayerHeavyHit.FullStrength = 70.f;
	PlayerHeavyHit.MinScale = 0.6f;
	PlayerHeavyHit.FOVPunch = 5.f;
	PlayerHeavyHit.PullIn = 0.35f;
	PlayerHeavyHit.ShoulderOffset = FVector2D(25.f, -5.f);
	PlayerHeavyHit.ChromaticAberration = 1.5f;
	PlayerHeavyHit.AttackTime = 0.06f;
	PlayerHeavyHit.HoldTime = 0.12f;
	PlayerHeavyHit.ReleaseTime = 0.45f;
	PlayerHeavyHit.Cooldown = 0.5f;

	Damaged.bEnabled = true;
	Damaged.FOVPunch = 2.f;
	Damaged.Push = 12.f;
	Damaged.ChromaticAberration = 0.8f;
	Damaged.AttackTime = 0.03f;
	Damaged.HoldTime = 0.05f;
	Damaged.ReleaseTime = 0.25f;

	PoiseBroken.bEnabled = true;
	PoiseBroken.FOVPunch = 4.f;
	PoiseBroken.Push = 20.f;
	PoiseBroken.ChromaticAberration = 2.f;
	PoiseBroken.HoldTime = 0.1f;
	PoiseBroken.ReleaseTime = 0.4f;

	Guarded.bEnabled = true;
	Guarded.FOVPunch = 1.f;
	Guarded.Push = 6.f;
	Guarded.AttackTime = 0.03f;
	Guarded.HoldTime = 0.03f;
	Guarded.ReleaseTime = 0.2f;

	Parried.bEnabled = true;
	Parried.FOVPunch = 3.f;
	Parried.PullIn = 0.15f;
	Parried.ChromaticAberration = 1.2f;
	Parried.AttackTime = 0.04f;
	Parried.HoldTime = 0.1f;
	Parried.ReleaseTime = 0.35f;

	GuardBroken.bEnabled = true;
	GuardBroken.FOVPunch = 3.f;
	GuardBroken.Push = 15.f;
	GuardBroken.ChromaticAberration = 1.5f;
	GuardBroken.ReleaseTime = 0.35f;

	MonsterImpact.bEnabled = true;
	MonsterImpact.MinStrength = 0.3f;
	MonsterImpact.FullStrength = 1.5f;
	MonsterImpact.FOVPunch = -2.f;
	MonsterImpact.ChromaticAberration = 0.5f;
	MonsterImpact.AttackTime = 0.04f;
	MonsterImpact.HoldTime = 0.05f;
	MonsterImpact.ReleaseTime = 0.4f;
	// MonsterImpactShake(CS_Monster_GroundImpact)는 에셋에서 지정한다.
}
