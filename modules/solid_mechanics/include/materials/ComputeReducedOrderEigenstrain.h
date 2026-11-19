//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "DenseMatrix.h"
#include "ComputeEigenstrainBase.h"
#include "RankTwoTensor.h"
#include "MooseTypes.h"

// libmesh includes
#include "libmesh/dense_vector.h"

// Forward Declarations
class Assembly;
class InputParameters;
class MooseObject;
class SubProblem;

template <bool is_ad>
class ComputeReducedOrderEigenstrainTempl : public ComputeEigenstrainBaseTempl<is_ad>
{
public:
  static InputParameters validParams();

  ComputeReducedOrderEigenstrainTempl(const InputParameters & parameters);

  void initQpStatefulProperties();
  void computeProperties();
  void computeQpEigenstrain();

private:
  /// Subtract adjusted eigenstrain from strain
  void applyEigenstrain(MaterialProperty<RankTwoTensor> & strain);

  /// Add contributions from every eigenstrain at each integration point
  void sumEigenstrain();

  /// Compute either the volume average or linear eigenstrain field in an element
  void prepareEigenstrain();

  std::vector<MaterialPropertyName> _input_eigenstrain_names;
  std::vector<const GenericMaterialProperty<RankTwoTensor, is_ad> *> _eigenstrains;

  SubProblem & _subproblem;
  /// Number of columns in A matrix (1 plus mesh dimension)
  const unsigned _ncols;
  /// Whether the mesh is made of second order elements
  const bool _second_order;
  /// The sum of all eigenstrains at each integration point
  std::vector<GenericRankTwoTensor<is_ad>> _eigsum;
  /// The (num q points x ncols) array for the least squares.  Holds 1, xcoor, ycoor, zcoor.
  DenseMatrix<Real> _A;
  /// The b array holding the unique eigenstrain components for each integration point.
  std::vector<GenericDenseVector<is_ad>> _b;
  /// Transpose of A
  DenseMatrix<Real> _AT;
  /// Inverse of A transpose times A
  DenseMatrix<Real> _A_inverse;
  /// Transpose of A times b.
  GenericDenseVector<is_ad> _ATb;
  /// The solution vector for each unique component of the adjusted eigenstrain
  std::vector<GenericDenseVector<is_ad>> _x;
  /// Vector to hold the adjusted strain as computed with _x
  std::vector<GenericReal<is_ad>> _vals;
  /// Filled with _vals and subracted from strain
  GenericRankTwoTensor<is_ad> _adjusted_eigenstrain;
};

using ComputeReducedOrderEigenstrain = ComputeReducedOrderEigenstrainTempl<false>;
using ADComputeReducedOrderEigenstrain = ComputeReducedOrderEigenstrainTempl<true>;
