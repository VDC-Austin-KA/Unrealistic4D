// Copyright Earth4D. Licensed for project use.
using UnrealBuildTool;
using System.Collections.Generic;

// Editor target — opens the project + Earth4D authoring tools in the UE 5.8 editor.
public class Earth4DTemplateEditorTarget : TargetRules
{
	public Earth4DTemplateEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("Earth4DTemplate");
	}
}
