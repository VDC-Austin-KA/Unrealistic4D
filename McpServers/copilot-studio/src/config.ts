import path from "node:path";

function required(name: string): string {
  const v = process.env[name];
  if (!v) {
    throw new Error(
      `Missing required environment variable ${name}. See McpServers/copilot-studio/README.md.`
    );
  }
  return v;
}

export interface ServerConfig {
  port: number;
  /** Absolute directory the file/command tools are confined to. */
  workspaceRoot: string;
  /** Entra ID (Azure AD) tenant that issues tokens. */
  tenantId: string;
  /** Expected audience of inbound tokens: the app registration's Application ID URI or client id. */
  audience: string[];
  /** Optional allowlist of caller app ids (azp/appid claim). Empty = any app in the tenant with a valid token. */
  allowedAppIds: string[];
  /** Public base URL of this server (used in OAuth resource metadata). */
  publicBaseUrl: string;
  /** Set EARTH4D_MCP_DISABLE_AUTH=1 ONLY for local development behind a private tunnel. */
  authDisabled: boolean;
  commandTimeoutMs: number;
  maxOutputBytes: number;
}

export function loadConfig(): ServerConfig {
  const authDisabled = process.env.EARTH4D_MCP_DISABLE_AUTH === "1";
  const tenantId = authDisabled ? (process.env.ENTRA_TENANT_ID ?? "") : required("ENTRA_TENANT_ID");
  const rawAudience = authDisabled ? (process.env.ENTRA_AUDIENCE ?? "") : required("ENTRA_AUDIENCE");

  // Accept both "api://<client-id>" and the bare client id, since Entra
  // issues either depending on how the scope was requested.
  const audience = new Set<string>();
  for (const a of rawAudience.split(",").map((s) => s.trim()).filter(Boolean)) {
    audience.add(a);
    if (a.startsWith("api://")) audience.add(a.slice("api://".length));
    else audience.add(`api://${a}`);
  }

  const port = Number(process.env.PORT ?? 3020);
  return {
    port,
    workspaceRoot: path.resolve(process.env.WORKSPACE_ROOT ?? path.resolve(process.cwd(), "..", "..")),
    tenantId,
    audience: [...audience],
    allowedAppIds: (process.env.ENTRA_ALLOWED_APP_IDS ?? "")
      .split(",")
      .map((s) => s.trim())
      .filter(Boolean),
    publicBaseUrl: (process.env.PUBLIC_BASE_URL ?? `http://localhost:${port}`).replace(/\/$/, ""),
    authDisabled,
    commandTimeoutMs: Number(process.env.COMMAND_TIMEOUT_MS ?? 600_000),
    maxOutputBytes: Number(process.env.MAX_OUTPUT_BYTES ?? 256_000),
  };
}
