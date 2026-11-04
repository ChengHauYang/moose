# SCZM/NEML2 work: historical implementation entry point

> **CUDA/Linux continuation:** stop here and read
> [`handoverToCuda.md`](handoverToCuda.md). It is the single authoritative handoff for the current
> NVIDIA qualification work. The remainder of this file describes an older implementation stage.

The active implementation plan for unified Kokkos-resident NEML2 input gathering was:

- [`archive/neml2_kokkos_batch_maps.md`](archive/neml2_kokkos_batch_maps.md) for the immediate,
  pre-gatherer batch-map slice.
- [`archive/kokkos_quantity_to_neml2.md`](archive/kokkos_quantity_to_neml2.md) for the complete
  bulk/interface input-transfer architecture and later phases.

The batch-map plan is authoritative for the next implementation slice. The unified gatherer plan records the decision
to support both domains through the `KokkosQuantityToNEML2` family, avoid `TorchFEInterpolation`, gather
both interface sides with one object, preserve `NEML2BatchIndexGenerator` ordering, and defer any CUDA
qualification claim until NVIDIA compilation and execution pass.

For the broader completed SCZM history, distributed verification notes, and older work tracks, see:

- [`see_this_is_enough.md`](see_this_is_enough.md)
- [`pk2-to-pk1-interface-conversion.md`](pk2-to-pk1-interface-conversion.md)
- [`kokkos_ad_sczm_plan.md`](kokkos_ad_sczm_plan.md)

Files under `archive/` are historical implementation records. For the current task, start with the
batch-map plan above, then use the unified gatherer plan for the downstream architecture rather than
reconstructing intent from older SCZM notes.
