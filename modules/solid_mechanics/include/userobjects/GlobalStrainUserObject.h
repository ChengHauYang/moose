//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "ElementUserObject.h"
#include "GlobalStrainUserObjectInterface.h"
#include "RankTwoTensor.h"
#include "RankFourTensor.h"
#include "MooseTypes.h"

template <bool is_ad>
class GlobalStrainUserObjectTempl : public ElementUserObject,
                                    public GlobalStrainUserObjectInterfaceTempl<is_ad>
{
public:
  static InputParameters validParams();

  GlobalStrainUserObjectTempl(const InputParameters & parameters);

  virtual void initialize() override;
  virtual void execute() override;
  virtual void threadJoin(const UserObject & uo) override;
  virtual void finalize() override;

  virtual const GenericRankTwoTensor<is_ad> & getResidual() const override { return _residual; }
  virtual const GenericRankFourTensor<is_ad> & getJacobian() const override { return _jacobian; }
  virtual const VectorValue<bool> & getPeriodicDirections() const override { return _periodic_dir; }

protected:
  virtual void computeAdditionalStress() {}

  const std::string _base_name;

  const GenericMaterialProperty<RankTwoTensor, is_ad> & _stress;

  const MaterialProperty<RankFourTensor> * _dstress_dstrain;

  GenericRankTwoTensor<is_ad> _residual;
  GenericRankFourTensor<is_ad> _jacobian;

  const unsigned int _dim;

  const std::vector<const MooseVariableFieldBase *> _disp_var;

  VectorValue<bool> _periodic_dir;

  RankTwoTensor _applied_stress_tensor;
};

using GlobalStrainUserObject = GlobalStrainUserObjectTempl<false>;
using ADGlobalStrainUserObject = GlobalStrainUserObjectTempl<true>;
