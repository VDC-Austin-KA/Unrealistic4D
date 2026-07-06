# HyperAIStudio Test Commands

Run these from the project root or through the HyperAIStudio panel.

## Test connection in terminal
Runs the project-local MCP readiness probe and writes .hyperai/hyperai-status.json.


```powershell
powershell -ExecutionPolicy Bypass -File 'C:/Users/minio/Source/Claude Co Work/Earth4D/Unrealistic4D/Earth4DTemplate/.hyperai/Wait-HyperAIStudioMCP.ps1' -Port 8000 -UrlPath '/mcp' -TimeoutSeconds 90
```

Prompt intent:

Run the terminal readiness test, then report whether Unreal MCP is reachable.

## Test in agent app
Asks the external agent to confirm unreal-mcp connectivity without editing the project.


Prompt intent:

Use native `unreal-mcp` only. Say connected and ready, then wait.

## Current level and actors
Asks the agent to inspect the open level and selected actors through Unreal MCP.


Prompt intent:

Use native MCP only. Call `editor_toolset.toolsets.scene.SceneTools.get_current_level`, then `editor_toolset.toolsets.scene.SceneTools.find_actors` with `{ "name": "", "tag": "", "collision_channels": [] }`. Return the level and actor names.

## Inspect agent instructions
Checks that generated instructions exist and are clear.


Prompt intent:

Check the generated instruction file for your agent and mention any missing setup files.

## Instruction file check
Checks that generated agent rules point at native Unreal MCP.


Prompt intent:

Check the generated instruction file for your agent and tell me whether it names native `unreal-mcp` and the direct level/actor fast paths.

## Reconnect after crash
Exercises the start/wait/reconnect instructions without assuming a persistent session.


Prompt intent:

Explain the exact steps you would take if Unreal Editor crashed, including which .hyperai scripts to run and how you reconnect to unreal-mcp.

