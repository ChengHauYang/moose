#include "AddSpatioTemporalPathAction.h"
#include "FEProblem.h"
#include "SpatioTemporalPath.h"

registerMooseAction("MooseApp", AddSpatioTemporalPathAction, "add_userobject");

InputParameters
AddSpatioTemporalPathAction::validParams()
{
  return MooseObjectAction::validParams();
}

AddSpatioTemporalPathAction::AddSpatioTemporalPathAction(const InputParameters & params)
  : MooseObjectAction(params)
{
}

void
AddSpatioTemporalPathAction::act()
{
  _problem->addObject<SpatioTemporalPath>(_type, _name, _moose_object_pars);
}
