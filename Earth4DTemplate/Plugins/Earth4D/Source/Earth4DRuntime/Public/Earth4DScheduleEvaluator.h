// Copyright Earth4D. Licensed for project use.
// Port of the web app's fourd/animationEngine.ts (evaluateSchedule + styleState).
// Pure functions: given the schedule and a day, produce per-element states.
#pragma once

#include "CoreMinimal.h"
#include "Earth4DTypes.h"

class UEarth4DSchedule;

namespace Earth4DScheduleEvaluator
{
	/** Per-element animation state for every element at `Day`. */
	EARTH4DRUNTIME_API void Evaluate(const UEarth4DSchedule& Schedule, float Day, TMap<FString, FEarth4DObjectState>& OutStates);

	/** Style → element state at progress p in [0,1] (0 = not present, 1 = built). */
	EARTH4DRUNTIME_API FEarth4DObjectState StyleState(
		EEarth4DAnimStyle Style, EEarth4DDirection Direction, float P,
		const FEarth4DElement& Element, float Distance);
}
