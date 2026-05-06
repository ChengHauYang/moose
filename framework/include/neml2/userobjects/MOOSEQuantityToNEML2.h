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
#include "NEML2Utils.h"
#include "DomainUserObject.h"
#include <set>

#include "RankTwoTensor.h"
#include "SymmetricRankTwoTensor.h"

/**
 * Gather a MOOSE quantity for insertion into the NEML2 model.
 */
template <typename T, unsigned int state>
class MOOSEQuantityToNEML2 : public MOOSEToNEML2, public DomainUserObject
{
public:
  static InputParameters validParams();

  MOOSEQuantityToNEML2(const InputParameters & params);

#ifndef NEML2_ENABLED
  void initialize() override {}
  void execute() override {}
  void finalize() override {}
  void threadJoin(const UserObject &) override {}
#else
  void initialize() override;
  void executeOnElement() override;
  void executeOnBoundary() override;
  void executeOnInterface() override;
  void finalize() override {}
  void threadJoin(const UserObject &) override;

  neml2::Tensor gatheredData() const override;

protected:
  using ElemSide = std::tuple<dof_id_type, unsigned int>;

  void gatherFromCurrentElemSide(bool use_neighbor);
  T qpData(unsigned int qp, bool use_neighbor = false) const;

  /// MOOSE quantity type to read from
  const NEML2Utils::MOOSEIOType _type;

  ///@{
  /// candidate MOOSE quantities to read data from
  const VariableValue * _var_scalar = nullptr;
  const VariableValue * _var_scalar_old = nullptr;
  const Function * _func = nullptr;
  const MaterialProperty<T> * _mat_prop = nullptr;
  const MaterialProperty<T> * _mat_prop_old = nullptr;
  const MaterialProperty<T> * _neighbor_mat_prop = nullptr;
  const MaterialProperty<T> * _neighbor_mat_prop_old = nullptr;
  const VariableValue * _var = nullptr;
  const VariableValue * _var_old = nullptr;
  const VariableValue * _neighbor_var = nullptr;
  const VariableValue * _neighbor_var_old = nullptr;
  ///@}

  /// Whether the gathered data should be batched
  bool _batched = false;

  /// Intermediate data buffer, filled during the element/side loop
  std::vector<T> _buffer;

  /// Track visited element sides so shared interfaces are only gathered once per side orientation
  std::set<ElemSide> _visited_elem_sides;
#endif
};

#define defineMOOSEQuantityToNEML2(T)                                                              \
  using MOOSE##T##ToNEML2 = MOOSEQuantityToNEML2<T, 0>;                                            \
  using MOOSEOld##T##ToNEML2 = MOOSEQuantityToNEML2<T, 1>
defineMOOSEQuantityToNEML2(Real);
defineMOOSEQuantityToNEML2(RankTwoTensor);
defineMOOSEQuantityToNEML2(SymmetricRankTwoTensor);
defineMOOSEQuantityToNEML2(RealVectorValue);
#undef defineMOOSEQuantityToNEML2
