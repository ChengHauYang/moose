# Solid Mechanics

The MOOSE Solid Mechanics module provides finite-element models for deformation, stress,
inelastic material behavior, fracture, contact, and related mechanics applications. See the
[Solid Mechanics documentation](https://mooseframework.inl.gov/modules/solid_mechanics/index.html)
for usage details and examples.

## Prerequisites

Install MOOSE's compiler and dependency stack by following the
[MOOSE installation instructions](https://mooseframework.inl.gov/getting_started/installation/index.html),
then clone the MOOSE repository. The commands below assume the repository is located at
`<path_to_moose>`.

If MOOSE was installed with Conda, activate its environment before configuring, building, or
testing:

```bash
conda activate moose
```

## Build

For the standard CPU build:

```bash
cd <path_to_moose>/modules/solid_mechanics
make -j4
```

Replace `4` with the number of build jobs appropriate for your system. The resulting optimized
executable is `solid_mechanics-opt`.

### Build With Kokkos

Kokkos objects are not included in the standard configuration. Configure MOOSE from the
repository root before building the module:

```bash
cd <path_to_moose>
./configure --with-kokkos=cpu
cd modules/solid_mechanics
make -j4
```

Use a MOOSE dependency stack whose PETSc installation includes Kokkos support. After changing
the MOOSE configuration or updating dependencies, clean the framework before rebuilding to avoid
stale generated headers or library metadata:

```bash
cd <path_to_moose>/framework
make clean
cd ../modules/solid_mechanics
make -j4
```

## Test

Run the complete Solid Mechanics test suite from the module directory:

```bash
cd <path_to_moose>/modules/solid_mechanics
./run_tests -j4
```

For a CPU Kokkos build, select the CPU compute device:

```bash
./run_tests --compute-device=cpu -j4
```

To run only the Kokkos isotropic linear elasticity tests:

```bash
./run_tests --re=kokkos.linear_elasticity --compute-device=cpu -j2
```

## Run

Run an input file with the optimized executable:

```bash
./solid_mechanics-opt -i <input_file.i>
```

For a Kokkos input, also select the configured compute device:

```bash
./solid_mechanics-opt -i <input_file.i> --compute-device=cpu
```
