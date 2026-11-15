import os
import re


ANSI_RE = re.compile(r"\x1b\[[0-9;]*m")
NUMBER = r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?"
TIME_STEP_RE = re.compile(r"^Time Step\s+(\d+)\b")
NONLINEAR_RESIDUAL_RE = re.compile(r"^\s*(\d+)\s+Nonlinear\s*\|R\|\s*=\s*(%s)" % NUMBER)
LINEAR_RESIDUAL_RE = re.compile(r"^\s*(\d+)\s+Linear\s*\|R\|", re.IGNORECASE)
# -ksp_monitor prints "KSP Residual norm"; -ksp_monitor_true_residual prints
# "KSP preconditioned resid norm ... true resid norm ...".
KSP_RESIDUAL_RE = re.compile(r"^\s*(\d+)\s+KSP\b.*\bresid(?:ual)? norm", re.IGNORECASE)
SNES_RESIDUAL_RE = re.compile(r"^\s*(\d+)\s+SNES Function norm\s+(%s)" % NUMBER)
# PETSc -ksp_converged_reason / -snes_converged_reason wording, e.g.
#   "Linear solve converged due to CONVERGED_RTOL iterations 22"
#   "Nonlinear solve did not converge due to DIVERGED_MAX_IT iterations 50"
# An options prefix, when set, appears between "Linear"/"Nonlinear" and "solve".
LINEAR_TERMINAL_RE = re.compile(
    r"^\s*Linear\s+(?:\S+\s+)?solve\s+(converged|did not converge)\s+due to\s+"
    r"([A-Z_]+)\s+iterations\s+(\d+)\b"
)
NONLINEAR_TERMINAL_RE = re.compile(
    r"^\s*Nonlinear\s+(?:\S+\s+)?solve\s+(converged|did not converge)\s+due to\s+"
    r"([A-Z_]+)\s+iterations\s+(\d+)\b"
)
SNES_REASON_RE = re.compile(
    r"^\s*SNES\s+(?:Converged|Diverged|Termination)\s+reason:\s*(\S+)",
    re.IGNORECASE,
)
DIVERGED_RE = re.compile(r"\bDIVERGED_[A-Z_]+\b")
# FEProblemSolve prints "Solve Converged!" / "Solve Did NOT Converge!", prefixed by
# "System <name>: " when there are multiple systems.
OUTER_CONVERGED_RE = re.compile(r"^\s*(?:System \S+: )?Solve Converged!\s*$")
OUTER_FAILED_RE = re.compile(r"^\s*(?:System \S+: )?Solve Did NOT Converge!\s*$")
# Output that MOOSE or PETSc prints only once the application finishes: the MOOSE
# performance graph (perf_graph / --timing), PETSc's unused-option report, and -log_view.
COMPLETION_RE = re.compile(
    r"^(?:Performance Graph:|WARNING! There are options you set that were not used!"
    r"|.*PETSc Performance Summary)"
)
# TestHarness per-test result line (default --term-format njcstm): name, dot justification,
# optional [caveats], status, then optional [time] and [memory], e.g.
#   test:kokkos/foo.bar ........ [cuda not in compute devices] SKIP
#   solid_mechanics/test:kokkos/foo.bar ........ FAILED (EXODIFF) [12.3s]
# Pending lines (RUNNING, QUEUED, HOLD) are ignored; tester output echoed as "<name>: ..." has no
# dot run and does not match.
TEST_RESULT_RE = re.compile(
    r"^\S+\s+\.+\s+(?:\[[^\]]*\]\s+)?"
    r"(OK|SKIP|FAILED(?:\s+\(.*\))?|DIFF|ERROR|RACE|TIMEOUT|DELETED)"
    r"(?:\s+\[[^\]]*\])*\s*$"
)
# TestHarness final summary, printed once all tests finish, e.g. "8 passed, 6 skipped, 1 FAILED".
TEST_SUMMARY_RE = re.compile(r"^(\d+) passed, (\d+) skipped, (\d+) (?:failed|FAILED)\b(.*)$")
# mooseError banner (always fatal); a MultiApp prefixes each line with "<app>: ".
ERROR_RE = re.compile(r"^(?:\S+: )?\*\*\* ERROR \*\*\*\s*$")
# Message lines kept after the banner, which ends at the stack trace or MPI_ABORT rule.
ERROR_MESSAGE_LINES = 5


def _clean(line):
    return ANSI_RE.sub("", line).rstrip()


def _is_candidate(line):
    # Every pattern in analyze_stream requires one of these substrings. Most lines of a large
    # log match none, and an inline "in" chain rejects them several times faster than the
    # regex cascade or a combined regex. Tokens are tested on the raw line, so none may span a
    # point where color codes can sit (TestHarness colors the dot run, hence ".." not " .").
    return (
        "|R|" in line
        or "Time Step" in line
        or "solve" in line
        or "Solve" in line
        or "KSP" in line
        or "SNES" in line
        or "DIVERGED_" in line
        or ".." in line
        or "passed" in line
        or "*** ERROR ***" in line
        or "Performance" in line
        or "options you set" in line
        or "|r|" in line
    )


def _flush_iteration_fallback(summary, kind):
    key = "%s_active" % kind
    terminal_key = "%s_terminal" % kind
    active = summary[key]
    if active is not None and not summary[terminal_key]:
        if kind == "nonlinear":
            summary["newton"] += active
        else:
            summary["linear"] += active
    summary[key] = None
    summary[terminal_key] = False


def _add_reason(summary, reason):
    if reason not in summary["reasons"]:
        summary["reasons"].append(reason)


def _open_solve(summary):
    summary["solve_open"] = True
    summary["completed"] = False


def _close_solve(summary, event):
    summary["outer_event"] = event
    summary["solve_open"] = False
    summary["completed"] = False


def _new_summary():
    return {
        "timesteps": 0,
        "newton": 0,
        "linear": 0,
        "snes": 0,
        "final": None,
        "reasons": [],
        "failed_attempts": 0,
        "status": "INCOMPLETE",
        "test_failed": False,
        "test_skipped": False,
        "test_passed": False,
        # (passed, skipped, failed, trailer) from the TestHarness final summary line.
        "test_summary": None,
        # mooseError message lines, or None when no error banner appeared.
        "error": None,
        "current_timestep": None,
        "timestep_counted": False,
        # A timestep or nonlinear solve has started without a MOOSE converged/failed line.
        "solve_open": False,
        # Application-exit output appeared after the last MOOSE converged/failed line.
        "completed": False,
        "outer_event": None,
        "solver_event": None,
        "nonlinear_active": None,
        "nonlinear_terminal": False,
        "linear_active": None,
        "linear_terminal": False,
    }


def analyze_stream(lines):
    summary = _new_summary()
    saw_content = False
    capturing_error = False
    for raw_line in lines:
        if not capturing_error and not _is_candidate(raw_line):
            if not saw_content:
                saw_content = bool(_clean(raw_line))
            continue
        line = _clean(raw_line)
        if not line:
            continue
        saw_content = True
        if capturing_error:
            if line.startswith("Stack frames:") or line.startswith("-----"):
                capturing_error = False
            else:
                summary["error"].append(line)
                capturing_error = len(summary["error"]) < ERROR_MESSAGE_LINES

        if ERROR_RE.match(line):
            # Keep the first error; it is fatal, so any later banner is from another rank.
            if summary["error"] is None:
                summary["error"] = []
                capturing_error = True
            continue

        match = TEST_RESULT_RE.match(line)
        if match:
            result = match.group(1)
            if result == "OK":
                summary["test_passed"] = True
            elif result == "SKIP":
                summary["test_skipped"] = True
            else:
                summary["test_failed"] = True
            continue

        match = TEST_SUMMARY_RE.match(line)
        if match:
            passed, skipped, failed, trailer = match.groups()
            summary["test_summary"] = (int(passed), int(skipped), int(failed), trailer)
            continue

        match = TIME_STEP_RE.match(line)
        if match:
            _flush_iteration_fallback(summary, "nonlinear")
            _flush_iteration_fallback(summary, "linear")
            summary["current_timestep"] = int(match.group(1))
            summary["timestep_counted"] = False
            # MOOSE prints "Time Step 0" for the initial condition, which has no solve.
            if summary["current_timestep"] > 0:
                _open_solve(summary)
            continue

        match = NONLINEAR_RESIDUAL_RE.match(line)
        if match:
            iteration = int(match.group(1))
            if iteration == 0:
                _flush_iteration_fallback(summary, "nonlinear")
                _flush_iteration_fallback(summary, "linear")
                summary["nonlinear_active"] = 0
                _open_solve(summary)
            elif summary["nonlinear_active"] is not None:
                summary["nonlinear_active"] = max(summary["nonlinear_active"], iteration)
            summary["final"] = match.group(2)
            continue

        match = LINEAR_RESIDUAL_RE.match(line)
        if match:
            iteration = int(match.group(1))
            if iteration == 0:
                _flush_iteration_fallback(summary, "linear")
                summary["linear_active"] = 0
            elif summary["linear_active"] is not None:
                summary["linear_active"] = max(summary["linear_active"], iteration)
            continue

        match = KSP_RESIDUAL_RE.match(line)
        if match:
            iteration = int(match.group(1))
            if iteration == 0:
                _flush_iteration_fallback(summary, "linear")
                summary["linear_active"] = 0
            elif summary["linear_active"] is not None:
                summary["linear_active"] = max(summary["linear_active"], iteration)
            continue

        match = SNES_RESIDUAL_RE.match(line)
        if match:
            iteration = int(match.group(1))
            if iteration == 0:
                _flush_iteration_fallback(summary, "nonlinear")
                summary["nonlinear_active"] = 0
            elif summary["nonlinear_active"] is not None:
                summary["nonlinear_active"] = max(summary["nonlinear_active"], iteration)
            summary["snes"] += 1 if iteration > 0 else 0
            summary["final"] = match.group(2)
            continue

        match = LINEAR_TERMINAL_RE.match(line)
        if match:
            event, reason, iterations = match.groups()
            summary["linear"] += int(iterations)
            summary["linear_active"] = None
            summary["linear_terminal"] = True
            _add_reason(summary, reason)
            if event != "converged":
                summary["solver_event"] = "failed"
            continue

        match = NONLINEAR_TERMINAL_RE.match(line)
        if match:
            event, reason, iterations = match.groups()
            summary["newton"] += int(iterations)
            summary["nonlinear_active"] = None
            summary["nonlinear_terminal"] = True
            _add_reason(summary, reason)
            if event != "converged":
                summary["solver_event"] = "failed"
            continue

        match = SNES_REASON_RE.match(line)
        if match:
            reason = match.group(1).upper()
            _add_reason(summary, reason)
            if reason.startswith("DIVERGED_"):
                summary["solver_event"] = "failed"
            elif reason.startswith("CONVERGED_"):
                summary["solver_event"] = "converged"
            continue

        if OUTER_CONVERGED_RE.match(line):
            _close_solve(summary, "converged")
            # With multiple systems, each system's solve reports separately.
            if (summary["current_timestep"] or 0) > 0 and not summary["timestep_counted"]:
                summary["timesteps"] += 1
                summary["timestep_counted"] = True
            continue

        if OUTER_FAILED_RE.match(line):
            _close_solve(summary, "failed")
            summary["failed_attempts"] += 1
            continue

        if COMPLETION_RE.match(line):
            summary["completed"] = True
            continue

        match = DIVERGED_RE.search(line)
        if match:
            _add_reason(summary, match.group(0))
            summary["solver_event"] = "failed"

    _flush_iteration_fallback(summary, "nonlinear")
    _flush_iteration_fallback(summary, "linear")
    test_summary = summary["test_summary"]
    if not saw_content:
        summary["status"] = "EMPTY"
    elif (
        summary["test_failed"]
        or summary["test_passed"]
        or summary["test_skipped"]
        or test_summary
    ):
        # A TestHarness log: its verdict is authoritative, and the echoed output of
        # individual tests is not parsed as a single solve.
        if summary["test_failed"] or (
            test_summary and (test_summary[2] or test_summary[3].strip())
        ):
            # A nonzero failed count, or a trailer such as FATAL PARSER ERROR.
            summary["status"] = "TEST_FAILED"
        elif not test_summary:
            # The log ends before the final summary, so tests may still be missing.
            summary["status"] = "UNCONFIRMED"
        elif test_summary[0]:
            summary["status"] = "TEST_PASSED"
        elif test_summary[1]:
            summary["status"] = "SKIPPED"
        else:
            summary["status"] = "NO_TESTS"
    elif summary["error"] is not None:
        # mooseError aborts the run, whatever state the last solve was in.
        summary["status"] = "MOOSE_ERROR"
    elif summary["solve_open"]:
        # The log ends inside a timestep or solve: truncated, killed, or crashed.
        summary["status"] = "INCOMPLETE"
    elif summary["outer_event"] == "failed" or (
        not summary["outer_event"] and summary["solver_event"] == "failed"
    ):
        # The last attempt failed and no retry followed; a recovered cutback ends with a
        # converged solve instead and is reported through retries.
        summary["status"] = "SOLVER_FAILED"
    elif summary["outer_event"] or summary["solver_event"]:
        # Every solve converged, but a log cut between timesteps looks the same unless
        # application-exit output follows the last solve.
        summary["status"] = "CONVERGED" if summary["completed"] else "UNCONFIRMED"
    elif summary["newton"] or summary["linear"] or summary["snes"]:
        summary["status"] = "UNKNOWN"
    else:
        summary["status"] = "INCOMPLETE"
    if summary["error"] is not None:
        summary["error"] = " ".join(summary["error"])
    for key in (
        "current_timestep",
        "timestep_counted",
        "solve_open",
        "completed",
        "outer_event",
        "solver_event",
        "nonlinear_active",
        "nonlinear_terminal",
        "linear_active",
        "linear_terminal",
    ):
        del summary[key]
    return summary


def analyze_log(text):
    return analyze_stream(text.splitlines())


def run_logstat(args):
    return_code = 0
    for path in args.logs:
        if not os.path.isfile(path):
            print("%s: status=MISSING" % path)
            return_code = 1
            continue
        try:
            with open(path, errors="replace") as stream:
                summary = analyze_stream(stream)
        except OSError as error:
            print("%s: status=ERROR detail=%s" % (path, error))
            return_code = 1
            continue
        print(
            "%s: timesteps=%d newton=%d linear=%d snes=%d final=%s retries=%d status=%s"
            % (
                path,
                summary["timesteps"],
                summary["newton"],
                summary["linear"],
                summary["snes"],
                summary["final"] if summary["final"] is not None else "n/a",
                summary["failed_attempts"],
                summary["status"],
            )
        )
        if args.verbose:
            if summary["reasons"]:
                print("  reasons: %s" % ", ".join(summary["reasons"]))
            if summary["error"] is not None:
                print("  error: %s" % summary["error"])
            print(
                "  test_failed=%s skipped=%s"
                % (summary["test_failed"], summary["test_skipped"])
            )
            if summary["test_summary"]:
                print(
                    "  tests: passed=%d skipped=%d failed=%d%s" % summary["test_summary"]
                )
        if summary["status"] in (
            "EMPTY",
            "NO_TESTS",
            "INCOMPLETE",
            "UNCONFIRMED",
            "UNKNOWN",
            "SOLVER_FAILED",
            "TEST_FAILED",
            "MOOSE_ERROR",
        ):
            return_code = 1
    return return_code
