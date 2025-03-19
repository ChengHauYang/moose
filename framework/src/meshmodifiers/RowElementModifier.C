//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "RowElementModifier.h"

registerMooseObject("MooseApp", RowElementModifier);

InputParameters
RowElementModifier::validParams()
{
  InputParameters params = ElementSubdomainModifier::validParams();

  params.addRequiredParam<SubdomainID>("subdomain_id_change_from", "Subdomain ID to change from.");

  params.addRequiredParam<SubdomainID>("subdomain_id_change_to", "Subdomain ID to change to.");

  params.addRequiredParam<Real>("x_min", "Domain x min.");
  params.addRequiredParam<Real>("x_max", "Domain x max.");

  params.addRequiredParam<Real>("y_min", "Domain y min.");
  params.addRequiredParam<Real>("y_max", "Domain y max.");

  params.addParam<Real>("z_min", -100, "Domain z min.");
  params.addParam<Real>("z_max", -100, "Domain z max.");

  params.addParam<Real>(
      "number_of_elements", 1, "Number of elements needed to change in x-dir each time.");

  params.addParam<bool>(
      "change_one_row", false, "Change the whole element of one row for each time.");

  return params;
}

RowElementModifier::RowElementModifier(const InputParameters & parameters)
  : ElementSubdomainModifier(parameters),
    _subdomain_id_change_from(getParam<SubdomainID>("subdomain_id_change_from")),
    _subdomain_id_change_to(getParam<SubdomainID>("subdomain_id_change_to")),
    _coord_min(Point(getParam<Real>("x_min"), getParam<Real>("y_min"), getParam<Real>("z_min"))),
    _coord_max(Point(getParam<Real>("x_max"), getParam<Real>("y_max"), getParam<Real>("z_max"))),
    _number_of_elements(getParam<Real>("number_of_elements")),
    _change_one_row(getParam<bool>("change_one_row"))
{
}

SubdomainID
RowElementModifier::computeSubdomainID()
{
  const Elem * elem = this->_current_elem;
  if (!elem)
    mooseError("RowElementModifier: _current_elem is null!");

  bool change_back_to_original = false;
  if (_t == 0 and elem->subdomain_id() == _subdomain_id_change_to)
  {
    change_back_to_original = true;
    _previous_t = 0;
  }

  // if (_t == 0)
  // {
  //   _previous_t = 0;
  // }

  if (_previous_t != _t)
  {
    _previous_coord = _latest_coord;
    _previous_t = _t;
  }

  const unsigned int n_nodes = elem->n_nodes();
  std::vector<Point> node_locations(n_nodes);

  Real nodal_x_min = std::numeric_limits<Real>::max();
  Real nodal_x_max = std::numeric_limits<Real>::lowest();
  Real nodal_y_min = std::numeric_limits<Real>::max();
  Real nodal_y_max = std::numeric_limits<Real>::lowest();

  for (unsigned int node = 0; node < n_nodes; ++node)
  {
    node_locations[node] = elem->point(node);
    nodal_x_min = std::min(nodal_x_min, node_locations[node](0));
    nodal_x_max = std::max(nodal_x_max, node_locations[node](0));
    nodal_y_min = std::min(nodal_y_min, node_locations[node](1));
    nodal_y_max = std::max(nodal_y_max, node_locations[node](1));
  }

  Real h = std::pow(this->_current_elem->volume(), 1.0 / this->_mesh.dimension());

  // std::cout << "_t = " << _t << std::endl;

  // std::cout << "std::fmod(h * _t, (_coord_max(0) - _coord_min(0))) = "
  //           << std::fmod(h * _t, (_coord_max(0) - _coord_min(0))) << std::endl;

  Real domain_width = _coord_max(0) - _coord_min(0);
  Real domain_height = _coord_max(1) - _coord_min(1);

  if (_t == 0)
  {
    int x_step = std::fmod(_t * _number_of_elements * h, domain_width) / (_number_of_elements * h);
    int y_step = std::floor(_t * _number_of_elements * h / domain_width);

    _latest_coord = Point(_coord_min(0) + x_step * _number_of_elements * h,
                          _coord_max(1) - (y_step + 1) * _number_of_elements * h,
                          0.0);
  }
  else
  {
    if (_previous_coord(0) + _number_of_elements * h > _coord_max(0))
    {
      _latest_coord = Point(_coord_min(0), _previous_coord(1) - _number_of_elements * h, 0.0);
    }
    else
    {
      _latest_coord = Point(_previous_coord(0) + _number_of_elements * h, _previous_coord(1), 0.0);
    }
  }

  // if (_t == 14)
  // {
  //   std::cout << "_t = " << _t << std::endl;

  //   std::cout << "h = " << h << std::endl;
  //   std::cout << " std::fmod(_t * _number_of_elements * h, domain_width) = "
  //             << std::fmod(_t * _number_of_elements * h, domain_width) << std::endl;

  //   std::cout << "_t * _number_of_elements * h / domain_width = "
  //             << _t * _number_of_elements * h / domain_width << std::endl;
  //   std::cout << "std::floor(_t * _number_of_elements * h / domain_width) = "
  //             << std::floor(_t * _number_of_elements * h / domain_width) << std::endl;

  //   std::cout << "x_step = " << x_step << std::endl;
  //   std::cout << "y_step = " << y_step << std::endl;
  // }

  // std::cout << "_latest_coord = ";
  // _latest_coord.print();
  // std::cout << std::endl;

  Real eps = 1e-5;

  if (elem->subdomain_id() == _subdomain_id_change_from or change_back_to_original)
  {

    // std::cout << "nodal_x_min = " << nodal_x_min << std::endl;
    // std::cout << "nodal_x_max = " << nodal_x_max << std::endl;
    // std::cout << "nodal_y_min = " << nodal_y_min << std::endl;
    // std::cout << "nodal_y_max = " << nodal_y_max << std::endl;

    // Generate the filename using `_t`
    // std::ostringstream filename;
    // filename << "corner_pt_" << _t << ".txt";

    // // Open the file with the generated filename
    // std::ofstream fout1(filename.str(), std::ios::app);
    // if (fout1.is_open())
    // {
    //   fout1 << _latest_coord(0) << ", " << _latest_coord(1) << ", " << _t << "\n";
    //   fout1.close();
    // }
    // else
    // {
    //   std::cerr << "Error: Unable to open " << filename.str() << " for writing!" << std::endl;
    // }

    // std::cout << "_coord_max(1) - (_t + 1) * _number_of_elements * h - eps = "
    //           << _coord_max(1) - (_t + 1) * _number_of_elements * h - eps << std::endl;

    // std::cout << "_number_of_elements = " << _number_of_elements << std::endl;

    // std::cout << "nodal_y_min = " << nodal_y_min << std::endl;

    // std::cout << "_coord_min(0) - eps = " << _coord_min(0) - eps << std::endl;
    // std::cout << "_coord_max(0) + eps = " << _coord_max(0) + eps << std::endl;

    // std::cout << "nodal_x_min = " << nodal_x_min << std::endl;
    // std::cout << "nodal_x_max = " << nodal_x_max << std::endl;

    if (_change_one_row)
    {
      if (nodal_x_min >= _coord_min(0) - eps && nodal_x_max <= _coord_max(0) + eps &&
          nodal_y_min >= _coord_max(1) - (_t + 1) * _number_of_elements * h - eps
          /*&& nodal_y_max <= _coord_max(1) - _t * _number_of_elements * h + eps*/)
      {
        // std::cout << "change marker\n";
        return _subdomain_id_change_to;
      }
    }
    else
    {

      if (nodal_x_min >= _latest_coord(0) - eps &&
          nodal_x_max <= _latest_coord(0) + _number_of_elements * h + eps &&
          nodal_y_min >= _latest_coord(1) - eps &&
          nodal_y_max <= _latest_coord(1) + _number_of_elements * h + eps)
      {

        // std::cout << "change marker\n";

        // std::cout << "_subdomain_id_change_to = " << _subdomain_id_change_to << std::endl;
        return _subdomain_id_change_to;
      }
    }
  }

  if (change_back_to_original)
  {
    return _subdomain_id_change_from;
  }

  return elem->subdomain_id();
}
