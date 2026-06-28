// Copyright Earth4D. Licensed for project use.
// The georeferenced "site": ties the Earth4D region-local ENU frame to the world and
// (when available) streams Google Photorealistic 3D Tiles.
//
// Cesium for Unreal is OPTIONAL (WITH_EARTH4D_CESIUM, auto-detected in Build.cs):
//   • Cesium present  → re-origins an ACesiumGeoreference (cartographic origin → UE
//     (0,0,0)) and streams Google tiles via an ACesium3DTileset.
//   • Cesium absent   → AEarth4DSite computes its OWN WGS84 ENU georeference (a local
//     tangent plane at the region origin), so element placement, fly-to and geocode
//     all work; Google tiles come from the native AEarth4DGoogleTiles path.
//
// Either way the contract the subsystem relies on is identical: region-local ENU
// metres (+X=east, +Y=north, +Z=up) ↔ Unreal world centimetres, with north mapped to
// Unreal -Y so the two backends share one handedness.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Earth4DSite.generated.h"

#if WITH_EARTH4D_CESIUM
class ACesiumGeoreference;
class ACesium3DTileset;
#endif
class AEarth4DGoogleTiles;

UCLASS(BlueprintType, Blueprintable, HideCategories = (Rendering, Replication, Input, Actor, LOD, Cooking))
class EARTH4DRUNTIME_API AEarth4DSite : public AActor
{
	GENERATED_BODY()

public:
	AEarth4DSite();

	// ---- Project region (the web app's "set region center" + extent) ----
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Earth4D|Region") double OriginLatitude = 30.2849;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Earth4D|Region") double OriginLongitude = -97.7341;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Earth4D|Region") double OriginHeight = 0.0;
	/** Half-extent of the (square) project region in metres; used for framing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Region", meta = (ClampMin = "10.0")) double RegionHalfExtentMeters = 500.0;

	// ---- Tiles config ----
	/** Prefer Cesium ion (asset 2275207) over the direct Google key. Only honoured when Cesium is installed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Tiles") bool bUseCesiumIon = false;
	/** Stream Google Photorealistic 3D Tiles. Uses Cesium when present, else the native loader. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Tiles") bool bStreamTiles = true;

#if WITH_EARTH4D_CESIUM
	// NOTE: deliberately NOT UPROPERTY — UHT forbids reflected members guarded by a
	// custom preprocessor macro (only WITH_EDITORONLY_DATA is permitted). These are
	// world-owned actors (the level keeps them alive) used only on the opt-in Cesium
	// backend, so plain pointers are sufficient here.
	ACesiumGeoreference* Georeference = nullptr;
	ACesium3DTileset* GoogleTileset = nullptr;
#endif
	/** The Cesium-free Google tiles streamer (spawned when Cesium is unavailable). */
	UPROPERTY(BlueprintReadOnly, Category = "Earth4D|Tiles") TObjectPtr<AEarth4DGoogleTiles> NativeTiles = nullptr;

	// AActor
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	/** Find-or-spawn the tiles backend + apply the region origin. bAllowSpawn=false (edit-time) only binds. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Site") void EnsureTilesBackend(bool bAllowSpawn = true);
	/** Push the current region origin onto the active backend + recompute the local frame. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Site") void ApplyRegionOrigin();
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Site") void SetRegionOrigin(double Lat, double Lon, double Height);

	// ---- Conversions (the contract the subsystem relies on) ----
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Convert") FVector GeodeticToUnreal(double Lat, double Lon, double Height) const;
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Convert") bool UnrealToGeodetic(const FVector& UnrealCm, double& OutLat, double& OutLon, double& OutHeight) const;

	FVector RegionLocalEnuToUnreal(const FVector& EnuMeters) const;
	FVector RegionLocalEnuOffsetToUnreal(const FVector& EnuMeters) const;
	FVector UnrealToRegionLocalEnu(const FVector& UnrealCm) const;
	FVector GetRegionOriginUnreal() const;

	/** Cesium ion asset id for Google Photorealistic 3D Tiles. */
	static constexpr int64 GooglePhotorealisticIonAssetId = 2275207;

	// ---- WGS84 helpers (also used by the native tiles path) ----
	static FVector GeodeticToEcef(double LatDeg, double LonDeg, double HeightM);
	static void EcefToGeodetic(const FVector& Ecef, double& OutLatDeg, double& OutLonDeg, double& OutHeightM);

private:
	void ConfigureTiles(bool bAllowSpawn);
	/** Recompute OriginEcef + the ENU basis at the region origin (the Cesium-free frame). */
	void RecomputeLocalFrame();

	/** Map a region-local ENU vector (metres) to Unreal world cm with our shared handedness. */
	static FVector EnuToUnrealLocal(const FVector& EnuMeters) { return FVector(EnuMeters.X * 100.0, -EnuMeters.Y * 100.0, EnuMeters.Z * 100.0); }
	static FVector UnrealLocalToEnu(const FVector& UnrealCm) { return FVector(UnrealCm.X / 100.0, -UnrealCm.Y / 100.0, UnrealCm.Z / 100.0); }

	// Cached Cesium-free local-tangent frame at the origin (ECEF metres + ENU basis).
	FVector OriginEcef = FVector::ZeroVector;
	FVector AxisEast = FVector(1, 0, 0);
	FVector AxisNorth = FVector(0, 1, 0);
	FVector AxisUp = FVector(0, 0, 1);

#if WITH_EARTH4D_CESIUM
	FMatrix CachedEsuToUnreal() const;
#endif
};
