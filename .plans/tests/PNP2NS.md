# Plan: Coupling PNP to Navier-Stokes (EHD Force)

To couple the Poisson-Nernst-Planck (PNP) solver to the Navier-Stokes (NS) momentum equations, we must apply the electrohydrodynamic (EHD) body force $\vec{F} = \rho_e \vec{E}$.
In terms of variables: $\vec{F} = (c_+ - c_-)(-\nabla \phi)$.

## The Challenge
MOOSE's `INSAD` (Incompressible Navier-Stokes Automatic Differentiation) module uses VMS/SUPG stabilization, which means any body force applied to the momentum equations must also be included in the "strong residual" calculated by the `INSADMaterial` for stabilization to work correctly.

## Proposed Solution: Custom Kernel
We will create a custom C++ kernel `INSADMomentumEHDForce` that applies this force and seamlessly registers it with the NS strong residual.

### 1. Header (`include/kernels/INSADMomentumEHDForce.h`)
Inherits from `ADVectorKernelValue`.
Requires:
- `concentrations` (variadic coupled AD scalar variables, one per species)
- `valences` (`std::vector<Real>`, one per species, same order as `concentrations`)
- `potential` (coupled AD scalar variable)

### 2. Source (`src/kernels/INSADMomentumEHDForce.C`)
- Inside the constructor, we will inform the `INSADObjectTracker` that we are adding a coupled force, similar to how `INSADMomentumCoupledForce` does. 
- However, since we compute the force internally rather than from an external variable, we may need to define our own AD material property (e.g., `ehd_strong_residual`) and inject it into the stabilization, OR we simply add it to the momentum residual and accept slightly degraded VMS stabilization for the EHD source term if we don't hook it into the `INSADMaterial`.
- **Computation**:
  ```cpp
  ADRealVectorValue INSADMomentumEHDForce::precomputeQpResidual()
  {
      // charge_density = sum_i z_i * c_i(qp)
      ADReal charge_density = 0;
      for (const auto i : make_range(_n_species))
          charge_density += _valences[i] * (*_concentrations[i])[_qp];

      // Physical force: F = charge_density * (-grad phi)
      // Kernel is on LHS (M - F = 0), so we return -F = charge_density * grad phi.
      return charge_density * _grad_phi[_qp];
  }
  ```

### 3. Usage in Input File
```ini
[Kernels]
  [ehd_force]
    type = INSADMomentumEHDForce
    variable = velocity
    c_positive = c_positive
    c_negative = c_negative
    potential = potential
  []
[]
```

## Alternative (Without C++)
If you want to avoid C++, you can use an `AuxVariable` for the force, computed by a `ParsedVectorAux`. However, `AuxVariables` are lagged by one iteration/timestep, which breaks the implicit coupling and may cause the solver to diverge during the highly non-linear instability phase. A custom C++ kernel is the robust way to handle it.

## Verification
After implementing the forcing from PNP in NS, we must add the `INSADMomentumEHDForce` kernel to `modules/combined/test/tests/electro_osmotic_flow/electro_convective_instability.i`. This will verify that the flow can be successfully driven by the PNP system.
