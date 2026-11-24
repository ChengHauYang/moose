//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "ComputeReducedOrderEigenstrain.h"

#include "MooseMesh.h"
#include "libmesh/quadrature.h"

registerMooseObject("SolidMechanicsApp", ComputeReducedOrderEigenstrain);
registerMooseObject("SolidMechanicsApp", ADComputeReducedOrderEigenstrain);

template <bool is_ad>
InputParameters
ComputeReducedOrderEigenstrainTempl<is_ad>::validParams()
{
  InputParameters params = ComputeEigenstrainBaseTempl<is_ad>::validParams();
  params.addClassDescription("accepts eigenstrains and computes a reduced order eigenstrain for "
                             "consistency in the order of strain and eigenstrains.");
  params.addRequiredParam<std::vector<MaterialPropertyName>>(
      "input_eigenstrain_names", "List of eigenstrains to be applied in this strain calculation");
  return params;
}

template <bool is_ad>
ComputeReducedOrderEigenstrainTempl<is_ad>::ComputeReducedOrderEigenstrainTempl(
    const InputParameters & parameters)
  : ComputeEigenstrainBaseTempl<is_ad>(parameters),
    _input_eigenstrain_names(
        this->template getParam<std::vector<MaterialPropertyName>>("input_eigenstrain_names")),
    _eigenstrains(_input_eigenstrain_names.size()),
    _subproblem(*parameters.get<SubProblem *>("_subproblem")),
    _ncols(1 + _subproblem.mesh().dimension()),
    _second_order(_subproblem.mesh().hasSecondOrderElements()),
    _eigsum(),
    _A(),
    _b(6),
    _AT(),
    _A_inverse(),
    _ATb(_ncols),
    _x(6, GenericDenseVector<is_ad>(_ncols)),
    _vals(6),
    _adjusted_eigenstrain()
{
  for (unsigned int i = 0; i < _eigenstrains.size(); ++i)
  {
    _input_eigenstrain_names[i] = this->_base_name + _input_eigenstrain_names[i];
    _eigenstrains[i] = &this->template getGenericMaterialProperty<RankTwoTensor, is_ad>(
        _input_eigenstrain_names[i]);
  }
}

template <bool is_ad>
void
ComputeReducedOrderEigenstrainTempl<is_ad>::initQpStatefulProperties()
{
  this->_eigenstrain[this->_qp].zero();
}

template <bool is_ad>
void
ComputeReducedOrderEigenstrainTempl<is_ad>::computeProperties()
{
  sumEigenstrain();

  prepareEigenstrain();

  Material::computeProperties();
}

template <bool is_ad>
void
ComputeReducedOrderEigenstrainTempl<is_ad>::computeQpEigenstrain()
{
  if (_second_order)
  {
    for (unsigned i = 0; i < 6; ++i)
    {
      _vals[i] = _x[i](0);
      for (unsigned j = 1; j < _ncols; ++j)
        _vals[i] += _x[i](j) * this->_q_point[this->_qp](j - 1);
    }
    _adjusted_eigenstrain.fillFromInputVector(_vals);
  }
  this->_eigenstrain[this->_qp] = _adjusted_eigenstrain;
}

template <bool is_ad>
void
ComputeReducedOrderEigenstrainTempl<is_ad>::sumEigenstrain()
{
  _eigsum.resize(this->_qrule->n_points());
  for (this->_qp = 0; this->_qp < this->_qrule->n_points(); ++this->_qp)
  {
    _eigsum[this->_qp].zero();
    for (auto es : _eigenstrains)
      _eigsum[this->_qp] += (*es)[this->_qp];
  }
}

template <bool is_ad>
void
ComputeReducedOrderEigenstrainTempl<is_ad>::prepareEigenstrain()
{
  // The eigenstrains can either be constant in an element or linear in x, y, z
  // If constant, do volume averaging.
  if (!_second_order || this->_qrule->n_points() == 1)
  {
    // Volume average
    _adjusted_eigenstrain.zero();
    GenericReal<is_ad> vol = 0.0;
    for (unsigned qp = 0; qp < this->_qrule->n_points(); ++qp)
    {
      _adjusted_eigenstrain += _eigsum[qp] * this->_JxW[qp] * this->_coord[qp];
      vol += this->_JxW[qp] * this->_coord[qp];
    }
    _adjusted_eigenstrain /= vol;
  }
  else
  {
    // 1 x y z

    // Six sets (one for each unique component of eigen) of qp data
    _A.resize(this->_qrule->n_points(), _ncols);
    for (auto && b : _b)
      b.resize(this->_qrule->n_points());

    for (unsigned qp = 0; qp < this->_qrule->n_points(); ++qp)
    {
      _A(qp, 0) = 1.0;
      for (unsigned j = 1; j < _ncols; ++j)
        _A(qp, j) = this->_q_point[qp](j - 1);

      _b[0](qp) = _eigsum[qp](0, 0);
      _b[1](qp) = _eigsum[qp](1, 1);
      _b[2](qp) = _eigsum[qp](2, 2);
      _b[3](qp) = _eigsum[qp](1, 2);
      _b[4](qp) = _eigsum[qp](0, 2);
      _b[5](qp) = _eigsum[qp](0, 1);
    }

    _A.get_transpose(_AT);
    _A.left_multiply(_AT);

    // Form the geometric inverse with Real solves, then apply it to AD right-hand sides so the
    // projection preserves input-eigenstrain derivatives.
    _A_inverse.resize(_ncols, _ncols);
    DenseVector<Real> unit_vector(_ncols);
    DenseVector<Real> inverse_column(_ncols);
    for (const auto column : make_range(_ncols))
    {
      unit_vector.zero();
      unit_vector(column) = 1.0;
      _A.cholesky_solve(unit_vector, inverse_column);
      for (const auto row : make_range(_ncols))
        _A_inverse(row, column) = inverse_column(row);
    }

    for (const auto i : make_range(6))
    {
      _AT.vector_mult(_ATb, _b[i]);
      _A_inverse.vector_mult(_x[i], _ATb);
    }
  }
}

template class ComputeReducedOrderEigenstrainTempl<false>;
template class ComputeReducedOrderEigenstrainTempl<true>;
