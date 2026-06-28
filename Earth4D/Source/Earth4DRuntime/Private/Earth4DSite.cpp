// Copyright Earth4D. Licensed for project use.
#include "Earth4DSite.h"

#include "Earth4DSubsystem.h"
#include "EngineUtils.h"          // TActorIterator
#include "Engine/World.h"

// Cesium for Unreal (module: CesiumRuntime). VERIFY: these headers match your
// installed Cesium for Unreal version (paths are stable across 2.x).
#include "CesiumGeoreference.h"
#include "Cesium3DTileset.h"

AEarth4DSite::AEarth4DSite()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AEarth4DSite::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Edit-time: bind to existing actors + align the origin so the region lines up in
	// the viewport, but do NOT spawn from a construction script (unsafe / duplicates).
	EnsureCesiumActors(/*bAllowSpawn=*/false);
}

void AEarth4DSite::BeginPlay()
{
	Super::BeginPlay();
	EnsureCesiumActors(/*bAllowSpawn=*/true);
	// Bind the command layer to this site so element placement + location verbs use Cesium.
	if (UWorld* World = GetWorld())
	{
		if (UEarth4DSubsystem* Sub = World->GetSubsystem<UEarth4DSubsystem>())
		{
			Sub->SetSite(this);
		}
	}
}

void AEarth4DSite::EnsureCesiumActors(bool bAllowSpawn)
{
	UWorld* World = GetWorld();
	if (!World) return;

	// --- Georeference: reuse the level's if present, else (optionally) spawn one. ---
	if (!Georeference)
	{
		for (TActorIterator<ACesiumGeoreference> It(World); It; ++It) { Georeference = *It; break; }
	}
	if (!Georeference && bAllowSpawn)
	{
		FActorSpawnParameters Params;
		Params.Name = TEXT("Earth4DGeoreference");
		Georeference = World->SpawnActor<ACesiumGeoreference>(ACesiumGeoreference::StaticClass(), Params);
	}

	// --- Google tileset: find an existing one, else (optionally) spawn. ---
	if (!GoogleTileset)
	{
		for (TActorIterator<ACesium3DTileset> It(World); It; ++It) { GoogleTileset = *It; break; }
	}
	if (!GoogleTileset && bAllowSpawn)
	{
		FActorSpawnParameters Params;
		Params.Name = TEXT("Earth4DGoogleTiles");
		GoogleTileset = World->SpawnActor<ACesium3DTileset>(ACesium3DTileset::StaticClass(), Params);
	}

	if (Georeference)
	{
		ApplyRegionOriginToGeoreference();
	}
	if (GoogleTileset)
	{
		ConfigureGoogleTileset();
	}
}

void AEarth4DSite::ApplyRegionOriginToGeoreference()
{
	if (!Georeference) return;
	// VERIFY: Cesium for Unreal uses (Longitude, Latitude, Height) ordering in this FVector,
	// and SetOriginLongitudeLatitudeHeight sets OriginPlacement to CartographicOrigin (origin -> UE 0,0,0).
	Georeference->SetOriginLongitudeLatitudeHeight(FVector(OriginLongitude, OriginLatitude, OriginHeight));
}

void AEarth4DSite::SetRegionOrigin(double Lat, double Lon, double Height)
{
	OriginLatitude = Lat;
	OriginLongitude = Lon;
	OriginHeight = Height;
	ApplyRegionOriginToGeoreference();
}

void AEarth4DSite::ConfigureGoogleTileset()
{
	if (!GoogleTileset) return;

	// Bind tileset to our georeference.
	// VERIFY: SetGeoreference exists; otherwise set the `Georeference` UPROPERTY directly.
	GoogleTileset->SetGeoreference(Georeference);

	FString IonToken, GoogleKey;
	GConfig->GetString(TEXT("Earth4D.Tiles"), TEXT("CesiumIonToken"), IonToken, GEngineIni);
	GConfig->GetString(TEXT("Earth4D.Tiles"), TEXT("GoogleMapTilesApiKey"), GoogleKey, GEngineIni);

	// VERIFY: ETilesetSource enum + setter names against your Cesium for Unreal version.
	if (bUseCesiumIon && !IonToken.IsEmpty())
	{
		GoogleTileset->SetTilesetSource(ETilesetSource::FromCesiumIon);
		GoogleTileset->SetIonAssetID(GooglePhotorealisticIonAssetId);
		GoogleTileset->SetIonAccessToken(IonToken);
	}
	else if (!GoogleKey.IsEmpty())
	{
		// Direct Google Map Tiles API key (no Cesium ion dependency).
		GoogleTileset->SetTilesetSource(ETilesetSource::FromUrl);
		GoogleTileset->SetUrl(FString::Printf(
			TEXT("https://tile.googleapis.com/v1/3dtiles/root.json?key=%s"), *GoogleKey));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Earth4D] No tiles key configured. Set [Earth4D.Tiles] "
			"GoogleMapTilesApiKey or CesiumIonToken in DefaultEngine.ini (do not commit keys)."));
	}
}

// ---- Conversions ----------------------------------------------------------

FVector AEarth4DSite::GetRegionOriginUnreal() const
{
	if (!Georeference) return FVector::ZeroVector;
	// VERIFY: param order (Lon, Lat, Height).
	return Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(
		FVector(OriginLongitude, OriginLatitude, OriginHeight));
}

FVector AEarth4DSite::GeodeticToUnreal(double Lat, double Lon, double Height) const
{
	if (!Georeference) return FVector::ZeroVector;
	return Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(FVector(Lon, Lat, Height));
}

bool AEarth4DSite::UnrealToGeodetic(const FVector& UnrealCm, double& OutLat, double& OutLon, double& OutHeight) const
{
	if (!Georeference) return false;
	// VERIFY: returns FVector(Lon, Lat, Height).
	const FVector Llh = Georeference->TransformUnrealPositionToLongitudeLatitudeHeight(UnrealCm);
	OutLon = Llh.X; OutLat = Llh.Y; OutHeight = Llh.Z;
	return true;
}

FMatrix AEarth4DSite::CachedEsuToUnreal() const
{
	if (!Georeference) return FMatrix::Identity;
	// VERIFY: ComputeEastSouthUpToUnrealTransformation returns the transform from the local
	// East-South-Up frame (Cesium is left-handed, hence ESU not ENU) at the given Unreal
	// location to Unreal world space (cm). We use its rotation part for direction transforms.
	return Georeference->ComputeEastSouthUpToUnrealTransformation(GetRegionOriginUnreal());
}

FVector AEarth4DSite::RegionLocalEnuOffsetToUnreal(const FVector& EnuMeters) const
{
	if (!Georeference) return EnuMeters * 100.0; // fallback: naive metres -> cm (matches no-site path)
	// ENU -> ESU (south = -north), metres -> cm, then rotate into Unreal world.
	const FVector EsuCm(EnuMeters.X * 100.0, -EnuMeters.Y * 100.0, EnuMeters.Z * 100.0);
	return CachedEsuToUnreal().TransformVector(EsuCm); // TransformVector => rotation only (ignores translation)
}

FVector AEarth4DSite::RegionLocalEnuToUnreal(const FVector& EnuMeters) const
{
	if (!Georeference) return EnuMeters * 100.0;
	return GetRegionOriginUnreal() + RegionLocalEnuOffsetToUnreal(EnuMeters);
}

FVector AEarth4DSite::UnrealToRegionLocalEnu(const FVector& UnrealCm) const
{
	if (!Georeference) return UnrealCm / 100.0;
	const FVector DeltaCm = UnrealCm - GetRegionOriginUnreal();
	const FVector EsuCm = CachedEsuToUnreal().InverseTransformVector(DeltaCm);
	// ESU cm -> ENU metres (north = -south).
	return FVector(EsuCm.X / 100.0, -EsuCm.Y / 100.0, EsuCm.Z / 100.0);
}
