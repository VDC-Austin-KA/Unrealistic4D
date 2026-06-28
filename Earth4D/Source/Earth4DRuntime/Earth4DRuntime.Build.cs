// Copyright Earth4D. Licensed for project use.
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
			"HTTP",          // Claude Messages API client
			"Json",          // tool schema + request/response
			"JsonUtilities",
			"Projects",
			"DatasmithContent", // imported metadata (UDatasmithAssetUserData) for elements / TimeLiner linking
			"CesiumRuntime",    // Phase 2: ACesiumGeoreference / ACesium3DTileset (Google tiles) + geo conversions
		});

		// NOTE: CesiumRuntime is a direct dependency — the Earth4DTemplate project
		// already requires Cesium for Unreal. If you need the 4D core to compile in a
		// project WITHOUT Cesium installed, move "CesiumRuntime" out and wrap the
		// AEarth4DSite usage / includes behind a WITH_CESIUM define.

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
