#!/usr/bin/env python3
# This file is part of the MOOSE framework
# https://mooseframework.inl.gov
#
# All rights reserved, see COPYRIGHT for full restrictions
# https://github.com/idaholab/moose/blob/master/COPYRIGHT
#
# Licensed under LGPL 2.1, please see LICENSE for details
# https://www.gnu.org/licenses/lgpl-2.1.html

"""Record CUDA dependency and executable identities without loading their libraries."""

import hashlib
import json
import os
import shlex
import subprocess
import sys
import uuid
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
STACK = ROOT / "kokkos-cuda-stack"
PREFIX = Path(os.environ.get("PREFIX", STACK / "prefix")).resolve()
STATE = STACK / ".cuda-cli"
STEPS = ("openmpi", "petsc", "libmesh", "wasp", "neml2")


def digest(value):
    """Hash a JSON value independently of dictionary insertion order."""
    return hashlib.sha256(json.dumps(value, sort_keys=True).encode()).hexdigest()


def identity(path):
    """Identify a resolved file by its path, size, and timestamps."""
    path = Path(path).resolve()
    if not path.is_file():
        return [str(path), None]
    stat = path.stat()
    return [str(path), stat.st_size, stat.st_mtime_ns, stat.st_ctime_ns]


def toolchain():
    """Describe the selected compilers, CUDA target, and install prefix."""
    names = (
        "CUDA_DIR",
        "CUDA_ARCH",
        "TORCH_CUDA_ARCH_LIST",
        "OMPI_CC",
        "OMPI_CXX",
        "OMPI_FC",
        "OpenBLAS_HOME",
        "FORTRAN_LIB_DIR",
        "BLAS_LIB",
        "LAPACK_LIB",
        "MINIFORGE_BIN",
        "MINIFORGE_PYTHON",
    )
    return {
        "prefix": str(PREFIX),
        "settings": {key: os.environ.get(key, "") for key in names},
        "compilers": [
            identity(os.environ[key]) for key in ("OMPI_CC", "OMPI_CXX", "OMPI_FC")
        ],
        "nvcc": identity(Path(os.environ["CUDA_DIR"]) / "bin/nvcc"),
        "python_config": identity(Path(os.environ["MINIFORGE_BIN"]) / "python3-config"),
    }


def paths():
    """Resolve installed Torch and NEML2 module directories in the stack venv."""
    python = STACK / "neml2-venv/bin/python3"
    code = (
        "import importlib.util, json; "
        "from pathlib import Path; "
        "print(json.dumps({name: "
        "str(Path(importlib.util.find_spec(name).origin).resolve().parent) "
        "for name in ('torch', 'neml2')}))"
    )
    return json.loads(subprocess.check_output([str(python), "-c", code], text=True))


def read(name):
    """Read a recorded identity, or return None when it has not been recorded."""
    path = STATE / f"{name}.json"
    return json.loads(path.read_text()) if path.exists() else None


def record(name, snapshot, provenance="built"):
    """Atomically record a snapshot with a new generation identifier."""
    STATE.mkdir(parents=True, exist_ok=True)
    path = STATE / f"{name}.json"
    temporary = path.with_suffix(f".tmp-{os.getpid()}")
    temporary.write_text(
        json.dumps(
            {"id": uuid.uuid4().hex, "snapshot": snapshot, "provenance": provenance},
            sort_keys=True,
        )
        + "\n"
    )
    temporary.replace(path)


def source_identity(folder):
    """Hash the source revision, tracked edits, and untracked source contents."""
    if not (folder / ".git").exists():
        return None

    def git(*args):
        return subprocess.check_output(["git", "-C", str(folder), *args])

    value = hashlib.sha256(
        git("rev-parse", "HEAD")
        + git("diff", "--binary", "HEAD")
        + git("submodule", "status", "--recursive")
    )
    for name in git("ls-files", "--others", "--exclude-standard", "-z").split(b"\0"):
        if name:
            path = folder / os.fsdecode(name)
            value.update(name)
            if path.is_file():
                value.update(path.read_bytes())
    return value.hexdigest()


def dependency(step):
    """Describe an installation and the inputs that determine its validity."""
    sources = {
        "petsc": ROOT / "petsc",
        "libmesh": ROOT / "libmesh",
        "wasp": ROOT / "framework/contrib/wasp",
        "neml2": ROOT / "framework/contrib/neml2",
    }
    files = {
        "openmpi": ["bin/opal_wrapper", "lib/libmpi.so", "include/mpi.h"],
        "petsc": ["lib/libpetsc.so", "include/petscconf.h"],
        "libmesh": ["bin/libmesh-config"]
        + [f"lib/libmesh_{m}.so" for m in ("opt", "oprof", "dbg", "devel")],
        "wasp": ["lib/libwaspcore.so"],
    }
    artifacts = [identity(PREFIX / name) for name in files.get(step, [])]
    if step == "neml2":
        artifacts.append(identity(STACK / "neml2-venv/bin/python3"))
        for module, path in paths().items():
            artifacts.append(identity(Path(path) / "__init__.py"))
            if module == "torch":
                artifacts.extend(
                    identity(Path(path) / "lib" / name)
                    for name in (
                        "libtorch.so",
                        "libtorch_cpu.so",
                        "libtorch_cuda.so",
                        "libc10.so",
                        "libc10_cuda.so",
                    )
                )
            else:
                artifacts.extend(
                    identity(p) for p in sorted(Path(path).rglob("libneml2*.so*"))
                )
    parents = {
        name: (read(f"dependency-{name}") or {}).get("id")
        for name in STEPS[: STEPS.index(step)]
    }
    recipe = b"".join(
        (STACK / "scripts" / name).read_bytes()
        for name in ("env.sh", f"build_{step}.sh")
    )
    return {
        "toolchain": toolchain(),
        "source": source_identity(sources[step]) if step in sources else None,
        "recipe": hashlib.sha256(recipe).hexdigest(),
        "parents": parents,
        "artifacts": artifacts,
    }


def configuration():
    """Describe the generated MOOSE configuration and selected dependencies."""
    enabled = os.environ.get("NEML2_SUPPORT", "1") == "1"
    return {
        "toolchain": toolchain(),
        "paths": paths() if enabled else {},
        "dependencies": {
            name: (read(f"dependency-{name}") or {}).get("id")
            for name in STEPS
            if enabled or name != "neml2"
        },
        "conf_vars": (ROOT / "conf_vars.mk").read_text(),
        "options": subprocess.check_output(
            [str(ROOT / "config.status"), "--config"], text=True
        ),
    }


def main():
    """Dispatch state checks and records requested by the shell CLI."""
    action = sys.argv[1]
    name = sys.argv[2] if len(sys.argv) > 2 else ""
    if action == "check-toolchain":
        previous = read("configuration")
        if previous and previous["snapshot"]["toolchain"] != toolchain():
            sys.exit(
                "CUDA toolchain/prefix changed. "
                "Use a separate checkout/install prefix; "
                "do not reuse objects built by the previous toolchain."
            )
    elif action == "check-paths":
        variables = dict(
            line.split(":=", 1)
            for line in (ROOT / "conf_vars.mk").read_text().splitlines()
            if ":=" in line
        )
        variables = {key.strip(): value.strip() for key, value in variables.items()}
        expected = paths()
        return int(
            any(
                Path(variables.get(key, "/missing")).resolve()
                != Path(expected[module]).resolve()
                for key, module in (("LIBTORCH_DIR", "torch"), ("NEML2_DIR", "neml2"))
            )
        )
    elif action == "configure-args":
        options = (
            subprocess.check_output(
                [str(ROOT / "config.status"), "--config"], text=True
            )
            if (ROOT / "config.status").is_file()
            else ""
        )
        owned = (
            "--with-kokkos",
            "--with-libtorch",
            "--with-neml2",
            "--without-kokkos",
            "--without-libtorch",
            "--without-neml2",
        )
        args = [
            arg for arg in shlex.split(options) if arg.split("=", 1)[0] not in owned
        ]
        args.append("--with-kokkos=cuda")
        if os.environ.get("NEML2_SUPPORT", "1") == "1":
            expected = paths()
            args.extend(
                (
                    f"--with-libtorch={expected['torch']}",
                    f"--with-neml2={expected['neml2']}",
                )
            )
        print("\n".join(args))
    elif action in ("check-dependency", "record-dependency"):
        snapshot = dependency(name)
        key = f"dependency-{name}"
        if action == "record-dependency":
            record(key, snapshot, sys.argv[3] if len(sys.argv) > 3 else "built")
        else:
            previous = read(key)
            return int(not previous or previous["snapshot"] != snapshot)
    elif action in ("check-config", "record-config"):
        snapshot = configuration()
        if action == "record-config":
            record("configuration", snapshot)
        else:
            previous = read("configuration")
            return int(not previous or previous["snapshot"] != snapshot)
    elif action in ("check-target", "record-target"):
        snapshot = {
            "configuration": digest(read("configuration")),
            "method": os.environ.get("METHOD", "opt"),
        }
        key = f"target-{name}-{snapshot['method']}"
        if action == "record-target":
            record(key, snapshot)
        else:
            previous = read(key)
            return int(not previous or previous["snapshot"] != snapshot)
    else:
        sys.exit(f"Unknown state action: {action}")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        sys.exit(str(error))
