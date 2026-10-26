#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Camera/knightRadialPullMath.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FKnightRadialPullMathTest, "Knight.Camera.RadialPull.EnvelopeAndProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FKnightRadialPullMathTest::RunTest(const FString& Parameters)
{
	using namespace KnightRadialPull;
	TestEqual(TEXT("start is neutral"), Envelope(0, .22f, .035f, .185f), 0.f);
	TestTrue(TEXT("attack reaches peak"), FMath::IsNearlyEqual(Envelope(.035, .22f, .035f, .185f), 1.f));
	TestEqual(TEXT("complete restoration"), Envelope(.23, .22f, .035f, .185f), 0.f);
	TestEqual(TEXT("zero duration disabled"), Envelope(0, 0, .035f, .185f), 0.f);
	TestEqual(TEXT("oversized segments respect total duration"), Envelope(.23, .22f, 1.f, 1.f), 0.f);
	TestTrue(TEXT("repeat starts at existing weight"), FMath::IsNearlyEqual(Envelope(0, .22f, .035f, .185f, .6f), .6f));
	for (int32 I=0; I<240; ++I)
		TestTrue(TEXT("pulse bounded; no stacking"), Envelope(I*.001, .22f, .035f, .185f, .6f) >= 0.f
			&& Envelope(I*.001, .22f, .035f, .185f, .6f) <= 1.f);
	TestTrue(TEXT("center projection"), ProjectFocus(FVector::ZeroVector,FMatrix::Identity).Equals(FVector2D(.5,.5)));
	TestTrue(TEXT("off-center NDC, no desktop offsets"), ProjectFocus(FVector(.4,.2,0),FMatrix::Identity).Equals(FVector2D(.7,.4)));
	TestTrue(TEXT("offscreen fallback"), ProjectFocus(FVector(5,0,0),FMatrix::Identity).Equals(FVector2D(.5,.5)));
	FMatrix Behind=FMatrix::Identity; Behind.M[3][3]=-1;
	TestTrue(TEXT("behind-camera fallback"), ProjectFocus(FVector::ZeroVector,Behind).Equals(FVector2D(.5,.5)));
	return true;
}
#endif
