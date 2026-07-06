// Copyright Earth4D. Licensed for project use.
#include "Earth4DVehicle.h"
#include "Earth4DSite.h"
#include "Earth4DSubsystem.h"
#include "Components/SplineComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"

AEarth4DVehicle::AEarth4DVehicle()
{
	PrimaryActorTick.bCanEverTick = false;
	Route = CreateDefaultSubobject<USplineComponent>(TEXT("Route"));
	RootComponent = Route;
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Route);
	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	if (Cube.Succeeded()) Body->SetStaticMesh(Cube.Object);
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetWorldScale3D(FVector(3.0f, 1.6f, 1.4f)); // truck-ish proportions (×100cm)
}

void AEarth4DVehicle::RebuildRoute(const AEarth4DSite* Site)
{
	if (!Route) return;
	Route->ClearSplinePoints(false);
	for (const FVector& Enu : RouteEnuMeters)
	{
		const FVector World = Site ? Site->RegionLocalEnuToUnreal(Enu) : Enu * 100.f;
		Route->AddSplinePoint(World, ESplineCoordinateSpace::World, false);
	}
	Route->UpdateSpline();
	bRouteBuilt = RouteEnuMeters.Num() >= 2;
}

void AEarth4DVehicle::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* W = GetWorld())
		if (UEarth4DSubsystem* Sub = W->GetSubsystem<UEarth4DSubsystem>())
		{
			Sub->RegisterDayDriven(this);
			RebuildRoute(Sub->GetSite());
			ApplyDay(Sub->CurrentDay, Sub->GetSite());
		}
}

void AEarth4DVehicle::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UWorld* W = GetWorld())
		if (UEarth4DSubsystem* Sub = W->GetSubsystem<UEarth4DSubsystem>())
			Sub->UnregisterDayDriven(this);
	Super::EndPlay(Reason);
}

void AEarth4DVehicle::ApplyDay(float Day, const AEarth4DSite* Site)
{
	if (!bRouteBuilt) RebuildRoute(Site);
	if (!Route || Route->GetNumberOfSplinePoints() < 2)
	{
		if (Body) Body->SetVisibility(false);
		return;
	}

	float T = (Day - StartDay) / FMath::Max(KINDA_SMALL_NUMBER, Days); // 0..1 across the window
	const bool bInWindow = (T >= 0.f && T <= 1.f);
	if (bLoop) T = T - FMath::FloorToFloat(T);
	else       T = FMath::Clamp(T, 0.f, 1.f);

	const bool bVisible = bLoop || !bHideOutsideWindow || bInWindow;
	if (Body) Body->SetVisibility(bVisible);
	if (!bVisible) return;

	const float Dist = T * Route->GetSplineLength();
	const FVector Loc = Route->GetLocationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);
	const FRotator Rot = Route->GetRotationAtDistanceAlongSpline(Dist, ESplineCoordinateSpace::World);
	if (Body)
	{
		Body->SetWorldLocation(Loc);
		Body->SetWorldRotation(FRotator(0.f, Rot.Yaw, 0.f));
	}
}
