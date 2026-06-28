// Copyright Earth4D. Licensed for project use.
// Spline-driven vehicle/equipment (web app's vehicles + routes + traffic). Follows a
// route (its USplineComponent) across a day window; points are authored in
// region-local ENU so the route stays put on the model. Chat/MCP spawn a simple
// two-point straight route; in-editor you can add spline points for curved paths.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Earth4DDayDriven.h"
#include "Earth4DVehicle.generated.h"

class UStaticMeshComponent;
class USplineComponent;

UCLASS(BlueprintType)
class EARTH4DRUNTIME_API AEarth4DVehicle : public AActor, public IEarth4DDayDriven
{
	GENERATED_BODY()

public:
	AEarth4DVehicle();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Earth4D") TObjectPtr<USplineComponent> Route = nullptr;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Earth4D") TObjectPtr<UStaticMeshComponent> Body = nullptr;

	/** Route waypoints in region-local ENU metres (authoring source of truth). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Vehicle") TArray<FVector> RouteEnuMeters;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Vehicle") float StartDay = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Vehicle") float Days = 5.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Vehicle") bool bLoop = false;
	/** Hide the vehicle outside its day window (off for permanent traffic). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Vehicle") bool bHideOutsideWindow = true;

	/** Rebuild the spline from RouteEnuMeters using the site's ENU→world conversion. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Vehicle") void RebuildRoute(const AEarth4DSite* Site);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void ApplyDay(float Day, const AEarth4DSite* Site) override;

private:
	bool bRouteBuilt = false;
};
