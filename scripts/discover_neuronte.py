#!/usr/bin/env python3
"""Write the build-time fallback paths for libneuronte.

The runtime searches NEURONTE_LIB, NEURONTE_WEIGHTS, NEURONTE_HOME, and
NEURONTE_DIR first. When a build can already see a Neuronte checkout, this
script records that library and weight file so the compiled SDK still finds
them with the variables unset. The header is generated under build/ and is
not committed.

Discovery walks parent directories for projects/neuronte. Nothing here names
a machine or a home directory.
"""

from __future__ import annotations

import os
import sys
from pathlib import Path

LIB_REL = Path("build") / "libneuronte.so"
WEIGHT_RELS = tuple(Path("weights") / f"neuronte-split-v{version}.nrt"
                    for version in (5, 4))


def c_string(value: str) -> str:
    pieces = ['"']
    for char in value:
        if char in '\\"':
            pieces.append("\\" + char)
        elif char == "\n":
            pieces.append("\\n")
        elif ord(char) < 32 or ord(char) > 126:
            pieces.append(f"\\{ord(char):03o}")
        else:
            pieces.append(char)
    pieces.append('"')
    return "".join(pieces)


def pair_from_root(root: Path) -> tuple[str, str]:
    library = root / LIB_REL
    weights = next((root / rel for rel in WEIGHT_RELS
                    if (root / rel).is_file()), None)
    return (
        str(library) if library.is_file() else "",
        str(weights) if weights is not None else "",
    )


def discover() -> tuple[str, str]:
    forced_lib = os.environ.get("NEURONTE_LIB", "")
    forced_weights = os.environ.get("NEURONTE_WEIGHTS", "")
    if (
        forced_lib
        and forced_weights
        and Path(forced_lib).is_file()
        and Path(forced_weights).is_file()
    ):
        return forced_lib, forced_weights

    for key in ("NEURONTE_HOME", "NEURONTE_DIR"):
        root = os.environ.get(key, "")
        if not root:
            continue
        library, weights = pair_from_root(Path(root))
        if library and weights:
            return library, weights

    here = Path(__file__).resolve()
    for ancestor in here.parents:
        library, weights = pair_from_root(ancestor / "projects" / "neuronte")
        if library and weights:
            return library, weights

    library = forced_lib if forced_lib and Path(forced_lib).is_file() else ""
    weights = forced_weights if forced_weights and Path(forced_weights).is_file() else ""
    return library, weights


def header_text(library: str, weights: str) -> str:
    return (
        "/* Generated at build time. Do not commit. */\n"
        "#ifndef NSDK_DEFAULTS_H\n"
        "#define NSDK_DEFAULTS_H\n"
        f"#define NSDK_DEFAULT_LIB {c_string(library)}\n"
        f"#define NSDK_DEFAULT_WEIGHTS {c_string(weights)}\n"
        "#endif\n"
    )


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print("usage: discover_neuronte.py OUTPUT.h", file=sys.stderr)
        return 2
    library, weights = discover()
    if not library or not weights:
        print(
            "neuronte-sdk: no fallback library or weights found",
            file=sys.stderr,
        )
    text = header_text(library, weights)
    output = Path(argv[1])
    output.parent.mkdir(parents=True, exist_ok=True)
    if not output.is_file() or output.read_text(encoding="utf-8") != text:
        output.write_text(text, encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
