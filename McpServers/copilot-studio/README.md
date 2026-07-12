# Earth4D DevTools MCP server for Microsoft Copilot Studio

A standalone [Model Context Protocol](https://modelcontextprotocol.io) server over
**Streamable HTTP** that a Microsoft Copilot Studio agent connects to as a tool.
It gives the agent a full development loop over this repository — read/write/edit
files, search code, run shell commands (including the UE build scripts in
`Scripts/`), and fetch arbitrary URLs from the internet — secured with
**Microsoft Entra ID** bearer tokens.

> ⚠️ `run_command` executes arbitrary shell commands on the machine hosting this
> server. Run it only on a machine you intend Copilot Studio to control (e.g. the
> Earth4D Windows dev box), keep Entra auth enabled, and prefer setting
> `ENTRA_ALLOWED_APP_IDS` so only your connector's app registration can call it.

## Tools exposed

| Tool | Purpose |
| --- | --- |
| `workspace_info` | Orient: workspace root, git branch/status, recent commits, top-level layout |
| `read_file` | Read a text file (optional line range) |
| `write_file` | Create/overwrite a file |
| `edit_file` | Exact-string find/replace in a file |
| `list_directory` | List a directory |
| `search_code` | Regex search over tracked files (`git grep`) |
| `run_command` | Run a shell command in the workspace (builds, git, tests, debugging) |
| `web_fetch` | Fetch any http(s) URL — open internet access for docs/APIs |

All file and command tools are confined to `WORKSPACE_ROOT` (default: this repo's
root). `web_fetch` is the deliberate exception.

## Run it

```bash
cd McpServers/copilot-studio
npm install
npm run build
cp .env.example .env   # fill in values, or export them in the environment
npm start              # serves POST /mcp on PORT (default 3020)
```

`GET /healthz` is an unauthenticated liveness check. On the Earth4D dev box the
agent can compile with `run_command` → `Scripts\Build.bat /close` (the editor must
be closed; see the repo `CLAUDE.md` build notes).

Copilot Studio needs a **public HTTPS** URL. For development use a
[dev tunnel](https://learn.microsoft.com/azure/developer/dev-tunnels/) or ngrok
pointing at the port, and set `PUBLIC_BASE_URL` to the tunnel URL. For production,
host behind TLS (reverse proxy, container app, etc.).

## Entra ID setup (server side)

1. **Entra admin center → App registrations → New registration** — e.g.
   `Earth4D DevTools MCP`. Single tenant.
2. **Expose an API** → Set the Application ID URI (`api://<client-id>`) → **Add a
   scope**, e.g. `Mcp.Invoke` (admins and users can consent).
3. Configure this server:
   - `ENTRA_TENANT_ID` = Directory (tenant) ID
   - `ENTRA_AUDIENCE` = `api://<client-id>` from step 2

## Copilot Studio setup (client side)

1. Create a **second app registration** for the connector (e.g.
   `Copilot Studio → Earth4D MCP`), add a **client secret**, and under
   **API permissions** add the `Mcp.Invoke` delegated scope exposed by the server
   app (grant admin consent). Add the redirect URI Copilot Studio shows you
   (`https://global.consent.azure-apim.net/redirect`).
2. In **Copilot Studio → your agent → Tools → Add a tool → New tool → Model
   Context Protocol**:
   - Server URL: `https://<your-public-host>/mcp`
   - Authentication: **OAuth 2.0** → Microsoft Entra ID, with the connector app's
     client ID/secret, authorization URL
     `https://login.microsoftonline.com/<tenant>/oauth2/v2.0/authorize`, token URL
     `https://login.microsoftonline.com/<tenant>/oauth2/v2.0/token`, and scope
     `api://<server-client-id>/Mcp.Invoke`.
3. Optionally set `ENTRA_ALLOWED_APP_IDS=<connector-app-client-id>` on the server
   so tokens from any other app in the tenant are rejected.
4. In the agent's instructions, tell it to call `workspace_info` first, then use
   the file/command tools to implement and debug changes.

## Local smoke test (no Copilot Studio)

```bash
EARTH4D_MCP_DISABLE_AUTH=1 npm start
curl -s http://localhost:3020/mcp \
  -H 'content-type: application/json' -H 'accept: application/json, text/event-stream' \
  -d '{"jsonrpc":"2.0","id":1,"method":"tools/list","params":{}}'
```

`EARTH4D_MCP_DISABLE_AUTH=1` skips token validation — local/dev-tunnel use only,
never on an exposed host.
