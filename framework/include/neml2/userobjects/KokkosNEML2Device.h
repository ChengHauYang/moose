//* This file is part of the MOOSE framework
//* https://mooseframework.inl.gov
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#pragma once

#ifdef NEML2_ENABLED

#include <ATen/ATen.h>

/**
 * Torch device backing the Kokkos execution space.
 *
 * The Kokkos->NEML2 gatherers wrap their device buffers as torch tensors with at::from_blob, which
 * records only the pointer's device and performs no copy. The buffer lives in the Kokkos execution
 * space's memory (host for a CPU Kokkos build, the GPU for a CUDA/HIP/SYCL build), so the tensor
 * must be labeled with that device. NEML2ModelExecutor::fillInputs then moves each input to the
 * requested NEML2 device with .to(device()): a no-op when they already match (preserving zero-copy
 * GPU execution) and a device-to-host copy when NEML2 runs on the CPU. Labeling the tensor with the
 * NEML2 device directly would instead alias a GPU pointer as host memory, which crashes once a host
 * kernel dereferences it.
 *
 * NEML2ToKokkosMaterialProperty uses the same device to stage NEML2 outputs before Kokkos reads
 * them back.
 */
at::Device kokkosTorchDevice();

#endif
