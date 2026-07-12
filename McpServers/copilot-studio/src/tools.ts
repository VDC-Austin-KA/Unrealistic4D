import { exec } from "node:child_process";
import fs from "node:fs/promises";
import path from "node:path";
import { z } from "zod";
import type { McpServer } from "@modelcontextprotocol/sdk/server/mcp.js";
import type { ServerConfig } from "./config.js";

// Development tool surface exposed to the Copilot Studio agent. Everything
// file- or command-shaped is confined to config.workspaceRoot; web_fetch is
// the deliberate exception (open internet access for research/docs).

function textResult(text: string) {
  return { content: [{ type: "text" as const, text }] };
}

function errorResult(text: string) {
  return { content: [{ type: "text" as const, text }], isError: true };
}

function resolveInWorkspace(root: string, p: string): string {
  const abs = path.resolve(root, p);
  if (abs !== root && !abs.startsWith(root + path.sep)) {
    throw new Error(`Path escapes the workspace root: ${p}`);
  }
  return abs;
}

function truncate(text: string, maxBytes: number): string {
  const buf = Buffer.from(text, "utf8");
  if (buf.byteLength <= maxBytes) return text;
  return (
    buf.subarray(0, maxBytes).toString("utf8") +
    `\n… [truncated: output exceeded ${maxBytes} bytes]`
  );
}

function runShell(
  command: string,
  cwd: string,
  timeoutMs: number,
  maxOutputBytes: number
): Promise<string> {
  return new Promise((resolve) => {
    exec(
      command,
      { cwd, timeout: timeoutMs, maxBuffer: 16 * 1024 * 1024, windowsHide: true },
      (err, stdout, stderr) => {
        const parts = [
          stdout && `stdout:\n${stdout}`,
          stderr && `stderr:\n${stderr}`,
          err && `exit: ${err.code ?? `signal ${err.signal}`}${err.killed ? " (timed out)" : ""}`,
        ].filter(Boolean);
        resolve(truncate(parts.join("\n\n") || "(no output, exit 0)", maxOutputBytes));
      }
    );
  });
}

export function registerTools(server: McpServer, config: ServerConfig): void {
  const root = config.workspaceRoot;

  server.registerTool(
    "read_file",
    {
      title: "Read file",
      description:
        "Read a UTF-8 text file from the workspace. Optionally pass offset/limit (1-based line range) for large files.",
      inputSchema: {
        path: z.string().describe("Path relative to the workspace root"),
        offset: z.number().int().min(1).optional().describe("First line to return (1-based)"),
        limit: z.number().int().min(1).optional().describe("Maximum number of lines to return"),
      },
    },
    async ({ path: p, offset, limit }) => {
      try {
        const abs = resolveInWorkspace(root, p);
        let text = await fs.readFile(abs, "utf8");
        if (offset || limit) {
          const lines = text.split("\n");
          const start = (offset ?? 1) - 1;
          text = lines.slice(start, limit ? start + limit : undefined).join("\n");
        }
        return textResult(truncate(text, config.maxOutputBytes));
      } catch (err) {
        return errorResult(String(err));
      }
    }
  );

  server.registerTool(
    "write_file",
    {
      title: "Write file",
      description:
        "Create or overwrite a UTF-8 text file in the workspace. Parent directories are created as needed.",
      inputSchema: {
        path: z.string().describe("Path relative to the workspace root"),
        content: z.string().describe("Full file content to write"),
      },
    },
    async ({ path: p, content }) => {
      try {
        const abs = resolveInWorkspace(root, p);
        await fs.mkdir(path.dirname(abs), { recursive: true });
        await fs.writeFile(abs, content, "utf8");
        return textResult(`Wrote ${Buffer.byteLength(content, "utf8")} bytes to ${p}`);
      } catch (err) {
        return errorResult(String(err));
      }
    }
  );

  server.registerTool(
    "edit_file",
    {
      title: "Edit file",
      description:
        "Replace an exact string in a workspace file. Fails if the string is missing, or ambiguous without replace_all.",
      inputSchema: {
        path: z.string().describe("Path relative to the workspace root"),
        old_string: z.string().describe("Exact text to find"),
        new_string: z.string().describe("Replacement text"),
        replace_all: z.boolean().optional().describe("Replace every occurrence (default: false)"),
      },
    },
    async ({ path: p, old_string, new_string, replace_all }) => {
      try {
        const abs = resolveInWorkspace(root, p);
        const text = await fs.readFile(abs, "utf8");
        const count = text.split(old_string).length - 1;
        if (count === 0) return errorResult(`old_string not found in ${p}`);
        if (count > 1 && !replace_all) {
          return errorResult(
            `old_string occurs ${count} times in ${p}; pass replace_all or a longer unique string`
          );
        }
        const updated = replace_all
          ? text.split(old_string).join(new_string)
          : text.replace(old_string, new_string);
        await fs.writeFile(abs, updated, "utf8");
        return textResult(`Replaced ${replace_all ? count : 1} occurrence(s) in ${p}`);
      } catch (err) {
        return errorResult(String(err));
      }
    }
  );

  server.registerTool(
    "list_directory",
    {
      title: "List directory",
      description: "List entries of a workspace directory (name, kind, size).",
      inputSchema: {
        path: z.string().optional().describe("Directory relative to the workspace root (default: root)"),
      },
    },
    async ({ path: p }) => {
      try {
        const abs = resolveInWorkspace(root, p ?? ".");
        const entries = await fs.readdir(abs, { withFileTypes: true });
        const lines = await Promise.all(
          entries.map(async (e) => {
            if (e.isDirectory()) return `${e.name}/`;
            const stat = await fs.stat(path.join(abs, e.name)).catch(() => null);
            return `${e.name}  (${stat?.size ?? "?"} bytes)`;
          })
        );
        return textResult(lines.sort().join("\n") || "(empty)");
      } catch (err) {
        return errorResult(String(err));
      }
    }
  );

  server.registerTool(
    "search_code",
    {
      title: "Search code",
      description:
        "Regex search across workspace files using git grep (tracked files) with line numbers. Use pathspec to narrow, e.g. 'Earth4D/Source/**/*.cpp'.",
      inputSchema: {
        pattern: z.string().describe("Extended regex to search for"),
        pathspec: z.string().optional().describe("Optional git pathspec to limit the search"),
      },
    },
    async ({ pattern, pathspec }) => {
      const quoted = `'${pattern.replaceAll("'", `'\\''`)}'`;
      const spec = pathspec ? ` -- '${pathspec.replaceAll("'", `'\\''`)}'` : "";
      const out = await runShell(
        `git grep -nE ${quoted}${spec} || echo '(no matches)'`,
        root,
        60_000,
        config.maxOutputBytes
      );
      return textResult(out);
    }
  );

  server.registerTool(
    "run_command",
    {
      title: "Run command",
      description:
        "Run a shell command inside the workspace (build scripts, git, tests, debuggers). Returns stdout, stderr, and exit status. Long builds are allowed up to the server's timeout — e.g. 'Scripts/Build.bat /close' compiles the Earth4D UE plugin on the Windows host.",
      inputSchema: {
        command: z.string().describe("Shell command line to execute"),
        cwd: z.string().optional().describe("Working directory relative to the workspace root (default: root)"),
        timeout_ms: z
          .number()
          .int()
          .min(1000)
          .optional()
          .describe(`Timeout in ms (default/max ${config.commandTimeoutMs})`),
      },
    },
    async ({ command, cwd, timeout_ms }) => {
      try {
        const dir = resolveInWorkspace(root, cwd ?? ".");
        const timeout = Math.min(timeout_ms ?? config.commandTimeoutMs, config.commandTimeoutMs);
        return textResult(await runShell(command, dir, timeout, config.maxOutputBytes));
      } catch (err) {
        return errorResult(String(err));
      }
    }
  );

  server.registerTool(
    "web_fetch",
    {
      title: "Fetch URL",
      description:
        "Fetch an http(s) URL from the open internet and return the response body as text (for docs, APIs, package registries). Response is truncated to the server's output cap.",
      inputSchema: {
        url: z.string().url().describe("Absolute http(s) URL"),
        method: z.enum(["GET", "POST"]).optional().describe("HTTP method (default GET)"),
        body: z.string().optional().describe("Request body for POST"),
        content_type: z.string().optional().describe("Content-Type header for POST bodies"),
      },
    },
    async ({ url, method, body, content_type }) => {
      try {
        const parsed = new URL(url);
        if (parsed.protocol !== "http:" && parsed.protocol !== "https:") {
          return errorResult("Only http/https URLs are supported");
        }
        const controller = new AbortController();
        const timer = setTimeout(() => controller.abort(), 60_000);
        try {
          const res = await fetch(url, {
            method: method ?? "GET",
            body,
            headers: content_type ? { "content-type": content_type } : undefined,
            signal: controller.signal,
            redirect: "follow",
          });
          const text = await res.text();
          return textResult(
            `HTTP ${res.status} ${res.statusText}\ncontent-type: ${res.headers.get("content-type") ?? "?"}\n\n` +
              truncate(text, config.maxOutputBytes)
          );
        } finally {
          clearTimeout(timer);
        }
      } catch (err) {
        return errorResult(String(err));
      }
    }
  );

  server.registerTool(
    "workspace_info",
    {
      title: "Workspace info",
      description:
        "Summarize the workspace: root path, git branch/status, and top-level layout. Call this first to orient.",
      inputSchema: {},
    },
    async () => {
      const out = await runShell(
        "git status --short --branch && echo --- && git log --oneline -5 && echo --- && ls",
        root,
        30_000,
        config.maxOutputBytes
      );
      return textResult(`workspace root: ${root}\n\n${out}`);
    }
  );
}
