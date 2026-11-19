//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "GlobalStrainUserObject.h"

#include "libmesh/quadrature.h"
#include "libmesh/dof_map.h"
#include "metaphysicl/parallel_numberarray.h"
#include "metaphysicl/parallel_dualnumber.h"
#include "metaphysicl/parallel_semidynamicsparsenumberarray.h"
#include "libmesh/parallel_algebra.h"

registerMooseObject("SolidMechanicsApp", GlobalStrainUserObject);
registerMooseObject("SolidMechanicsApp", ADGlobalStrainUserObject);

template <bool is_ad>
InputParameters
GlobalStrainUserObjectTempl<is_ad>::validParams()
{
  InputParameters params = ElementUserObject::validParams();
  params.addClassDescription(
      "Global Strain UserObject to provide Residual and diagonal Jacobian entry");
  params.addParam<std::vector<Real>>("applied_stress_tensor",
                                     "Vector of values defining the constant applied stress "
                                     "to add, in order 11, 22, 33, 23, 13, 12");
  params.addParam<std::string>("base_name", "Material properties base name");
  params.addCoupledVar("displacements", "The name of the displacement variables");
  params.set<ExecFlagEnum>("execute_on") = EXEC_LINEAR;

  return params;
}

template <bool is_ad>
GlobalStrainUserObjectTempl<is_ad>::GlobalStrainUserObjectTempl(const InputParameters & parameters)
  : ElementUserObject(parameters),
    GlobalStrainUserObjectInterfaceTempl<is_ad>(),
    _base_name(isParamValid("base_name") ? getParam<std::string>("base_name") + "_" : ""),
    _stress(this->template getGenericMaterialProperty<RankTwoTensor, is_ad>(_base_name + "stress")),
    _dstress_dstrain(
        is_ad ? nullptr
              : &this->template getMaterialProperty<RankFourTensor>(_base_name + "Jacobian_mult")),
    _dim(_mesh.dimension()),
    _disp_var(this->getFieldVars("displacements")),
    _periodic_dir(),
    _applied_stress_tensor()
{
  for (unsigned int dir = 0; dir < _dim; ++dir)
  {
    _periodic_dir(dir) = _mesh.isTranslatedPeriodic(*_disp_var[0], dir);

    for (unsigned int i = 1; i < _disp_var.size(); ++i)
      if (_mesh.isTranslatedPeriodic(*_disp_var[i], dir) != _periodic_dir(dir))
        mooseError("All the displacement components in a particular direction should have same "
                   "periodicity.");
  }

  if (isParamValid("applied_stress_tensor"))
    _applied_stress_tensor.fillFromInputVector(
        getParam<std::vector<Real>>("applied_stress_tensor"));
  else
    _applied_stress_tensor.zero();
}

template <bool is_ad>
void
GlobalStrainUserObjectTempl<is_ad>::initialize()
{
  _residual.zero();
  _jacobian.zero();
}

template <bool is_ad>
void
GlobalStrainUserObjectTempl<is_ad>::execute()
{
  computeAdditionalStress();

  for (unsigned int _qp = 0; _qp < _qrule->n_points(); ++_qp)
  {
    _residual += _JxW[_qp] * _coord[_qp] * (_stress[_qp] - _applied_stress_tensor);

    if (!is_ad && _dstress_dstrain)
      _jacobian += _JxW[_qp] * _coord[_qp] * (*_dstress_dstrain)[_qp];
  }
}

template <bool is_ad>
void
GlobalStrainUserObjectTempl<is_ad>::threadJoin(const UserObject & uo)
{
  const auto & pstuo = cast_ref<const GlobalStrainUserObjectTempl<is_ad> &>(uo);
  _residual += pstuo._residual;
  _jacobian += pstuo._jacobian;
}

template <bool is_ad>
void
GlobalStrainUserObjectTempl<is_ad>::finalize()
{
  if constexpr (is_ad)
  {
    // Apply local column constraints before reducing derivatives to the scalar-DOF owner.
    const auto & constraints = _sys.dofMap().get_dof_constraints();
    for (unsigned int i = 0; i < 3; ++i)
      for (unsigned int j = 0; j < 3; ++j)
      {
        auto & derivatives = _residual(i, j).derivatives();
        const auto indices = derivatives.nude_indices();
        for (const auto dof : indices)
          if (const auto constraint = constraints.find(dof); constraint != constraints.end())
          {
            const auto derivative = derivatives[dof];
            derivatives[dof] = 0;
            for (const auto & [master_dof, coefficient] : constraint->second)
              derivatives.insert(master_dof) += coefficient * derivative;
          }
      }
  }

  for (unsigned int i = 0; i < 3; ++i)
    for (unsigned int j = 0; j < 3; ++j)
      this->comm().sum(_residual(i, j));

  for (unsigned int i = 0; i < 3; ++i)
    for (unsigned int j = 0; j < 3; ++j)
      for (unsigned int k = 0; k < 3; ++k)
        for (unsigned int l = 0; l < 3; ++l)
          this->comm().sum(_jacobian(i, j, k, l));
}

template class GlobalStrainUserObjectTempl<false>;
template class GlobalStrainUserObjectTempl<true>;
