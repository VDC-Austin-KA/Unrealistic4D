// Copyright Earth4D. Licensed for project use.
using UnrealBuildTool;

public class Earth4DEditor : ModuleRules
{
	public Earth4DEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Earth4DRuntime",
			"HTTPServer",   // local MCP endpoint (FHttpRouteHandle is used in a public header)
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore",
			"UMG",
			"UnrealEd",
			"ToolMenus",
			"WorkspaceMenuStructure",
			"EditorStyle",
			"InputCore",
			"AppFramework",  // OpenColorPicker (element colour override)
			"HTTP",
			"Json",
			"JsonUtilities",
		});
	}
}
