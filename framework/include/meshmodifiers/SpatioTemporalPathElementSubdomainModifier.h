#pragma once

#include "ElementSubdomainModifier.h"
#include "SpatioTemporalPath.h"
#include "SpatioTemporalPathInterface.h"

class SpatioTemporalPathElementSubdomainModifier : public ElementSubdomainModifier,
                                                   public SpatioTemporalPathInterface
{
public:
  static InputParameters validParams();

  SpatioTemporalPathElementSubdomainModifier(const InputParameters & parameters);

protected:
  virtual SubdomainID computeSubdomainID() override;

  /// The path
  const SpatioTemporalPath & _path;

  /// Target subdomain ID
  const SubdomainID _subdomain_id;

  /// Radius
  const Real _r;
};
