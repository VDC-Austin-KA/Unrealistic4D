// Copyright Earth4D. Licensed for project use.
#include "Earth4DExcavation.h"
#include "Earth4DSite.h"
#include "Earth4DSubsystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"

AEarth4DExcavation::AEarth4DExcavation()
{
	PrimaryActorTick.bCanEverTick = false;
	Soil = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Soil"));
	RootComponent = Soil;
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) Soil->SetStaticMesh(Cube.Object);
	Soil->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AEarth4DExcavation::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* W = GetWorld())
		if (UEarth4DSubsystem* Sub = W->GetSubsystem<UEarth4DSubsystem>())
		{
			Sub->RegisterDayDriven(this);
			ApplyDay(Sub->CurrentDay, Sub->GetSite());
		}
}

void AEarth4DExcavation::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UWorld* W = GetWorld())
		if (UEarth4DSubsystem* Sub = W->GetSubsystem<UEarth4DSubsystem>())
			Sub->UnregisterDayDriven(this);
	Super::EndPlay(Reason);
}

void AEarth4DExcavation::ApplyDay(float Day, const AEarth4DSite* Site)
{
	// Progress of the dig (0 = full soil, 1 = fully excavated).
	const float P = FMath::Clamp((Day - StartDay) / FMath::Max(1.f, Days), 0.f, 1.f);

	// Place the pit centre on the soil surface (top of the block at ground level).
	const FVector TopEnu = EnuMeters + FVector(0, 0, 0);
	const FVector World = Site ? Site->RegionLocalEnuToUnreal(TopEnu) : TopEnu * 100.f;

	// The engine cube is 100cm and centred; scale to the pit size (metres→cm /100 of 100cm).
	const FVector ScaleFull(SizeMeters.X / 1.0f, SizeMeters.Y / 1.0f, SizeMeters.Z / 1.0f);
	// As the dig proceeds, the remaining soil shrinks downward and sinks out of view.
	const float Remain = 1.f - P;
	Soil->SetWorldScale3D(FVector(ScaleFull.X, ScaleFull.Y, FMath::Max(0.001f, ScaleFull.Z * Remain)));
	// Anchor the top at ground: centre sits half the (shrinking) height below the surface.
	const float HalfDepthCm = 0.5f * SizeMeters.Z * Remain * 100.f;
	SetActorLocation(World - FVector(0, 0, HalfDepthCm));
	SetActorHiddenInGame(P >= 1.f);

	// NOTE (Cesium present): drive CesiumPolygonRasterOverlay / clipping with this pit's
	// footprint + P here to carve the photoreal ground inside the pit outline.
}
