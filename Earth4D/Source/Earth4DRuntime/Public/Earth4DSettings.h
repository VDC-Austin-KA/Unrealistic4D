// Copyright Earth4D. Licensed for project use.
// Per-user settings for the in-app Claude chat and the editor MCP server. The API
// key is NEVER hard-coded or committed: it is entered in-app and saved to the
// per-user config (see SaveUserConfig). Surfaces in Project Settings → Plugins →
// Earth4D at edit time, and is readable at runtime in the packaged standalone app.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "Earth4DSettings.generated.h"

UCLASS(config = Earth4D, defaultconfig, meta = (DisplayName = "Earth4D"))
class EARTH4DRUNTIME_API UEarth4DSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UEarth4DSettings();

	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }

	/** Anthropic API key for the in-app chat. Stored per-user, never committed. */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Claude", meta = (PasswordField = true))
	FString ClaudeApiKey;

	/** Claude model id for the in-app chat. */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Claude")
	FString ClaudeModel = TEXT("claude-opus-4-8");

	/** Max tokens per chat turn. */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Claude", meta = (ClampMin = "256"))
	int32 ChatMaxTokens = 2048;

	/** Local port the editor MCP server binds (Claude Desktop/Code authoring). */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "MCP", meta = (ClampMin = "1024", ClampMax = "65535"))
	int32 McpServerPort = 8765;

	// ---- Tiles (Google Photorealistic 3D Tiles) ----
	/** Direct Google Map Tiles API key (used by the Cesium-free tiles path and by Cesium's Google source). */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Tiles", meta = (PasswordField = true))
	FString GoogleMapTilesApiKey;

	/** Cesium ion access token (only used when Cesium for Unreal is installed and bUseCesiumIon is set on the site). */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Tiles", meta = (PasswordField = true))
	FString CesiumIonToken;

	/** Convenience accessor. */
	UFUNCTION(BlueprintPure, Category = "Earth4D") static const UEarth4DSettings* Get();
	UFUNCTION(BlueprintCallable, Category = "Earth4D") static UEarth4DSettings* GetMutable();

	/** Set the API key (e.g. from the in-app chat) and persist it to the per-user config. */
	UFUNCTION(BlueprintCallable, Category = "Earth4D") void SetClaudeApiKey(const FString& InKey);
	/** Write the current settings to the per-user config file. */
	void SaveUserConfig();
};
