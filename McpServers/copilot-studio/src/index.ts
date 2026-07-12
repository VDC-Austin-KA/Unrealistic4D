import express from "express";
import { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import { StreamableHTTPServerTransport } from "@modelcontextprotocol/sdk/server/streamableHttp.js";
import { loadConfig } from "./config.js";
import { installAuth } from "./auth.js";
import { registerTools } from "./tools.js";

// Streamable-HTTP MCP endpoint for Microsoft Copilot Studio. The server is
// stateless: each POST gets a fresh McpServer + transport pair, which is the
// pattern Copilot Studio's MCP connector expects (no session pinning needed).

const config = loadConfig();
const app = express();
app.use(express.json({ limit: "8mb" }));

app.get("/healthz", (_req, res) => {
  res.json({ ok: true, workspaceRoot: config.workspaceRoot });
});

installAuth(app, config);

app.post("/mcp", async (req, res) => {
  const server = new McpServer({
    name: "earth4d-devtools",
    version: "0.1.0",
  });
  registerTools(server, config);

  const transport = new StreamableHTTPServerTransport({
    sessionIdGenerator: undefined, // stateless mode
  });
  res.on("close", () => {
    transport.close();
    server.close();
  });

  try {
    await server.connect(transport);
    await transport.handleRequest(req, res, req.body);
  } catch (err) {
    console.error("[mcp] request failed:", err);
    if (!res.headersSent) {
      res.status(500).json({
        jsonrpc: "2.0",
        error: { code: -32603, message: "Internal server error" },
        id: null,
      });
    }
  }
});

// Stateless servers have nothing to GET/DELETE on the MCP endpoint.
const methodNotAllowed = (_req: express.Request, res: express.Response) => {
  res.status(405).json({
    jsonrpc: "2.0",
    error: { code: -32000, message: "Method not allowed" },
    id: null,
  });
};
app.get("/mcp", methodNotAllowed);
app.delete("/mcp", methodNotAllowed);

app.listen(config.port, () => {
  console.log(`earth4d-devtools MCP listening on :${config.port}`);
  console.log(`  endpoint:  ${config.publicBaseUrl}/mcp`);
  console.log(`  workspace: ${config.workspaceRoot}`);
  console.log(`  auth:      ${config.authDisabled ? "DISABLED (dev only)" : `Entra ID tenant ${config.tenantId}`}`);
});
