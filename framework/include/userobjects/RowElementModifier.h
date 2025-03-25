#pragma once

#include "ElementSubdomainModifier.h"
#include "MooseEnum.h"
#include "Function.h"

#include "libmesh/quadrature_gauss.h"
#include "libmesh/fe.h"
#include "libmesh/mesh.h"
#include "libmesh/mesh_base.h"
#include "libmesh/elem.h"
#include "libmesh/point.h"
#include "libmesh/vtk_io.h"
#include "libmesh/numeric_vector.h"
#include "libmesh/system.h"
#include "libmesh/dof_map.h"
#include <fstream>
#include <iomanip>
#include <sstream>

class RowElementModifier : public ElementSubdomainModifier
{
public:
  static InputParameters validParams();
  RowElementModifier(const InputParameters & parameters);

protected:
  virtual SubdomainID computeSubdomainID() override;

private:
  /// IDs for subdomain classification
  SubdomainID _subdomain_id_change_from;
  SubdomainID _subdomain_id_change_to;

  Point _coord_min;
  Point _coord_max;

  Point _latest_coord;
  Point _previous_coord;

  Real _number_of_elements;

  Real _previous_t;

  bool _change_one_row;
};
