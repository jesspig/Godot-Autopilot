import net from "node:net";

type Mode = "pipe" | "tcp" | "unix";

interface Options {
  mode: Mode;
  name: string;
  port: number;
  path: string;
}

interface TestResult {
  name: string;
  ok: boolean;
  bytes: number;
  ms: number;
  error?: string;
}

class FrameReader {
  private buffer: Buffer = Buffer.alloc(0);
  private notify: (() => void) | null = null;
  private failure: Error | null = null;

  constructor(private readonly socket: net.Socket) {
    socket.on("data", (chunk: Buffer) => {
      this.buffer = Buffer.concat([this.buffer, chunk]);
      this.wake();
    });
    socket.on("error", (error: Error) => {
      this.failure = error;
      this.wake();
    });
    socket.on("close", () => {
      if (this.failure === null) this.failure = new Error("socket closed");
      this.wake();
    });
  }

  private wake(): void {
    const notify = this.notify;
    this.notify = null;
    notify?.();
  }

  async read(size: number): Promise<Buffer> {
    while (this.buffer.length < size) {
      if (this.failure !== null) throw this.failure;
      await new Promise<void>((resolve) => {
        this.notify = resolve;
      });
    }
    const out = this.buffer.subarray(0, size);
    this.buffer = this.buffer.subarray(size);
    return out;
  }
}

function parseArgs(argv: string[]): Options {
  const opts: Options = { mode: "tcp", name: "", port: 0, path: "" };
  for (let i = 0; i < argv.length; i += 1) {
    const arg = argv[i];
    if (arg === "--mode" && i + 1 < argv.length) {
      const mode = argv[++i];
      if (mode !== "pipe" && mode !== "tcp" && mode !== "unix") {
        throw new Error(`invalid mode: ${mode}`);
      }
      opts.mode = mode;
    } else if (arg === "--name" && i + 1 < argv.length) {
      opts.name = argv[++i];
    } else if (arg === "--port" && i + 1 < argv.length) {
      opts.port = Number(argv[++i]);
    } else if (arg === "--path" && i + 1 < argv.length) {
      opts.path = argv[++i];
    }
  }
  return opts;
}

function connect(opts: Options): Promise<net.Socket> {
  const socket =
    opts.mode === "tcp"
      ? net.createConnection({ host: "127.0.0.1", port: opts.port })
      : net.createConnection({
          path: opts.mode === "pipe" ? `\\\\.\\pipe\\${opts.name}` : opts.path,
        });
  return new Promise((resolve, reject) => {
    socket.once("connect", () => resolve(socket));
    socket.once("error", reject);
  });
}

function encode(payload: Buffer): Buffer {
  const header = Buffer.alloc(4);
  header.writeUInt32BE(payload.length, 0);
  return Buffer.concat([header, payload]);
}

async function request(
  socket: net.Socket,
  reader: FrameReader,
  payload: Buffer,
): Promise<Buffer> {
  socket.write(encode(payload));
  const header = await reader.read(4);
  const size = header.readUInt32BE(0);
  return await reader.read(size);
}

function withTimeout<T>(promise: Promise<T>, ms: number): Promise<T> {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error(`timeout after ${ms} ms`)), ms);
    promise.then(
      (value) => {
        clearTimeout(timer);
        resolve(value);
      },
      (error) => {
        clearTimeout(timer);
        reject(error);
      },
    );
  });
}

async function runTest(
  name: string,
  payload: Buffer,
  socket: net.Socket,
  reader: FrameReader,
  timeoutMs: number,
): Promise<TestResult> {
  const start = performance.now();
  try {
    const echoed = await withTimeout(request(socket, reader, payload), timeoutMs);
    const ok = Buffer.compare(echoed, payload) === 0;
    return {
      name,
      ok,
      bytes: payload.length,
      ms: Math.round(performance.now() - start),
      ...(ok ? {} : { error: "echo mismatch" }),
    };
  } catch (error) {
    return {
      name,
      ok: false,
      bytes: payload.length,
      ms: Math.round(performance.now() - start),
      error: error instanceof Error ? error.message : String(error),
    };
  }
}

async function runSequentialTests(
  socket: net.Socket,
  reader: FrameReader,
): Promise<TestResult[]> {
  const results: TestResult[] = [];
  results.push(
    await runTest(
      "json-small",
      Buffer.from(JSON.stringify({ jsonrpc: "2.0", id: 1, method: "ping" })),
      socket,
      reader,
      10_000,
    ),
  );
  results.push(
    await runTest("1mib", Buffer.alloc(1024 * 1024, 0x61), socket, reader, 15_000),
  );
  results.push(
    await runTest(
      "4mib",
      Buffer.alloc(4 * 1024 * 1024, 0x62),
      socket,
      reader,
      20_000,
    ),
  );
  for (let i = 0; i < 10; i += 1) {
    results.push(
      await runTest(
        `sequential-${i + 1}`,
        Buffer.alloc(64 * 1024, 0x63 + i),
        socket,
        reader,
        10_000,
      ),
    );
  }
  return results;
}

async function runConcurrentTests(opts: Options): Promise<TestResult[]> {
  const payload = Buffer.alloc(64 * 1024, 0x7a);
  const tasks = Array.from({ length: 10 }, async (_, index) => {
    const socket = await connect(opts);
    const reader = new FrameReader(socket);
    try {
      return await runTest(`concurrent-${index + 1}`, payload, socket, reader, 15_000);
    } finally {
      socket.destroy();
    }
  });
  return await Promise.all(tasks);
}

async function main(): Promise<number> {
  const opts = parseArgs(process.argv.slice(2));
  const results: TestResult[] = [];
  let connectError = "";

  try {
    const socket = await withTimeout(connect(opts), 10_000);
    const reader = new FrameReader(socket);
    results.push(...(await runSequentialTests(socket, reader)));
    socket.destroy();
  } catch (error) {
    connectError = error instanceof Error ? error.message : String(error);
  }

  if (connectError === "") {
    try {
      results.push(...(await runConcurrentTests(opts)));
    } catch (error) {
      results.push({
        name: "concurrent-connect",
        ok: false,
        bytes: 0,
        ms: 0,
        error: error instanceof Error ? error.message : String(error),
      });
    }
  }

  const ok = connectError === "" && results.length > 0 && results.every((item) => item.ok);
  const report = {
    schema_version: 1,
    mode: opts.mode,
    target:
      opts.mode === "tcp"
        ? `127.0.0.1:${opts.port}`
        : opts.mode === "pipe"
          ? `\\\\.\\pipe\\${opts.name}`
          : opts.path,
    ok,
    ...(connectError === "" ? {} : { connect_error: connectError }),
    tests: results,
  };
  process.stdout.write(`${JSON.stringify(report, null, 2)}\n`);
  return ok ? 0 : 1;
}

main().then(
  (code) => {
    process.exitCode = code;
  },
  (error) => {
    process.stderr.write(`${error instanceof Error ? error.stack ?? error.message : String(error)}\n`);
    process.exitCode = 1;
  },
);
