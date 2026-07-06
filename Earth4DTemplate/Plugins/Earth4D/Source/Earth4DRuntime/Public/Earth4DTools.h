// Copyright Earth4D. Licensed for project use.
// The single tool surface shared by the in-app Claude chat AND the editor MCP
// server. Each tool maps 1:1 onto a UEarth4DSubsystem command verb, so natural
// language and the UI can never drift apart.
#pragma once

#include "CoreMinimal.h"

class UEarth4DSubsystem;
class FJsonObject;

namespace Earth4DTools
{
	/** JSON array of tool definitions (Anthropic `tools` schema / MCP tool list). */
	EARTH4DRUNTIME_API FString GetToolsJson();

	/**
	 * Execute a tool call by name with JSON arguments against the command layer.
	 * Returns a human/agent-readable result string (used as the tool_result).
	 */
	EARTH4DRUNTIME_API FString Dispatch(const FString& ToolName, const TSharedPtr<FJsonObject>& Args, UEarth4DSubsystem* Subsystem);
}
