//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosComputeHomogenizedLagrangianStrain.h"

registerKokkosMaterial("SolidMechanicsApp", KokkosComputeHomogenizedLagrangianStrain);

InputParameters
KokkosComputeHomogenizedLagrangianStrain::validParams()
{
  InputParameters params = HomogenizationInterface<Material>::validParams();
  params.addClassDescription("Calculate the Kokkos homogenization gradient from the macro "
                             "gradient scalar variable used to satisfy the homogenization "
                             "constraints.");
  params.addParam<std::string>("base_name", "Material property base name");
  params.addParam<MaterialPropertyName>("homogenization_gradient_name",
                                        "homogenization_gradient",
                                        "Name of the constant gradient field");
  params.addRequiredCoupledVar("macro_gradient", "Scalar field defining the macro gradient");
  return params;
}

KokkosComputeHomogenizedLagrangianStrain::KokkosComputeHomogenizedLagrangianStrain(
    const InputParameters & parameters)
  : HomogenizationInterface<Material>(parameters),
    _macro_gradient(kokkosCoupledScalarValue("macro_gradient")),
    _index_i(cmap().size()),
    _index_j(cmap().size()),
    _homogenization_gradient(declareKokkosProperty<Real, 2>(
        (isParamValid("base_name") ? getParam<std::string>("base_name") + "_" : "") +
            getParam<MaterialPropertyName>("homogenization_gradient_name"),
        {3, 3}))
{
  if (getScalarVar("macro_gradient", 0)->order() != cmap().size())
    paramError("macro_gradient",
               "The order of the macro gradient variable must equal the number of constraints.");

  unsigned int h = 0;
  for (const auto & [indices, constraint] : cmap())
  {
    std::tie(_index_i[h], _index_j[h]) = indices;
    ++h;
  }
  _index_i.copyToDevice();
  _index_j.copyToDevice();
}
