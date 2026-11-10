import net from "node:net";

import { describe, expect, test } from "bun:test";

import {
  cachedEndpointAlive,
  endpointAlive,
  primeEndpointAlive,
  ENDPOINT_ALIVE_TTL_MS,
} from "../src/probe";

function listenLoopback(): Promise<{ server: net.Server; port: number }> {
  return new Promise((resolve, reject) => {
    const server = net.createServer();
    server.once("error", reject);
    server.listen(0, "127.0.0.1", () => {
      const address = server.address();
      if (address === null || typeof address === "string") {
        reject(new Error("no port"));
        return;
      }
      resolve({ server, port: address.port });
    });
  });
}

function closeServer(server: net.Server): Promise<void> {
  return new Promise((resolve) => server.close(() => resolve()));
}

describe("endpointAlive", () => {
  test("监听端口判活为 true 并写入缓存", async () => {
    const { server, port } = await listenLoopback();
    try {
      expect(await endpointAlive(port)).toBe(true);
      expect(cachedEndpointAlive(port)).toBe(true);
    } finally {
      await closeServer(server);
    }
  });

  test("无人监听判活为 false", async () => {
    const { server, port } = await listenLoopback();
    await closeServer(server);
    expect(await endpointAlive(port, 150)).toBe(false);
    expect(cachedEndpointAlive(port)).toBe(false);
  });

  test("缓存命中不再重新探测", async () => {
    const port = 65001;
    primeEndpointAlive(port, true);
    expect(cachedEndpointAlive(port)).toBe(true);
    expect(await endpointAlive(port, 50)).toBe(true);
    expect(ENDPOINT_ALIVE_TTL_MS).toBeGreaterThan(0);
  });

  test("未探测端口返回 null", () => {
    expect(cachedEndpointAlive(64999)).toBeNull();
  });
});