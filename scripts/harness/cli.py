#!/usr/bin/env python3
import argparse
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

from compare_util import run_compare
from logstat_util import run_logstat


def main(argv=None):
    parser = argparse.ArgumentParser(prog="moose-harness")
    sub = parser.add_subparsers(dest="command", required=True)
    c = sub.add_parser(
        "compare", help="compare Exodus or CSV output", description="compare Exodus or CSV output"
    )
    c.add_argument("file1")
    c.add_argument("file2")
    c.add_argument("--cmp", dest="cmp_file", default=None)
    c.add_argument("--verbose", action="store_true")
    l = sub.add_parser(
        "logstat", help="summarize solver convergence", description="summarize solver convergence"
    )
    l.add_argument("logs", nargs="+")
    l.add_argument("--verbose", action="store_true")
    args = parser.parse_args(argv)
    if args.command == "compare":
        return run_compare(args)
    return run_logstat(args)


if __name__ == "__main__":
    raise SystemExit(main())
