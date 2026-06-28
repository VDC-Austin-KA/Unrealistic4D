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
			"HTTP",
			"HTTPServer",   // local MCP endpoint
			"Json",
			"JsonUtilities",
		});
	}
}
