// Copyright Earth4D. Licensed for project use.
using UnrealBuildTool;
using System.Collections.Generic;

// Game target — builds the standalone packaged Earth4D app the whole team runs.
public class Earth4DTemplateTarget : TargetRules
{
	public Earth4DTemplateTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V7; // 5.8: V7 adopts the engine's Error-level warning defaults (Unreachable/ReturnType/Dangling) so an installed-engine target doesn't diverge on shared build products
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Earth4DTemplate");
	}
}
