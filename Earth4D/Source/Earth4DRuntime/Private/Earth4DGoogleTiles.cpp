// Copyright Earth4D. Licensed for project use.
#include "Earth4DGoogleTiles.h"
#include "Earth4DSite.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "ProceduralMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Materials/MaterialInterface.h"
#include "Engine/Engine.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"

// Engine glTF parser (Interchange plugin) — decodes Draco-compressed primitives internally.
#include "GLTFAsset.h"
#include "GLTFMesh.h"
#include "GLTFNode.h"
#include "GLTFReader.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorViewportClient.h"
#endif

using namespace Earth4DTiles;

namespace
{
	// glTF is Y-up; 3D Tiles / ECEF is Z-up. Rotate +90° about X: (x,y,z) -> (x,-z,y).
	static const FMatrix GYupToZup(
		FPlane(1, 0, 0, 0),
		FPlane(0, 0, 1, 0),
		FPlane(0, -1, 0, 0),
		FPlane(0, 0, 0, 1));

	// Read a 3D Tiles column-major 16-element transform into a UE (row-vector) FMatrix.
	FMatrix ReadTransformArray(const TArray<TSharedPtr<FJsonValue>>& A)
	{
		if (A.Num() != 16) return FMatrix::Identity;
		FMatrix M;
		for (int32 R = 0; R < 4; ++R)
			for (int32 C = 0; C < 4; ++C)
				M.M[R][C] = A[R * 4 + C]->AsNumber(); // column-major array → row-vector matrix
		return M;
	}

	// Collapse a 3D Tiles bounding volume (box / sphere / region) to an ECEF bounding sphere.
	FBounds ReadBounds(const TSharedPtr<FJsonObject>& BV, const FMatrix& ToEcef)
	{
		FBounds Out;
		if (!BV.IsValid()) return Out;

		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (BV->TryGetArrayField(TEXT("box"), Arr) && Arr->Num() == 12)
		{
			auto N = [&](int32 i) { return (*Arr)[i]->AsNumber(); };
			const FVector CenterLocal(N(0), N(1), N(2));
			const FVector U(N(3), N(4), N(5)), V(N(6), N(7), N(8)), W(N(9), N(10), N(11));
			Out.CenterEcef = ToEcef.TransformPosition(CenterLocal);
			const double Ru = ToEcef.TransformVector(U).Size();
			const double Rv = ToEcef.TransformVector(V).Size();
			const double Rw = ToEcef.TransformVector(W).Size();
			Out.Radius = Ru + Rv + Rw; // conservative sphere bounding the OBB
			Out.bValid = true;
		}
		else if (BV->TryGetArrayField(TEXT("sphere"), Arr) && Arr->Num() == 4)
		{
			auto N = [&](int32 i) { return (*Arr)[i]->AsNumber(); };
			Out.CenterEcef = ToEcef.TransformPosition(FVector(N(0), N(1), N(2)));
			Out.Radius = ToEcef.TransformVector(FVector(N(3), 0, 0)).Size();
			Out.bValid = true;
		}
		else if (BV->TryGetArrayField(TEXT("region"), Arr) && Arr->Num() == 6)
		{
			// [west, south, east, north, minH, maxH] in radians/metres (EPSG:4979).
			auto N = [&](int32 i) { return (*Arr)[i]->AsNumber(); };
			const double W = N(0), S = N(1), E = N(2), Nn = N(3), MinH = N(4), MaxH = N(5);
			const double MidLat = FMath::RadiansToDegrees((S + Nn) * 0.5);
			const double MidLon = FMath::RadiansToDegrees((W + E) * 0.5);
			const double MidH = (MinH + MaxH) * 0.5;
			Out.CenterEcef = AEarth4DSite::GeodeticToEcef(MidLat, MidLon, MidH);
			const FVector Corner = AEarth4DSite::GeodeticToEcef(
				FMath::RadiansToDegrees(Nn), FMath::RadiansToDegrees(E), MaxH);
			Out.Radius = (Corner - Out.CenterEcef).Size();
			Out.bValid = true;
		}
		return Out;
	}
}

AEarth4DGoogleTiles::AEarth4DGoogleTiles()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	USceneComponent* RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(RootScene);
}

bool AEarth4DGoogleTiles::ShouldTickIfViewportsOnly() const
{
	return true; // keep streaming in the level-editor viewport, not just PIE
}

// ---- Lifecycle -------------------------------------------------------------

void AEarth4DGoogleTiles::BeginStreaming()
{
	if (ApiKey.IsEmpty())
	{
		Report(false, TEXT("No Google Map Tiles API key set (Project Settings → Earth4D)."));
		return;
	}
	StopStreaming();

	if (!TileMaterial)
	{
		// Neutral lit material; per-tile base-colour textures are a follow-up.
		TileMaterial = LoadObject<UMaterialInterface>(nullptr, TEXT("/Engine/EngineMaterials/WorldGridMaterial.WorldGridMaterial"));
	}

	bStreaming = true;
	bConnected = false;
	SessionToken.Reset();
	FetchRootTileset();
}

void AEarth4DGoogleTiles::StopStreaming()
{
	bStreaming = false;
	TearDownTree();
	Root.Reset();
	TilesVisible = 0;
	TilesLoaded = 0;
}

void AEarth4DGoogleTiles::OnOriginChanged()
{
	// Re-place resident tile meshes against the new region frame next tick.
	bReplacePlacement = true;
}

void AEarth4DGoogleTiles::EndPlay(const EEndPlayReason::Type Reason)
{
	StopStreaming();
	Super::EndPlay(Reason);
}

void AEarth4DGoogleTiles::BeginDestroy()
{
	TearDownTree();
	Root.Reset();
	Super::BeginDestroy();
}

// ---- Tick / streaming ------------------------------------------------------

void AEarth4DGoogleTiles::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bStreaming || !Root.IsValid() || !Site) return;

	++FrameCounter;

	if (bReplacePlacement)
	{
		// Origin moved: drop all built meshes; they rebuild at the new frame as re-selected.
		TearDownTree(/*bKeepTree=*/true);
		bReplacePlacement = false;
	}

	UpdateStreaming();
	EvictStale();
}

void AEarth4DGoogleTiles::UpdateStreaming()
{
	FVector CamLoc;
	double SsePixelsPerMeter = 0.0;
	if (!GetViewPoint(CamLoc, SsePixelsPerMeter)) return;

	int32 LoadBudget = FMath::Max(1, MaxLoadsPerFrame);
	TArray<FTile*> Visible;
	SelectTile(Root, CamLoc, SsePixelsPerMeter, LoadBudget, Visible);

	// Show selected tiles, hide everything else that is resident.
	TSet<FTile*> VisibleSet(Visible);
	int32 LoadedCount = 0;
	TFunction<void(const TSharedPtr<FTile>&)> Sweep = [&](const TSharedPtr<FTile>& T)
	{
		if (!T.IsValid()) return;
		if (T->IsRenderable())
		{
			++LoadedCount;
			SetTileVisible(*T, VisibleSet.Contains(T.Get()));
		}
		for (const TSharedPtr<FTile>& C : T->Children) Sweep(C);
	};
	Sweep(Root);

	TilesVisible = Visible.Num();
	TilesLoaded = LoadedCount;
}

// Recursive SSE refinement. Returns nothing; appends renderable tiles to OutVisible and
// spends OutLoadBudget kicking off downloads/decodes for tiles that need to become ready.
void AEarth4DGoogleTiles::SelectTile(TSharedPtr<FTile> Tile, const FVector& CamLocation, double SsePixelsPerMeter,
                                     int32& OutLoadBudget, TArray<FTile*>& OutVisible)
{
	if (!Tile.IsValid()) return;
	Tile->LastSelectedFrame = FrameCounter;

	const double Sse = ComputeScreenSpaceError(*Tile, CamLocation, SsePixelsPerMeter);
	const bool bHasChildren = Tile->Children.Num() > 0 || Tile->bContentIsExternalTileset;
	const bool bWantChildren = bHasChildren && Sse > MaxScreenSpaceError;

	// Make sure this tile's own geometry is loading/ready if it has any.
	auto EnsureContent = [&](TSharedPtr<FTile>& T)
	{
		if (!T->HasContent() || T->bContentIsExternalTileset) return;
		if (T->State == EState::Unloaded && OutLoadBudget > 0 && InFlightRequests < MaxConcurrentRequests)
		{
			--OutLoadBudget;
			FetchContent(T);
		}
		else if (T->State == EState::ContentReady && OutLoadBudget > 0)
		{
			--OutLoadBudget;
			DecodeAndBuild(T);
		}
	};

	if (!bWantChildren)
	{
		// This tile is detailed enough (or a leaf): render it.
		EnsureContent(Tile);
		if (Tile->IsRenderable()) OutVisible.Add(Tile.Get());
		return;
	}

	// Need more detail. Resolve external tileset children if necessary.
	if (Tile->bContentIsExternalTileset && !Tile->bChildrenResolved)
	{
		if (Tile->State == EState::Unloaded && InFlightRequests < MaxConcurrentRequests)
			FetchExternalTileset(Tile);
		// Until children arrive, keep showing our own/ancestor LOD.
		EnsureContent(Tile);
		if (Tile->IsRenderable()) OutVisible.Add(Tile.Get());
		return;
	}

	// REPLACE: show this tile until all children are renderable, then defer to them.
	const int32 VisibleBefore = OutVisible.Num();
	bool bAllChildrenReady = Tile->Children.Num() > 0;
	for (TSharedPtr<FTile>& Child : Tile->Children)
	{
		SelectTile(Child, CamLocation, SsePixelsPerMeter, OutLoadBudget, OutVisible);
		if (!(Child->IsRenderable() || Child->Children.Num() > 0)) bAllChildrenReady = false;
	}

	const bool bChildrenCovered = (OutVisible.Num() > VisibleBefore) && bAllChildrenReady;
	if (Tile->Refine == ERefine::Add || !bChildrenCovered)
	{
		// ADD refinement always shows the parent; REPLACE shows it until children cover it.
		EnsureContent(Tile);
		if (Tile->IsRenderable()) OutVisible.Add(Tile.Get());
	}
}

double AEarth4DGoogleTiles::ComputeScreenSpaceError(const FTile& Tile, const FVector& CamLocation, double SsePixelsPerMeter) const
{
	if (Tile.GeometricError <= 0.0) return 0.0; // leaf precision → never needs refinement
	const FVector CenterUnreal = TileCenterUnreal(Tile);
	const double RadiusCm = Tile.Bounds.Radius * 100.0;
	double DistCm = FVector::Distance(CamLocation, CenterUnreal) - RadiusCm;
	const double DistMeters = FMath::Max(DistCm, 1.0) / 100.0;
	return (Tile.GeometricError * SsePixelsPerMeter) / DistMeters;
}

FVector AEarth4DGoogleTiles::TileCenterUnreal(const FTile& Tile) const
{
	return Site ? Site->EcefMetersToUnreal(Tile.Bounds.CenterEcef) : Tile.Bounds.CenterEcef;
}

bool AEarth4DGoogleTiles::GetViewPoint(FVector& OutCamLocation, double& OutSsePixelsPerMeter) const
{
	UWorld* World = GetWorld();
	if (!World) return false;

	double FovYDeg = 60.0;
	double ViewportH = 1080.0;

	if (World->IsGameWorld())
	{
		APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(World, 0);
		if (!Cam) return false;
		OutCamLocation = Cam->GetCameraLocation();
		FovYDeg = Cam->GetFOVAngle();
		if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
			ViewportH = FMath::Max(1, GEngine->GameViewport->Viewport->GetSizeXY().Y);
	}
#if WITH_EDITOR
	else if (GEditor)
	{
		FEditorViewportClient* Best = nullptr;
		for (FEditorViewportClient* VC : GEditor->GetAllViewportClients())
			if (VC && VC->IsPerspective() && VC->GetWorld() == World) { Best = VC; break; }
		if (!Best) return false;
		OutCamLocation = Best->GetViewLocation();
		FovYDeg = Best->ViewFOV;
		if (Best->Viewport) ViewportH = FMath::Max(1, Best->Viewport->GetSizeXY().Y);
	}
#endif
	else return false;

	const double TanHalf = FMath::Tan(FMath::DegreesToRadians(FovYDeg) * 0.5);
	OutSsePixelsPerMeter = ViewportH / FMath::Max(2.0 * TanHalf, KINDA_SMALL_NUMBER);
	return true;
}

// ---- Fetch -----------------------------------------------------------------

void AEarth4DGoogleTiles::FetchRootTileset()
{
	const FString Url = FString::Printf(TEXT("https://tile.googleapis.com/v1/3dtiles/root.json?key=%s"), *ApiKey);
	++InFlightRequests;
	FHttpRequestRef Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(Url);
	Req->SetVerb(TEXT("GET"));
	Req->SetHeader(TEXT("Accept"), TEXT("application/json"));
	TWeakObjectPtr<AEarth4DGoogleTiles> WeakThis(this);
	Req->OnProcessRequestComplete().BindLambda(
		[WeakThis](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
		{
			AEarth4DGoogleTiles* Self = WeakThis.Get();
			if (!Self) return;
			--Self->InFlightRequests;
			if (!Self->bStreaming) return;
			if (!bOk || !Resp.IsValid() || Resp->GetResponseCode() >= 300)
			{
				Self->Report(false, FString::Printf(TEXT("Root tileset fetch failed (HTTP %d). Check the API key + 'Map Tiles API' enablement."),
					Resp.IsValid() ? Resp->GetResponseCode() : 0));
				return;
			}
			TSharedPtr<FTile> NewRoot;
			if (!Self->ParseTileset(Resp->GetContentAsString(), FMatrix::Identity, NewRoot) || !NewRoot.IsValid())
			{
				Self->Report(false, TEXT("Root tileset JSON parse error."));
				return;
			}
			Self->Root = NewRoot;
			Self->bConnected = true;
			Self->Report(true, TEXT("Connected to Google Photorealistic 3D Tiles — streaming."));
		});
	Req->ProcessRequest();
}

void AEarth4DGoogleTiles::FetchExternalTileset(TSharedPtr<FTile> Tile)
{
	if (!Tile.IsValid()) return;
	Tile->State = EState::TilesetLoading;
	CaptureSessionToken(Tile->ContentUri);
	const FString Url = WithKeyAndSession(Tile->ContentUri);
	const FMatrix ParentTransform = Tile->ToEcef;

	++InFlightRequests;
	FHttpRequestRef Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(Url);
	Req->SetVerb(TEXT("GET"));
	TWeakObjectPtr<AEarth4DGoogleTiles> WeakThis(this);
	TWeakPtr<FTile> WeakTile(Tile);
	Req->OnProcessRequestComplete().BindLambda(
		[WeakThis, WeakTile, ParentTransform](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
		{
			AEarth4DGoogleTiles* Self = WeakThis.Get();
			TSharedPtr<FTile> T = WeakTile.Pin();
			if (!Self) return;
			--Self->InFlightRequests;
			if (!T.IsValid() || !Self->bStreaming) return;
			if (!bOk || !Resp.IsValid() || Resp->GetResponseCode() >= 300)
			{
				T->State = EState::Failed;
				return;
			}
			TSharedPtr<FTile> SubRoot;
			if (Self->ParseTileset(Resp->GetContentAsString(), ParentTransform, SubRoot) && SubRoot.IsValid())
			{
				T->Children.Add(SubRoot);           // splice the external tileset's root in
				T->bChildrenResolved = true;
				T->State = EState::Unloaded;
			}
			else
			{
				T->State = EState::Failed;
			}
		});
	Req->ProcessRequest();
}

void AEarth4DGoogleTiles::FetchContent(TSharedPtr<FTile> Tile)
{
	if (!Tile.IsValid()) return;
	Tile->State = EState::ContentLoading;
	CaptureSessionToken(Tile->ContentUri);
	const FString Url = WithKeyAndSession(Tile->ContentUri);

	++InFlightRequests;
	FHttpRequestRef Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(Url);
	Req->SetVerb(TEXT("GET"));
	TWeakObjectPtr<AEarth4DGoogleTiles> WeakThis(this);
	TWeakPtr<FTile> WeakTile(Tile);
	Req->OnProcessRequestComplete().BindLambda(
		[WeakThis, WeakTile](FHttpRequestPtr, FHttpResponsePtr Resp, bool bOk)
		{
			AEarth4DGoogleTiles* Self = WeakThis.Get();
			TSharedPtr<FTile> T = WeakTile.Pin();
			if (!Self) return;
			--Self->InFlightRequests;
			if (!T.IsValid() || !Self->bStreaming) return;
			if (!bOk || !Resp.IsValid() || Resp->GetResponseCode() >= 300 || Resp->GetContent().Num() == 0)
			{
				T->State = EState::Failed;
				return;
			}
			T->PendingGlb = Resp->GetContent();
			T->State = EState::ContentReady; // decoded on a later tick within the load budget
		});
	Req->ProcessRequest();
}

FString AEarth4DGoogleTiles::WithKeyAndSession(const FString& Uri) const
{
	FString Full = Uri.StartsWith(TEXT("http")) ? Uri : (TEXT("https://tile.googleapis.com") + Uri);
	const TCHAR Sep = Full.Contains(TEXT("?")) ? TEXT('&') : TEXT('?');
	Full += FString::Printf(TEXT("%ckey=%s"), Sep, *ApiKey);
	if (!SessionToken.IsEmpty() && !Full.Contains(TEXT("session="))) Full += FString::Printf(TEXT("&session=%s"), *SessionToken);
	return Full;
}

void AEarth4DGoogleTiles::CaptureSessionToken(const FString& Uri)
{
	if (!SessionToken.IsEmpty()) return;
	const int32 Idx = Uri.Find(TEXT("session="));
	if (Idx == INDEX_NONE) return;
	FString Tok = Uri.Mid(Idx + 8);
	int32 Amp; if (Tok.FindChar(TEXT('&'), Amp)) Tok = Tok.Left(Amp);
	SessionToken = Tok;
}

// ---- Tileset JSON → tile tree ---------------------------------------------

bool AEarth4DGoogleTiles::ParseTileset(const FString& Json, const FMatrix& ParentTransform, TSharedPtr<FTile>& OutRoot)
{
	TSharedPtr<FJsonObject> Doc;
	TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(R, Doc) || !Doc.IsValid()) return false;

	const TSharedPtr<FJsonObject>* RootObj = nullptr;
	if (!Doc->TryGetObjectField(TEXT("root"), RootObj)) return false;
	OutRoot = ParseTileNode(*RootObj, ParentTransform);
	return OutRoot.IsValid();
}

TSharedPtr<FTile> AEarth4DGoogleTiles::ParseTileNode(const TSharedPtr<FJsonObject>& Node, const FMatrix& ParentTransform)
{
	if (!Node.IsValid()) return nullptr;
	TSharedPtr<FTile> Tile = MakeShared<FTile>();

	// Accumulated local→ECEF: this tile's transform applied first, then the parent's.
	FMatrix Local = FMatrix::Identity;
	const TArray<TSharedPtr<FJsonValue>>* Xform = nullptr;
	if (Node->TryGetArrayField(TEXT("transform"), Xform)) Local = ReadTransformArray(*Xform);
	Tile->ToEcef = Local * ParentTransform;

	Node->TryGetNumberField(TEXT("geometricError"), Tile->GeometricError);

	FString Refine;
	if (Node->TryGetStringField(TEXT("refine"), Refine))
		Tile->Refine = Refine.Equals(TEXT("ADD"), ESearchCase::IgnoreCase) ? ERefine::Add : ERefine::Replace;

	const TSharedPtr<FJsonObject>* BV = nullptr;
	if (Node->TryGetObjectField(TEXT("boundingVolume"), BV))
		Tile->Bounds = ReadBounds(*BV, Tile->ToEcef);

	const TSharedPtr<FJsonObject>* Content = nullptr;
	if (Node->TryGetObjectField(TEXT("content"), Content))
	{
		FString Uri;
		if ((*Content)->TryGetStringField(TEXT("uri"), Uri) || (*Content)->TryGetStringField(TEXT("url"), Uri))
		{
			CaptureSessionToken(Uri);
			Tile->ContentUri = Uri;
			Tile->bContentIsExternalTileset = Uri.Contains(TEXT(".json"));
		}
	}

	const TArray<TSharedPtr<FJsonValue>>* ChildArr = nullptr;
	if (Node->TryGetArrayField(TEXT("children"), ChildArr))
		for (const TSharedPtr<FJsonValue>& C : *ChildArr)
			if (TSharedPtr<FTile> Kid = ParseTileNode(C->AsObject(), Tile->ToEcef))
				Tile->Children.Add(Kid);

	return Tile;
}

// ---- Decode (GLTFCore / Draco) + georeferenced mesh build ------------------

void AEarth4DGoogleTiles::DecodeAndBuild(TSharedPtr<FTile> Tile)
{
	if (!Tile.IsValid() || !Site) return;
	Tile->State = EState::Decoding;

	// GLTFCore reads from a disk path; stage the downloaded glB to a temp file.
	const FString TempDir = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("Earth4DTiles"));
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
	PF.CreateDirectoryTree(*TempDir);
	const FString TempPath = FPaths::Combine(TempDir, FString::Printf(TEXT("tile_%llu.glb"), (uint64)FrameCounter * 1000 + (uint64)(UPTRINT)Tile.Get() % 1000));
	if (!FFileHelper::SaveArrayToFile(Tile->PendingGlb, *TempPath))
	{
		Tile->State = EState::Failed;
		Tile->PendingGlb.Empty();
		return;
	}
	Tile->PendingGlb.Empty();

	GLTF::FFileReader Reader;
	GLTF::FAsset Asset;
	Reader.ReadFile(TempPath, /*bLoadImageData=*/false, /*bLoadMetadata=*/false, Asset);
	PF.DeleteFile(*TempPath);

	if (Asset.Meshes.Num() == 0)
	{
		Tile->State = EState::Failed;
		return;
	}

	UProceduralMeshComponent* PMC = NewObject<UProceduralMeshComponent>(this);
	PMC->SetupAttachment(GetRootComponent());
	PMC->RegisterComponent();
	PMC->bUseAsyncCooking = false;
	PMC->SetMobility(EComponentMobility::Movable);

	// Combined glTF-local → ECEF: Y-up→Z-up first, then the tile's accumulated transform.
	const FMatrix LocalToEcef = GYupToZup * Tile->ToEcef;

	int32 SectionIndex = 0;
	auto BuildPrimitive = [&](const GLTF::FPrimitive& Prim, const FMatrix& NodeToEcef)
	{
		if (Prim.Mode != GLTF::FPrimitive::EMode::Triangles || !Prim.HasPositions()) return;

		TArray<FVector3f> P; Prim.GetPositions(P);
		TArray<uint32> Indices; Prim.GetTriangleIndices(Indices);
		if (P.Num() == 0 || Indices.Num() < 3) return;

		TArray<FVector3f> Nrm; if (Prim.HasNormals()) Prim.GetNormals(Nrm);
		TArray<FVector2f> UV; if (Prim.HasTexCoords(0)) Prim.GetTexCoords(0, UV);

		TArray<FVector> Positions; Positions.Reserve(P.Num());
		TArray<FVector> Normals; Normals.Reserve(P.Num());
		TArray<FVector2D> UV0; UV0.Reserve(P.Num());
		for (int32 i = 0; i < P.Num(); ++i)
		{
			const FVector Local(P[i].X, P[i].Y, P[i].Z);
			const FVector Ecef = NodeToEcef.TransformPosition(Local);
			Positions.Add(Site->EcefMetersToUnreal(Ecef));
			if (Nrm.IsValidIndex(i))
			{
				const FVector NLocal(Nrm[i].X, Nrm[i].Y, Nrm[i].Z);
				Normals.Add(Site->EcefDirectionToUnreal(NodeToEcef.TransformVector(NLocal)));
			}
			else Normals.Add(FVector::UpVector);
			UV0.Add(UV.IsValidIndex(i) ? FVector2D(UV[i].X, UV[i].Y) : FVector2D::ZeroVector);
		}

		// The ENU→Unreal frame mirrors one axis, so reverse winding to keep faces outward.
		TArray<int32> Tris; Tris.Reserve(Indices.Num());
		for (int32 t = 0; t + 2 < Indices.Num(); t += 3)
		{
			Tris.Add((int32)Indices[t]);
			Tris.Add((int32)Indices[t + 2]);
			Tris.Add((int32)Indices[t + 1]);
		}

		const TArray<FProcMeshTangent> NoTangents;
		const TArray<FColor> NoColors;
		PMC->CreateMeshSection(SectionIndex, Positions, Tris, Normals, UV0, NoColors, NoTangents, /*bCreateCollision=*/false);
		if (TileMaterial) PMC->SetMaterial(SectionIndex, TileMaterial);
		++SectionIndex;
	};

	// Prefer the node graph (carries per-node transforms); fall back to raw meshes.
	if (Asset.Nodes.Num() > 0)
	{
		for (const GLTF::FNode& GNode : Asset.Nodes)
		{
			if (GNode.MeshIndex < 0 || !Asset.Meshes.IsValidIndex(GNode.MeshIndex)) continue;
			const FMatrix NodeToEcef = GNode.Transform.ToMatrixWithScale() * LocalToEcef;
			for (const GLTF::FPrimitive& Prim : Asset.Meshes[GNode.MeshIndex].Primitives)
				BuildPrimitive(Prim, NodeToEcef);
		}
	}
	else
	{
		for (const GLTF::FMesh& Mesh : Asset.Meshes)
			for (const GLTF::FPrimitive& Prim : Mesh.Primitives)
				BuildPrimitive(Prim, LocalToEcef);
	}

	if (SectionIndex == 0)
	{
		PMC->DestroyComponent();
		Tile->State = EState::Failed;
		return;
	}

	Tile->Mesh = PMC;
	Tile->State = EState::Ready;
}

// ---- Visibility / eviction -------------------------------------------------

void AEarth4DGoogleTiles::SetTileVisible(FTile& Tile, bool bVisible)
{
	if (UProceduralMeshComponent* PMC = Tile.Mesh.Get())
		if (PMC->IsVisible() != bVisible)
			PMC->SetVisibility(bVisible);
}

void AEarth4DGoogleTiles::EvictStale()
{
	if (!Root.IsValid()) return;
	const int64 Cutoff = FrameCounter - FMath::Max(10, EvictAfterFrames);

	int32 Resident = 0;
	TArray<FTile*> Evictable;
	TFunction<void(const TSharedPtr<FTile>&)> Walk = [&](const TSharedPtr<FTile>& T)
	{
		if (!T.IsValid()) return;
		if (T->IsRenderable())
		{
			++Resident;
			if (T->LastSelectedFrame < Cutoff) Evictable.Add(T.Get());
		}
		for (const TSharedPtr<FTile>& C : T->Children) Walk(C);
	};
	Walk(Root);

	for (FTile* T : Evictable) DestroyTileMesh(*T);

	// Memory bound: if still over budget, evict the least-recently-selected resident tiles.
	if (Resident - Evictable.Num() > MaxLoadedTiles)
	{
		TArray<FTile*> Residents;
		TFunction<void(const TSharedPtr<FTile>&)> Collect = [&](const TSharedPtr<FTile>& T)
		{
			if (!T.IsValid()) return;
			if (T->IsRenderable()) Residents.Add(T.Get());
			for (const TSharedPtr<FTile>& C : T->Children) Collect(C);
		};
		Collect(Root);
		Residents.Sort([](const FTile& A, const FTile& B) { return A.LastSelectedFrame < B.LastSelectedFrame; });
		const int32 ToDrop = Residents.Num() - MaxLoadedTiles;
		for (int32 i = 0; i < ToDrop; ++i) DestroyTileMesh(*Residents[i]);
	}
}

void AEarth4DGoogleTiles::DestroyTileMesh(FTile& Tile)
{
	if (UProceduralMeshComponent* PMC = Tile.Mesh.Get())
		PMC->DestroyComponent();
	Tile.Mesh = nullptr;
	Tile.State = EState::Unloaded;
}

void AEarth4DGoogleTiles::TearDownTree()
{
	TearDownTree(/*bKeepTree=*/false);
}

void AEarth4DGoogleTiles::TearDownTree(bool bKeepTree)
{
	if (!Root.IsValid()) return;
	TFunction<void(const TSharedPtr<FTile>&)> Walk = [&](const TSharedPtr<FTile>& T)
	{
		if (!T.IsValid()) return;
		DestroyTileMesh(*T);
		for (const TSharedPtr<FTile>& C : T->Children) Walk(C);
	};
	Walk(Root);
	if (!bKeepTree) { /* tree dropped by caller */ }
}

void AEarth4DGoogleTiles::Report(bool bOkConnected, const FString& Message)
{
	UE_LOG(LogTemp, Display, TEXT("[Earth4D] %s"), *Message);
	OnStatus.Broadcast(bOkConnected, Message);
}
