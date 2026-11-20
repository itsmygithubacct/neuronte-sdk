#!/usr/bin/env python3
"""Print a weights directory that contains the omp terminal bundles.

The directory is a build-machine path. Callers pass it as a compiler
definition. It is not written into committed source.
"""
import os
import sys


NEED = ("neuronte-split-v9.nrt", "neuronte-term-symbol-v1.nrt", "neuronte-term-math-v1.nrt")


def has_bundles(path):
    return os.path.isdir(path) and all(os.path.isfile(os.path.join(path, name)) for name in NEED)


def main():
    home = os.environ.get("HOME", "")
    found = []
    seeds = []
    for key in ("NEUROTERM_WEIGHTS", "NEURONTE_HOME"):
        value = os.environ.get(key, "")
        if value:
            seeds.append(value if key == "NEUROTERM_WEIGHTS" else os.path.join(value, "weights"))
    if home:
        seeds.append(os.path.join(home, ".local/gpu_terminal/sources/neuronte/weights"))
        roots = [
            os.path.join(home, "gpu_terminal/worktrees"),
            os.path.join(home, "projects"),
        ]
        for root in roots:
            if not os.path.isdir(root):
                continue
            try:
                names = os.listdir(root)
            except OSError:
                continue
            for name in names:
                seeds.append(os.path.join(root, name, "weights"))
    seeds.append("/usr/share/neuronte/weights")
    for path in seeds:
        if has_bundles(path):
            found.append(path)
    if not found:
        return 0
    found.sort(key=lambda path: os.path.getmtime(os.path.join(path, "neuronte-split-v9.nrt")))
    sys.stdout.write(found[-1])
    return 0


if __name__ == "__main__":
    sys.exit(main())
