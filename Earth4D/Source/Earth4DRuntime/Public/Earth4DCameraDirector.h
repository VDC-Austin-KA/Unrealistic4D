// Copyright Earth4D. Licensed for project use.
// Film / camera keyframes tied to the construction day (web app's film/camera path).
// A lightweight alternative to a full LevelSequence: capture viewpoints at days, then
// PlayFilm drives the active view along the interpolated path as the schedule plays —
// a recordable fly-through of the build. (Movie Render Queue can capture the result.)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Earth4DDayDriven.h"
#include "Earth4DCameraDirector.generated.h"

USTRUCT(BlueprintType)
struct FEarth4DCamKey
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float Day = 0.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FVector Location = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FRotator Rotation = FRotator::ZeroRotator;
};

UCLASS(BlueprintType)
class EARTH4DRUNTIME_API AEarth4DCameraDirector : public AActor, public IEarth4DDayDriven
{
	GENERATED_BODY()

public:
	AEarth4DCameraDirector();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Film") TArray<FEarth4DCamKey> Keys;
	/** While true, ApplyDay drives the active camera/viewport along the keyframes. */
	UPROPERTY(BlueprintReadOnly, Category = "Earth4D|Film") bool bDriveView = false;

	UFUNCTION(BlueprintCallable, Category = "Earth4D|Film") void AddKey(float Day, FVector Location, FRotator Rotation);
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Film") void ClearKeys();
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Film") void SetDriveView(bool bEnable) { bDriveView = bEnable; }
	int32 NumKeys() const { return Keys.Num(); }

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void ApplyDay(float Day, const AEarth4DSite* Site) override;

private:
	/** Interpolate the camera pose at a day (clamped to the key range). */
	bool PoseAtDay(float Day, FVector& OutLoc, FRotator& OutRot) const;
	/** Drive the active PIE player or editor viewport to a pose. */
	void DriveView(const FVector& Loc, const FRotator& Rot);
};
