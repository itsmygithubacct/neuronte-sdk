#!/usr/bin/env python3
"""Print a Neuronte checkout of origin/main that can open tiled bundles.

Used when the default tree is older than neuronte_set_untrained. The path
is a build-machine location and is not written into committed source.
"""
import os
import subprocess
import sys


def git_out(path, *args):
    try:
        return subprocess.check_output(
            ["git", "-C", path, *args], stderr=subprocess.DEVNULL, text=True
        ).strip()
    except (subprocess.CalledProcessError, OSError):
        return ""


def has_api(path):
    header = os.path.join(path, "include", "neuronte.h")
    source = os.path.join(path, "src", "neuronte.c")
    if not (os.path.isfile(header) and os.path.isfile(source)):
        return False
    with open(header, "r", errors="replace") as handle:
        return "int neuronte_set_untrained(" in handle.read()


def main():
    home = os.environ.get("HOME", "")
    if not home:
        return 0
    base = os.path.join(home, "projects", "neuronte")
    origin = git_out(base, "rev-parse", "origin/main")
    roots = [base]
    worktrees = os.path.join(home, "gpu_terminal", "worktrees")
    if os.path.isdir(worktrees):
        for name in sorted(os.listdir(worktrees)):
            roots.append(os.path.join(worktrees, name))
    for root in roots:
        if not has_api(root):
            continue
        if origin and git_out(root, "rev-parse", "HEAD") != origin:
            continue
        if not origin and root != base:
            continue
        sys.stdout.write(root)
        return 0
    return 0


if __name__ == "__main__":
    sys.exit(main())
