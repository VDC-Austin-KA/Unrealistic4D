// Copyright Earth4D. Licensed for project use.
// Cesium-free Google Photorealistic 3D Tiles streamer.
//
// WHY THIS EXISTS: Cesium for Unreal is the usual rendering path for Google tiles, but
// it may lag a fresh engine release (e.g. UE 5.8). When Cesium is absent this actor
// talks to the Google Map Tiles API directly and streams the basemap itself.
//
// WHAT IT DOES (dynamic LOD streaming, the Cesium-equivalent path):
//   • Fetches + parses the root tileset, captures the Google session token.
//   • Builds a tile tree (bounding volume + geometric error + accumulated ECEF
//     transform per node); external child tilesets (.json) are spliced in on demand.
//   • Every tick it runs screen-space-error (SSE) refinement against the active camera:
//     low-detail ancestors are shown until higher-detail descendants are ready, then
//     swapped (REPLACE) — exactly like a 3D Tiles renderer.
//   • In-view tile content (.glb) is downloaded + decoded (Draco via the engine's
//     GLTFCore), built into a UProceduralMeshComponent, and georeferenced through the
//     owning AEarth4DSite so it lands in the project's region frame.
//   • Tiles not selected for a while are evicted (mesh destroyed) to bound memory.
//
// The decode is Draco-aware because GLTFCore links the engine's bundled Draco; no
// third-party library is vendored here.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interfaces/IHttpRequest.h"
#include "Earth4DGoogleTiles.generated.h"

class AEarth4DSite;
class UProceduralMeshComponent;
class UMaterialInterface;
class UTexture2D;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEarth4DTilesStatus, bool, bConnected, const FString&, Message);

namespace Earth4DTiles
{
	/** A 3D Tiles bounding volume reduced to a bounding sphere in ECEF metres (enough for
	 *  distance-based culling + SSE). Box/sphere/region inputs all collapse to this. */
	struct FBounds
	{
		FVector CenterEcef = FVector::ZeroVector; // metres
		double  Radius = 0.0;                     // metres
		bool    bValid = false;
	};

	enum class ERefine : uint8 { Replace, Add };

	enum class EState : uint8
	{
		Unloaded,        // no content requested
		TilesetLoading,  // external .json children being fetched
		ContentLoading,  // .glb being downloaded
		ContentReady,    // .glb bytes in hand, awaiting decode
		Decoding,        // mesh being built
		Ready,           // mesh built + visible-able
		Failed
	};

	/** One node of the streamed tile tree. Plain C++ (not a UObject); the built mesh it
	 *  owns IS a UObject and is tracked weakly + destroyed on eviction. */
	struct FTile
	{
		FBounds   Bounds;
		double    GeometricError = 0.0;
		FMatrix   ToEcef = FMatrix::Identity; // accumulated local→ECEF transform (double)
		ERefine   Refine = ERefine::Replace;

		FString   ContentUri;                 // .glb leaf geometry OR .json external tileset
		bool      bContentIsExternalTileset = false;

		TArray<TSharedPtr<FTile>> Children;
		bool      bChildrenResolved = false;  // external tileset fetched (or none needed)

		EState    State = EState::Unloaded;
		TWeakObjectPtr<UProceduralMeshComponent> Mesh;
		TArray<uint8> PendingGlb;             // downloaded bytes awaiting decode
		int64     LastSelectedFrame = -1;

		bool IsRenderable() const { return State == EState::Ready && Mesh.IsValid(); }
		bool HasContent() const { return !ContentUri.IsEmpty(); }
	};
}

UCLASS(BlueprintType)
class EARTH4DRUNTIME_API AEarth4DGoogleTiles : public AActor
{
	GENERATED_BODY()

public:
	AEarth4DGoogleTiles();

	/** The site that owns the region origin / ECEF conversions. */
	UPROPERTY(BlueprintReadWrite, Category = "Earth4D|Tiles") TObjectPtr<AEarth4DSite> Site = nullptr;
	/** Google Map Tiles API key (provided by the site from per-user settings; never committed). */
	UPROPERTY(BlueprintReadWrite, Category = "Earth4D|Tiles") FString ApiKey;
	/** Cesium ion access token. If set (and no direct key), the streamer resolves the ion
	 *  Google Photorealistic 3D Tiles asset endpoint to obtain the Google tileset URL/key. */
	UPROPERTY(BlueprintReadWrite, Category = "Earth4D|Tiles") FString CesiumIonToken;

	/** Target screen-space error in pixels. Lower = sharper tiles + more streaming. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Tiles", meta = (ClampMin = "1.0", ClampMax = "64.0")) float MaxScreenSpaceError = 16.0f;
	/** Max tile content downloads/decodes started per frame (throttles hitches + bandwidth). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Tiles", meta = (ClampMin = "1", ClampMax = "16")) int32 MaxLoadsPerFrame = 4;
	/** Max concurrent in-flight HTTP requests (content + external tilesets). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Tiles", meta = (ClampMin = "1", ClampMax = "32")) int32 MaxConcurrentRequests = 8;
	/** Evict a tile's mesh after it has gone unselected for this many frames. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Tiles", meta = (ClampMin = "10")) int32 EvictAfterFrames = 120;
	/** Hard cap on simultaneously-built tile meshes (memory bound). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Earth4D|Tiles", meta = (ClampMin = "16")) int32 MaxLoadedTiles = 600;

	/** True once the root tileset fetched and a session token was obtained. */
	UPROPERTY(BlueprintReadOnly, Category = "Earth4D|Tiles") bool bConnected = false;
	/** Tiles currently selected for display this frame (diagnostic). */
	UPROPERTY(BlueprintReadOnly, Category = "Earth4D|Tiles") int32 TilesVisible = 0;
	/** Tile meshes currently built/resident (diagnostic). */
	UPROPERTY(BlueprintReadOnly, Category = "Earth4D|Tiles") int32 TilesLoaded = 0;

	/** Fires when connectivity is established or fails. */
	UPROPERTY(BlueprintAssignable, Category = "Earth4D|Tiles") FEarth4DTilesStatus OnStatus;

	/** Fetch the root tileset + begin streaming. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Tiles") void BeginStreaming();
	/** Stop streaming and tear down all built tile meshes. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Tiles") void StopStreaming();
	/** Region origin moved — rebuild placement of resident tiles next tick. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D|Tiles") void OnOriginChanged();

	// AActor
	virtual void Tick(float DeltaSeconds) override;
	virtual bool ShouldTickIfViewportsOnly() const override; // stream in the editor viewport too
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void BeginDestroy() override;

private:
	// ---- Tree ----
	TSharedPtr<Earth4DTiles::FTile> Root;
	FString SessionToken;            // appended to child URIs (Google requirement)
	bool    bStreaming = false;
	int64   FrameCounter = 0;
	int32   InFlightRequests = 0;
	bool    bReplacePlacement = false; // origin changed → re-place resident meshes
	bool    bFramedRegionOnConnect = false; // one-shot: frame the site once tiles connect (web app's flyToRegion)

	// Per-frame view info.
	bool GetViewPoint(FVector& OutCamLocation, double& OutSsePixelsPerMeter) const;

	// ---- Tileset / content fetch ----
	void ResolveIonEndpointThenStream(); // Cesium ion token → Google tileset URL + key
	void FetchRootTileset();
	void FetchRootTilesetUrl(const FString& Url);
	void FetchExternalTileset(TSharedPtr<Earth4DTiles::FTile> Tile);
	void FetchContent(TSharedPtr<Earth4DTiles::FTile> Tile);
	FString WithKeyAndSession(const FString& Uri) const;
	void CaptureSessionToken(const FString& Uri);

	// Parse a tileset JSON document into `OutTile` (its root node), using ParentTransform.
	bool ParseTileset(const FString& Json, const FMatrix& ParentTransform, TSharedPtr<Earth4DTiles::FTile>& OutRoot);
	TSharedPtr<Earth4DTiles::FTile> ParseTileNode(const TSharedPtr<class FJsonObject>& Node, const FMatrix& ParentTransform);

	// ---- Selection / refinement (per tick) ----
	void UpdateStreaming();
	void SelectTile(TSharedPtr<Earth4DTiles::FTile> Tile, const FVector& CamLocation, double SsePixelsPerMeter,
	                int32& OutLoadBudget, TArray<Earth4DTiles::FTile*>& OutVisible);
	double ComputeScreenSpaceError(const Earth4DTiles::FTile& Tile, const FVector& CamLocation, double SsePixelsPerMeter) const;
	FVector TileCenterUnreal(const Earth4DTiles::FTile& Tile) const;

	// ---- Decode + mesh build (GLTFCore / Draco) ----
	void DecodeAndBuild(TSharedPtr<Earth4DTiles::FTile> Tile);
	UTexture2D* DecodeImageToTexture(const uint8* Data, int32 NumBytes) const; // JPEG/PNG → texture
	void EvictStale();
	void SetTileVisible(Earth4DTiles::FTile& Tile, bool bVisible);
	void DestroyTileMesh(Earth4DTiles::FTile& Tile);
	void TearDownTree();
	void TearDownTree(bool bKeepTree); // bKeepTree: drop meshes but keep the parsed tile tree

	UPROPERTY(Transient) TObjectPtr<UMaterialInterface> TileMaterial; // base material for textured tiles

	void Report(bool bOkConnected, const FString& Message);
};
