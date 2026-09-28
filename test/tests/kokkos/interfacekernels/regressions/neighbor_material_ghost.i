# This file is part of the MOOSE framework
# https://mooseframework.inl.gov
#
# All rights reserved, see COPYRIGHT for full restrictions
# https://github.com/idaholab/moose/blob/master/COPYRIGHT
#
# Licensed under LGPL 2.1, please see LICENSE for details
# https://www.gnu.org/licenses/lgpl-2.1.html

# Regression: neighbor-side material execution across an MPI partition.
# This must execute without SIGBUS / illegal memory access when the
# neighbor element is a one-layer ghost (-p2 --distributed-mesh).
# The material checks that a parallel run actually includes a ghost neighbor.
!include ../interface_material.i
