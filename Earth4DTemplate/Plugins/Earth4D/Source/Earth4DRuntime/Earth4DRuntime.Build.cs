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

		// Cesium for Unreal is OPTIONAL and OFF BY DEFAULT. As of UE 5.8 the Cesium
		// plugin is not yet released, so building against it (or even auto-detecting a
		// stale install for an older engine) breaks the build. We therefore default to
		// the Cesium-free path — AEarth4DSite uses its own WGS84 ENU georeferencing and
		// the native Google Map Tiles loader supplies the basemap. Opt IN explicitly,
		// only once Cesium ships for your engine version, by setting the environment
		// variable EARTH4D_FORCE_CESIUM=1 before generating project files / building.
		bool bHasCesium = (Environment.GetEnvironmentVariable("EARTH4D_FORCE_CESIUM") == "1");
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
