// Copyright Earth4D. Licensed for project use.
using System;
using System.IO;
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
		});

		// Cesium for Unreal is OPTIONAL. As of UE 5.8 the Cesium plugin may not yet be
		// available; rather than hard-fail the build we auto-detect it. When present we
		// stream Google Photorealistic 3D Tiles + use Cesium's georeference. When absent
		// AEarth4DSite falls back to its own WGS84 ENU georeferencing (the 4D core works
		// fully; the native Google Map Tiles path supplies tiles). Toggle/force via the
		// EARTH4D_FORCE_CESIUM / EARTH4D_FORCE_NOCESIUM environment variables.
		bool bHasCesium = DetectCesium(Target);
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

	/** Find the CesiumForUnreal plugin among engine + project plugins (or honour overrides). */
	private static bool DetectCesium(ReadOnlyTargetRules Target)
	{
		if (Environment.GetEnvironmentVariable("EARTH4D_FORCE_NOCESIUM") == "1") return false;
		if (Environment.GetEnvironmentVariable("EARTH4D_FORCE_CESIUM") == "1") return true;

		try
		{
			var Roots = new System.Collections.Generic.List<string>();
			Roots.Add(Path.Combine(Unreal.EngineDirectory.FullName, "Plugins"));
			if (Target.ProjectFile != null)
				Roots.Add(Path.Combine(Target.ProjectFile.Directory.FullName, "Plugins"));

			foreach (string Root in Roots)
			{
				if (Directory.Exists(Root) &&
					Directory.GetFiles(Root, "CesiumForUnreal.uplugin", SearchOption.AllDirectories).Length > 0)
				{
					return true;
				}
			}
		}
		catch (Exception) { /* detection is best-effort; default to the Cesium-free path */ }
		return false;
	}
}
