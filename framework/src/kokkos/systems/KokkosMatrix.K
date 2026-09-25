//* This file is part of the MOOSE framework
//* https://www.mooseframework.org
//*
//* All rights reserved, see COPYRIGHT for full restrictions
//* https://github.com/idaholab/moose/blob/master/COPYRIGHT
//*
//* Licensed under LGPL 2.1, please see LICENSE for details
//* https://www.gnu.org/licenses/lgpl-2.1.html

#include "KokkosMatrix.h"
#include "KokkosFESystem.h"
#include "MooseUtils.h"

namespace Moose::Kokkos
{

void
Matrix::create(libMesh::SparseMatrix<PetscScalar> & matrix, const System & system)
{
  if (_is_alloc && _matrix == &matrix)
    return;

  if (_is_alloc)
    destroy();

  auto petsc_matrix = dynamic_cast<libMesh::PetscMatrix<PetscScalar> *>(&matrix);
  _matrix = &matrix;
  _petsc_matrix = petsc_matrix ? petsc_matrix->mat() : PETSC_NULLPTR;
  _is_host = true;

  if (petsc_matrix)
  {
    PetscMemType mtype;
    LibmeshPetscCallQ(MatGetCurrentMemType(_petsc_matrix, &mtype));
    _is_host = PetscMemTypeHost(mtype);

#ifndef MOOSE_ENABLE_KOKKOS_GPU
    if (!_is_host)
      mooseError("PETSc matrices must be on host when Kokkos device capabilities are disabled.");
#endif
  }

  auto & sparsity = system.getSparsity();

  _nr = sparsity.row_ptr.size() - 1;
  _col_idx = sparsity.col_idx;
  _row_idx = sparsity.row_idx;
  _row_ptr = sparsity.row_ptr;

  if (!_is_host)
    _val.createDevice(_col_idx.size());
  else
    _val.create(_col_idx.size());

  if (petsc_matrix)
  {
    std::vector<PetscInt> col_idx(&_col_idx.begin(), &_col_idx.end());
    std::vector<PetscInt> row_idx(&_row_idx.begin(), &_row_idx.end());

    LibmeshPetscCallQ(
        MatSetPreallocationCOO(_petsc_matrix, col_idx.size(), row_idx.data(), col_idx.data()));
  }

  _is_alloc = true;
}

void
Matrix::destroy()
{
  _matrix = nullptr;
  _petsc_matrix = PETSC_NULLPTR;
  _nr = 0;

  _col_idx.destroy();
  _row_idx.destroy();
  _row_ptr.destroy();
  _val.destroy();

  _is_host = false;
  _is_alloc = false;
}

void
Matrix::close()
{
  if (_is_host)
    _val.copyToHost();

  if (_petsc_matrix)
    LibmeshPetscCallQ(
        MatSetValuesCOO(_petsc_matrix, _is_host ? _val.hostData() : _val.deviceData(), ADD_VALUES));
  else
  {
    for (const auto i : index_range(_row_idx))
      _matrix->add(_row_idx[i], _col_idx[i], _val.hostData()[i]);
    _matrix->close();
  }

  ::Kokkos::fence();
}

} // namespace Moose::Kokkos
