// Copyright Earth4D. Licensed for project use.
using UnrealBuildTool;
using System.Collections.Generic;

// Game target — builds the standalone packaged Earth4D app the whole team runs.
public class Earth4DTemplateTarget : TargetRules
{
	public Earth4DTemplateTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V6; // V6 = newest on UE 5.7 (V7 is 5.8-only)
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Earth4DTemplate");
	}
}
