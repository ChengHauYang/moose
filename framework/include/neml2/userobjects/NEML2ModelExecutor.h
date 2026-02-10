//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#include "NEML2ModelInterface.h"
#include "GeneralUserObject.h"
#include "NEML2BatchIndexGenerator.h"
#include "NEML2SideBatchIndexGenerator.h"

#include <unordered_map>

class MOOSEToNEML2;

/**
 * NEML2ModelExecutor executes a NEML2 model. The NEML2 input variables and model parameters are
 * gathered by UserObjects derived from MOOSEToNEML2. This class is derived from GeneralUserObject
 * and is not threaded. It relies on a NEML2BatchIndexGenerator to generate batch indices.
 */
class NEML2ModelExecutor : public NEML2ModelInterface<GeneralUserObject>
{
public:
  /// Parameters that can be specified under the NEML2Action common area
  static InputParameters actionParams();

  static InputParameters validParams();

  NEML2ModelExecutor(const InputParameters & params);

#ifndef NEML2_ENABLED
  void initialize() override {}
  void execute() override {}
  void finalize() override {}
#else
  void initialize() override;
  void meshChanged() override;
  void execute() override;
  void finalize() override;

  void initialSetup() override;

  /// Get the batch index for the given element ID
  std::size_t getBatchIndex(dof_id_type elem_id) const;

  /**
   * @brief Metadata for mapping MOOSE geometric entities to NEML2 batch indices.
   * * In NEML2, data is processed in a flattened batch tensor where each entry corresponds
   * to a quadrature point. This structure provides the necessary information to locate
   * and validate the data segment associated with a specific element or side.
   * * @param start  The global starting index of the first quadrature point in the
   * flattened NEML2 batch tensor.
   * @param nqp    The number of quadrature points associated with the entity.
   * Used for data length validation and loop bounds.
   * @param local  Indicates whether the data is computed on the current processor (true)
   * or retrieved from the ghost/global cache (false) during MPI sync.
   */
  struct BatchInfo
  {
    std::size_t start = 0;
    unsigned int nqp = 0;
    bool local = true;
  };

  /// Get batch info for a given element ID (local first, then ghost cache)
  BatchInfo getBatchInfo(dof_id_type elem_id) const;

  /// Get batch info for a given element/side/boundary (side batches only)
  BatchInfo getSideBatchInfo(dof_id_type elem_id, unsigned int side, BoundaryID boundary_id) const;

  /// Whether a local batch index exists for the given element ID
  bool hasLocalBatchIndex(dof_id_type elem_id) const;

  /// Get output tensor (local or ghost cache)
  const neml2::Tensor & getOutputTensor(const neml2::VariableName & output_name, bool global) const;

  /// Get output derivative tensor (local or ghost cache)
  const neml2::Tensor & getOutputDerivativeTensor(const neml2::VariableName & output_name,
                                                  const neml2::VariableName & input_name,
                                                  bool global) const;

  /// Get output parameter derivative tensor (local or ghost cache)
  const neml2::Tensor & getOutputParameterDerivativeTensor(const neml2::VariableName & output_name,
                                                           const std::string & parameter_name,
                                                           bool global) const;

  /// Get side output tensor (local or ghost cache)
  const neml2::Tensor & getSideOutputTensor(const neml2::VariableName & output_name,
                                            bool global) const;

  /// Get side output derivative tensor (local or ghost cache)
  const neml2::Tensor & getSideOutputDerivativeTensor(const neml2::VariableName & output_name,
                                                      const neml2::VariableName & input_name,
                                                      bool global) const;

  /// Get side output parameter derivative tensor (local or ghost cache)
  const neml2::Tensor & getSideOutputParameterDerivativeTensor(
      const neml2::VariableName & output_name, const std::string & parameter_name, bool global) const;

  /// Get a reference(!) to the requested output view
  const neml2::Tensor & getOutput(const neml2::VariableName & output_name) const;

  /// Get a reference(!) to the requested output derivative view
  const neml2::Tensor & getOutputDerivative(const neml2::VariableName & output_name,
                                            const neml2::VariableName & input_name) const;

  /// Get a reference(!) to the requested output parameter derivative view
  const neml2::Tensor & getOutputParameterDerivative(const neml2::VariableName & output_name,
                                                     const std::string & parameter_name) const;

  /// check if the output is fully computed and ready to be fetched
  bool outputReady() const { return _output_ready; }

protected:
  /// Register a NEML2 input variable gathered by a gatherer
  virtual void addGatheredVariable(const UserObjectName &, const neml2::VariableName &);

  /// Register a NEML2 model parameter gathered by a gatherer
  virtual void addGatheredParameter(const UserObjectName &, const std::string &);

  /// Prevent output and derivative retrieval after construction
  virtual void checkExecutionStage() const final;

  /// Fill input variables and model parameters using the gatherers
  virtual void fillInputs();

  /// Apply the predictor to set current trial state
  virtual void applyPredictor();

  /// Perform the material update
  virtual bool solve();

  /// Extract output derivatives with respect to input variables and model parameters
  virtual void extractOutputs();

  /// Expand tensor shapes if necessary to conformal sizes
  virtual void expandInputs();

  /// The NEML2BatchIndexGenerator used to generate the element-to-batch-index map
  const NEML2BatchIndexGenerator & _batch_index_generator;
  /// The NEML2SideBatchIndexGenerator used to generate the side-to-batch-index map
  const NEML2SideBatchIndexGenerator * _side_batch_index_generator = nullptr;

  /// flag that indicates if output data has been fully computed
  bool _output_ready;

  /// The model parameters to update (gathered from MOOSE)
  std::map<std::string, neml2::Tensor> _model_params;

  /// The input variables of the material model
  neml2::ValueMap _in;
  /// The side input variables of the material model
  neml2::ValueMap _in_side;

  /// The output variables of the material model
  neml2::ValueMap _out;

  /// The derivative of the output variables w.r.t. the input variables
  neml2::DerivMap _dout_din;

  // set of variables to skip
  std::set<neml2::VariableName> _skip_vars;

  // set of gathered NEML2 input variables
  std::set<neml2::VariableName> _gathered_variable_names;

  // set of gathered NEML2 model parameters
  std::set<std::string> _gathered_parameter_names;

  /// MOOSE data gathering user objects
  std::vector<const MOOSEToNEML2 *> _gatherers;
  /// MOOSE side data gathering user objects
  std::vector<const MOOSEToNEML2 *> _side_gatherers;

  /// Combined batch offsets for side data
  std::size_t _side_batch_offset = 0;
  std::size_t _side_batch_size = 0;

  /// set of output variables that were retrieved (by other objects)
  mutable neml2::ValueMap _retrieved_outputs;

  /// set of derivatives that were retrieved (by other objects)
  mutable neml2::DerivMap _retrieved_derivatives;

  /// set of parameter derivatives that were retrieved (by other objects)
  mutable std::map<neml2::VariableName, std::map<std::string, neml2::Tensor>>
      _retrieved_parameter_derivatives;

  /// Whether the model has any state variable
  bool _has_state = false;

  /// Whether the model has any old state variable
  bool _has_old_state = false;

  /// Sync local outputs to build ghost-accessible cache
  void syncGlobalOutputs();
  /// Sync local side outputs to build ghost-accessible cache
  void syncGlobalSideOutputs();

  struct CachedTensor
  {
    std::vector<Real> data;
    neml2::Tensor tensor;
  };

  /// Global element batch info for ghost access
  std::unordered_map<dof_id_type, std::pair<std::size_t, unsigned int>> _global_elem_batch;

  /// Global output cache for ghost access
  std::map<neml2::VariableName, CachedTensor> _global_outputs;

  /// Global derivative cache for ghost access
  std::map<neml2::VariableName, std::map<neml2::VariableName, CachedTensor>> _global_derivatives;

  /// Global parameter derivative cache for ghost access
  std::map<neml2::VariableName, std::map<std::string, CachedTensor>> _global_parameter_derivatives;

  /// Global side batch info for ghost access
  std::map<NEML2SideBatchIndexGenerator::SideKey, std::pair<std::size_t, unsigned int>>
      _global_side_batch;

  /// Global side output cache for ghost access
  std::map<neml2::VariableName, CachedTensor> _global_side_outputs;

  /// Global side derivative cache for ghost access
  std::map<neml2::VariableName, std::map<neml2::VariableName, CachedTensor>>
      _global_side_derivatives;

  /// Global side parameter derivative cache for ghost access
  std::map<neml2::VariableName, std::map<std::string, CachedTensor>>
      _global_side_parameter_derivatives;

  /// Whether the global cache is ready
  bool _global_cache_ready = false;
  /// Whether the global side cache is ready
  bool _global_side_cache_ready = false;

private:
  /// Whether an error was encountered
  bool _error;
  /// Error message
  std::string _error_message;
#endif
};
