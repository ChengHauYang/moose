//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "LinearViscoelasticityManager.h"
#include "libmesh/quadrature.h"

registerMooseObject("SolidMechanicsApp", LinearViscoelasticityManager);
registerMooseObject("SolidMechanicsApp", ADLinearViscoelasticityManager);

template <bool is_ad>
InputParameters
LinearViscoelasticityManagerTempl<is_ad>::validParams()
{
  InputParameters params = ElementUserObject::validParams();
  params.addClassDescription("Manages the updating of the semi-implicit "
                             "single-step first-order finite difference "
                             "time-stepping scheme");
  params.addRequiredParam<std::string>("viscoelastic_model",
                                       "name of the LinearViscoelasticityBase object to manage");
  params.addParam<std::string>(
      "stress_name", "stress", "name of the stress tensor used for the viscoelastic update");
  params.addParam<std::string>("creep_strain_name",
                               "creep_strain",
                               "name of the creep strain tensor used for the viscoelastic update");
  params.addParam<std::string>(
      "elastic_strain_name",
      "elastic_strain",
      "name of the elastic strain tensor used for the viscoelastic update");
  params.set<ExecFlagEnum>("execute_on") = {EXEC_TIMESTEP_BEGIN, EXEC_TIMESTEP_END};
  params.suppressParameter<ExecFlagEnum>("execute_on");
  return params;
}

template <bool is_ad>
LinearViscoelasticityManagerTempl<is_ad>::LinearViscoelasticityManagerTempl(
    const InputParameters & parameters)
  : ElementUserObject(parameters),
    _stress_name(this->template getParam<std::string>("stress_name")),
    _stress(this->template getGenericMaterialProperty<RankTwoTensor, is_ad>(_stress_name)),
    _creep_strain_name(this->template getParam<std::string>("creep_strain_name")),
    _creep_strain(
        this->template getGenericMaterialProperty<RankTwoTensor, is_ad>(_creep_strain_name)),
    _elastic_strain_name(this->template getParam<std::string>("elastic_strain_name")),
    _elastic_strain(
        this->template getGenericMaterialProperty<RankTwoTensor, is_ad>(_elastic_strain_name)),
    _viscoelastic_model_name(this->template getParam<std::string>("viscoelastic_model")),
    _viscoelastic_model(nullptr)
{
}

template <bool is_ad>
void
LinearViscoelasticityManagerTempl<is_ad>::initialSetup()
{
  MaterialBase * test = &this->getMaterialByName(_viscoelastic_model_name, /*no_warn =*/true);
  if (!test)
    this->mooseError(_viscoelastic_model_name + " does not exist");

  _viscoelastic_model = dynamic_cast<LinearViscoelasticityBaseTempl<is_ad> *>(test);
  if (!_viscoelastic_model)
    this->mooseError(_viscoelastic_model_name +
                     " is not a LinearViscoelasticityBase object matching the AD specialization");
}

template <bool is_ad>
void
LinearViscoelasticityManagerTempl<is_ad>::execute()
{
  if (_mi_feproblem.getCurrentExecuteOnFlag() == EXEC_TIMESTEP_BEGIN)
  {
    for (unsigned int _qp = 0; _qp < _qrule->n_points(); ++_qp)
      _viscoelastic_model->recomputeQpApparentProperties(_qp);
  }
}

template class LinearViscoelasticityManagerTempl<false>;
template class LinearViscoelasticityManagerTempl<true>;
