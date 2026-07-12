// Copyright Earth4D. Licensed for project use.
using System;
using UnrealBuildTool;

public class Earth4DRuntime : ModuleRules
{
	public Earth4DRuntime(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"HTTP",          // Claude Messages API client + Google Tiles root fetch
			"Json",          // tool schema + request/response
			"JsonUtilities",
			"Projects",
			"DeveloperSettings", // UEarth4DSettings (per-user API/tiles keys)
			"DatasmithContent", // imported metadata (UDatasmithAssetUserData) for elements / TimeLiner linking
			// ---- Native Google Photorealistic 3D Tiles (Cesium-free streaming) ----
			"GLTFCore",                 // engine glTF parser; decodes Draco-compressed primitives
			"ProceduralMeshComponent",  // builds the streamed tile geometry at runtime
			"RenderCore",               // RHI vertex formats used by the procedural mesh
		});

		// Cesium for Unreal is ON BY DEFAULT. Now that the Cesium plugin has shipped
		// for UE 5.8, AEarth4DSite uses Cesium's georeferencing + Google Photorealistic
		// 3D Tiles as the primary basemap path (the code behind WITH_EARTH4D_CESIUM).
		// The native Cesium-free Google Tiles loader remains as a fallback: opt OUT by
		// setting EARTH4D_DISABLE_CESIUM=1 before generating project files / building
		// (e.g. on an engine version that doesn't have the Cesium plugin installed).
		// EARTH4D_FORCE_CESIUM=1 is still honoured as an explicit opt-in for symmetry.
		bool bDisableCesium = (Environment.GetEnvironmentVariable("EARTH4D_DISABLE_CESIUM") == "1");
		bool bHasCesium = !bDisableCesium;
		if (bHasCesium)
		{
			PrivateDependencyModuleNames.Add("CesiumRuntime");
			PublicDefinitions.Add("WITH_EARTH4D_CESIUM=1");
		}
		else
		{
			PublicDefinitions.Add("WITH_EARTH4D_CESIUM=0");
		}

		// Editor-only: FlyToLatLon/FrameRegion move the level-editor viewport when not
		// in PIE. These deps are compiled out of the packaged standalone app (the
		// WITH_EDITOR code in Earth4DSubsystem.cpp is stripped), keeping the shipped
		// runtime free of editor modules.
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"UnrealEd",
				"LevelEditor",
			});
		}
	}
}
