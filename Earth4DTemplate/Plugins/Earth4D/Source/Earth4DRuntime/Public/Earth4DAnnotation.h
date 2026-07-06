// Copyright Earth4D. Licensed for project use.
// 3D-text site annotation (web app's tags / 3D text). Anchored in region-local ENU
// so it sits on the model regardless of georeference, and gated by a day window so
// callouts appear/disappear with the sequence.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Earth4DDayDriven.h"
#include "Earth4DAnnotation.generated.h"

class UTextRenderComponent;

UCLASS(BlueprintType)
class EARTH4DRUNTIME_API AEarth4DAnnotation : public AActor, public IEarth4DDayDriven
{
	GENERATED_BODY()

public:
	AEarth4DAnnotation();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Earth4D") TObjectPtr<UTextRenderComponent> Label = nullptr;

	/** Anchor in region-local ENU metres (+X=east, +Y=north, +Z=up). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Annotation") FVector EnuMeters = FVector::ZeroVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Annotation") float AppearDay = -1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Annotation") float DisappearDay = -1.f;
	/** When true the label yaws to face the active camera each update. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Annotation") bool bFaceCamera = true;

	UFUNCTION(BlueprintCallable, Category = "Earth4D|Annotation") void SetText(const FString& InText);

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	// IEarth4DDayDriven
	virtual void ApplyDay(float Day, const AEarth4DSite* Site) override;
};
