#!/usr/bin/env python3
"""Check a staged public source tree without printing suspected secrets."""
import argparse
from pathlib import Path
import re
import sys

TEXT_SUFFIXES = {".c", ".cc", ".cpp", ".h", ".hpp", ".md", ".txt", ".xml",
                 ".earth", ".json", ".py", ".ps1", ".cmd", ".sh", ".cmake",
                 ".in", ".ui", ".qrc", ".svg", ".cjs", ".html", ".yml", ".yaml"}
FORBIDDEN_SUFFIXES = {".exe", ".dll", ".so", ".dylib", ".lib", ".a", ".pdb"}
REQUIRED = {"LICENSE", "README.md", "THIRD_PARTY_NOTICES.md", "CONTRIBUTING.md",
            "sdk/FreeEarthSDK/README.md", "sdk/videoSDK/README.md"}
FORBIDDEN_PARTS = {"private", "dependencies", "build", "release", "dist", "cache_flat"}
PATTERNS = {
    "private IPv4 address": re.compile(r"\b(?:10(?:\.\d{1,3}){3}|192\.168(?:\.\d{1,3}){2}|172\.(?:1[6-9]|2\d|3[01])(?:\.\d{1,3}){2})\b"),
    "machine-specific source path": re.compile(
        r"(?i)(?:[A-Z]:[/\\]SH-IMS|/home/[a-z][a-z0-9_-]*/ShDisplayViewer|"
        r"/Users/[a-z][a-z0-9_-]*/Documents/SH-IMS)"),
    "private key": re.compile(r"-----BEGIN (?:RSA |OPENSSH |EC |DSA )?PRIVATE KEY-----"),
    "credential in literal URL": re.compile(r"(?i)\b(?:rtsps?|https?)://[^\s/\"'<>]+:[^\s/@\"'<>]+@"),
}

def check(root: Path) -> list[str]:
    issues = []
    if not root.is_dir():
        return [f"missing directory: {root}"]
    for required in sorted(REQUIRED):
        if not (root / required).is_file():
            issues.append(f"missing required file: {required}")
    for path in sorted(root.rglob("*")):
        relative = path.relative_to(root)
        name = relative.as_posix()
        if path.is_symlink():
            issues.append(f"symlink: {name}")
            continue
        if not path.is_file():
            continue
        if any(part in FORBIDDEN_PARTS for part in relative.parts):
            issues.append(f"private/build path: {name}")
        if path.suffix.lower() in FORBIDDEN_SUFFIXES:
            issues.append(f"prebuilt binary: {name}")
        if path.suffix.lower() not in TEXT_SUFFIXES and path.name not in {"LICENSE", ".gitignore"}:
            continue
        try:
            lines = path.read_text(encoding="utf-8-sig").splitlines()
        except UnicodeDecodeError:
            issues.append(f"non-UTF-8 text: {name}")
            continue
        for number, line in enumerate(lines, 1):
            for category, pattern in PATTERNS.items():
                found = pattern.search(line)
                if found and category == "credential in literal URL" and "%" in found.group():
                    continue  # Qt format placeholders, not embedded credentials.
                if found:
                    issues.append(f"{name}:{number}: {category}")
    return issues

def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("root", type=Path, help="staged public source directory")
    args = parser.parse_args()
    issues = check(args.root)
    for issue in issues:
        print(issue)
    if issues:
        print(f"Public source check failed: {len(issues)} finding(s).")
        return 1
    print("Public source check passed: no forbidden binaries, private paths or obvious embedded secrets.")
    print("This automated check does not prove copyright ownership or binary-license compliance.")
    return 0

if __name__ == "__main__":
    sys.exit(main())
