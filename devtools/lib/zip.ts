import fs from "node:fs";
import { deflateRawSync, inflateRawSync } from "node:zlib";

const CRC_TABLE = (() => {
  const table = new Uint32Array(256);
  for (let index = 0; index < 256; index += 1) {
    let value = index;
    for (let bit = 0; bit < 8; bit += 1) {
      value = (value & 1) !== 0 ? 0xedb88320 ^ (value >>> 1) : value >>> 1;
    }
    table[index] = value >>> 0;
  }
  return table;
})();

function crc32(data: Uint8Array): number {
  let value = 0xffffffff;
  for (const byte of data) value = (CRC_TABLE[(value ^ byte) & 0xff] ^ (value >>> 8)) >>> 0;
  return (value ^ 0xffffffff) >>> 0;
}

export type ZipSourceEntry = {
  name: string;
  data: Uint8Array;
  mtime: Date;
};

export type ZipArchiveEntry = {
  name: string;
  data: Buffer;
};

function dosDateTime(mtime: Date): { time: number; date: number } {
  const year = Math.max(mtime.getFullYear(), 1980);
  const time =
    (mtime.getHours() << 11) | (mtime.getMinutes() << 5) | Math.floor(mtime.getSeconds() / 2);
  const date = ((year - 1980) << 9) | ((mtime.getMonth() + 1) << 5) | mtime.getDate();
  return { time, date };
}

function isAscii(value: string): boolean {
  return /^[\x20-\x7e]*$/.test(value);
}

export function writeZip(archivePath: string, entries: ZipSourceEntry[]): void {
  const localParts: Buffer[] = [];
  const centralParts: Buffer[] = [];
  let offset = 0;
  for (const entry of entries) {
    const nameBytes = Buffer.from(entry.name, "utf8");
    const raw = Buffer.from(entry.data);
    const isDirectory = entry.name.endsWith("/");
    const compressed = isDirectory ? raw : deflateRawSync(raw);
    const useDeflate = !isDirectory && compressed.length < raw.length;
    const payload = useDeflate ? compressed : raw;
    const method = useDeflate ? 8 : 0;
    const checksum = crc32(raw);
    const flags = isAscii(entry.name) ? 0 : 0x0800;
    const stamp = dosDateTime(entry.mtime);

    const local = Buffer.alloc(30);
    local.writeUInt32LE(0x04034b50, 0);
    local.writeUInt16LE(20, 4);
    local.writeUInt16LE(flags, 6);
    local.writeUInt16LE(method, 8);
    local.writeUInt16LE(stamp.time, 10);
    local.writeUInt16LE(stamp.date, 12);
    local.writeUInt32LE(checksum, 14);
    local.writeUInt32LE(payload.length, 18);
    local.writeUInt32LE(raw.length, 22);
    local.writeUInt16LE(nameBytes.length, 26);
    local.writeUInt16LE(0, 28);
    localParts.push(local, nameBytes, payload);

    const central = Buffer.alloc(46);
    central.writeUInt32LE(0x02014b50, 0);
    central.writeUInt16LE(0x0314, 4);
    central.writeUInt16LE(20, 6);
    central.writeUInt16LE(flags, 8);
    central.writeUInt16LE(method, 10);
    central.writeUInt16LE(stamp.time, 12);
    central.writeUInt16LE(stamp.date, 14);
    central.writeUInt32LE(checksum, 16);
    central.writeUInt32LE(payload.length, 20);
    central.writeUInt32LE(raw.length, 24);
    central.writeUInt16LE(nameBytes.length, 28);
    central.writeUInt16LE(0, 30);
    central.writeUInt16LE(0, 32);
    central.writeUInt16LE(0, 34);
    central.writeUInt16LE(0, 36);
    central.writeUInt32LE(isDirectory ? ((0o40755 << 16) | 0x10) >>> 0 : (0o100644 << 16) >>> 0, 38);
    central.writeUInt32LE(offset, 42);
    centralParts.push(central, nameBytes);

    offset += local.length + nameBytes.length + payload.length;
  }

  const centralDirectory = Buffer.concat(centralParts);
  const localDirectory = Buffer.concat(localParts);
  const end = Buffer.alloc(22);
  end.writeUInt32LE(0x06054b50, 0);
  end.writeUInt16LE(0, 4);
  end.writeUInt16LE(0, 6);
  end.writeUInt16LE(entries.length, 8);
  end.writeUInt16LE(entries.length, 10);
  end.writeUInt32LE(centralDirectory.length, 12);
  end.writeUInt32LE(localDirectory.length, 16);
  end.writeUInt16LE(0, 18);

  fs.writeFileSync(archivePath, Buffer.concat([localDirectory, centralDirectory, end]));
}

function findEndOfCentralDirectory(buffer: Buffer): number {
  if (buffer.length < 22) return -1;
  const start = Math.max(0, buffer.length - 22 - 0xffff);
  for (let offset = buffer.length - 22; offset >= start; offset -= 1) {
    if (buffer.readUInt32LE(offset) === 0x06054b50) return offset;
  }
  return -1;
}

function readEntryData(
  buffer: Buffer,
  localOffset: number,
  method: number,
  compressedSize: number,
  uncompressedSize: number,
  checksum: number,
  name: string,
): Buffer {
  if (buffer.readUInt32LE(localOffset) !== 0x04034b50) {
    throw new Error(`broken local header for ${name}`);
  }
  const nameLength = buffer.readUInt16LE(localOffset + 26);
  const extraLength = buffer.readUInt16LE(localOffset + 28);
  const dataStart = localOffset + 30 + nameLength + extraLength;
  const payload = buffer.subarray(dataStart, dataStart + compressedSize);
  let data: Buffer;
  if (method === 0) data = Buffer.from(payload);
  else if (method === 8) data = inflateRawSync(payload);
  else throw new Error(`unsupported compression method ${method} for ${name}`);
  if (data.length !== uncompressedSize) throw new Error(`size mismatch for ${name}`);
  if (crc32(data) !== checksum) throw new Error(`crc mismatch for ${name}`);
  return data;
}

export function readZip(archivePath: string): ZipArchiveEntry[] {
  const buffer = fs.readFileSync(archivePath);
  const end = findEndOfCentralDirectory(buffer);
  if (end < 0) throw new Error("end of central directory not found");
  const count = buffer.readUInt16LE(end + 10);
  const directoryOffset = buffer.readUInt32LE(end + 16);
  if (directoryOffset === 0xffffffff) throw new Error("zip64 archives are not supported");
  const entries: ZipArchiveEntry[] = [];
  let cursor = directoryOffset;
  for (let index = 0; index < count; index += 1) {
    if (buffer.readUInt32LE(cursor) !== 0x02014b50) throw new Error("broken central directory");
    const method = buffer.readUInt16LE(cursor + 10);
    const checksum = buffer.readUInt32LE(cursor + 16);
    const compressedSize = buffer.readUInt32LE(cursor + 20);
    const uncompressedSize = buffer.readUInt32LE(cursor + 24);
    const nameLength = buffer.readUInt16LE(cursor + 28);
    const extraLength = buffer.readUInt16LE(cursor + 30);
    const commentLength = buffer.readUInt16LE(cursor + 32);
    const localOffset = buffer.readUInt32LE(cursor + 42);
    const name = buffer.subarray(cursor + 46, cursor + 46 + nameLength).toString("utf8");
    if (name.endsWith("/")) {
      entries.push({ name, data: Buffer.alloc(0) });
    } else {
      entries.push({
        name,
        data: readEntryData(
          buffer,
          localOffset,
          method,
          compressedSize,
          uncompressedSize,
          checksum,
          name,
        ),
      });
    }
    cursor += 46 + nameLength + extraLength + commentLength;
  }
  return entries;
}
