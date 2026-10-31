#!/usr/bin/env python3
# This file is part of the MOOSE framework
# https://mooseframework.inl.gov
#
# All rights reserved, see COPYRIGHT for full restrictions
# https://github.com/idaholab/moose/blob/master/COPYRIGHT
#
# Licensed under LGPL 2.1, please see LICENSE for details
# https://www.gnu.org/licenses/lgpl-2.1.html

"""Test moose-neml2-deps (run by conda-build as run_test.py)."""

import os
import subprocess
import sys

import nmhit

if __name__ == "__main__":
    # NEML2's Findnmhit.cmake links the static library and headers that the
    # nmhit Python package installs
    nmhit_dir = os.path.dirname(nmhit.__file__)
    for path in ["lib/libnmhit.a", "include/nmhit/nmhit.h"]:
        if not os.path.isfile(os.path.join(nmhit_dir, path)):
            sys.exit(f"Missing {path} in {nmhit_dir}")

    # torch is the only requirement left to MOOSE's PyTorch build
    result = subprocess.run(
        [sys.executable, "-m", "pip", "check"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        check=False,
    )
    print(result.stdout)
    problems = [
        line
        for line in result.stdout.splitlines()
        if line.strip()
        and not line.startswith("No broken requirements")
        and "requires torch, which is not installed" not in line
    ]
    if problems:
        sys.exit("Unexpected pip check failures:\n" + "\n".join(problems))
