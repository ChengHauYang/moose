//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "GenericKernel.h"

// Forward Declarations

/**
 * Adds the pore-pressure contribution \f$-B p \nabla \psi_c\f$ to a displacement equation,
 * where \f$B\f$ is the Biot coefficient and \f$c\f$ selects the displacement component.
 */
template <bool is_ad>
class PoroMechanicsCouplingTempl : public GenericKernel<is_ad>
{
public:
  static InputParameters validParams();

  PoroMechanicsCouplingTempl(const InputParameters & parameters);

protected:
  virtual GenericReal<is_ad> computeQpResidual() override;

  virtual Real computeQpJacobian() override;

  virtual Real computeQpOffDiagJacobian(unsigned int jvar) override;

private:
  /// Biot coefficient
  const GenericMaterialProperty<Real, is_ad> & _coefficient;

  const GenericVariableValue<is_ad> & _porepressure;

  unsigned int _porepressure_var_num;

  /// An integer corresponding to the direction this kernel acts in
  unsigned int _component;

  usingGenericKernelMembers;
};

using PoroMechanicsCoupling = PoroMechanicsCouplingTempl<false>;
using ADPoroMechanicsCoupling = PoroMechanicsCouplingTempl<true>;
