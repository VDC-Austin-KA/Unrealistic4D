// Copyright Earth4D. Licensed for project use.
#include "Earth4DSite.h"
#include "Earth4DSubsystem.h"
#include "Earth4DSettings.h"
#include "Earth4DGoogleTiles.h"
#include "EngineUtils.h"          // TActorIterator
#include "Engine/World.h"

#if WITH_EARTH4D_CESIUM
// Cesium for Unreal (module: CesiumRuntime). Only compiled when the plugin is present.
#include "CesiumGeoreference.h"
#include "Cesium3DTileset.h"
#endif

namespace
{
	// WGS84 ellipsoid constants.
	constexpr double WGS84_A = 6378137.0;                 // semi-major axis (m)
	constexpr double WGS84_F = 1.0 / 298.257223563;       // flattening
	const double WGS84_E2 = WGS84_F * (2.0 - WGS84_F);    // first eccentricity squared
}

AEarth4DSite::AEarth4DSite()
{
	PrimaryActorTick.bCanEverTick = false;
	RecomputeLocalFrame();
}

void AEarth4DSite::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RecomputeLocalFrame();
	// Edit-time: bind to existing backend actors + align the origin, but never spawn
	// from a construction script (unsafe / would duplicate).
	EnsureTilesBackend(/*bAllowSpawn=*/false);
}

void AEarth4DSite::BeginPlay()
{
	Super::BeginPlay();
	RecomputeLocalFrame();
	EnsureTilesBackend(/*bAllowSpawn=*/true);
	if (UWorld* World = GetWorld())
		if (UEarth4DSubsystem* Sub = World->GetSubsystem<UEarth4DSubsystem>())
			Sub->SetSite(this);
}

// ---- Backend (Cesium when present, native Google tiles otherwise) ----------

void AEarth4DSite::EnsureTilesBackend(bool bAllowSpawn)
{
	UWorld* World = GetWorld();
	if (!World) return;
	ConfigureTiles(bAllowSpawn);
	ApplyRegionOrigin();
}

void AEarth4DSite::ConfigureTiles(bool bAllowSpawn)
{
	UWorld* World = GetWorld();
	if (!World) return;

	const UEarth4DSettings* Settings = UEarth4DSettings::Get();

#if WITH_EARTH4D_CESIUM
	if (!Georeference)
		for (TActorIterator<ACesiumGeoreference> It(World); It; ++It) { Georeference = *It; break; }
	if (!Georeference && bAllowSpawn)
	{
		FActorSpawnParameters P; P.Name = TEXT("Earth4DGeoreference");
		Georeference = World->SpawnActor<ACesiumGeoreference>(ACesiumGeoreference::StaticClass(), P);
	}
	if (!GoogleTileset)
		for (TActorIterator<ACesium3DTileset> It(World); It; ++It) { GoogleTileset = *It; break; }
	if (!GoogleTileset && bAllowSpawn && bStreamTiles)
	{
		FActorSpawnParameters P; P.Name = TEXT("Earth4DGoogleTiles");
		GoogleTileset = World->SpawnActor<ACesium3DTileset>(ACesium3DTileset::StaticClass(), P);
	}

	if (GoogleTileset)
	{
		// VERIFY against the installed Cesium version: SetGeoreference / ETilesetSource /
		// SetIonAssetID / SetIonAccessToken / SetUrl.
		GoogleTileset->SetGeoreference(Georeference);
		if (bUseCesiumIon && !Settings->CesiumIonToken.IsEmpty())
		{
			GoogleTileset->SetTilesetSource(ETilesetSource::FromCesiumIon);
			GoogleTileset->SetIonAssetID(GooglePhotorealisticIonAssetId);
			GoogleTileset->SetIonAccessToken(Settings->CesiumIonToken);
		}
		else if (!Settings->GoogleMapTilesApiKey.IsEmpty())
		{
			GoogleTileset->SetTilesetSource(ETilesetSource::FromUrl);
			GoogleTileset->SetUrl(FString::Printf(
				TEXT("https://tile.googleapis.com/v1/3dtiles/root.json?key=%s"), *Settings->GoogleMapTilesApiKey));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("[Earth4D] No tiles key. Set Google Map Tiles API key or Cesium ion token in Project Settings → Earth4D."));
		}
	}
#else
	// Cesium-free: drive the native Google Photorealistic 3D Tiles loader.
	if (bStreamTiles)
	{
		if (!NativeTiles)
			for (TActorIterator<AEarth4DGoogleTiles> It(World); It; ++It) { NativeTiles = *It; break; }
		if (!NativeTiles && bAllowSpawn)
		{
			FActorSpawnParameters P; P.Name = TEXT("Earth4DGoogleTiles");
			NativeTiles = World->SpawnActor<AEarth4DGoogleTiles>(AEarth4DGoogleTiles::StaticClass(), P);
		}
		if (NativeTiles)
		{
			NativeTiles->Site = this;
			NativeTiles->ApiKey = Settings->GoogleMapTilesApiKey;
			NativeTiles->BeginStreaming();
		}
		else if (Settings->GoogleMapTilesApiKey.IsEmpty())
		{
			UE_LOG(LogTemp, Warning, TEXT("[Earth4D] Cesium not installed and no Google Map Tiles API key set — no basemap will load. Set the key in Project Settings → Earth4D."));
		}
	}
#endif
}

void AEarth4DSite::ApplyRegionOrigin()
{
	RecomputeLocalFrame();
#if WITH_EARTH4D_CESIUM
	if (Georeference)
	{
		// VERIFY: (Longitude, Latitude, Height) ordering; SetOriginLongitudeLatitudeHeight
		// also sets OriginPlacement to CartographicOrigin (origin → UE 0,0,0).
		Georeference->SetOriginLongitudeLatitudeHeight(FVector(OriginLongitude, OriginLatitude, OriginHeight));
	}
#endif
	if (NativeTiles) NativeTiles->OnOriginChanged();
}

void AEarth4DSite::SetRegionOrigin(double Lat, double Lon, double Height)
{
	OriginLatitude = Lat;
	OriginLongitude = Lon;
	OriginHeight = Height;
	ApplyRegionOrigin();
}

// ---- WGS84 / local-frame math ---------------------------------------------

FVector AEarth4DSite::GeodeticToEcef(double LatDeg, double LonDeg, double HeightM)
{
	const double Lat = FMath::DegreesToRadians(LatDeg);
	const double Lon = FMath::DegreesToRadians(LonDeg);
	const double SinLat = FMath::Sin(Lat), CosLat = FMath::Cos(Lat);
	const double SinLon = FMath::Sin(Lon), CosLon = FMath::Cos(Lon);
	const double N = WGS84_A / FMath::Sqrt(1.0 - WGS84_E2 * SinLat * SinLat);
	return FVector(
		(N + HeightM) * CosLat * CosLon,
		(N + HeightM) * CosLat * SinLon,
		(N * (1.0 - WGS84_E2) + HeightM) * SinLat);
}

void AEarth4DSite::EcefToGeodetic(const FVector& Ecef, double& OutLatDeg, double& OutLonDeg, double& OutHeightM)
{
	const double X = Ecef.X, Y = Ecef.Y, Z = Ecef.Z;
	OutLonDeg = FMath::RadiansToDegrees(FMath::Atan2(Y, X));
	const double P = FMath::Sqrt(X * X + Y * Y);
	double Lat = FMath::Atan2(Z, P * (1.0 - WGS84_E2)); // initial guess
	double N = WGS84_A, Height = 0.0;
	for (int32 i = 0; i < 6; ++i) // Bowring-style fixed-point; converges in a few iters
	{
		const double SinLat = FMath::Sin(Lat);
		N = WGS84_A / FMath::Sqrt(1.0 - WGS84_E2 * SinLat * SinLat);
		Height = P / FMath::Cos(Lat) - N;
		Lat = FMath::Atan2(Z, P * (1.0 - WGS84_E2 * N / (N + Height)));
	}
	OutLatDeg = FMath::RadiansToDegrees(Lat);
	OutHeightM = Height;
}

void AEarth4DSite::RecomputeLocalFrame()
{
	OriginEcef = GeodeticToEcef(OriginLatitude, OriginLongitude, OriginHeight);
	const double Lat = FMath::DegreesToRadians(OriginLatitude);
	const double Lon = FMath::DegreesToRadians(OriginLongitude);
	const double SinLat = FMath::Sin(Lat), CosLat = FMath::Cos(Lat);
	const double SinLon = FMath::Sin(Lon), CosLon = FMath::Cos(Lon);
	AxisEast  = FVector(-SinLon, CosLon, 0.0);
	AxisNorth = FVector(-SinLat * CosLon, -SinLat * SinLon, CosLat);
	AxisUp    = FVector(CosLat * CosLon, CosLat * SinLon, SinLat);
}

// ---- Conversions ----------------------------------------------------------

FVector AEarth4DSite::GetRegionOriginUnreal() const
{
#if WITH_EARTH4D_CESIUM
	if (Georeference)
		return Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(
			FVector(OriginLongitude, OriginLatitude, OriginHeight));
#endif
	return FVector::ZeroVector; // cartographic origin → UE (0,0,0)
}

FVector AEarth4DSite::GeodeticToUnreal(double Lat, double Lon, double Height) const
{
#if WITH_EARTH4D_CESIUM
	if (Georeference)
		return Georeference->TransformLongitudeLatitudeHeightPositionToUnreal(FVector(Lon, Lat, Height));
#endif
	// Cesium-free: ECEF delta → ENU metres → Unreal cm.
	const FVector D = GeodeticToEcef(Lat, Lon, Height) - OriginEcef;
	const FVector Enu(FVector::DotProduct(AxisEast, D), FVector::DotProduct(AxisNorth, D), FVector::DotProduct(AxisUp, D));
	return EnuToUnrealLocal(Enu);
}

bool AEarth4DSite::UnrealToGeodetic(const FVector& UnrealCm, double& OutLat, double& OutLon, double& OutHeight) const
{
#if WITH_EARTH4D_CESIUM
	if (Georeference)
	{
		const FVector Llh = Georeference->TransformUnrealPositionToLongitudeLatitudeHeight(UnrealCm);
		OutLon = Llh.X; OutLat = Llh.Y; OutHeight = Llh.Z;
		return true;
	}
#endif
	const FVector Enu = UnrealLocalToEnu(UnrealCm);
	const FVector Ecef = OriginEcef + AxisEast * Enu.X + AxisNorth * Enu.Y + AxisUp * Enu.Z;
	EcefToGeodetic(Ecef, OutLat, OutLon, OutHeight);
	return true;
}

FVector AEarth4DSite::RegionLocalEnuOffsetToUnreal(const FVector& EnuMeters) const
{
#if WITH_EARTH4D_CESIUM
	if (Georeference)
	{
		// ENU → ESU (south = -north), metres → cm, then rotate into Unreal world.
		const FVector EsuCm(EnuMeters.X * 100.0, -EnuMeters.Y * 100.0, EnuMeters.Z * 100.0);
		return CachedEsuToUnreal().TransformVector(EsuCm); // rotation only
	}
#endif
	return EnuToUnrealLocal(EnuMeters);
}

FVector AEarth4DSite::RegionLocalEnuToUnreal(const FVector& EnuMeters) const
{
	return GetRegionOriginUnreal() + RegionLocalEnuOffsetToUnreal(EnuMeters);
}

FVector AEarth4DSite::UnrealToRegionLocalEnu(const FVector& UnrealCm) const
{
#if WITH_EARTH4D_CESIUM
	if (Georeference)
	{
		const FVector DeltaCm = UnrealCm - GetRegionOriginUnreal();
		const FVector EsuCm = CachedEsuToUnreal().InverseTransformVector(DeltaCm);
		return FVector(EsuCm.X / 100.0, -EsuCm.Y / 100.0, EsuCm.Z / 100.0);
	}
#endif
	return UnrealLocalToEnu(UnrealCm - GetRegionOriginUnreal());
}

#if WITH_EARTH4D_CESIUM
FMatrix AEarth4DSite::CachedEsuToUnreal() const
{
	if (!Georeference) return FMatrix::Identity;
	return Georeference->ComputeEastSouthUpToUnrealTransformation(GetRegionOriginUnreal());
}
#endif
