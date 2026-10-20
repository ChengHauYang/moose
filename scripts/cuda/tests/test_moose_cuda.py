# This file is part of the MOOSE framework
# https://mooseframework.inl.gov
#
# All rights reserved, see COPYRIGHT for full restrictions
# https://github.com/idaholab/moose/blob/master/COPYRIGHT
#
# Licensed under LGPL 2.1, please see LICENSE for details
# https://www.gnu.org/licenses/lgpl-2.1.html

"""Exercise the Linux CUDA CLI with isolated mock installations and tools."""

import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
PYTHON = sys.executable

MOCK = r"""
import json, os, pathlib, sys
p = pathlib.Path
op = p(sys.argv[0]).name
args = sys.argv[1:]
if op == 'mock': op, args = args[0], args[1:]
keys = ('PREFIX','MOOSE_DIR','CUDA_DIR','CUDA_HOME','CUDA_ARCH',
        'TORCH_CUDA_ARCH_LIST','OMPI_CC','OMPI_CXX','OMPI_FC','OpenBLAS_HOME',
        'MOOSE_JOBS','NEML2_SUPPORT','FORCE_REBUILD','CUDA_VISIBLE_DEVICES',
        'PETSC_OPTIONS','OMP_NUM_THREADS','METHOD','CONDA_PREFIX','CPATH',
        'CMAKE_ARGS','CXX_FOR_BUILD','TEST_JOBS')
record = dict(op=op,args=args,cwd=os.getcwd(),env={k:os.getenv(k) for k in keys})
with open(os.environ['TEST_TRACE'],'a') as f: f.write(json.dumps(record)+'\n')
if os.getenv('TEST_FAIL_OP') == op: sys.exit(7)
if op == 'uname': print('Linux')
elif op == 'nsys': print('Moose::Kokkos::FixtureKernel')
elif op == 'git':
    if 'rev-parse' in args: print('abc123')
    elif 'submodule' in args and 'status' in args: print(' abc123 fixture (heads/main)')
elif op == 'ompi_info':
    print('MPI extensions: cuda')
    print('x' * 100000)
elif op == 'python3':
    if args and args[0] == '-c' and 'importlib.util.find_spec' in args[1]:
        packages = p(os.environ['MOOSE_DIR'])/'kokkos-cuda-stack/neml2-venv/packages'
        print(json.dumps({name: str(packages/name) for name in ('torch','neml2')}))
        sys.exit(0)
    if args and args[0] == '-c' and 'import neml2' in args[1]:
        sys.exit(int(os.getenv('TEST_NEML2_MISSING','0')))
    os.execv(REAL_PYTHON, [REAL_PYTHON] + args)
elif op == 'configure':
    root = p(os.environ['MOOSE_DIR'])
    if p.cwd() != root: sys.exit(0)
    flags = ' '.join(args)
    (root/'config.status').write_text('#!/bin/sh\nprintf "%s\\n" '+repr(flags)+'\n')
    (root/'config.status').chmod(0o755)
    neml2 = any(arg.startswith('--with-neml2') for arg in args)
    packages = root/'kokkos-cuda-stack/neml2-venv/packages'
    (root/'conf_vars.mk').write_text(
        'ENABLE_KOKKOS := true\nENABLE_KOKKOS_GPU := true\n'
        + 'ENABLE_NEML2 := '+('true' if neml2 else 'no')+'\n'
        + 'ENABLE_LIBTORCH := '+('true' if neml2 else 'no')+'\n'
        + 'LIBTORCH_DIR := '+str(packages/'torch')+'\n'
        + 'NEML2_DIR := '+str(packages/'neml2')+'\n')
elif op.startswith('build_'):
    prefix = p(os.environ['PREFIX'])
    (prefix/'lib').mkdir(exist_ok=True)
    (prefix/'include').mkdir(exist_ok=True)
    if op == 'build_petsc':
        (prefix/'lib/libpetsc.so').touch()
        (prefix/'include/petscconf.h').write_text(
            '#define PETSC_HAVE_CUDA 1\n#define PETSC_HAVE_KOKKOS 1\n')
    elif op == 'build_libmesh': (prefix/'lib/libmesh_opt.so').touch()
    elif op == 'build_wasp': (prefix/'lib/libwaspcore.so').touch()
elif op.endswith('-opt'):
    if '--show-capabilities' in args:
        print('heading')
        print(json.dumps({k:{'value':v} for k,v in dict(kokkos='4.7',
            cuda=False if os.getenv('TEST_CAPS_FALSE') else '12.4',
            neml2=os.getenv('NEML2_SUPPORT') != '0').items()}))
        print('footer')
"""


class CudaCLI(unittest.TestCase):
    """Check command dispatch, build identities, and legacy stack compatibility."""

    def setUp(self):
        """Create an isolated stack with recorded dependency and target identities."""
        self.temp = tempfile.TemporaryDirectory(prefix="cuda cli ")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.cli = self.root / "scripts/cuda/moose_cuda.sh"
        self.cli.parent.mkdir(parents=True)
        for filename in (
            "moose_cuda.sh",
            "moose_cuda_common.sh",
            "moose_cuda_state.py",
        ):
            shutil.copy2(REPO / "scripts/cuda" / filename, self.cli.parent / filename)
        self.stack = self.root / "kokkos-cuda-stack"
        self.legacy = self.stack / "scripts"
        self.legacy.mkdir(parents=True)
        shutil.copy2(REPO / "kokkos-cuda-stack/scripts/env.sh", self.legacy / "env.sh")
        self.prefix = self.stack / "prefix"
        self.bin = self.prefix / "bin"
        self.bin.mkdir(parents=True)
        self.trace = self.root / "trace.jsonl"
        self.mock = "#!" + PYTHON + "\nREAL_PYTHON=" + repr(PYTHON) + "\n" + MOCK
        for name in (
            "uname",
            "git",
            "make",
            "python3",
            "nsys",
            "ompi_info",
            "mpicc",
            "mpicxx",
            "mpif90",
            "libmesh-config",
            "mock",
            "nvcc",
            "nvidia-smi",
            "cmake",
            "gcc",
            "g++",
            "gfortran",
        ):
            self.write_exe(self.bin / name, self.mock)
        self.write_exe(self.stack / "neml2-venv/bin/python3", self.mock)
        self.write_exe(self.root / "configure", self.mock)
        for sub in (
            "petsc",
            "libmesh",
            "framework/contrib/wasp",
            "framework/contrib/neml2/.git",
            "framework/contrib/pytorch",
        ):
            (self.root / sub).mkdir(parents=True, exist_ok=True)
        for target in (
            "test",
            "modules/solid_mechanics",
            "modules/shifted_boundary_method",
        ):
            folder = self.root / target
            folder.mkdir(parents=True, exist_ok=True)
            name = "moose_test" if target == "test" else Path(target).name
            self.write_exe(folder / (name + "-opt"), self.mock)
            self.write_exe(folder / "run_tests", self.mock)
        for name in ("openmpi", "petsc", "libmesh", "wasp", "neml2"):
            self.write_exe(
                self.legacy / ("build_" + name + ".sh"),
                '#!/bin/bash\nsource "$(dirname "$0")/env.sh"\n'
                'exec "$PREFIX/bin/mock" build_' + name + "\n",
            )
        self.write_exe(
            self.legacy / "run_benchmark.sh",
            '#!/bin/bash\nexec "$PREFIX/bin/mock" benchmark\n',
        )
        self.env = os.environ.copy()
        self.env.update(
            PATH=str(self.bin) + ":" + os.environ["PATH"],
            MOOSE_DIR=str(self.root),
            PREFIX=str(self.prefix),
            TEST_TRACE=str(self.trace),
            MOOSE_CUDA_STACK_KEEP=(
                "TEST_TRACE TEST_FAIL_OP TEST_CAPS_FALSE TEST_NEML2_MISSING"
            ),
            CUDA_DIR=str(self.prefix),
            MINIFORGE_BIN=str(self.bin),
            NEML2_SUPPORT="1",
            OMPI_CC=str(self.bin / "gcc"),
            OMPI_CXX=str(self.bin / "g++"),
            OMPI_FC=str(self.bin / "gfortran"),
            CUDA_VISIBLE_DEVICES="1",
            PETSC_OPTIONS="-use_gpu_aware_mpi 1",
            OMP_NUM_THREADS="2",
            CMAKE_ARGS="conda-contamination",
            CPATH="/bad/conda/include",
            CXX_FOR_BUILD="bad-conda-cxx",
            OpenBLAS_HOME="/custom/blas",
            CUDA_ARCH="90",
            TORCH_CUDA_ARCH_LIST="9.0",
            MOOSE_JOBS="8",
            TEST_JOBS="1",
            METHOD="opt",
        )
        self.install_deps()
        self.configure()
        for dependency in ("openmpi", "petsc", "libmesh", "wasp", "neml2"):
            self.state("record-dependency", dependency)
        self.state("record-config")
        for target in (
            "framework",
            "test",
            "solid_mechanics",
            "shifted_boundary_method",
        ):
            self.state("record-target", target)
        self.trace.write_text("")

    def state(self, *args):
        """Run the state helper against the temporary installation."""
        subprocess.run(
            [PYTHON, str(self.cli.parent / "moose_cuda_state.py"), *args],
            env=self.env,
            check=True,
        )

    def write_exe(self, path, text):
        """Write an executable fixture and create its parent directories."""
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)
        path.chmod(0o755)

    def install_deps(self):
        """Create the minimal installed artifacts required by dependency checks."""
        (self.prefix / "include").mkdir(exist_ok=True)
        (self.prefix / "lib").mkdir(exist_ok=True)
        (self.prefix / "include/petscconf.h").write_text(
            "#define PETSC_HAVE_CUDA 1\n#define PETSC_HAVE_KOKKOS 1\n"
        )
        for name in ("libpetsc.so", "libmesh_opt.so", "libwaspcore.so"):
            (self.prefix / "lib" / name).touch()

    def configure(self, neml2=True):
        """Create a CUDA configuration with optional NEML2 support."""
        flags = "--with-kokkos=cuda" + (
            " --with-libtorch --with-neml2" if neml2 else ""
        )
        self.write_exe(
            self.root / "config.status",
            '#!/bin/sh\nprintf "%s\\n" ' + repr(flags) + "\n",
        )
        (self.root / "conf_vars.mk").write_text(
            "ENABLE_KOKKOS := true\nENABLE_KOKKOS_GPU := true\n"
            + "ENABLE_NEML2 := "
            + ("true" if neml2 else "no")
            + "\n"
            + "ENABLE_LIBTORCH := "
            + ("true" if neml2 else "no")
            + "\n"
            + "LIBTORCH_DIR := "
            + str(self.stack / "neml2-venv/packages/torch")
            + "\n"
            + "NEML2_DIR := "
            + str(self.stack / "neml2-venv/packages/neml2")
            + "\n"
        )

    def run_cli(self, *args, rc=0, env=None):
        """Invoke the CLI and assert the expected exit status."""
        result = subprocess.run(
            ["/bin/bash", str(self.cli), *args],
            cwd=self.root,
            env=self.env if env is None else env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        self.assertEqual(result.returncode, rc, result.stdout)
        return result.stdout

    def events(self, op=None):
        """Read mock tool invocations, optionally filtered by tool name."""
        items = (
            [json.loads(line) for line in self.trace.read_text().splitlines()]
            if self.trace.exists()
            else []
        )
        return [x for x in items if op is None or x["op"] == op]

    def test_read_only_commands(self):
        """Keep help, doctor, and status free of build-state mutations."""
        before = {p.name: p.read_bytes() for p in (self.stack / ".cuda-cli").iterdir()}
        for command in ("help", "doctor", "status"):
            with self.subTest(command=command):
                self.run_cli(command)
        self.assertFalse((self.stack / "logs").exists())
        self.assertEqual(
            before,
            {p.name: p.read_bytes() for p in (self.stack / ".cuda-cli").iterdir()},
        )
        self.assertFalse(self.events("make"))

    def test_incremental_compile_and_environment(self):
        """Use incremental makes with the selected clean toolchain environment."""
        self.run_cli("compile", "solid_mechanics", "-j", "3")
        makes = self.events("make")
        self.assertEqual(
            [x["args"] for x in makes],
            [
                ["-C", str(self.root / "framework"), "-j", "3"],
                ["-C", str(self.root / "modules/solid_mechanics"), "-j", "3"],
            ],
        )
        for item in makes:
            env = item["env"]
            for key in ("CONDA_PREFIX", "CMAKE_ARGS", "CPATH", "CXX_FOR_BUILD"):
                self.assertIsNone(env[key])
            for key, value in dict(
                MOOSE_JOBS="3",
                CUDA_VISIBLE_DEVICES="1",
                CUDA_ARCH="90",
                TORCH_CUDA_ARCH_LIST="9.0",
                OpenBLAS_HOME="/custom/blas",
                METHOD="opt",
            ).items():
                self.assertEqual(env[key], value)
        self.assertFalse(self.events("configure"))

    def test_framework_compile_does_not_validate_old_app(self):
        """Build the framework without validating an unrelated old executable."""
        self.env["TEST_CAPS_FALSE"] = "1"
        self.run_cli("compile", "framework")
        self.assertEqual(len(self.events("make")), 1)
        self.assertFalse(self.events("moose_test-opt"))

    def test_verify_framework_builds_test(self):
        """Build the framework test application before running its harness."""
        self.run_cli("verify", "framework", "kokkos", "-j8")
        self.assertEqual(self.events("make")[-1]["args"][1], str(self.root / "test"))
        self.assertEqual(
            self.events("run_tests")[-1]["args"],
            ["-j", "1", "--re", "kokkos", "--compute-device=cuda"],
        )

    def test_run_relative_input_and_device(self):
        """Preserve relative input paths and explicit compute-device overrides."""
        path = self.root / "input file.i"
        path.touch()
        for options in ((), ("--compute-device=cpu",), ("--compute-device", "cpu")):
            with self.subTest(options=options):
                self.run_cli("run", "solid_mechanics", path.name, *options)
                item = [
                    x for x in self.events("solid_mechanics-opt") if "-i" in x["args"]
                ][-1]
                self.assertEqual(item["cwd"], str(self.root))
                self.assertEqual(
                    item["args"],
                    ["-i", path.name] + list(options or ("--compute-device=cuda",)),
                )

    def test_test_defaults_and_passthrough(self):
        """Forward harness options and allocate enough MPI and thread slots."""
        self.run_cli("test", "--re", "kokkos", "--compute-device", "cpu")
        self.assertEqual(
            self.events("run_tests")[-1]["args"],
            ["-j", "1", "--re", "kokkos", "--compute-device", "cpu"],
        )
        self.run_cli(
            "test",
            "solid_mechanics",
            "kokkos",
            "-p2",
            "--n-threads=3",
            "--distributed-mesh",
        )
        self.assertEqual(
            self.events("run_tests")[-1]["args"],
            [
                "-j",
                "6",
                "--re",
                "kokkos",
                "--compute-device=cuda",
                "-p2",
                "--n-threads=3",
                "--distributed-mesh",
            ],
        )

    def test_init_reuses_deps_and_does_not_build_apps(self):
        """Reuse matching installations without building MOOSE applications."""
        self.run_cli("init", "-j4")
        self.assertFalse([x for x in self.events() if x["op"].startswith("build_")])
        self.assertFalse(self.events("make"))
        self.assertFalse(self.events("configure"))

    def test_init_force_propagates_to_all_installers(self):
        """Propagate forced rebuilds and job settings to every installer."""
        self.run_cli("init", "--force", "-j5")
        builds = [x for x in self.events() if x["op"].startswith("build_")]
        self.assertEqual(
            [x["op"] for x in builds],
            [
                "build_openmpi",
                "build_petsc",
                "build_libmesh",
                "build_wasp",
                "build_neml2",
            ],
        )
        for item in builds:
            self.assertEqual(item["env"]["FORCE_REBUILD"], "1")
            self.assertEqual(item["env"]["MOOSE_JOBS"], "5")

    def test_init_no_neml2_configures_without_install(self):
        """Disable NEML2 configuration without invoking its installer."""
        self.run_cli("init", "--no-neml2")
        self.assertEqual(self.events("configure")[-1]["args"], ["--with-kokkos=cuda"])
        self.assertFalse(self.events("build_neml2"))

    def test_compile_neml2_forces_source_install_with_jobs(self):
        """Force a NEML2 source install with the requested job count."""
        self.run_cli("compile", "neml2", "-j2")
        item = self.events("build_neml2")[-1]
        self.assertEqual(item["env"]["FORCE_REBUILD"], "1")
        self.assertEqual(item["env"]["MOOSE_JOBS"], "2")

    def test_compile_all_and_full_pipeline(self):
        """Build supported targets and keep benchmarks out of the full pipeline."""
        self.run_cli("compile", "all")
        self.assertEqual(len(self.events("make")), 4)
        self.run_cli("all", "-j4")
        self.assertEqual(
            self.events("run_tests")[-1]["args"], ["-j", "1", "--compute-device=cuda"]
        )
        self.assertFalse(self.events("benchmark"))

    def test_failure_propagation(self):
        """Stop verification after a compile failure and report the retry command."""
        self.env["TEST_FAIL_OP"] = "make"
        output = self.run_cli("verify", "framework", rc=7)
        self.assertIn("retry:", output)
        self.assertFalse(self.events("run_tests"))

    def test_capability_failure(self):
        """Reject executables that lack the selected CUDA capabilities."""
        self.env["TEST_CAPS_FALSE"] = "1"
        self.run_cli("compile", "solid_mechanics", rc=1)
        self.run_cli("test", "solid_mechanics", rc=1)
        self.assertFalse(self.events("run_tests"))

    def test_invalid_controls(self):
        """Reject invalid options before invoking builds or tests."""
        for args in (
            ("compile", "-j0"),
            ("compile", "--force"),
            ("compile", "--no-neml2"),
            ("init", "--unknown"),
            ("test", "solid_mechanics", "-p0"),
        ):
            with self.subTest(args=args):
                self.run_cli(*args, rc=1)
        self.assertFalse(self.events("make"))
        self.assertFalse(self.events("run_tests"))

    def test_linux_guard(self):
        """Reject build commands on a non-Linux host before creating build logs."""
        self.write_exe(self.bin / "uname", "#!/bin/sh\necho Darwin\n")
        self.run_cli("init", rc=1)
        self.assertFalse(self.events("make"))
        self.assertFalse((self.stack / "logs").exists())

    def test_legacy_env_defaults_without_cli(self):
        """Retain the CUDA machine defaults when sourcing the legacy environment."""
        env = {
            key: os.environ[key]
            for key in ("HOME", "USER", "PATH")
            if key in os.environ
        }
        env.update(TEST_TRACE=str(self.trace), MOOSE_CUDA_STACK_KEEP="TEST_TRACE")
        result = subprocess.run(
            [
                "/bin/bash",
                "-c",
                'source "$1"; "$PREFIX/bin/mock" legacy-env',
                "_",
                str(self.legacy / "env.sh"),
            ],
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        self.assertEqual(result.returncode, 0, result.stdout)
        actual = self.events("legacy-env")[-1]["env"]
        for key, value in dict(
            MOOSE_DIR=str(self.root),
            PREFIX=str(self.prefix),
            CUDA_DIR="/usr/local/cuda",
            OMPI_CC="/usr/bin/gcc-11",
            OMPI_CXX="/usr/bin/g++-11",
            OMPI_FC="/usr/bin/gfortran-9",
            MOOSE_JOBS="8",
        ).items():
            self.assertEqual(actual[key], value)

    def test_legacy_openmpi_force_and_overrides(self):
        """Honor forced OpenMPI rebuilds and compiler and CUDA overrides."""
        shutil.copy2(
            REPO / "kokkos-cuda-stack/scripts/build_openmpi.sh",
            self.legacy / "build_openmpi.sh",
        )
        self.write_exe(self.stack / "src/openmpi-4.1.6/configure", self.mock)
        self.run_cli("init", "--force", "-j3")
        configure = [
            x
            for x in self.events("configure")
            if "--enable-mpi-fortran=usempi" in x["args"]
        ]
        self.assertEqual(len(configure), 1)
        for arg in (
            "--with-cuda=" + str(self.prefix),
            "CC=" + str(self.bin / "gcc"),
            "CXX=" + str(self.bin / "g++"),
            "FC=" + str(self.bin / "gfortran"),
        ):
            self.assertIn(arg, configure[0]["args"])

    def test_legacy_activation_is_repeatable(self):
        """Keep PATH ordering stable when activating the stack repeatedly."""
        shutil.copy2(
            REPO / "kokkos-cuda-stack/scripts/activate.sh", self.legacy / "activate.sh"
        )
        result = subprocess.run(
            [
                "/bin/bash",
                "-c",
                'source "$1"; first="$PATH"; source "$1"; '
                '[[ "$first" == "$PATH" ]]; "$PREFIX/bin/mock" activated',
                "_",
                str(self.legacy / "activate.sh"),
            ],
            env=self.env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
        )
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertEqual(
            self.events("activated")[-1]["env"]["PREFIX"], str(self.prefix)
        )

    def test_legacy_benchmark_uses_venv_and_reports_failure(self):
        """Use the stack venv for benchmarks and propagate profiler failures."""
        shutil.copy2(
            REPO / "kokkos-cuda-stack/scripts/run_benchmark.sh",
            self.legacy / "run_benchmark.sh",
        )
        bench = (
            self.root
            / "modules/solid_mechanics/test/tests/kokkos/linear_elasticity"
            / "benchmark_cpu_and_gpu.py"
        )
        self.write_exe(bench, self.mock)
        self.env["TEST_FAIL_OP"] = "benchmark_cpu_and_gpu.py"
        self.run_cli("benchmark", rc=7)
        self.assertTrue(self.events("nsys"))
        self.assertTrue(self.events("benchmark_cpu_and_gpu.py"))

    def test_old_library_paths_are_reconfigured(self):
        """Replace stale library paths while retaining unrelated configure options."""
        self.write_exe(
            self.root / "config.status",
            "#!/bin/sh\nprintf '%s\\n' '--with-kokkos=cuda "
            "--with-libtorch=/old/torch --with-neml2=/old/neml2 "
            "--with-derivative-size=128'\n",
        )
        with (self.root / "conf_vars.mk").open("a") as stream:
            stream.write("LIBTORCH_DIR := /old/torch\nNEML2_DIR := /old/neml2\n")
        self.run_cli("init")
        args = self.events("configure")[-1]["args"]
        self.assertIn(
            "--with-libtorch=" + str(self.stack / "neml2-venv/packages/torch"), args
        )
        self.assertIn(
            "--with-neml2=" + str(self.stack / "neml2-venv/packages/neml2"), args
        )
        self.assertIn("--with-derivative-size=128", args)
        self.assertFalse(any("/old/" in arg for arg in args))

    def test_upstream_rebuild_invalidates_downstream_and_targets(self):
        """Rebuild downstream dependencies and clean affected targets only once."""
        (self.prefix / "lib/libpetsc.so").unlink()
        self.run_cli("init")
        self.assertEqual(
            [x["op"] for x in self.events() if x["op"].startswith("build_")],
            ["build_petsc", "build_libmesh", "build_wasp", "build_neml2"],
        )
        self.run_cli("test", "solid_mechanics", rc=1)
        self.assertFalse(self.events("run_tests"))
        self.run_cli("compile", "solid_mechanics")
        cleans = [x for x in self.events("make") if "clean" in x["args"]]
        self.assertEqual(
            [x["args"][1] for x in cleans],
            [str(self.root / "framework"), str(self.root / "modules/solid_mechanics")],
        )
        self.run_cli("compile", "solid_mechanics")
        self.assertEqual(
            len([x for x in self.events("make") if "clean" in x["args"]]), 2
        )

    def test_failed_downstream_rebuild_resumes_without_reusing_old_state(self):
        """Resume an interrupted cascade without repeating its successful steps."""
        (self.prefix / "lib/libpetsc.so").unlink()
        self.env["TEST_FAIL_OP"] = "build_libmesh"
        self.run_cli("init", rc=7)
        self.env.pop("TEST_FAIL_OP")
        self.run_cli("init")
        self.assertEqual(len(self.events("build_petsc")), 1)
        self.assertEqual(len(self.events("build_libmesh")), 2)
        self.assertEqual(len(self.events("build_neml2")), 1)

    def test_external_artifact_change_rejects_compile(self):
        """Reject compilation when an installed dependency changes externally."""
        (self.prefix / "lib/libpetsc.so").write_text("different installed PETSc")
        self.run_cli("compile", "solid_mechanics", rc=1)
        self.assertFalse(self.events("make"))

    def test_toolchain_change_is_rejected_before_installers(self):
        """Reject changed CUDA targets before invoking dependency installers."""
        self.env["CUDA_ARCH"] = "80"
        self.run_cli("init", rc=1)
        self.assertFalse([x for x in self.events() if x["op"].startswith("build_")])

    def test_failed_compile_rejects_existing_executable(self):
        """Invalidate executable reuse after a failed compilation."""
        self.env["TEST_FAIL_OP"] = "make"
        self.run_cli("compile", "solid_mechanics", rc=7)
        self.env.pop("TEST_FAIL_OP")
        self.run_cli("test", "solid_mechanics", rc=1)

    def test_adopts_existing_installations_without_rebuilding(self):
        """Record valid existing installations with adopted provenance."""
        shutil.rmtree(self.stack / ".cuda-cli")
        self.run_cli("init")
        self.assertFalse([x for x in self.events() if x["op"].startswith("build_")])
        state = json.loads((self.stack / ".cuda-cli/dependency-petsc.json").read_text())
        self.assertEqual(state["provenance"], "adopted")

    def test_plan_command_groups_stop_on_failure(self):
        """Stop qualification command groups at their first unexpected failure."""
        result = subprocess.run(
            ["/bin/bash", "-c", "set -euo pipefail; (false; echo should-not-run)"],
            capture_output=True,
            text=True,
        )
        self.assertNotEqual(result.returncode, 0)
        self.assertNotIn("should-not-run", result.stdout)


if __name__ == "__main__":
    unittest.main(verbosity=2)
