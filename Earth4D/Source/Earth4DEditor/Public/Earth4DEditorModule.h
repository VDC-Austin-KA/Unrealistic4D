// Copyright Earth4D. Licensed for project use.
#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

class FEarth4DMcpServer;

/**
 * Editor module: registers the dockable Earth4D tab (Gantt + element editor +
 * chat authoring) and starts the local MCP server that exposes the command-layer
 * tools to Claude Desktop/Code at authoring time.
 */
class FEarth4DEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	static const FName TabId;

private:
	TSharedRef<class SDockTab> SpawnTab(const class FSpawnTabArgs& Args);
	TSharedPtr<FEarth4DMcpServer> McpServer;
};
