# This file is part of the MOOSE framework
# https://mooseframework.inl.gov
#
# All rights reserved, see COPYRIGHT for full restrictions
# https://github.com/idaholab/moose/blob/master/COPYRIGHT
#
# Licensed under LGPL 2.1, please see LICENSE for details
# https://www.gnu.org/licenses/lgpl-2.1.html

"""Build an old ref with the current harness and installed dependencies."""

import argparse
import os
import subprocess
import tempfile
from pathlib import Path


def main():
    """Create a detached baseline and verify it with installed dependencies."""
    parser = argparse.ArgumentParser()
    parser.add_argument("root", type=Path)
    parser.add_argument("harness", type=Path)
    parser.add_argument("ref")
    parser.add_argument("scope")
    parser.add_argument("options", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    revision = subprocess.check_output(
        [
            "git",
            "-C",
            str(args.root),
            "rev-parse",
            "--verify",
            "--end-of-options",
            args.ref + "^{commit}",
        ],
        text=True,
    ).strip()
    # Retain the worktree and outputs for inspection even when compilation fails.
    directory = Path(tempfile.mkdtemp(prefix="moose-baseline-")) / "checkout"
    subprocess.check_call(
        [
            "git",
            "-C",
            str(args.root),
            "worktree",
            "add",
            "--detach",
            str(directory),
            revision,
        ]
    )
    print(
        "BASELINE ref=%s revision=%s scope=%s directory=%s neml2=%s conda=%s"
        % (
            args.ref,
            revision,
            args.scope,
            directory,
            os.environ.get("MOOSE_NEML2_SITE", "conda"),
            os.environ["CONDA_PREFIX"],
        ),
        flush=True,
    )
    print(
        "Reuse the installed NEML2; dependency versions are not historical. "
        "The worktree and outputs are retained.",
        flush=True,
    )
    # Installed Conda dependencies need no source checkout. Only large_media supplies
    # test inputs; clone its recorded revision locally, preserving the main submodule.
    if (directory / ".gitmodules").exists():
        media = subprocess.run(
            [
                "git",
                "-C",
                str(directory),
                "config",
                "-f",
                ".gitmodules",
                "--get",
                "submodule.large_media.path",
            ],
            capture_output=True,
            text=True,
        )
        if media.returncode == 0:
            source = args.root / media.stdout.strip()
            if not (source / ".git").exists():
                print(
                    "ERROR: initialize large_media in the main checkout first.",
                    flush=True,
                )
                return 1
            subprocess.check_call(
                [
                    "git",
                    "-C",
                    str(directory),
                    "-c",
                    "protocol.file.allow=always",
                    "-c",
                    "submodule.large_media.url=" + str(source),
                    "submodule",
                    "update",
                    "--init",
                    "--no-fetch",
                    "--",
                    media.stdout.strip(),
                ]
            )
    os.execv(
        str(args.harness),
        [
            str(args.harness),
            "-C",
            str(directory),
            "verify",
            args.scope,
            "--skip-neml2-sync",
            *args.options,
        ],
    )


if __name__ == "__main__":
    raise SystemExit(main())
