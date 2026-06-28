// Copyright Earth4D. Licensed for project use.
// The contract for anything whose state is a function of the construction day:
// vehicles moving along routes, annotations appearing, excavation progressing, the
// film camera. They register with UEarth4DSubsystem and are ticked from the SAME
// EvaluateAndApply pass as scheduled elements, so the whole scene stays on one clock.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Earth4DDayDriven.generated.h"

class AEarth4DSite;

UINTERFACE(MinimalAPI, BlueprintType)
class UEarth4DDayDriven : public UInterface
{
	GENERATED_BODY()
};

class IEarth4DDayDriven
{
	GENERATED_BODY()

public:
	/** Update this object's state for the given construction day. Site may be null. */
	virtual void ApplyDay(float Day, const AEarth4DSite* Site) = 0;
};
