import type { NextFunction, Request, Response, Express } from "express";
import { createRemoteJWKSet, jwtVerify } from "jose";
import type { AuthInfo } from "@modelcontextprotocol/sdk/server/auth/types.js";
import type { ServerConfig } from "./config.js";

// Bearer-token middleware validating Microsoft Entra ID access tokens.
// Copilot Studio's MCP connector obtains these via the OAuth 2.0 flow you
// configure on the custom connector (see README), then sends them as
// Authorization: Bearer <token> on every MCP request.

declare global {
  // eslint-disable-next-line @typescript-eslint/no-namespace
  namespace Express {
    interface Request {
      auth?: AuthInfo;
    }
  }
}

export function installAuth(app: Express, config: ServerConfig): void {
  // RFC 9728 protected-resource metadata so MCP clients can discover the
  // authorization server without out-of-band configuration.
  app.get("/.well-known/oauth-protected-resource", (_req, res) => {
    res.json({
      resource: `${config.publicBaseUrl}/mcp`,
      authorization_servers: [
        `https://login.microsoftonline.com/${config.tenantId}/v2.0`,
      ],
      bearer_methods_supported: ["header"],
    });
  });

  if (config.authDisabled) {
    console.warn(
      "[auth] EARTH4D_MCP_DISABLE_AUTH=1 — requests are NOT authenticated. Dev-tunnel use only."
    );
    return;
  }

  const jwks = createRemoteJWKSet(
    new URL(`https://login.microsoftonline.com/${config.tenantId}/discovery/v2.0/keys`)
  );
  // Entra issues v1.0 or v2.0 tokens depending on the app manifest.
  const issuers = [
    `https://login.microsoftonline.com/${config.tenantId}/v2.0`,
    `https://sts.windows.net/${config.tenantId}/`,
  ];

  app.use("/mcp", async (req: Request, res: Response, next: NextFunction) => {
    const header = req.headers.authorization ?? "";
    const token = header.startsWith("Bearer ") ? header.slice("Bearer ".length) : "";
    if (!token) {
      return unauthorized(res, config, "missing bearer token");
    }
    try {
      const { payload } = await jwtVerify(token, jwks, {
        issuer: issuers,
        audience: config.audience,
      });
      const appId = (payload.azp ?? payload.appid) as string | undefined;
      if (config.allowedAppIds.length > 0 && (!appId || !config.allowedAppIds.includes(appId))) {
        return forbidden(res, `app ${appId ?? "<none>"} is not in ENTRA_ALLOWED_APP_IDS`);
      }
      req.auth = {
        token,
        clientId: appId ?? "unknown",
        scopes: typeof payload.scp === "string" ? payload.scp.split(" ") : [],
        expiresAt: payload.exp,
        extra: { payload },
      };
      next();
    } catch (err) {
      return unauthorized(res, config, err instanceof Error ? err.message : "invalid token");
    }
  });
}

function unauthorized(res: Response, config: ServerConfig, detail: string): void {
  res
    .status(401)
    .set(
      "WWW-Authenticate",
      `Bearer resource_metadata="${config.publicBaseUrl}/.well-known/oauth-protected-resource"`
    )
    .json({
      jsonrpc: "2.0",
      error: { code: -32001, message: `Unauthorized: ${detail}` },
      id: null,
    });
}

function forbidden(res: Response, detail: string): void {
  res.status(403).json({
    jsonrpc: "2.0",
    error: { code: -32003, message: `Forbidden: ${detail}` },
    id: null,
  });
}
