// Copyright Earth4D. Licensed for project use.
#include "Earth4DEditorModule.h"
#include "Earth4DMcpServer.h"
#include "SEarth4DGanttPanel.h"
#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "ToolMenus.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "Earth4DEditor"

const FName FEarth4DEditorModule::TabId(TEXT("Earth4DPanel"));

void FEarth4DEditorModule::StartupModule()
{
	FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
			TabId, FOnSpawnTab::CreateRaw(this, &FEarth4DEditorModule::SpawnTab))
		.SetDisplayName(LOCTEXT("Earth4DTab", "Earth4D"))
		.SetTooltipText(LOCTEXT("Earth4DTabTip", "4D construction sequencing"))
		.SetGroup(WorkspaceMenu::GetMenuStructure().GetToolsCategory());

	// Start the local MCP server so Claude Desktop/Code can drive the editor.
	McpServer = MakeShared<FEarth4DMcpServer>();
	McpServer->Start(8765);
}

void FEarth4DEditorModule::ShutdownModule()
{
	if (McpServer.IsValid()) { McpServer->Stop(); McpServer.Reset(); }
	if (FSlateApplication::IsInitialized())
	{
		FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabId);
	}
}

TSharedRef<SDockTab> FEarth4DEditorModule::SpawnTab(const FSpawnTabArgs&)
{
	return SNew(SDockTab)
		.TabRole(ETabRole::NomadTab)
		[
			SNew(SEarth4DGanttPanel)
		];
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FEarth4DEditorModule, Earth4DEditor)
