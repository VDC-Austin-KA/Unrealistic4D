<!-- BEGIN HYPERAISTUDIO MANAGED BLOCK -->
# HyperAIStudio Agent Instructions

## Project

- Root: `C:/Users/minio/Source/Claude Co Work/Earth4D/Unrealistic4D/Earth4DTemplate/`
- Native MCP: `unreal-mcp` at `http://127.0.0.1:8000/mcp`

- Use native MCP for Unreal editor questions; do not hand-roll HTTP/PowerShell MCP calls.
- Connection check: use native MCP only; no PowerShell/HTTP fallback. Say connected and ready, then wait.
- If `unreal-mcp` tools are not available, say this agent session is probably not opened with the Unreal project as its working directory. Ask the user to open a new agent session from `C:/Users/minio/Source/Claude Co Work/Earth4D/Unrealistic4D/Earth4DTemplate/` so project MCP config loads; this is required for the quick native MCP workflow. Then wait.
- Use normal filesystem tools for source files.
- Do not overlap Unreal MCP calls.

## Fast Paths

- Current level: `call_tool` -> `editor_toolset.toolsets.scene.SceneTools` / `get_current_level` / `{}`.
- List actors: `call_tool` -> `editor_toolset.toolsets.scene.SceneTools` / `find_actors` / `{"name":"","tag":"","collision_channels":[]}`.

## Reconnect

- If Unreal is closed or disconnected, run `.hyperai/Start-HyperAIStudioEditor.ps1`.
- Then run `.hyperai/Wait-HyperAIStudioMCP.ps1 -TimeoutSeconds 90` before retrying MCP actions.

## Context Packs

- Only read `.hyperai/context/` files when the prompt explicitly references them.

## Extra MCP Servers

- `hyper-knowledge-mcp` priority 70: command python - Optional packaged knowledge/search MCP for HyperAIStudio project docs and curated context. Domains: docs, knowledge, hyperai. Instructions: Use Hyper Knowledge MCP for HyperAIStudio project docs and curated local context., Use unreal-mcp for live Unreal Editor actions..
- `hyper-ue-mcp` priority 95: command python - Bundled standalone Hyper UE MCP runtime owned by HyperAIStudio. It does not replace Epic Unreal MCP. Domains: unreal, editor, hyper-ue. Instructions: Use unreal-mcp as the locked primary Epic Unreal MCP server for normal editor actions., Use Hyper UE MCP when the task explicitly needs the expanded packaged 534-tool Hyper UE surface., Hyper UE tool calls target the local editor TCP bridge at 127.0.0.1:55557; the optional HTTP fallback bridge is http://127.0.0.1:8765 and should only be used when explicitly requested..

Use extra MCP servers only when relevant to the task.

<!-- END HYPERAISTUDIO MANAGED BLOCK -->
