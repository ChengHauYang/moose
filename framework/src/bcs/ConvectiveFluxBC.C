//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "ConvectiveFluxBC.h"

registerMooseObject("MooseApp", ConvectiveFluxBC);

InputParameters
ConvectiveFluxBC::validParams()
{
  InputParameters params = IntegratedBC::validParams();
  params.set<Real>("rate") = 7500;
  params.set<Real>("rate_final") = 7500;
  params.set<Real>("initial") = 500;
  params.set<Real>("final") = 500;
  params.set<Real>("duration") = 0.0;
  params.addParam<bool>("neglect_side_btw_two_default_blocks",
                        false,
                        "If true, the side between two default blocks will be neglected. ");
  params.addClassDescription(
      "Determines boundary values via the initial and final values, flux, and exposure duration");

  return params;
}

ConvectiveFluxBC::ConvectiveFluxBC(const InputParameters & parameters)
  : IntegratedBC(parameters),
    _initial(getParam<Real>("initial")),
    _final(getParam<Real>("final")),
    _rate(getParam<Real>("rate")),
    _rate_final(isParamValid("rate_final") ? getParam<Real>("rate_final") : getParam<Real>("rate")),
    _duration(getParam<Real>("duration")),
    _neglect_side_btw_two_default_blocks(getParam<bool>("neglect_side_btw_two_default_blocks"))
{
}

Real
ConvectiveFluxBC::computeQpResidual()
{

  if (_neglect_side_btw_two_default_blocks)
    if (neighbor_is_default_block())
      return 0.0;

  Real value;
  Real rate;

  if (_t < _duration)
  {
    value = _initial + (_final - _initial) * std::sin((0.5 / _duration) * libMesh::pi * _t);
    rate = _rate;
  }
  else
  {
    value = _final;
    rate = _rate_final;
  }

  return -(_test[_i][_qp] * rate * (value - _u[_qp]));
}

Real
ConvectiveFluxBC::computeQpJacobian()
{

  if (_neglect_side_btw_two_default_blocks)
    if (neighbor_is_default_block())
      return 0.0;

  Real rate;

  if (_t < _duration)
    rate = _rate;
  else
    rate = _rate_final;

  return -(_test[_i][_qp] * rate * (-_phi[_j][_qp]));
}

bool
ConvectiveFluxBC::neighbor_is_default_block() const
{
  // std::cout << "n_neighbors ()= " << _current_elem->n_neighbors() << std::endl;
  // std::cout << "_current_side = " << _current_side << std::endl;
  if (_current_elem->neighbor_ptr(_current_side))
    if (_fe_problem.isElemInDefaultBlock(_current_elem->neighbor_ptr(_current_side)))
      // _current_elem->neighbor_ptr(_current_side)->centroid().print(std::cout);
      return true;
    else
      return false;
  else
    return false;
}
