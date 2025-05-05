//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "MeshGenerator.h"
#include "MooseEnum.h"
#include "Function.h"
#include "MooseApp.h"
#include "MooseMesh.h"
#include "FunctionParserUtils.h"

class SubdomainInterceptedGenerator : public MeshGenerator, public FunctionParserUtils<false>
{
public:
  static InputParameters validParams();
  SubdomainInterceptedGenerator(const InputParameters & parameters);

  std::unique_ptr<libMesh::MeshBase> generate() override;

protected:
  std::unique_ptr<libMesh::MeshBase> & _input;

  /// Parsed function
  SymFunctionPtr _parsed_function;

protected:
  /// IDs for subdomain classification
  SubdomainID _subdomain_id_inside;
  SubdomainID _subdomain_id_outside;
  /// Threshold value for classification
  Real _threshold;

  /// Lambda parameter for false intersection classification
  Real _lambda;

  /// Outer boundary handling flag
  bool _outer_boundary;

  /// MarkNeighborOfIntercept
  bool _multi_geo;

  /// @brief  Quadrature order used for active‑area estimation
  int _qrule_order;

  usingFunctionParserUtilsMembers(false);
};
