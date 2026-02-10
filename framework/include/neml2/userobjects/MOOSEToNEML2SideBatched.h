//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "MOOSEToNEML2.h"
#include "SideUserObject.h"

/**
 * @brief Generic gatherer for collecting "batched" MOOSE data for NEML2 on sidesets
 *
 * It is generic in the sense that it can be used for most MOOSE data types that take the form of
 * MooseArray<T>.
 *
 * The collected data is a flat buffer of side quadrature point values, ordered by the side batch
 * index generator.
 *
 * @tparam T Type of the underlying MOOSE data, e.g., Real, SymmetricRankTwoTensor, etc.
 */
template <typename T>
class MOOSEToNEML2SideBatched : public MOOSEToNEML2, public SideUserObject
{
public:
  static InputParameters validParams();

  MOOSEToNEML2SideBatched(const InputParameters & params);

#ifndef NEML2_ENABLED
  void initialize() override {}
  void execute() override {}
  void finalize() override {}
  void threadJoin(const UserObject &) override {}
#else
  void initialize() override;
  void execute() override;
  void finalize() override {}
  void threadJoin(const UserObject &) override;

  neml2::Tensor gatheredData() const override;

  // The number of batches
  std::size_t size() const { return _buffer.size(); }

protected:
  /// MOOSE data for the current side
  virtual const MooseArray<T> & sideMOOSEData() const = 0;

  /// Intermediate data buffer, filled during the side loop
  std::vector<T> _buffer;
#endif
};

template <typename T>
InputParameters
MOOSEToNEML2SideBatched<T>::validParams()
{
  auto params = MOOSEToNEML2::validParams();
  params += SideUserObject::validParams();

  // Since we use the NEML2 model to evaluate the residual AND the Jacobian at the same time, we
  // want to execute this user object only at execute_on = LINEAR (i.e. during residual evaluation).
  // The NONLINEAR exec flag below is for computing Jacobian during automatic scaling.
  ExecFlagEnum execute_options = MooseUtils::getDefaultExecFlagEnum();
  execute_options = {EXEC_INITIAL, EXEC_LINEAR, EXEC_NONLINEAR};
  params.set<ExecFlagEnum>("execute_on") = execute_options;

  return params;
}

template <typename T>
MOOSEToNEML2SideBatched<T>::MOOSEToNEML2SideBatched(const InputParameters & params)
  : MOOSEToNEML2(params), SideUserObject(params) // side/boundary context (face QPs)
{
}

#ifdef NEML2_ENABLED
template <typename T>
void
MOOSEToNEML2SideBatched<T>::initialize()
{
  _buffer.clear();
}

template <typename T>
void
MOOSEToNEML2SideBatched<T>::execute()
{
  const auto & side_data = this->sideMOOSEData();
  for (auto i : index_range(side_data))
    _buffer.push_back(side_data[i]);
}

template <typename T>
void
MOOSEToNEML2SideBatched<T>::threadJoin(const UserObject & uo)
{
  // append vectors
  const auto & m2n = static_cast<const MOOSEToNEML2SideBatched<T> &>(uo);
  _buffer.insert(_buffer.end(), m2n._buffer.begin(), m2n._buffer.end());
}

template <typename T>
neml2::Tensor
MOOSEToNEML2SideBatched<T>::gatheredData() const
{
  return NEML2Utils::fromBlob(_buffer);
}
#endif
