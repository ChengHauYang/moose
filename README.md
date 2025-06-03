MOOSE
=====

[![Build status](https://civet.inl.gov/idaholab/moose/master/branch_status.svg)](https://civet.inl.gov/repo/idaholab/moose/)

The Multiphysics Object-Oriented Simulation Environment (MOOSE) is a finite-element, multiphysics framework primarily developed by [Idaho National Laboratory](http://www.inl.gov). It provides a high-level interface to some of the most sophisticated [nonlinear solver technology](http://www.mcs.anl.gov/petsc/) on the planet. MOOSE presents a straightforward API that aligns well with the real-world problems scientists and engineers need to tackle. Every detail about how an engineer interacts with MOOSE has been thought through, from the installation process through running your simulation on state of the art supercomputers, the MOOSE system will accelerate your research.

Some of the capability at your fingertips:

* Fully-coupled, fully-implicit multiphysics solver
* Dimension independent physics
* Automatically parallel (largest runs >100,000 CPU cores!)
* Modular development simplifies code reuse
* Built-in mesh adaptivity
* Continuous and Discontinuous Galerkin (DG) (at the same time!)
* Intuitive parallel multiscale solves (see videos below)
* Dimension agnostic, parallel geometric search (for contact related applications)
* Flexible, pluggable graphical user interface
* ~30 pluggable interfaces allow specialization of every part of the solve

More Information
================

**For more information, including installation instructions, please see the official website: [https://mooseframework.inl.gov](https://mooseframework.inl.gov)**

Contributing
============

For information on how to contribute code changes to MOOSE please [see this article](https://mooseframework.inl.gov/framework/contributing.html).


Compilation NEML2
============
```bash
git submodule update --checkout --init --recursive modules/solid_mechanics/contrib/neml2
cd /Users/chenghau.yang/Documents/Package/moose
Open setup_libtorch.sh in a text editor:
Change VERSION=2.1.0 to VERSION=2.2.0
./scripts/setup_libtorch.sh
./configure --with-libtorch

or

rm -rf framework/contrib/libtorch # need to do this!
./configure --with-libtorch=/Users/cheng-hauyang/miniforge/envs/moose/lib/python3.11/site-packages/torch

cd /Users/chenghau.yang/Documents/Package/moose/modules/solid_mechanics

make clobberall
make clean
make -j10

If HPC, such as mom show error regarding libmesh, you may need to do
* git submodule update --init --recursive
*./scripts/update_and_rebuild_libmesh.sh
```


Running cases
============

* MMS study for IC setting using nodal patch recovery (npr):
```bash
modules/heat_transfer/test/tests/mms_npr
```

* Directed Energy Deposition (DED) simulation with an ellipsoidal heat source
```bash
modules/heat_transfer/test/tests/esm_path_heat/esm-bfm-heat.i
```

* Thermo-mechanical Simulation of Directed Energy Deposition (DED)
```bash
modules/combined/test/tests/elastic_thermal_patch/esm-3D-coupled-layer-by-layer-neml2_linear_isotropic_harden.i
modules/combined/test/tests/elastic_thermal_patch/esm-bfm-act-before-sol-2D.i
modules/combined/test/tests/elastic_thermal_patch/esm-bfm-act-before-sol-2D-neml2-npr-IC.i
```
