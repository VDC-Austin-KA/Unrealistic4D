// Copyright Earth4D. Licensed for project use.
#include "Earth4DAnnotation.h"
#include "Earth4DSite.h"
#include "Earth4DSubsystem.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

AEarth4DAnnotation::AEarth4DAnnotation()
{
	PrimaryActorTick.bCanEverTick = false;
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("Label"));
	RootComponent = Label;
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetWorldSize(120.f);                  // cm-tall text, legible on-site
	Label->SetTextRenderColor(FColor(235, 240, 250));
}

void AEarth4DAnnotation::SetText(const FString& InText)
{
	if (Label) Label->SetText(FText::FromString(InText));
}

void AEarth4DAnnotation::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* W = GetWorld())
		if (UEarth4DSubsystem* Sub = W->GetSubsystem<UEarth4DSubsystem>())
		{
			Sub->RegisterDayDriven(this);
			ApplyDay(Sub->CurrentDay, Sub->GetSite());
		}
}

void AEarth4DAnnotation::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UWorld* W = GetWorld())
		if (UEarth4DSubsystem* Sub = W->GetSubsystem<UEarth4DSubsystem>())
			Sub->UnregisterDayDriven(this);
	Super::EndPlay(Reason);
}

void AEarth4DAnnotation::ApplyDay(float Day, const AEarth4DSite* Site)
{
	const bool bVisible =
		(AppearDay < 0.f || Day >= AppearDay) &&
		(DisappearDay < 0.f || Day < DisappearDay);
	SetActorHiddenInGame(!bVisible);
	if (Label) Label->SetVisibility(bVisible);
	if (!bVisible) return;

	const FVector World = Site ? Site->RegionLocalEnuToUnreal(EnuMeters) : EnuMeters * 100.f;
	SetActorLocation(World);

	if (bFaceCamera)
	{
		FVector CamLoc; bool bHaveCam = false;
		if (UWorld* W = GetWorld())
			if (APlayerController* PC = UGameplayStatics::GetPlayerController(W, 0))
				if (PC->PlayerCameraManager) { CamLoc = PC->PlayerCameraManager->GetCameraLocation(); bHaveCam = true; }
		if (bHaveCam)
		{
			FRotator Face = (CamLoc - World).Rotation();
			Face.Pitch = 0.f; Face.Roll = 0.f;
			// Text faces +X; rotate so its front points at the camera.
			SetActorRotation(Face + FRotator(0.f, 180.f, 0.f));
		}
	}
}
