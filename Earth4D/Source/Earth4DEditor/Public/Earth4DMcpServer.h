// Copyright Earth4D. Licensed for project use.
// A minimal local MCP endpoint that exposes the SAME Earth4DTools tool surface to
// Claude Desktop/Code for edit-time control. The in-app chat (runtime) uses the
// Claude API directly; this server is the authoring-time equivalent.
#pragma once

#include "CoreMinimal.h"

class IHttpRouter;

class FEarth4DMcpServer
{
public:
	/** Start an HTTP listener serving the MCP tool list + tools/call. */
	void Start(uint32 Port);
	void Stop();

private:
	TSharedPtr<IHttpRouter> Router;
	uint32 BoundPort = 0;
	TArray<FHttpRouteHandle> RouteHandles;

	// Resolves the active editor-world UEarth4DSubsystem to dispatch tools against.
	class UEarth4DSubsystem* ResolveSubsystem() const;

	/** Handle one MCP JSON-RPC 2.0 request body; returns the response JSON string. */
	FString HandleJsonRpc(const FString& Body) const;
};
