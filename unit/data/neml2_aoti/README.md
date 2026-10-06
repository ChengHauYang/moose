# AOTI implicit dynamic-batch unit test

`NEML2AOTIDynamicBatchTest` exercises the MOOSE `AOTIModelHandle` with the same
eight material-point histories in batches `(8,)` and `(2, 4)`. It checks the
returned stress, tangent, tensor state, and scalar state shapes before flattening
stress and tangent for comparison with an independent flat cpp-AOTI history.
A second timestep consumes the nonzero state returned by the first. The value-only path is also compared
with the Jacobian evaluation. Both model handles use cpp-AOTI; the unit-test
process does not construct an eager model or import Python NEML2.

Compile the platform-specific perfect-plasticity artifact from the repository
root as a separate setup process, before launching the C++ unit tests:

```sh
neml2-compile modules/solid_mechanics/test/tests/neml2/plasticity/perfect_neml2.i \
  --model model --device cpu --output-dir /tmp/moose-neml2-aoti-unit \
  -d neml2_stress:neml2_strain
```

Build the NEML2-enabled unit-test executable from `unit/`, then run from the
repository root in the same environment:

```sh
MOOSE_NEML2_AOTI_TEST_ARTIFACT=/tmp/moose-neml2-aoti-unit/model \
  ./unit/moose-unit-opt --gtest_filter='BatchShapes/NEML2AOTIDynamicBatchTest.*'
```

An existing artifact compiled from that same source with the stress/strain
derivative can be used instead. The test skips when the artifact environment
variable is absent; an invalid artifact path or evaluation failure fails the
test. A plain Python NEML2 test bypasses the MOOSE adapter and does not exercise
this framework fix.
