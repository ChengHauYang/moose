//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "gtest/gtest.h"
#include "NEML2ModelHandle.h"

#ifdef NEML2_ENABLED
#include <cstdlib>

/** Tests the MOOSE AOTI adapter with identical quadrature-point data in different batch layouts. */
class NEML2AOTIDynamicBatchTest : public testing::TestWithParam<std::vector<int64_t>>
{
};

TEST_P(NEML2AOTIDynamicBatchTest, ImplicitIFT)
{
  const auto artifact = std::getenv("MOOSE_NEML2_AOTI_TEST_ARTIFACT");
  if (!artifact)
    GTEST_SKIP() << "Set MOOSE_NEML2_AOTI_TEST_ARTIFACT to the compiled perfect-plasticity "
                    "artifact folder (see unit/data/neml2_aoti/README.md).";

  const NEML2Utils::ScopedDefaultDtype dtype;
  const NEML2Utils::ScopedNumThreads threads(1);
  AOTIModelHandle model(artifact, "model", {"cpu"}, {0}, nullptr);
  AOTIModelHandle reference(artifact, "model", {"cpu"}, {0}, nullptr);

  const auto & batch_shape = GetParam();
  auto strain_shape = batch_shape;
  strain_shape.push_back(6);
  auto tangent_shape = strain_shape;
  tangent_shape.push_back(6);
  const auto options = at::TensorOptions().dtype(at::kDouble).device(at::kCPU);
  auto strain = at::zeros({8, 6}, options);
  // Young's modulus is 1e5 and yield stress is 5. The axial strains straddle
  // yield at 5e-5 without placing a point exactly on that nonsmooth transition.
  const auto axial = at::arange(1, 9, options) * 2e-5;
  strain.select(1, 0).copy_(axial);
  // Poisson's ratio is 0.3; these lateral strains give an initially uniaxial stress.
  strain.select(1, 1).copy_(-0.3 * axial);
  strain.select(1, 2).copy_(-0.3 * axial);

  std::map<std::string, at::Tensor> inputs = {{"neml2_strain", strain.reshape(strain_shape)},
                                              {"plastic_strain~1", at::zeros({6}, options)},
                                              {"flow_rate~1", at::zeros({}, options)},
                                              {"t", at::zeros({}, options)},
                                              {"t~1", at::zeros({}, options)}};
  auto reference_inputs = inputs;
  reference_inputs["neml2_strain"] = strain;

  // Use the timestep size of the perfect-plasticity regression.
  constexpr double dt = 1e-3;
  // The second evaluation exercises tensor and scalar state in the returned batch layout.
  for (const auto step : make_range(2))
  {
    inputs["t"] = at::scalar_tensor((step + 1) * dt, options);
    reference_inputs["t"] = inputs["t"];
    inputs["neml2_strain"] = ((step + 1) * strain).reshape(strain_shape);
    reference_inputs["neml2_strain"] = (step + 1) * strain;

    auto [out, jacobian] = model.jacobian(inputs);
    auto [expected, expected_jacobian] = reference.jacobian(reference_inputs);
    const auto & stress = libmesh_map_find(out, "neml2_stress");
    const auto & tangent =
        libmesh_map_find(libmesh_map_find(jacobian, "neml2_stress"), "neml2_strain");
    ASSERT_EQ(stress.sizes().vec(), strain_shape);
    ASSERT_EQ(tangent.sizes().vec(), tangent_shape);
    ASSERT_EQ(libmesh_map_find(out, "plastic_strain").sizes().vec(), strain_shape);
    ASSERT_EQ(libmesh_map_find(out, "flow_rate").sizes().vec(), batch_shape);

    // Compare the native-layout evaluation against an independent flat AOTI history.
    // The absolute tolerance covers cancellation in near-zero IFT tangent entries
    // relative to modulus 1e5.
    constexpr double rtol = 1e-10;
    constexpr double atol = 1e-8;
    EXPECT_TRUE(at::allclose(
        stress.reshape({8, 6}), libmesh_map_find(expected, "neml2_stress"), rtol, atol));
    EXPECT_TRUE(at::allclose(
        tangent.reshape({8, 6, 6}),
        libmesh_map_find(libmesh_map_find(expected_jacobian, "neml2_stress"), "neml2_strain"),
        rtol,
        atol));
    EXPECT_TRUE(
        at::allclose(libmesh_map_find(model.value(inputs), "neml2_stress"), stress, rtol, atol));

    ASSERT_GT(libmesh_map_find(out, "plastic_strain").abs().max().item<double>(), 0);
    for (const auto & name : {"plastic_strain", "flow_rate"})
    {
      const auto & state = libmesh_map_find(out, name);
      const auto & flat_state = libmesh_map_find(expected, name);
      EXPECT_TRUE(at::allclose(state.reshape(flat_state.sizes()), flat_state, rtol, atol));
      inputs[std::string(name) + "~1"] = state;
      reference_inputs[std::string(name) + "~1"] = libmesh_map_find(expected, name);
    }
    inputs["t~1"] = inputs["t"];
    reference_inputs["t~1"] = reference_inputs["t"];
  }
}

INSTANTIATE_TEST_SUITE_P(BatchShapes,
                         NEML2AOTIDynamicBatchTest,
                         testing::Values(std::vector<int64_t>{8}, std::vector<int64_t>{2, 4}));
#endif
