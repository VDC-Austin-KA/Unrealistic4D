// Copyright Earth4D. Licensed for project use.
#include "Earth4DCameraDirector.h"
#include "Earth4DSubsystem.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorViewportClient.h"
#include "LevelEditorViewport.h"
#endif

AEarth4DCameraDirector::AEarth4DCameraDirector()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AEarth4DCameraDirector::AddKey(float Day, FVector Location, FRotator Rotation)
{
	FEarth4DCamKey K; K.Day = Day; K.Location = Location; K.Rotation = Rotation;
	Keys.Add(K);
	Keys.Sort([](const FEarth4DCamKey& A, const FEarth4DCamKey& B) { return A.Day < B.Day; });
}

void AEarth4DCameraDirector::ClearKeys() { Keys.Reset(); }

void AEarth4DCameraDirector::BeginPlay()
{
	Super::BeginPlay();
	if (UWorld* W = GetWorld())
		if (UEarth4DSubsystem* Sub = W->GetSubsystem<UEarth4DSubsystem>())
			Sub->RegisterDayDriven(this);
}

void AEarth4DCameraDirector::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UWorld* W = GetWorld())
		if (UEarth4DSubsystem* Sub = W->GetSubsystem<UEarth4DSubsystem>())
			Sub->UnregisterDayDriven(this);
	Super::EndPlay(Reason);
}

bool AEarth4DCameraDirector::PoseAtDay(float Day, FVector& OutLoc, FRotator& OutRot) const
{
	if (Keys.Num() == 0) return false;
	if (Keys.Num() == 1 || Day <= Keys[0].Day) { OutLoc = Keys[0].Location; OutRot = Keys[0].Rotation; return true; }
	const FEarth4DCamKey& Last = Keys.Last();
	if (Day >= Last.Day) { OutLoc = Last.Location; OutRot = Last.Rotation; return true; }
	for (int32 i = 0; i < Keys.Num() - 1; ++i)
	{
		const FEarth4DCamKey& A = Keys[i];
		const FEarth4DCamKey& B = Keys[i + 1];
		if (Day >= A.Day && Day <= B.Day)
		{
			const float T = (Day - A.Day) / FMath::Max(KINDA_SMALL_NUMBER, B.Day - A.Day);
			const float S = FMath::SmoothStep(0.f, 1.f, T); // ease the cut between viewpoints
			OutLoc = FMath::Lerp(A.Location, B.Location, S);
			OutRot = FQuat::Slerp(A.Rotation.Quaternion(), B.Rotation.Quaternion(), S).Rotator();
			return true;
		}
	}
	return false;
}

void AEarth4DCameraDirector::ApplyDay(float Day, const AEarth4DSite* /*Site*/)
{
	if (!bDriveView) return;
	FVector Loc; FRotator Rot;
	if (PoseAtDay(Day, Loc, Rot)) DriveView(Loc, Rot);
}

void AEarth4DCameraDirector::DriveView(const FVector& Loc, const FRotator& Rot)
{
	UWorld* World = GetWorld();
	if (World && World->IsGameWorld())
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0))
		{
			if (APawn* Pawn = PC->GetPawn())
				Pawn->SetActorLocation(Loc, false, nullptr, ETeleportType::TeleportPhysics);
			PC->SetControlRotation(Rot);
		}
		return;
	}
#if WITH_EDITOR
	if (GEditor)
	{
		if (FLevelEditorViewportClient* VC = GCurrentLevelEditingViewportClient
			? GCurrentLevelEditingViewportClient
			: (GEditor->GetLevelViewportClients().Num() > 0 ? GEditor->GetLevelViewportClients()[0] : nullptr))
		{
			VC->SetViewLocation(Loc);
			VC->SetViewRotation(Rot);
			VC->Invalidate();
		}
	}
#endif
}
