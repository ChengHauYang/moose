#pragma once

#include "MooseObjectAction.h"

/**
 * Add SpatioTemporalPath
 */
class AddSpatioTemporalPathAction : public MooseObjectAction
{
public:
  AddSpatioTemporalPathAction(const InputParameters & params);

  static InputParameters validParams();

  void act() override final;
};
