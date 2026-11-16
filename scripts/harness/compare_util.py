import csv
import math
import os
import re
import shutil
import subprocess
import sys


REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
FLOAT = r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?"


class CompareError(Exception):
    pass


def _run(command):
    process = subprocess.run(
        command, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True
    )
    return process.returncode, process.stdout


def _exodiff():
    local = os.path.join(REPO, "framework", "contrib", "exodiff", "exodiff")
    if os.path.isfile(local) and os.access(local, os.X_OK):
        return local
    executable = shutil.which("exodiff")
    if executable:
        return executable
    raise CompareError("exodiff is unavailable; compile the framework first")


def _kind(path):
    extension = os.path.splitext(path)[1].lower()
    if extension == ".csv":
        return "csv"
    if extension in (".e", ".exo", ".exodus") or re.search(r"\.e-s\d+$", path):
        return "exodus"
    raise CompareError("unsupported file type: %s" % path)


def _format(value):
    return "%.6g" % value


def _read_csv(path):
    with open(path, newline="") as stream:
        reader = csv.reader(stream)
        try:
            headers = [header.strip() for header in next(reader)]
        except StopIteration as error:
            raise CompareError("empty file: %s" % path) from error
        if not headers or any(not header for header in headers):
            raise CompareError("empty CSV header in %s" % path)
        duplicates = sorted({header for header in headers if headers.count(header) > 1})
        if duplicates:
            raise CompareError(
                "duplicate CSV variable(s) in %s: %s" % (path, ", ".join(duplicates))
            )
        rows = []
        for row_number, row in enumerate(reader, start=2):
            if not row or all(not value.strip() for value in row):
                continue
            if len(row) != len(headers):
                raise CompareError(
                    "%s row %d has %d values; expected %d"
                    % (path, row_number, len(row), len(headers))
                )
            values = []
            for header, value in zip(headers, row):
                try:
                    number = float(value)
                except ValueError:
                    number = None
                if number is not None and not math.isfinite(number):
                    raise CompareError(
                        "%s row %d variable %s is non-finite"
                        % (path, row_number, header)
                    )
                values.append(number)
            rows.append(values)
    return headers, rows


def _csv_stats(file1, file2):
    headers1, rows1 = _read_csv(file1)
    headers2, rows2 = _read_csv(file2)
    missing = [(name, "second") for name in headers1 if name not in headers2]
    missing.extend((name, "first") for name in headers2 if name not in headers1)
    if missing:
        details = ", ".join("%s (%s file)" % item for item in missing)
        raise CompareError("missing CSV variable(s): %s" % details)
    if len(rows1) != len(rows2):
        raise CompareError("CSV row counts differ: %d vs %d" % (len(rows1), len(rows2)))
    if not rows1:
        raise CompareError("CSV files contain no data rows")
    indices2 = {name: headers2.index(name) for name in headers1}
    stats = {}
    numeric_headers = []
    for index1, name in enumerate(headers1):
        maximum_absolute = (0.0, 0, 0.0)
        maximum_relative = (0.0, 0, 0.0)
        compared = False
        for row_index, (row1, row2) in enumerate(zip(rows1, rows2)):
            value1 = row1[index1]
            value2 = row2[indices2[name]]
            if value1 is None or value2 is None:
                if value1 != value2:
                    raise CompareError(
                        "CSV variable %s has incompatible text at row %d" % (name, row_index)
                    )
                continue
            compared = True
            absolute = abs(value1 - value2)
            reference = max(abs(value1), abs(value2))
            relative = absolute / reference if reference else 0.0
            if absolute > maximum_absolute[0]:
                maximum_absolute = (absolute, row_index, reference)
            if relative > maximum_relative[0]:
                maximum_relative = (relative, row_index, reference)
        if compared:
            numeric_headers.append(name)
            stats[name] = (maximum_absolute, maximum_relative)
    if not numeric_headers:
        raise CompareError("CSV files contain no numeric variables")
    time_index = next(
        (index for index, name in enumerate(headers1) if name.lower() == "time"), None
    )
    times = (
        [row[time_index] for row in rows1]
        if time_index is not None and all(row[time_index] is not None for row in rows1)
        else []
    )
    return numeric_headers, stats, len(rows1), times


def _row_time(times, row):
    return _format(times[row]) if times and 0 <= row < len(times) else "n/a"


def _print_csv_summary(file1, file2, verbose):
    headers, stats, row_count, times = _csv_stats(file1, file2)
    for name in headers:
        absolute, relative = stats[name]
        print(
            "  %s: max_abs=%s row=%d time=%s max_rel=%s row=%d time=%s ref=%s"
            % (
                name,
                _format(absolute[0]),
                absolute[1],
                _row_time(times, absolute[1]),
                _format(relative[0]),
                relative[1],
                _row_time(times, relative[1]),
                _format(max(absolute[2], relative[2])),
            )
        )
    worst_name = max(headers, key=lambda name: stats[name][0][0])
    absolute, relative = stats[worst_name]
    print(
        "worst: %s row=%d time=%s abs=%s rel=%s ref=%s"
        % (
            worst_name,
            absolute[1],
            _row_time(times, absolute[1]),
            _format(absolute[0]),
            _format(relative[0]),
            _format(max(absolute[2], relative[2])),
        )
    )
    if verbose:
        print("rows: %d" % row_count)
    return any(
        absolute[0] != 0.0 or relative[0] != 0.0
        for absolute, relative in stats.values()
    )


def _exodiff_variables(output):
    variables = set()
    in_section = False
    for line in output.splitlines():
        if re.match(r"(?:Global|Nodal|Element|Nodeset|Sideset) variables to be compared:", line):
            in_section = True
            continue
        if in_section and not line.strip():
            in_section = False
            continue
        if in_section:
            match = re.match(r"\s+(\S+)", line)
            if match:
                variables.add(match.group(1))
        match = re.search(r'The \w+ variable "([^"]+)" is in the', line)
        if match:
            variables.add(match.group(1))
    return variables


def _exodiff_differences(output):
    result = {}
    timestep = None
    time = None
    pattern = re.compile(
        r"\s+(\S+)\s+(?:abs|rel) diff:\s*(%s)\s*~\s*(%s)\s*=\s*(%s)"
        % (FLOAT, FLOAT, FLOAT)
    )
    for line in output.splitlines():
        match = re.search(r"Time step\s+(\d+),\s*(%s)\s*~" % FLOAT, line)
        if match:
            timestep = int(match.group(1))
            time = float(match.group(2))
            continue
        match = pattern.match(line)
        if not match:
            continue
        variable = match.group(1)
        value1 = float(match.group(2))
        value2 = float(match.group(3))
        difference = float(match.group(4))
        record = (difference, timestep, time, max(abs(value1), abs(value2)))
        if variable not in result or difference > result[variable][0]:
            result[variable] = record
    return result


def _exodiff_issues(output):
    issues = []
    for line in output.splitlines():
        if "exodiff: ERROR:" in line or "exodiff: DIFFERENCE" in line:
            issues.append(line.strip())
    return issues


def _print_exodus_summary(file1, file2, verbose):
    executable = _exodiff()
    absolute_rc, absolute_output = _run(
        [executable, "-absolute", "-tolerance", "0", file1, file2]
    )
    relative_rc, relative_output = _run(
        [executable, "-relative", "-tolerance", "0", file1, file2]
    )
    variables = _exodiff_variables(absolute_output) | _exodiff_variables(relative_output)
    absolute = _exodiff_differences(absolute_output)
    relative = _exodiff_differences(relative_output)
    variables.update(absolute)
    variables.update(relative)
    issues = _exodiff_issues(absolute_output)
    for issue in issues:
        print("issue: %s" % issue)
    for name in sorted(variables):
        absolute_record = absolute.get(name, (0.0, None, None, 0.0))
        relative_record = relative.get(name, (0.0, None, None, 0.0))
        print(
            "  %s: max_abs=%s t=%s max_rel=%s t=%s ref=%s"
            % (
                name,
                _format(absolute_record[0]),
                "n/a" if absolute_record[2] is None else _format(absolute_record[2]),
                _format(relative_record[0]),
                "n/a" if relative_record[2] is None else _format(relative_record[2]),
                _format(max(absolute_record[3], relative_record[3])),
            )
        )
    if variables:
        worst_name = max(
            sorted(variables), key=lambda name: absolute.get(name, (0.0,))[0]
        )
        absolute_record = absolute.get(worst_name, (0.0, None, None, 0.0))
        relative_record = relative.get(worst_name, (0.0, None, None, 0.0))
        print(
            "worst: %s timestep=%s time=%s abs=%s rel=%s ref=%s"
            % (
                worst_name,
                absolute_record[1] if absolute_record[1] is not None else "n/a",
                "n/a" if absolute_record[2] is None else _format(absolute_record[2]),
                _format(absolute_record[0]),
                _format(relative_record[0]),
                _format(max(absolute_record[3], relative_record[3])),
            )
        )
    if verbose:
        print("--- exodiff absolute output ---")
        print(absolute_output.rstrip())
        print("--- exodiff relative output ---")
        print(relative_output.rstrip())
    incompatible = any("exodiff: ERROR:" in issue for issue in issues)
    different = absolute_rc != 0 or relative_rc != 0
    return different, incompatible


def _authoritative_csv(file1, file2, comparison_file, verbose):
    tool = os.path.join(REPO, "python", "mooseutils", "csvdiff.py")
    rc, output = _run(
        [sys.executable, tool, file1, file2, "--comparison-file", comparison_file]
    )
    if verbose or rc:
        print("--- csvdiff output ---")
        print(output.rstrip())
    return rc


def _authoritative_exodus(file1, file2, comparison_file, verbose):
    rc, output = _run([_exodiff(), "-f", comparison_file, file1, file2])
    if verbose or rc:
        print("--- exodiff criteria output ---")
        print(output.rstrip())
    return rc


def run_compare(args):
    try:
        if not os.path.isfile(args.file1):
            raise CompareError("file not found: %s" % args.file1)
        if not os.path.isfile(args.file2):
            raise CompareError("file not found: %s" % args.file2)
        if args.cmp_file and not os.path.isfile(args.cmp_file):
            raise CompareError("comparison file not found: %s" % args.cmp_file)
        kind1 = _kind(args.file1)
        kind2 = _kind(args.file2)
        if kind1 != kind2:
            raise CompareError("file types differ: %s vs %s" % (kind1, kind2))
        print("compare: %s" % kind1)
        print("files: %s %s" % (args.file1, args.file2))
        try:
            if kind1 == "csv":
                different = _print_csv_summary(args.file1, args.file2, args.verbose)
                incompatible = False
            else:
                different, incompatible = _print_exodus_summary(
                    args.file1, args.file2, args.verbose
                )
        except CompareError as error:
            if not args.cmp_file:
                raise
            print("issue: %s" % error)
            different = True
            incompatible = True
        if args.cmp_file:
            print("cmp: %s" % args.cmp_file)
            if kind1 == "csv":
                rc = _authoritative_csv(
                    args.file1, args.file2, args.cmp_file, args.verbose
                )
            else:
                rc = _authoritative_exodus(
                    args.file1, args.file2, args.cmp_file, args.verbose
                )
            print("status: %s" % ("PASS" if rc == 0 else "FAIL"))
            return 0 if rc == 0 else 1
        if incompatible:
            print("status: INCOMPATIBLE")
            return 1
        print("status: %s" % ("DIFFERENT" if different else "SAME"))
        return 1 if different else 0
    except (CompareError, OSError) as error:
        print("compare: ERROR %s" % error)
        return 2
