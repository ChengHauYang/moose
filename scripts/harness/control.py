# This file is part of the MOOSE framework
# https://mooseframework.inl.gov
#
# All rights reserved, see COPYRIGHT for full restrictions
# https://github.com/idaholab/moose/blob/master/COPYRIGHT
#
# Licensed under LGPL 2.1, please see LICENSE for details
# https://www.gnu.org/licenses/lgpl-2.1.html

"""Local harness process guards, result reporting, and formatting."""

import argparse
import contextlib
import fcntl
import os
import re
import signal
import subprocess
import sys
from pathlib import Path

from logstat_util import ANSI_RE, TEST_RESULT_RE, TEST_SUMMARY_RE


def exit_status(code):
    """Convert subprocess signal return codes to shell exit statuses."""
    return 128 - code if code < 0 else code


def run_process(command, **kwargs):
    """Forward termination to the whole command and wait before releasing its lock."""
    with subprocess.Popen(command, start_new_session=True, **kwargs) as process:
        previous = {}
        interrupted = []

        def forward(signum, frame):
            interrupted.append(signum)
            with contextlib.suppress(ProcessLookupError):
                os.killpg(process.pid, signum)

        for signum in (signal.SIGINT, signal.SIGTERM, signal.SIGHUP):
            previous[signum] = signal.signal(signum, forward)
        try:
            if process.stdout is not None:
                for line in process.stdout:
                    yield line
            code = process.wait()
            yield -interrupted[-1] if interrupted else code
        finally:
            for signum, handler in previous.items():
                signal.signal(signum, handler)


def guard(args):
    """Hold a checkout lock until the command and its nested calls finish."""
    root = str(Path(args.root).resolve())
    # A single lock serializes compilation, tests, runs and their nested calls.
    # flock releases it on process exit; stale metadata never blocks a later invocation.
    lock_path = subprocess.check_output(
        ["git", "-C", root, "rev-parse", "--git-path", "moose-harness.lock"], text=True
    ).strip()
    lock_path = Path(root, lock_path)
    inherited = os.environ.get("MOOSE_HARNESS_LOCK_FD")
    if inherited and os.environ.get("MOOSE_HARNESS_LOCK_ROOT") == root:
        try:
            descriptor = int(inherited)
            held = os.fstat(descriptor)
            current = lock_path.stat()
            if (held.st_dev, held.st_ino) == (current.st_dev, current.st_ino):
                os.set_inheritable(descriptor, True)
                os.execvp(args.command[0], args.command)
        except (ValueError, OSError):
            pass
    with lock_path.open("a+") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            lock.seek(0)
            print("ERROR: checkout busy: " + lock.read().strip(), file=sys.stderr)
            return 1
        lock.seek(0)
        lock.truncate()
        lock.write(
            "pid=%d operation=%s root=%s\n" % (os.getpid(), args.operation, root)
        )
        lock.flush()
        environment = dict(
            os.environ,
            MOOSE_HARNESS_LOCK_ROOT=root,
            MOOSE_HARNESS_LOCK_FD=str(lock.fileno()),
        )
        for result in run_process(
            args.command, env=environment, pass_fds=(lock.fileno(),)
        ):
            pass
        return exit_status(result)


def test(args):
    """Stream test output and report counts alongside the process exit status."""
    counts = None
    failures = []
    fatal = False
    for result in run_process(
        args.command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        errors="replace",
        bufsize=1,
    ):
        if isinstance(result, int):
            code = exit_status(result)
            break
        print(result, end="", flush=True)
        line = ANSI_RE.sub("", result).strip()
        match = TEST_SUMMARY_RE.match(line)
        if match:
            counts = tuple(map(int, match.groups()[:3]))
            fatal |= bool(match.group(4).strip())
        match = TEST_RESULT_RE.match(line)
        if match and match.group(1) not in ("OK", "SKIP"):
            failures.append(line.split()[0])
        # Unit tests have no TestHarness footer.
        if args.unit:
            match = re.match(r"\[==========\] (\d+) tests? from .* ran\.", line)
            if match:
                counts = (int(match.group(1)), 0, 0)
            match = re.match(r"\[\s+(PASSED|FAILED|SKIPPED)\s+\] (\d+) tests?", line)
            if match and counts is not None:
                values = list(counts)
                values[{"PASSED": 0, "SKIPPED": 1, "FAILED": 2}[match.group(1)]] = int(
                    match.group(2)
                )
                counts = tuple(values)
            match = re.match(r"\[  FAILED  \] (\S+)", line)
            if match and not match.group(1).isdigit():
                failures.append(match.group(1))
    if counts is None:
        passed = skipped = failed = "unknown"
        status = "ERROR"
        reason = "missing_summary"
    else:
        passed, skipped, failed = counts
        if failed or failures:
            status, reason = "FAIL", "test_failure"
        elif code or fatal:
            status, reason = "ERROR", "process_or_harness_error"
        elif passed + skipped == 0:
            status, reason = "ERROR", "no_tests_selected"
        else:
            status, reason = "PASS", "complete"
    for name in dict.fromkeys(failures):
        print("FAILED scope=%s test=%s" % (args.scope, name))
    if reason == "no_tests_selected":
        print(
            "ERROR: no tests selected. Check scope/pattern: framework-kokkos (test/), "
            "neml2 or neml2-aoti (solid_mechanics/), sczm (shifted_boundary_method/)."
        )
    final = code or (0 if status == "PASS" else 1)
    print(
        "RESULT scope=%s passed=%s failed=%s skipped=%s "
        "status=%s exit=%d process_exit=%d reason=%s"
        % (args.scope, passed, failed, skipped, status, final, code, reason),
        flush=True,
    )
    return final


def format_files(args):
    """Check or format changed C++ files with the repository's Conda formatter."""
    root = Path(args.root).resolve()
    formatter = Path(os.environ["CONDA_PREFIX"], "bin", "clang-format")
    version = subprocess.check_output([str(formatter), "--version"], text=True)
    if not re.search(r"version 21\.", version):
        raise ValueError(
            "format requires Conda clang-format v21; found " + version.strip()
        )
    # Compare against HEAD by default, including staged, unstaged and untracked files.
    changed = subprocess.check_output(
        ["git", "diff", "--name-only", "--diff-filter=ACMR", "-z", args.base, "--"],
        cwd=root,
    )
    untracked = subprocess.check_output(
        ["git", "ls-files", "--others", "--exclude-standard", "-z"], cwd=root
    )
    files = sorted(
        {
            os.fsdecode(path)
            for path in (changed + untracked).split(b"\0")
            if path and Path(os.fsdecode(path)).suffix in (".C", ".K", ".h")
        }
    )
    # Match the repository's pre-commit exclusions for dependencies and generated trees.
    files = [
        path
        for path in files
        if not re.match(
            r"(?:libmesh|petsc|large_media|framework/contrib|python/contrib|"
            r"framework/doc/content/contrib|modules/[^/]+/contrib)/",
            path,
        )
    ]
    if not (root / ".clang-format").is_file():
        raise ValueError("missing repository .clang-format")
    status = 0
    for path in files:
        print("FORMAT %s" % path, flush=True)
        status |= subprocess.call(
            [
                str(formatter),
                "--style=file",
                "--fallback-style=none",
                *(["--dry-run", "--Werror"] if args.check else ["-i"]),
                path,
            ],
            cwd=root,
        )
    print("FORMAT files=%d check=%s" % (len(files), str(args.check).lower()))
    return status


def main():
    """Dispatch a process guard, test runner or formatting operation."""
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="action", required=True)
    guarded = sub.add_parser("guard")
    guarded.add_argument("root")
    guarded.add_argument("operation")
    guarded.add_argument("command", nargs=argparse.REMAINDER)
    runner = sub.add_parser("test")
    runner.add_argument("--unit", action="store_true")
    runner.add_argument("scope")
    runner.add_argument("command", nargs=argparse.REMAINDER)
    formatter = sub.add_parser("format")
    formatter.add_argument("root")
    formatter.add_argument("base", nargs="?", default="HEAD")
    formatter.add_argument("--check", action="store_true")
    args = parser.parse_args()
    try:
        return {"guard": guard, "test": test, "format": format_files}[args.action](args)
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print("ERROR: %s" % error, file=sys.stderr)
        if args.action == "test":
            print(
                "RESULT scope=%s passed=unknown failed=unknown skipped=unknown "
                "status=ERROR exit=1 process_exit=unknown reason=launch_error"
                % args.scope
            )
        return 1


if __name__ == "__main__":
    sys.exit(main())
