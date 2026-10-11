"""Reject em dashes in project source, documentation and build configuration."""
from pathlib import Path

root = Path(__file__).resolve().parents[1]
extensions = {".c", ".h", ".md", ".py", ".js", ".html", ".css", ".json",
              ".mk", ".sh", ".bat", ".yml", ".yaml", ".toml", ".txt"}
files = [root / name for name in ("README.md", "Makefile", "freepad.mk", "Dockerfile")]
for folder in ("src", "web", "docs", "tests", "tools", "payload"):
    files.extend(path for path in (root / folder).rglob("*")
                 if path.is_file() and path.suffix in extensions)
failures = []
for path in files:
    if not path.exists():
        continue
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if chr(0x2014) in line:
            failures.append(f"{path.relative_to(root)}:{number}: em dash is not allowed")
if failures:
    raise SystemExit("\n".join(failures))
print("Project punctuation check passed")
