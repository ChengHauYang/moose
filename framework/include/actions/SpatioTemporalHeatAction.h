#pragma once

#include "Action.h"

/**
 * An Action that sets up a spatiotemporal path heat source simulation.
 * It creates:
 *   - A UserObject for the path
 *   - A MeshModifier to set active/inactive subdomains
 *   - A Material for the heat source
 *   - A Kernel to apply the heat source
 */
class SpatioTemporalHeatAction : public Action
{
public:
  static InputParameters validParams();
  SpatioTemporalHeatAction(const InputParameters & parameters);

  virtual void act() override;
};
