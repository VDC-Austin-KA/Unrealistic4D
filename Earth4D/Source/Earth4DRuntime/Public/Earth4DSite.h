// Copyright Earth4D. Licensed for project use.
// The georeferenced "site": ties the Earth4D region-local ENU frame to the
// Cesium-georeferenced world and streams Google Photorealistic 3D Tiles.
//
// Phase 2 anchor: the project region origin (lat/lon/height) is pushed onto the
// CesiumGeoreference so that Earth4D's region-local ENU metres (+X=east,
// +Y=north, +Z=up) line up with the engine's local ENU at the georeference
// origin. The subsystem queries this actor's conversion helpers when placing
// scheduled elements onto the tiles.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Earth4DSite.generated.h"

class ACesiumGeoreference;
class ACesium3DTileset;

/**
 * Drop ONE of these into the level. On construction/begin-play it ensures a
 * CesiumGeoreference + a Google Photorealistic 3D Tileset exist, re-origins the
 * georeference to the project region, and exposes geodetic<->UE and
 * region-local-ENU<->UE conversions used by UEarth4DSubsystem.
 */
UCLASS(BlueprintType, Blueprintable, HideCategories = (Rendering, Replication, Input, Actor, LOD, Cooking))
class EARTH4DRUNTIME_API AEarth4DSite : public AActor
{
	GENERATED_BODY()

public:
	AEarth4DSite();

	// ---- Project region (the web app's "set region center" + extent) ----
	/** Region origin latitude in degrees (WGS84). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Earth4D|Region") double OriginLatitude = 30.2849;
	/** Region origin longitude in degrees (WGS84). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Earth4D|Region") double OriginLongitude = -97.7341;
	/** Region origin ellipsoidal height in metres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Earth4D|Region") double OriginHeight = 0.0;
	/** Half-extent of the (square) project region in metres; used for framing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Region", meta = (ClampMin = "10.0")) double RegionHalfExtentMeters = 500.0;

	// ---- Cesium tiles config ----
	/** If true the tileset is configured against Cesium ion (CesiumIonToken). Otherwise the Google Map Tiles API key (direct URL). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Tiles") bool bUseCesiumIon = false;

	// ---- Resolved Cesium actors (auto-found / spawned) ----
	UPROPERTY(BlueprintReadOnly, Category = "Earth4D|Tiles") TObjectPtr<ACesiumGeoreference> Georeference = nullptr;
	UPROPERTY(BlueprintReadOnly, Category = "Earth4D|Tiles") TObjectPtr<ACesium3DTileset> GoogleTileset = nullptr;

	// AActor
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginPlay() override;

	/** Find-or-spawn the CesiumGeoreference + Google tileset and apply config + origin.
	 *  bAllowSpawn=false (used at edit-time construction) only binds to existing actors. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Site") void EnsureCesiumActors(bool bAllowSpawn = true);
	/** Push the current region origin onto the CesiumGeoreference (re-origin world). */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Site") void ApplyRegionOriginToGeoreference();
	/** Set the region origin and re-origin the georeference. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Site") void SetRegionOrigin(double Lat, double Lon, double Height);

	// ---- Conversions (the contract the subsystem relies on) ----
	/** Geodetic (lat,lon,height) -> Unreal world position (cm). */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Convert") FVector GeodeticToUnreal(double Lat, double Lon, double Height) const;
	/** Unreal world position (cm) -> geodetic (degrees + metres). Returns false if no georeference. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Convert") bool UnrealToGeodetic(const FVector& UnrealCm, double& OutLat, double& OutLon, double& OutHeight) const;

	/** Region-local ENU metres -> absolute Unreal world position (cm). */
	FVector RegionLocalEnuToUnreal(const FVector& EnuMeters) const;
	/** Region-local ENU metres treated as a DELTA -> Unreal world delta (cm), rotation only. */
	FVector RegionLocalEnuOffsetToUnreal(const FVector& EnuMeters) const;
	/** Unreal world position (cm) -> region-local ENU metres (inverse of RegionLocalEnuToUnreal). */
	FVector UnrealToRegionLocalEnu(const FVector& UnrealCm) const;

	/** The Unreal-world position (cm) of the region origin (≈0 under cartographic origin placement). */
	FVector GetRegionOriginUnreal() const;

	/** Cesium ion asset id for Google Photorealistic 3D Tiles. */
	static constexpr int64 GooglePhotorealisticIonAssetId = 2275207;

private:
	void ConfigureGoogleTileset();
	/** ESU(cm-at-origin)->Unreal matrix, or identity if no georeference. */
	FMatrix CachedEsuToUnreal() const;
};
