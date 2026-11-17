#!/usr/bin/env python3
# This file is part of the MOOSE framework
# https://mooseframework.inl.gov
#
# All rights reserved, see COPYRIGHT for full restrictions
# https://github.com/idaholab/moose/blob/master/COPYRIGHT
#
# Licensed under LGPL 2.1, please see LICENSE for details
# https://www.gnu.org/licenses/lgpl-2.1.html

"""Expose shared comparison and log-analysis commands."""

import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

# Resolve sibling utilities after adding the script directory for package imports.
from compare_util import run_compare, run_exodiff  # noqa: E402
from logstat_util import run_logstat  # noqa: E402


def main(argv=None):
    """Parse and dispatch a shared harness command."""
    parser = argparse.ArgumentParser(prog="moose-harness")
    sub = parser.add_subparsers(dest="command", required=True)
    c = sub.add_parser(
        "compare",
        help="compare Exodus or CSV output",
        description="compare Exodus or CSV output",
    )
    c.add_argument("file1")
    c.add_argument("file2")
    c.add_argument("--cmp", dest="cmp_file", default=None)
    c.add_argument("--verbose", action="store_true")
    e = sub.add_parser("exodiff", help="compare Exodus with TestHarness tolerances")
    e.add_argument("file1")
    e.add_argument("file2")
    e.add_argument("options", nargs=argparse.REMAINDER)
    logstat = sub.add_parser(
        "logstat",
        help="summarize solver convergence",
        description="summarize solver convergence",
    )
    logstat.add_argument("logs", nargs="+")
    logstat.add_argument("--verbose", action="store_true")
    args = parser.parse_args(argv)
    if args.command == "compare":
        return run_compare(args)
    if args.command == "exodiff":
        return run_exodiff(args)
    return run_logstat(args)


if __name__ == "__main__":
    raise SystemExit(main())
