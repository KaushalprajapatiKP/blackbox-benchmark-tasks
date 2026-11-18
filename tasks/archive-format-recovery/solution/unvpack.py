#!/usr/bin/env python3
# unvpack - reference decoder for vpack archives (format version 3).
# Copyright (c) 2026 the task author. MIT License.
#
# Usage: unvpack ARCHIVE OUTDIR
import os
import struct
import sys
import zlib

MAGIC_HEAD = b"\x89VPK"
MAGIC_TAIL = b"\x1aKPV"
FOOTER_LEN = 20
FORMAT_VERSION = 3

HF_NAMETAB = 0x01
HF_TABLZ = 0x02

EK_FILE, EK_DIR, EK_LINK = 1, 2, 3
EK_NSEC = 0x04
EK_LZ = 0x08
EK_SHARED = 0x10
EK_NAMEREF = 0x20

DEFAULT_FILE_MODE = 0o644
DEFAULT_DIR_MODE = 0o755
LZ_BLOCK = 16384

# Strict mode also rejects structurally odd but checksum-consistent input.
STRICT = os.environ.get("UNVPACK_LENIENT") != "1"


class Corrupt(Exception):
    pass


class Reader:
    def __init__(self, data, pos=0, end=None):
        self.data = data
        self.pos = pos
        self.end = len(data) if end is None else end

    def u8(self):
        if self.pos >= self.end:
            raise Corrupt("unexpected end of data")
        v = self.data[self.pos]
        self.pos += 1
        return v

    def take(self, n):
        if n < 0 or self.pos + n > self.end:
            raise Corrupt("unexpected end of data")
        v = self.data[self.pos:self.pos + n]
        self.pos += n
        return bytes(v)

    def varint(self):
        c = self.u8()
        v = c & 0x7F
        count = 1
        while c & 0x80:
            count += 1
            if count > 10:
                raise Corrupt("varint too long")
            c = self.u8()
            v = ((v + 1) << 7) | (c & 0x7F)
        if v >= 1 << 64:
            raise Corrupt("varint out of range")
        return v

    def done(self):
        return self.pos >= self.end


def unzigzag(v):
    return (v >> 1) ^ -(v & 1)


def lz_decode(r, size):
    """Decode an LZ block stream from reader r producing exactly size bytes."""
    out = bytearray()
    while len(out) < size:
        h = r.varint()
        clen = h >> 1
        ulen = min(LZ_BLOCK, size - len(out))
        if h & 1:
            if clen != ulen:
                raise Corrupt("raw block length mismatch")
            out += r.take(clen)
            continue
        blk = r.take(clen)
        target = len(out) + ulen
        i = 0
        n = len(blk)
        while len(out) < target:
            if i >= n:
                raise Corrupt("LZ block too short")
            ctrl = blk[i]
            i += 1
            for bit in range(8):
                if len(out) >= target:
                    break
                if (ctrl >> bit) & 1:
                    if i >= n:
                        raise Corrupt("LZ block too short")
                    b0 = blk[i]
                    if b0 < 0x80:
                        if i + 2 > n:
                            raise Corrupt("LZ block too short")
                        ln = (b0 >> 4) + 3
                        dist = (((b0 & 0x0F) << 8) | blk[i + 1]) + 1
                        i += 2
                    else:
                        if i + 3 > n:
                            raise Corrupt("LZ block too short")
                        lcode = b0 & 0x7F
                        dist = (blk[i + 1] | (blk[i + 2] << 8)) + 1
                        i += 3
                        ln = lcode + 3
                        if lcode == 127:
                            br = Reader(blk, i)
                            ln += br.varint()
                            i = br.pos
                    if dist > len(out):
                        raise Corrupt("LZ match before start of data")
                    if len(out) + ln > target:
                        raise Corrupt("LZ match past end of block")
                    start = len(out) - dist
                    if dist >= ln:
                        out += out[start:start + ln]
                    else:
                        chunk = bytes(out[start:])
                        out += (chunk * (ln // dist + 1))[:ln]
                else:
                    if i >= n:
                        raise Corrupt("LZ block too short")
                    out.append(blk[i])
                    i += 1
        if STRICT and i != n:
            raise Corrupt("trailing bytes in LZ block")
    return bytes(out)


def parse(data):
    if len(data) < 4 + 2 + FOOTER_LEN:
        raise Corrupt("file too short")
    if data[:4] != MAGIC_HEAD:
        raise Corrupt("bad magic")
    table_off, table_len, table_crc, tail = struct.unpack("<QII4s", data[-FOOTER_LEN:])
    if tail != MAGIC_TAIL:
        raise Corrupt("bad trailer")
    if table_off + table_len + FOOTER_LEN != len(data):
        raise Corrupt("bad table location")
    hr = Reader(data, 4, table_off)
    if hr.u8() != FORMAT_VERSION:
        raise Corrupt("unsupported version")
    flags = hr.u8()
    if flags & ~(HF_NAMETAB | HF_TABLZ):
        raise Corrupt("unknown flags")
    base = unzigzag(hr.varint())
    root_mode = hr.varint() ^ DEFAULT_DIR_MODE
    rt = hr.varint()
    root_ns = hr.varint() if rt & 1 else 0
    root_time = (base + (rt >> 1)) * 1_000_000_000 + root_ns
    if root_mode > 0o7777 or root_ns >= 1_000_000_000:
        raise Corrupt("bad root attributes")
    hdr_end = hr.pos
    traw = data[table_off:table_off + table_len]
    if zlib.crc32(traw, zlib.crc32(data[:hdr_end])) != table_crc:
        raise Corrupt("table checksum mismatch")

    if flags & HF_TABLZ:
        tr = Reader(traw)
        rawlen = tr.varint()
        table = lz_decode(tr, rawlen)
        if not tr.done():
            raise Corrupt("trailing bytes after table")
    else:
        table = traw
    r = Reader(table)
    names = []
    if flags & HF_NAMETAB:
        for _ in range(r.varint()):
            names.append(r.take(r.varint()))
    count = r.varint()
    if count > len(table):
        raise Corrupt("bad entry count")
    entries = []
    seen = set()
    for idx in range(count):
        kind = r.u8()
        etype = kind & 3
        if etype == 0 or kind & 0xC0:
            raise Corrupt("bad entry kind")
        if etype != EK_FILE and kind & (EK_LZ | EK_SHARED):
            raise Corrupt("bad entry kind")
        parent = r.varint()
        if parent > idx or (parent and entries[parent - 1]["type"] != EK_DIR):
            raise Corrupt("bad parent")
        if kind & EK_NAMEREF:
            ni = r.varint()
            if ni >= len(names):
                raise Corrupt("bad name reference")
            name = names[ni]
        else:
            name = r.take(r.varint())
        if not name or b"/" in name or b"\0" in name or name in (b".", b".."):
            raise Corrupt("bad name")
        if (parent, name) in seen:
            raise Corrupt("duplicate name")
        seen.add((parent, name))
        e = {"type": etype, "parent": parent, "name": name}
        if etype == EK_FILE:
            e["mode"] = r.varint() ^ DEFAULT_FILE_MODE
        elif etype == EK_DIR:
            e["mode"] = r.varint() ^ DEFAULT_DIR_MODE
        if e.get("mode", 0) > 0o7777:
            raise Corrupt("bad mode")
        sec = base + r.varint()
        ns = r.varint() if kind & EK_NSEC else 0
        if ns >= 1_000_000_000:
            raise Corrupt("bad nanoseconds")
        e["mtime"] = sec * 1_000_000_000 + ns
        if etype == EK_FILE:
            size = r.varint()
            e["size"] = size
            e["shared"] = None
            if size:
                if kind & EK_SHARED:
                    ref = r.varint()
                    if ref >= idx:
                        raise Corrupt("bad shared reference")
                    src = entries[ref]
                    if src["type"] != EK_FILE or src["shared"] is not None or src["size"] != size:
                        raise Corrupt("bad shared reference")
                    e["shared"] = ref
                else:
                    e["lz"] = bool(kind & EK_LZ)
                    e["clen"] = r.varint() if e["lz"] else size
                    e["crc"] = struct.unpack("<I", r.take(4))[0]
            elif kind & (EK_LZ | EK_SHARED):
                raise Corrupt("bad empty file entry")
        elif etype == EK_LINK:
            e["target"] = r.take(r.varint())
            if not e["target"] or b"\0" in e["target"]:
                raise Corrupt("bad link target")
        entries.append(e)
    if not r.done():
        raise Corrupt("trailing bytes in table")

    pos = hdr_end
    for e in entries:
        if e["type"] != EK_FILE:
            continue
        if e["size"] == 0:
            e["data"] = b""
            continue
        if e["shared"] is not None:
            e["data"] = entries[e["shared"]]["data"]
            continue
        if pos + e["clen"] > table_off:
            raise Corrupt("data area too short")
        if e["lz"]:
            br = Reader(data, pos, pos + e["clen"])
            content = lz_decode(br, e["size"])
            if not br.done():
                raise Corrupt("trailing bytes in compressed data")
        else:
            content = bytes(data[pos:pos + e["clen"]])
        pos += e["clen"]
        if zlib.crc32(content) != e["crc"]:
            raise Corrupt("checksum mismatch")
        e["data"] = content
    if pos != table_off:
        raise Corrupt("data area size mismatch")
    return {"mode": root_mode, "mtime": root_time}, entries


def extract(root, entries, outdir):
    os.makedirs(outdir)
    paths = []
    for e in entries:
        parent = outdir if e["parent"] == 0 else paths[e["parent"] - 1]
        p = os.path.join(parent, e["name"])
        paths.append(p)
        if e["type"] == EK_DIR:
            os.mkdir(p)
        elif e["type"] == EK_FILE:
            with open(p, "wb") as f:
                f.write(e["data"])
        else:
            os.symlink(e["target"], p)
    for e, p in zip(entries, paths):
        if e["type"] != EK_LINK:
            os.chmod(p, e["mode"])
    for e, p in zip(entries, paths):
        if e["type"] != EK_DIR:
            os.utime(p, ns=(e["mtime"], e["mtime"]), follow_symlinks=False)
    for e, p in reversed(list(zip(entries, paths))):
        if e["type"] == EK_DIR:
            os.utime(p, ns=(e["mtime"], e["mtime"]), follow_symlinks=False)
    os.chmod(outdir, root["mode"])
    os.utime(outdir, ns=(root["mtime"], root["mtime"]))


def main(argv):
    if len(argv) != 3:
        sys.stderr.write("usage: unvpack ARCHIVE OUTDIR\n")
        return 2
    try:
        with open(argv[1], "rb") as f:
            data = f.read()
    except OSError as exc:
        sys.stderr.write("unvpack: cannot read %s: %s\n" % (argv[1], exc))
        return 2
    try:
        root, entries = parse(data)
    except Corrupt as exc:
        sys.stderr.write("unvpack: corrupt archive: %s\n" % exc)
        return 1
    extract(root, entries, os.fsencode(argv[2]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
