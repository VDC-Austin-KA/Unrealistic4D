<!-- BEGIN HYPERAISTUDIO MANAGED BLOCK -->
# Claude Code HyperAIStudio Notes

- Project root: `C:/Users/minio/Source/Claude Co Work/Earth4D/Unrealistic4D/Earth4DTemplate/`
- Native MCP: `unreal-mcp` at `http://127.0.0.1:8000/mcp`
- Use native MCP for Unreal editor questions; do not hand-roll HTTP/PowerShell MCP calls.
- Connection check: use native MCP only; no PowerShell/HTTP fallback. Say connected and ready, then wait.
- If `unreal-mcp` tools are not available, say this agent session is probably not opened with the Unreal project as its working directory. Ask the user to open a new agent session from `C:/Users/minio/Source/Claude Co Work/Earth4D/Unrealistic4D/Earth4DTemplate/` so project MCP config loads; this is required for the quick native MCP workflow. Then wait.
- Fast paths: current level = `SceneTools.get_current_level`; actors = `SceneTools.find_actors` via `call_tool`.
- If Unreal is disconnected, run `.hyperai/Start-HyperAIStudioEditor.ps1`, then `.hyperai/Wait-HyperAIStudioMCP.ps1 -TimeoutSeconds 90`.
- Only read `.hyperai/context/` files when the prompt explicitly references them.
- Do not overlap Unreal MCP calls.
<!-- END HYPERAISTUDIO MANAGED BLOCK -->
