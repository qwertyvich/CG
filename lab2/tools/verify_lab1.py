"""Verify byte-for-byte preservation of the original lab1 snapshot."""
import hashlib
import json
from pathlib import Path

lab2 = Path(__file__).resolve().parents[1]
root = lab2.parent
baseline = json.loads((lab2 / "docs/lab1-baseline.json").read_text(encoding="utf-8-sig"))
changed = []
for item in baseline:
    path = root / item["path"]
    if not path.exists() or hashlib.sha256(path.read_bytes()).hexdigest().upper() != item["sha256"]:
        changed.append(item["path"])
if changed:
    raise SystemExit("Changed lab1 files: " + ", ".join(changed))
print(f"PASS: all {len(baseline)} lab1 files match the original SHA-256 snapshot")
