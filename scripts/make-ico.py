import struct, os

d = r"D:\workspace\GuoDesk\artifacts\icon"
sizes = [16, 24, 32, 48, 64, 128, 256]
blobs = []
for s in sizes:
    with open(os.path.join(d, f"s{s}.png"), "rb") as f:
        blobs.append((s, f.read()))

out = struct.pack("<HHH", 0, 1, len(blobs))
offset = 6 + 16 * len(blobs)
entries = b""
data = b""
for s, blob in blobs:
    entries += struct.pack("<BBBBHHII", s % 256, s % 256, 0, 0, 1, 32, len(blob), offset)
    data += blob
    offset += len(blob)

with open(r"D:\workspace\GuoDesk\src\GuoDesk\app.ico", "wb") as f:
    f.write(out + entries + data)
print("app.ico written:", os.path.getsize(r"D:\workspace\GuoDesk\src\GuoDesk\app.ico"), "bytes")
