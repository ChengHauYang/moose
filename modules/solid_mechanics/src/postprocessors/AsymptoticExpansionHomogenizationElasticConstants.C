//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "AsymptoticExpansionHomogenizationElasticConstants.h"
#include "MooseMesh.h"

registerMooseObject("SolidMechanicsApp", AsymptoticExpansionHomogenizationElasticConstants);
registerMooseObject("SolidMechanicsApp", ADAsymptoticExpansionHomogenizationElasticConstants);

template <bool is_ad>
InputParameters
AsymptoticExpansionHomogenizationElasticConstantsTempl<is_ad>::validParams()
{
  InputParameters params = ElementIntegralPostprocessor::validParams();
  params.addClassDescription(
      "Postprocessor for asymptotic expansion homogenization for elasticity");
  params.addRequiredCoupledVar("dx_xx", "solution in xx");
  params.addRequiredCoupledVar("dy_xx", "solution in xx");
  params.addCoupledVar("dz_xx", "solution in xx");
  params.addRequiredCoupledVar("dx_yy", "solution in yy");
  params.addRequiredCoupledVar("dy_yy", "solution in yy");
  params.addCoupledVar("dz_yy", "solution in yy");
  params.addCoupledVar("dx_zz", "solution in zz");
  params.addCoupledVar("dy_zz", "solution in zz");
  params.addCoupledVar("dz_zz", "solution in zz");
  params.addRequiredCoupledVar("dx_xy", "solution in xy");
  params.addRequiredCoupledVar("dy_xy", "solution in xy");
  params.addCoupledVar("dz_xy", "solution in xy");
  params.addCoupledVar("dx_yz", "solution in yz");
  params.addCoupledVar("dy_yz", "solution in yz");
  params.addCoupledVar("dz_yz", "solution in yz");
  params.addCoupledVar("dx_zx", "solution in zx");
  params.addCoupledVar("dy_zx", "solution in zx");
  params.addCoupledVar("dz_zx", "solution in zx");
  params.addParam<std::string>("base_name",
                               "Optional parameter that allows the user to define "
                               "multiple mechanics material systems on the same "
                               "block, i.e. for multiple phases");
  MooseEnum column("xx yy zz yz xz xy");
  params.addRequiredParam<MooseEnum>("column",
                                     column,
                                     "The column of the material matrix this kernel acts in. "
                                     "(xx, yy, zz, yz, xz, or xy)");
  params.addRequiredParam<MooseEnum>("row",
                                     column,
                                     "The row of the material matrix this kernel acts in. "
                                     "(xx, yy, zz, yz, xz, or xy)");
  return params;
}

template <bool is_ad>
AsymptoticExpansionHomogenizationElasticConstantsTempl<is_ad>::
    AsymptoticExpansionHomogenizationElasticConstantsTempl(const InputParameters & parameters)
  : ElementIntegralPostprocessor(parameters),
    _base_name(isParamValid("base_name") ? getParam<std::string>("base_name") + "_" : ""),
    _grad({{{{&coupledGradient("dx_xx"),
              &coupledGradient("dy_xx"),
              (_subproblem.mesh().dimension() == 3 ? &coupledGradient("dz_xx") : &_grad_zero)}},
            {{&coupledGradient("dx_yy"),
              &coupledGradient("dy_yy"),
              (_subproblem.mesh().dimension() == 3 ? &coupledGradient("dz_yy") : &_grad_zero)}},
            {{(_subproblem.mesh().dimension() == 3 ? &coupledGradient("dx_zz") : &_grad_zero),
              (_subproblem.mesh().dimension() == 3 ? &coupledGradient("dy_zz") : &_grad_zero),
              (_subproblem.mesh().dimension() == 3 ? &coupledGradient("dz_zz") : &_grad_zero)}},
            {{(_subproblem.mesh().dimension() == 3 ? &coupledGradient("dx_yz") : &_grad_zero),
              (_subproblem.mesh().dimension() == 3 ? &coupledGradient("dy_yz") : &_grad_zero),
              (_subproblem.mesh().dimension() == 3 ? &coupledGradient("dz_yz") : &_grad_zero)}},
            {{(_subproblem.mesh().dimension() == 3 ? &coupledGradient("dx_zx") : &_grad_zero),
              (_subproblem.mesh().dimension() == 3 ? &coupledGradient("dy_zx") : &_grad_zero),
              (_subproblem.mesh().dimension() == 3 ? &coupledGradient("dz_zx") : &_grad_zero)}},
            {{(&coupledGradient("dx_xy")),
              (&coupledGradient("dy_xy")),
              (_subproblem.mesh().dimension() == 3 ? &coupledGradient("dz_xy") : &_grad_zero)}}}}),
    _elasticity_tensor(this->template getGenericMaterialPropertyByName<RankFourTensor, is_ad>(
        _base_name + "elasticity_tensor")),
    _column(getParam<MooseEnum>("column")),
    _row(getParam<MooseEnum>("row")),
    _ik_index({{0, 1, 2, 1, 0, 0}}),
    _jl_index({{0, 1, 2, 2, 2, 1}}),
    _i(_ik_index[_row]),
    _j(_jl_index[_row]),
    _k(_ik_index[_column]),
    _l(_jl_index[_column]),
    _volume(0),
    _integral_value(0)
{
}

template <bool is_ad>
void
AsymptoticExpansionHomogenizationElasticConstantsTempl<is_ad>::initialize()
{
  _integral_value = 0;
  _volume = 0;
}

template <bool is_ad>
void
AsymptoticExpansionHomogenizationElasticConstantsTempl<is_ad>::execute()
{
  _integral_value += computeIntegral();
  _volume += _current_elem_volume;
}

template <bool is_ad>
Real
AsymptoticExpansionHomogenizationElasticConstantsTempl<is_ad>::getValue() const
{
  return (_integral_value / _volume);
}

template <bool is_ad>
void
AsymptoticExpansionHomogenizationElasticConstantsTempl<is_ad>::finalize()
{
  gatherSum(_integral_value);
  gatherSum(_volume);
}

template <bool is_ad>
void
AsymptoticExpansionHomogenizationElasticConstantsTempl<is_ad>::threadJoin(const UserObject & y)
{
  const AsymptoticExpansionHomogenizationElasticConstantsTempl<is_ad> & pps =
      dynamic_cast<const AsymptoticExpansionHomogenizationElasticConstantsTempl<is_ad> &>(y);

  _integral_value += pps._integral_value;
  _volume += pps._volume;
}

template <bool is_ad>
Real
AsymptoticExpansionHomogenizationElasticConstantsTempl<is_ad>::computeQpIntegral()
{
  GenericReal<is_ad> value = _elasticity_tensor[_qp](_i, _j, _k, _l);
  for (const auto p : make_range(3))
    for (const auto q : make_range(3))
      value += _elasticity_tensor[_qp](_i, _j, p, q) * (*_grad[_column][p])[_qp](q);

  // Postprocessors report values only, so they intentionally discard AD derivatives here.
  return MetaPhysicL::raw_value(value);
}

template class AsymptoticExpansionHomogenizationElasticConstantsTempl<false>;
template class AsymptoticExpansionHomogenizationElasticConstantsTempl<true>;
