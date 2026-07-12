// Copyright Earth4D. Licensed for project use.
using UnrealBuildTool;
using System.Collections.Generic;

// Editor target — opens the project + Earth4D authoring tools in the UE 5.8 editor.
public class Earth4DTemplateEditorTarget : TargetRules
{
	public Earth4DTemplateEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7; // 5.8: V7 adopts the engine's Error-level warning defaults (Unreachable/ReturnType/Dangling) so an installed-engine Editor target doesn't diverge on shared build products
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Earth4DTemplate");
	}
}
