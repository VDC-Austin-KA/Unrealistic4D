// Copyright Earth4D. Licensed for project use.
// Excavation: a soil block that digs out over a day window (web app's "carve tiles").
// Cesium-free implementation — sinks/hides a soil volume so the pit appears on
// schedule. With Cesium installed, the same window can additionally drive Cesium
// polygon clipping on the tileset (see note in ApplyDay) to carve the photoreal
// ground; that hook is intentionally left for the Cesium-present integration.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Earth4DDayDriven.h"
#include "Earth4DExcavation.generated.h"

class UStaticMeshComponent;

UCLASS(BlueprintType)
class EARTH4DRUNTIME_API AEarth4DExcavation : public AActor, public IEarth4DDayDriven
{
	GENERATED_BODY()

public:
	AEarth4DExcavation();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Earth4D") TObjectPtr<UStaticMeshComponent> Soil = nullptr;

	/** Centre of the pit in region-local ENU metres. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Excavation") FVector EnuMeters = FVector::ZeroVector;
	/** Pit extents (metres). Z is the dig depth. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Excavation") FVector SizeMeters = FVector(20, 20, 5);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Excavation") float StartDay = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Excavation") float Days = 5.f;

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void ApplyDay(float Day, const AEarth4DSite* Site) override;
};
