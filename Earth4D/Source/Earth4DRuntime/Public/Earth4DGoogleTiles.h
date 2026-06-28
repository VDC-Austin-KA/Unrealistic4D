// Copyright Earth4D. Licensed for project use.
// Cesium-free Google Photorealistic 3D Tiles loader.
//
// WHY THIS EXISTS: Cesium for Unreal is the rendering path for Google tiles, but it
// may lag a fresh engine release (e.g. UE 5.8). When Cesium is absent this actor
// talks to the Google Map Tiles API directly so the project still georeferences and
// connects to the real basemap.
//
// SCOPE (be honest): Google 3D Tiles ship Draco-compressed glTF, so fully rendering
// them natively needs a Draco + glTF runtime decoder — a substantial dependency that
// is intentionally NOT bundled here. This actor implements the parts that ARE
// tractable and verifiable: authenticating with the key, fetching + parsing the root
// tileset, extracting the Google session token, and traversing the tile tree for the
// project region (reporting which tiles WOULD be loaded). The mesh-build step is a
// single well-marked hook (OnTileContentDownloaded) so a Draco/glTF decoder — or
// Cesium once it supports the engine version — can drop in without touching the rest.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/IHttpRequest.h"
#include "Earth4DGoogleTiles.generated.h"

class AEarth4DSite;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEarth4DTilesStatus, bool, bConnected, const FString&, Message);

UCLASS(BlueprintType)
class EARTH4DRUNTIME_API AEarth4DGoogleTiles : public AActor
{
	GENERATED_BODY()

public:
	AEarth4DGoogleTiles();

	/** The site that owns the region origin / conversions. */
	UPROPERTY(BlueprintReadWrite, Category = "Earth4D|Tiles") TObjectPtr<AEarth4DSite> Site = nullptr;
	/** Google Map Tiles API key (provided by the site from per-user settings; never committed). */
	UPROPERTY(BlueprintReadWrite, Category = "Earth4D|Tiles") FString ApiKey;

	/** Max tile-tree depth to traverse per refresh (bounds work + cost). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Tiles", meta = (ClampMin = "1", ClampMax = "24")) int32 MaxDepth = 16;

	/** True once the root tileset fetched and a session token was obtained. */
	UPROPERTY(BlueprintReadOnly, Category = "Earth4D|Tiles") bool bConnected = false;
	UPROPERTY(BlueprintReadOnly, Category = "Earth4D|Tiles") int32 TilesInRegion = 0;

	/** Fires when connectivity is established or fails. */
	UPROPERTY(BlueprintAssignable, Category = "Earth4D|Tiles") FEarth4DTilesStatus OnStatus;

	/** Fetch the root tileset + begin traversal for the current region. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Tiles") void BeginStreaming();
	/** Re-traverse after the region origin moved. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Tiles") void OnOriginChanged();

protected:
	/**
	 * Called for every in-region leaf tile whose glB content has been downloaded.
	 * HOOK: decode `Glb` (Draco + glTF) into a mesh and place it via Site conversions.
	 * Left unimplemented on purpose (see file header) — override or extend to render.
	 */
	virtual void OnTileContentDownloaded(const TArray<uint8>& Glb, const FString& ContentUri);

private:
	FString SessionToken;            // appended to child URIs (Google requirement)
	bool bRefreshQueued = false;

	void FetchTileset(const FString& Url, int32 Depth);
	void OnTilesetResponse(FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bOk, int32 Depth);
	FString WithKeyAndSession(const FString& Uri) const;
	void Report(bool bOkConnected, const FString& Message);
};
