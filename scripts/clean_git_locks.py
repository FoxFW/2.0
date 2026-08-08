#!/usr/bin/env python3

"""Removes stale .git lock/temp-object files left behind by tooling that
can't clean up after itself on this filesystem (index.lock, HEAD.lock,
refs/**/*.lock, objects/**/tmp_obj_*, etc.) from THIS repo's own .git dir
only - not the other FoxFW repos. This file ships as part of FoxFW2.0 and
runs for anyone who builds it, so it must never assume sibling repos exist
on disk (see the root-level clean_git_locks.bat for the personal,
all-5-repos version of this - that one's gitignored, this one isn't).

Called automatically by version.py on every fbt invocation, right before it
touches git itself - a stale lock left over from an earlier interrupted
session is otherwise enough to make "git tag -f" (and everything else
version.py does) fail silently. Safe to run any time: it only ever deletes
files matching these specific throwaway-lock name patterns, and a failed
delete (e.g. a lock genuinely held open by another process right now) is
swallowed, never fatal to the build.

Can also be run directly: `python scripts/clean_git_locks.py`.
"""

import os
from pathlib import Path


def _this_repo_dir():
    # scripts/clean_git_locks.py -> repo root is one level up
    return Path(__file__).resolve().parent.parent


def _is_stale_lock_name(name):
    lname = name.lower()
    return "lock" in lname or lname.startswith("tmp_obj_")


def clean_stale_git_locks(repo_dir=None):
    """Removes stale lock/temp files from repo_dir's .git dir (defaults to
    this repo). Returns the number removed. Never raises - every failure is
    swallowed since this must never be the reason a build fails."""
    if repo_dir is None:
        repo_dir = _this_repo_dir()

    git_dir = Path(repo_dir) / ".git"
    if not git_dir.is_dir():
        return 0

    removed = 0
    try:
        for root, _dirs, files in os.walk(git_dir):
            for name in files:
                if not _is_stale_lock_name(name):
                    continue
                path = os.path.join(root, name)
                try:
                    os.remove(path)
                    removed += 1
                except OSError:
                    # Genuinely in use right now, or the filesystem
                    # refused - leave it, not our problem to force.
                    pass
    except OSError:
        pass

    return removed


if __name__ == "__main__":
    count = clean_stale_git_locks()
    print(f"Removed {count} stale git lock/temp file(s)")
