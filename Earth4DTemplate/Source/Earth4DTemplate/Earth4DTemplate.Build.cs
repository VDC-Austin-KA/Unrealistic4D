// Copyright Earth4D. Licensed for project use.
using UnrealBuildTool;

// The template's (thin) primary game module. The 4D system lives in the Earth4D
// plugin; this module exists so the project is a C++ project that can be packaged
// into the standalone app and so the plugin's runtime module is pulled in.
public class Earth4DTemplate : ModuleRules
{
	public Earth4DTemplate(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Earth4DRuntime", // the 4D command layer + runtime
		});
	}
}
