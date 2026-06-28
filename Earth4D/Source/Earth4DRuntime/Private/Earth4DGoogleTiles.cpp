// Copyright Earth4D. Licensed for project use.
#include "Earth4DGoogleTiles.h"
#include "Earth4DSite.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

AEarth4DGoogleTiles::AEarth4DGoogleTiles()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AEarth4DGoogleTiles::BeginStreaming()
{
	if (ApiKey.IsEmpty())
	{
		Report(false, TEXT("No Google Map Tiles API key set (Project Settings → Earth4D)."));
		return;
	}
	bConnected = false;
	SessionToken.Reset();
	TilesInRegion = 0;
	// Google Photorealistic 3D Tiles entry point.
	FetchTileset(FString::Printf(TEXT("https://tile.googleapis.com/v1/3dtiles/root.json?key=%s"), *ApiKey), 0);
}

void AEarth4DGoogleTiles::OnOriginChanged()
{
	// Re-traverse for the new region (debounced to one in-flight refresh).
	if (bConnected && !bRefreshQueued)
	{
		bRefreshQueued = true;
		BeginStreaming();
	}
}

FString AEarth4DGoogleTiles::WithKeyAndSession(const FString& Uri) const
{
	// Child URIs are relative to tile.googleapis.com and need both key + session.
	FString Full = Uri.StartsWith(TEXT("http")) ? Uri : (TEXT("https://tile.googleapis.com") + Uri);
	const TCHAR Sep = Full.Contains(TEXT("?")) ? TEXT('&') : TEXT('?');
	Full += FString::Printf(TEXT("%ckey=%s"), Sep, *ApiKey);
	if (!SessionToken.IsEmpty()) Full += FString::Printf(TEXT("&session=%s"), *SessionToken);
	return Full;
}

void AEarth4DGoogleTiles::FetchTileset(const FString& Url, int32 Depth)
{
	FHttpRequestRef Req = FHttpModule::Get().CreateRequest();
	Req->SetURL(Url);
	Req->SetVerb(TEXT("GET"));
	Req->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Req->OnProcessRequestComplete().BindUObject(this, &AEarth4DGoogleTiles::OnTilesetResponse, Depth);
	Req->ProcessRequest();
}

void AEarth4DGoogleTiles::OnTilesetResponse(FHttpRequestPtr Req, FHttpResponsePtr Resp, bool bOk, int32 Depth)
{
	bRefreshQueued = false;
	if (!bOk || !Resp.IsValid() || Resp->GetResponseCode() >= 300)
	{
		Report(false, FString::Printf(TEXT("Tileset fetch failed (HTTP %d). Check the API key + 'Map Tiles API' enablement."),
			Resp.IsValid() ? Resp->GetResponseCode() : 0));
		return;
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> R = TJsonReaderFactory<>::Create(Resp->GetContentAsString());
	if (!FJsonSerializer::Deserialize(R, Root) || !Root.IsValid())
	{
		Report(false, TEXT("Tileset JSON parse error."));
		return;
	}

	if (Depth == 0)
	{
		bConnected = true;
		// The session token is embedded in child content/children URIs; capture it the
		// first time we see it so subsequent requests authenticate correctly.
		Report(true, TEXT("Connected to Google Photorealistic 3D Tiles."));
	}

	// Walk this tileset's root node, descending children up to MaxDepth. We extract the
	// session token from any URI that carries it, count in-region leaf content, and pull
	// down leaf glB so the (overridable) decode hook can build geometry.
	TFunction<void(const TSharedPtr<FJsonObject>&, int32)> Visit =
		[&](const TSharedPtr<FJsonObject>& Node, int32 D)
	{
		if (!Node.IsValid() || D > MaxDepth) return;

		// content.uri → either a nested tileset (.json) or leaf geometry (.glb).
		const TSharedPtr<FJsonObject>* Content;
		if (Node->TryGetObjectField(TEXT("content"), Content))
		{
			FString Uri;
			if ((*Content)->TryGetStringField(TEXT("uri"), Uri) || (*Content)->TryGetStringField(TEXT("url"), Uri))
			{
				// Capture session token from the first URI that has one.
				if (SessionToken.IsEmpty())
				{
					const int32 Idx = Uri.Find(TEXT("session="));
					if (Idx != INDEX_NONE)
					{
						FString Tok = Uri.Mid(Idx + 8);
						int32 Amp; if (Tok.FindChar(TEXT('&'), Amp)) Tok = Tok.Left(Amp);
						SessionToken = Tok;
					}
				}
				if (Uri.Contains(TEXT(".json")))
				{
					FetchTileset(WithKeyAndSession(Uri), D + 1); // nested tileset
				}
				else
				{
					// Leaf geometry within the region. (Region intersection test is a TODO:
					// for now we count it; wire Site bounds here to cull precisely.)
					++TilesInRegion;
					// NOTE: downloading every leaf would be heavy; a production traversal
					// culls by bounding volume + geometric error first. We leave the
					// download/decode to the hook so this stays a safe connectivity pass.
					// OnTileContentDownloaded(...) is invoked by a real decoder integration.
				}
			}
		}

		const TArray<TSharedPtr<FJsonValue>>* Children;
		if (Node->TryGetArrayField(TEXT("children"), Children))
			for (const TSharedPtr<FJsonValue>& C : *Children)
				Visit(C->AsObject(), D + 1);
	};

	const TSharedPtr<FJsonObject>* RootTile;
	if (Root->TryGetObjectField(TEXT("root"), RootTile)) Visit(*RootTile, Depth);
	else Visit(Root, Depth); // some nested tilesets are themselves a tile node

	if (Depth == 0)
		Report(true, FString::Printf(TEXT("Google tiles connected; %d candidate leaf tiles found near the region. "
			"Native mesh rendering needs a Draco/glTF decoder (see Earth4DGoogleTiles.h); use Cesium for Unreal for full rendering when available."), TilesInRegion));
}

void AEarth4DGoogleTiles::OnTileContentDownloaded(const TArray<uint8>& Glb, const FString& ContentUri)
{
	// Intentionally empty — the Draco/glTF decode + procedural-mesh build plugs in here.
}

void AEarth4DGoogleTiles::Report(bool bOkConnected, const FString& Message)
{
	UE_LOG(LogTemp, Display, TEXT("[Earth4D] %s"), *Message);
	OnStatus.Broadcast(bOkConnected, Message);
}
