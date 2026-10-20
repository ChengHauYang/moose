# This file is part of the MOOSE framework
# https://mooseframework.inl.gov
#
# All rights reserved, see COPYRIGHT for full restrictions
# https://github.com/idaholab/moose/blob/master/COPYRIGHT
#
# Licensed under LGPL 2.1, please see LICENSE for details
# https://www.gnu.org/licenses/lgpl-2.1.html

"""
Simple NEML2 R2 models for KokkosQuantityToNEML2 tests.

R2ScaleModel: G = 2 * F (full rank-two input and output).
R2TraceModel: t = trace(F) = F_xx + F_yy + F_zz (scalar output).

Both models carry a float64 parameter/buffer so the EagerModel dtype resolves
to float64 and accepts MOOSE's double-precision input tensors without casting.
"""

from __future__ import annotations

import neml2 as _neml2
import torch
from neml2.factory import register_neml2_object
from neml2.models.chain_rule import ChainRuleDict
from neml2.models.model import Model
from neml2.schema import HitSchema, input, output
from neml2.types import R2, Scalar


@register_neml2_object("R2ScaleModel")
class R2ScaleModel(Model):
    """
    G = 2 * F for a full rank-two (3x3) input tensor F.

    The ``scale`` buffer (float64) forces the EagerModel to run in float64.
    """

    hit = HitSchema(
        input("F", R2, "Input full rank-two tensor"),
        output("G", R2, "Output: G = 2 * F"),
    )

    def __init__(self):
        """Register the float64 scale buffer."""
        super().__init__()
        self.register_buffer("scale", torch.tensor(2.0, dtype=torch.float64))

    def forward(self, F: R2, v: ChainRuleDict | None = None):  # type: ignore[override]
        """Return G = 2*F, or the chain-rule pushforward when v is provided."""
        G = self.scale * F
        if v is None:
            return G
        return self.apply_chain_rule(v, "G", {"F": self.scale})


@register_neml2_object("R2TraceModel")
class R2TraceModel(Model):
    """
    t = trace(F) = F_xx + F_yy + F_zz (scalar output).

    The ``one`` buffer (float64) forces the EagerModel to run in float64.
    """

    hit = HitSchema(
        input("F", R2, "Input full rank-two tensor"),
        output("t", Scalar, "Output: trace of F"),
    )

    def __init__(self):
        """Register the float64 identity buffer."""
        super().__init__()
        self.register_buffer("one", torch.tensor(1.0, dtype=torch.float64))

    def forward(self, F: R2, v: ChainRuleDict | None = None):  # type: ignore[override]
        """Return t = trace(F), or the chain-rule pushforward when v is provided."""
        raw = F.data
        t = self.one * (raw[..., 0, 0] + raw[..., 1, 1] + raw[..., 2, 2])
        t_wrapped = _neml2.Scalar(t)
        if v is None:
            return t_wrapped
        return self.apply_chain_rule(v, "t", {"F": R2(self.one * _identity_r2(raw))})

    def _identity_r2(self, raw):
        raise NotImplementedError


def _identity_r2(raw):
    """Jacobian dt/dF = identity diagonal (used for chain rule)."""
    out = torch.zeros_like(raw)
    out[..., 0, 0] = 1.0
    out[..., 1, 1] = 1.0
    out[..., 2, 2] = 1.0
    return out
