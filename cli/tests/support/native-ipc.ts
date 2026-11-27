import net from "node:net";

export type NativeRequest = Record<string, unknown> & {
  jsonrpc?: string;
  id?: number;
  method?: string;
  params?: Record<string, unknown>;
};

export type NativeSend = (frame: unknown) => void;

export interface NativeServer {
  port: number;
  requests: NativeRequest[];
  close(): Promise<void>;
}

function encodeFrame(value: unknown): Buffer {
  const payload = Buffer.from(JSON.stringify(value), "utf8");
  const header = Buffer.alloc(4);
  header.writeUInt32BE(payload.length, 0);
  return Buffer.concat([header, payload]);
}

export async function startNativeServer(
  handler: (message: NativeRequest, socket: net.Socket, send: NativeSend) => void | Promise<void>,
): Promise<NativeServer> {
  const sockets = new Set<net.Socket>();
  const requests: NativeRequest[] = [];
  const server = net.createServer((socket) => {
    sockets.add(socket);
    socket.once("close", () => sockets.delete(socket));
    let buffer = Buffer.alloc(0);
    const send: NativeSend = (frame) => {
      if (!socket.destroyed) socket.write(encodeFrame(frame));
    };
    socket.on("data", (chunk: Buffer) => {
      buffer = Buffer.concat([buffer, chunk]);
      while (buffer.length >= 4) {
        const size = buffer.readUInt32BE(0);
        if (size > 4 * 1024 * 1024) return;
        if (buffer.length < 4 + size) return;
        const text = buffer.subarray(4, 4 + size).toString("utf8");
        buffer = buffer.subarray(4 + size);
        let message: NativeRequest;
        try {
          message = JSON.parse(text) as NativeRequest;
        } catch {
          continue;
        }
        requests.push(message);
        void handler(message, socket, send);
      }
    });
  });
  await new Promise<void>((resolve, reject) => {
    server.once("error", reject);
    server.listen(0, "127.0.0.1", resolve);
  });
  const address = server.address();
  if (address === null || typeof address === "string") {
    await new Promise<void>((resolve) => server.close(() => resolve()));
    throw new Error("native test server did not expose a port");
  }
  return {
    port: address.port,
    requests,
    close: () => {
      for (const socket of Array.from(sockets)) socket.destroy();
      return new Promise<void>((resolve) => server.close(() => resolve()));
    },
  };
}

export function isHandshake(message: NativeRequest): boolean {
  return message.method === "initialize" || message.method === "notifications/initialized";
}

export function sendHandshake(message: NativeRequest, send: NativeSend, version = "1"): void {
  if (message.method === "initialize") {
    send({
      jsonrpc: "2.0",
      id: message.id,
      result: {
        protocolVersion: version,
        capabilities: { tools: {} },
        serverInfo: { name: "gda-testbed", version: "1.0.0" },
      },
    });
  }
}

export function rpcResult(id: unknown, result: unknown): Record<string, unknown> {
  return { jsonrpc: "2.0", id, result };
}

export function rpcError(id: unknown, code: number, message: string, data?: unknown): Record<string, unknown> {
  return { jsonrpc: "2.0", id, error: data === undefined ? { code, message } : { code, message, data } };
}

export function contentResult(id: unknown, business: unknown, isError = false): Record<string, unknown> {
  return rpcResult(id, {
    isError,
    content: [{ type: "text", text: JSON.stringify(business) }],
  });
}

export function rawResult(id: unknown, business: unknown): Record<string, unknown> {
  return rpcResult(id, { content: [{ type: "text", text: JSON.stringify(business) }] });
}
