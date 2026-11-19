//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "Material.h"
#include "DerivativeMaterialInterface.h"

/**
 * CompositeTensorBase computes a simple T type MaterialProperty \f$ \bar T\f$
 * that is summed up from a list of other T objects (tensors) \f$ T_i \f$ with
 * associated weights \f$ w_i \f$.
 * \f$ \bar T = \sum_i T_i\cdot w_i \f$
 * Derivatives w.r.t. variables are computed accordingly.
 * This base class is used by the CompositeMobilityTensor and
 * CompositeElasticityTensor classes.
 *
 * \tparam T The type of the objects to sum together
 * \tparam U The material base class
 * \tparam is_ad Whether to use automatic differentiation material properties
 */
template <class T, class U, bool is_ad = false>
class CompositeTensorBase : public DerivativeMaterialInterface<U>
{
public:
  static InputParameters validParams();

  CompositeTensorBase(const InputParameters & parameters);

protected:
  /**
   * Input and derivative material properties are initialized here so that derived classes can
   * modify the tensor names and the output derivative property name.
   */
  void initializeDerivativeProperties(const std::string name);

  /**
   * Fill in the
   *  * main tensor property given by M
   *  * derivatives, set up by initializeDerivativeProperties
   *
   * The root_property is kept separate from the derivatives to allow the application
   * of this template to the Eigenstrain calculation, which contributes derivatives to a different
   * material property (and uses a derivative_prefactor of -1).
   */
  virtual void computeQpTensorProperties(GenericMaterialProperty<T, is_ad> & M,
                                         Real derivative_prefactor = 1.0);

  /// component tensor names
  std::vector<MaterialPropertyName> _tensor_names;
  /// component weight names
  std::vector<MaterialPropertyName> _weight_names;

  /// number of dependent variables
  unsigned int _num_args;
  /// number of compomemt tensors and weights
  unsigned int _num_comp;

  /// @{ Composed tensor and its derivatives for the non-AD specialization
  std::vector<MaterialProperty<T> *> _dM;
  std::vector<std::vector<MaterialProperty<T> *>> _d2M;
  /// @}

  /// @{ component tensors and their derivatives w.r.t. the args
  std::vector<const GenericMaterialProperty<T, is_ad> *> _tensors;
  std::vector<std::vector<const MaterialProperty<T> *>> _dtensors;
  std::vector<std::vector<std::vector<const MaterialProperty<T> *>>> _d2tensors;
  /// @}

  /// @{ component weights and their derivatives w.r.t. the args
  std::vector<const GenericMaterialProperty<Real, is_ad> *> _weights;
  std::vector<std::vector<const MaterialProperty<Real> *>> _dweights;
  std::vector<std::vector<std::vector<const MaterialProperty<Real> *>>> _d2weights;
  /// @}
};

template <class T, class U, bool is_ad>
CompositeTensorBase<T, U, is_ad>::CompositeTensorBase(const InputParameters & parameters)
  : DerivativeMaterialInterface<U>(parameters),
    _tensor_names(this->template getParam<std::vector<MaterialPropertyName>>("tensors")),
    _weight_names(this->template getParam<std::vector<MaterialPropertyName>>("weights")),
    _num_args(this->DerivativeMaterialInterface<U>::coupledComponents("coupled_variables")),
    _num_comp(_tensor_names.size()),
    _tensors(_num_comp),
    _weights(_num_comp)
{
  if (_num_comp != _weight_names.size())
    mooseError("The number of supplied 'tensors' and 'weights' must match.");

  if constexpr (!is_ad)
  {
    _dM.resize(_num_args);
    _d2M.resize(_num_args);
    _dtensors.resize(_num_comp);
    _d2tensors.resize(_num_comp);
    _dweights.resize(_num_comp);
    _d2weights.resize(_num_comp);
  }
}

template <class T, class U, bool is_ad>
InputParameters
CompositeTensorBase<T, U, is_ad>::validParams()
{
  InputParameters params = U::validParams();
  params.addRequiredParam<std::vector<MaterialPropertyName>>("tensors", "Component tensors");
  params.addRequiredParam<std::vector<MaterialPropertyName>>("weights", "Component weights");
  params.addRequiredCoupledVar("coupled_variables",
                               "Variable dependencies of the composite tensor");
  return params;
}

template <class T, class U, bool is_ad>
void
CompositeTensorBase<T, U, is_ad>::initializeDerivativeProperties(const std::string name)
{
  for (const auto i : make_range(_num_comp))
  {
    _tensors[i] = &this->template getGenericMaterialPropertyByName<T, is_ad>(_tensor_names[i]);
    _weights[i] = &this->template getGenericMaterialPropertyByName<Real, is_ad>(_weight_names[i]);
  }

  if constexpr (!is_ad)
  {
    // setup output composite tensor and derivatives
    for (const auto j : make_range(_num_args))
    {
      const VariableName & jname =
          this->DerivativeMaterialInterface<U>::getVar("coupled_variables", j)->name();
      _dM[j] = &this->template declarePropertyDerivative<T>(name, jname);
      _d2M[j].resize(j + 1);

      for (const auto k : make_range(j + 1))
      {
        const VariableName & kname =
            this->DerivativeMaterialInterface<U>::getVar("coupled_variables", k)->name();

        _d2M[j][k] = &this->template declarePropertyDerivative<T>(name, jname, kname);
      }
    }

    // setup input component derivatives
    for (const auto i : make_range(_num_comp))
    {
      _dtensors[i].resize(_num_args);
      _dweights[i].resize(_num_args);
      _d2tensors[i].resize(_num_args);
      _d2weights[i].resize(_num_args);

      for (const auto j : make_range(_num_args))
      {
        const VariableName & jname =
            this->DerivativeMaterialInterface<U>::getVar("coupled_variables", j)->name();

        _dtensors[i][j] =
            &this->template getMaterialPropertyDerivativeByName<T>(_tensor_names[i], jname);
        _dweights[i][j] =
            &this->template getMaterialPropertyDerivativeByName<Real>(_weight_names[i], jname);

        _d2tensors[i][j].resize(j + 1);
        _d2weights[i][j].resize(j + 1);

        for (const auto k : make_range(j + 1))
        {
          const VariableName & kname =
              this->DerivativeMaterialInterface<U>::getVar("coupled_variables", k)->name();

          _d2tensors[i][j][k] = &this->template getMaterialPropertyDerivativeByName<T>(
              _tensor_names[i], jname, kname);
          _d2weights[i][j][k] = &this->template getMaterialPropertyDerivativeByName<Real>(
              _weight_names[i], jname, kname);
        }
      }
    }
  }
}

template <class T, class U, bool is_ad>
void
CompositeTensorBase<T, U, is_ad>::computeQpTensorProperties(GenericMaterialProperty<T, is_ad> & M,
                                                            Real derivative_prefactor)
{
  const unsigned int qp = this->DerivativeMaterialInterface<U>::_qp;

  M[qp].zero();
  for (const auto i : make_range(_num_comp))
  {
    M[qp] += (*_tensors[i])[qp] * (*_weights[i])[qp];

    if constexpr (!is_ad)
      for (const auto j : make_range(_num_args))
      {
        if (i == 0)
          (*_dM[j])[qp].zero();

        (*_dM[j])[qp] += derivative_prefactor * ((*_tensors[i])[qp] * (*_dweights[i][j])[qp] +
                                                 (*_dtensors[i][j])[qp] * (*_weights[i])[qp]);

        for (const auto k : make_range(j + 1))
        {
          if (i == 0)
            (*_d2M[j][k])[qp].zero();

          (*_d2M[j][k])[qp] +=
              derivative_prefactor * ((*_dtensors[i][j])[qp] * (*_dweights[i][k])[qp] +
                                      (*_dtensors[i][k])[qp] * (*_dweights[i][j])[qp] +
                                      (*_tensors[i])[qp] * (*_d2weights[i][j][k])[qp] +
                                      (*_d2tensors[i][j][k])[qp] * (*_weights[i])[qp]);
        }
      }
  }
}
