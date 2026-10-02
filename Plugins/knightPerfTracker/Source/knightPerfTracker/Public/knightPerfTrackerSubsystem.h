#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "knightPerfTrackerSubsystem.generated.h"

struct FknightPerfCapture;

/**
 * Captures frame timings and game counters (knightPerfCounters.h) for one game world.
 *
 * Per frame: real frame time, game/render/RHI thread time, GPU time, used memory (sampled every 30 frames),
 * and every registered counter. On stop it writes a CSV and a summary (avg, p50, p95, p99, max, hitches) to
 * Saved/Profiling/knightPerf, and knight.Perf.Compare turns two summaries into a before/after table.
 *
 * Console (not in Shipping):
 *   knight.Perf.Start [Label] [Seconds] [WarmupSeconds]   Seconds 0 = until Stop
 *   knight.Perf.Stop
 *   knight.Perf.Overlay [0|1]                            on-screen numbers and non-zero counters
 *   knight.Perf.Compare <Before> <After>                 summary paths or labels (newest summary per label)
 *   knight.Perf.HitchMs <ms>                             hitch threshold, default 50
 *
 * Compare like with like: the same map, build configuration and window size, and preferably Standalone or a
 * packaged Development build rather than PIE, whose numbers include the editor. A running capture is saved
 * when the world ends (stopping PIE included).
 */
UCLASS()
class KNIGHTPERFTRACKER_API UknightPerfTrackerSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	UknightPerfTrackerSubsystem();

	//~ USubsystem
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Deinitialize() override;

	//~ FTickableGameObject
	virtual void Tick(float DeltaTime) override;
	virtual bool IsTickable() const override;
	virtual bool IsTickableWhenPaused() const override { return true; }
	virtual TStatId GetStatId() const override;

	/**
	 * Starts a capture, replacing one in progress (which is saved first).
	 * @param DurationSeconds  Stops and saves automatically after this long; 0 runs until StopCapture.
	 * @param WarmupSeconds    Frames in this window are not recorded (shader compiles, streaming, spawn hitches).
	 */
	UFUNCTION(BlueprintCallable, Category="Knight|Perf")
	void StartCapture(const FString& Label, float DurationSeconds = 0.f, float WarmupSeconds = 0.f);

	/** Saves the capture. Returns the summary file path, empty if nothing was recorded. */
	UFUNCTION(BlueprintCallable, Category="Knight|Perf")
	FString StopCapture();

	UFUNCTION(BlueprintPure, Category="Knight|Perf")
	bool IsCapturing() const { return bCapturing; }

	UFUNCTION(BlueprintCallable, Category="Knight|Perf")
	void SetOverlayEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category="Knight|Perf")
	bool IsOverlayEnabled() const { return bOverlayEnabled; }

	static UknightPerfTrackerSubsystem* Get(const UWorld* World);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	void SampleFrame(double Now, float FrameMs);
	void DrawOverlay(float FrameMs) const;

	// Shared pointer so the capture type can stay private to the module.
	TSharedPtr<FknightPerfCapture> Capture;

	bool bCapturing = false;
	bool bOverlayEnabled = false;
	double CaptureStartSeconds = 0.0;
	double SamplingStartSeconds = 0.0;
	double CaptureDuration = 0.0;
	double LastTickSeconds = 0.0;
	float LastMemoryMB = 0.f;
	float SmoothedFrameMs = 0.f;
	uint32 FramesSinceMemorySample = 0;
};
