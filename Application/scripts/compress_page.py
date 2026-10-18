"""Deterministic gzip asset for ESP32 flash and offline HTTP serving."""
Import("env")
import gzip
from pathlib import Path

root = Path(env["PROJECT_DIR"])
source = root / "web" / "index.html"
target = root / "web" / "index.html.gz"
compressed = gzip.compress(source.read_bytes(), compresslevel=9, mtime=0)
if not target.exists() or target.read_bytes() != compressed:
    target.write_bytes(compressed)
print(f"Dashboard: {source.stat().st_size} -> {len(compressed)} bytes (gzip)")
