// Copyright Earth4D. Licensed for project use.
#include "Earth4DSettings.h"
#include "Misc/ConfigCacheIni.h"

UEarth4DSettings::UEarth4DSettings()
{
	// Migration: pick up legacy [Earth4D] values from DefaultEngine.ini if a user
	// already set them there, without overwriting an explicit per-user value.
	if (ClaudeApiKey.IsEmpty())
		GConfig->GetString(TEXT("Earth4D"), TEXT("ClaudeApiKey"), ClaudeApiKey, GEngineIni);
	FString IniModel;
	if (GConfig->GetString(TEXT("Earth4D"), TEXT("ClaudeModel"), IniModel, GEngineIni) && !IniModel.IsEmpty())
		ClaudeModel = IniModel;

	// Tiles keys: honour the legacy [Earth4D.Tiles] section the site originally read.
	if (GoogleMapTilesApiKey.IsEmpty())
		GConfig->GetString(TEXT("Earth4D.Tiles"), TEXT("GoogleMapTilesApiKey"), GoogleMapTilesApiKey, GEngineIni);
	if (CesiumIonToken.IsEmpty())
		GConfig->GetString(TEXT("Earth4D.Tiles"), TEXT("CesiumIonToken"), CesiumIonToken, GEngineIni);
}

const UEarth4DSettings* UEarth4DSettings::Get()
{
	return GetDefault<UEarth4DSettings>();
}

UEarth4DSettings* UEarth4DSettings::GetMutable()
{
	return GetMutableDefault<UEarth4DSettings>();
}

void UEarth4DSettings::SetClaudeApiKey(const FString& InKey)
{
	ClaudeApiKey = InKey;
	SaveUserConfig();
}

void UEarth4DSettings::SaveUserConfig()
{
	// TryUpdateDefaultConfigFile writes to the project's Config/DefaultEarth4D.ini.
	// Keep this file OUT of source control (see .gitignore) so keys never get committed.
#if WITH_EDITOR
	TryUpdateDefaultConfigFile();
#else
	// In a packaged build, persist to the per-user save config.
	SaveConfig(CPF_Config, *GGameUserSettingsIni);
#endif
}
