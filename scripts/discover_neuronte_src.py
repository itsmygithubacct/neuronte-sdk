#!/usr/bin/env python3
"""Print a Neuronte checkout that can build the tiled library.

Used when the default tree is older than neuronte_set_untrained. Prefer a
checkout whose HEAD is origin/main. A descendant is accepted when it still
contains that commit and has the current library sources, and the closest
one is chosen. The path is a build-machine location and is not written
into committed source.
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
    bits = os.path.join(path, "src", "bits.c")
    if not (
        os.path.isfile(header) and os.path.isfile(source) and os.path.isfile(bits)
    ):
        return False
    with open(header, "r", errors="replace") as handle:
        return "int neuronte_set_untrained(" in handle.read()


def ahead_of(root, origin):
    if subprocess.call(
        ["git", "-C", root, "merge-base", "--is-ancestor", origin, "HEAD"],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    ) != 0:
        return None
    count = git_out(root, "rev-list", "--count", "%s..HEAD" % origin)
    if not count.isdigit():
        return None
    return int(count)


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
    found = []
    for root in roots:
        if not has_api(root):
            continue
        if origin:
            ahead = ahead_of(root, origin)
            if ahead is None:
                continue
        elif root != base:
            continue
        else:
            ahead = 0
        found.append((ahead, root))
    if not found:
        return 0
    found.sort()
    sys.stdout.write(found[0][1])
    return 0


if __name__ == "__main__":
    sys.exit(main())
