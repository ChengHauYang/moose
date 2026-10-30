"""Independent reference for the total-mode F-bar consistent tangent.

Goal: confirm, without MOOSE, that the ONLY defect in
KokkosComputeLagrangianStressCustomPK2.h is the cofactor typo at Cof[2][1]
(F[0][2]*F[2][0] instead of F[0][2]*F[1][0]), and that fixing it makes the
analytic tangent match a finite-difference of the full element residual,
including the non-local cross-quadrature-point F_avg coupling.

Constitutive law is irrelevant to the F-bar algebra; we use St. Venant-
Kirchhoff with an analytic material tangent so the only approximation in the
comparison is the central finite difference of the residual.
"""

import numpy as np

np.random.seed(0)
I3 = np.eye(3)

LAM, MU = 1.3, 0.8  # arbitrary nonzero Lame parameters


def svk_pk2(F):
    """St. Venant-Kirchhoff PK2 and dS/dF (analytic), arg = deformation gradient."""
    C = F.T @ F
    E = 0.5 * (C - I3)
    S = LAM * np.trace(E) * I3 + 2.0 * MU * E
    # dS_KJ/dF_mn = lam*delta_KJ*F_mn + mu*(delta_Kn*F_mJ + delta_Jn*F_mK)
    dS = np.zeros((3, 3, 3, 3))
    for K in range(3):
        for J in range(3):
            for m in range(3):
                for n in range(3):
                    dS[K, J, m, n] = (
                        LAM * (K == J) * F[m, n]
                        + MU * ((K == n) * F[m, J] + (J == n) * F[m, K])
                    )
    return S, dS


def invT_buggy(F):
    """Reproduce the code's Cof/detF with the Cof[2][1] typo."""
    Cof = np.zeros((3, 3))
    Cof[0, 0] = F[1, 1] * F[2, 2] - F[1, 2] * F[2, 1]
    Cof[0, 1] = -(F[1, 0] * F[2, 2] - F[1, 2] * F[2, 0])
    Cof[0, 2] = F[1, 0] * F[2, 1] - F[1, 1] * F[2, 0]
    Cof[1, 0] = -(F[0, 1] * F[2, 2] - F[0, 2] * F[2, 1])
    Cof[1, 1] = F[0, 0] * F[2, 2] - F[0, 2] * F[2, 0]
    Cof[1, 2] = -(F[0, 0] * F[2, 1] - F[0, 1] * F[2, 0])
    Cof[2, 0] = F[0, 1] * F[1, 2] - F[0, 2] * F[1, 1]
    Cof[2, 1] = -(F[0, 0] * F[1, 2] - F[0, 2] * F[2, 0])  # <-- TYPO: F[2,0]
    Cof[2, 2] = F[0, 0] * F[1, 1] - F[0, 1] * F[1, 0]
    detF = np.linalg.det(F)
    return Cof / detF


def invT_fixed(F):
    """Same, with the corrected Cof[2][1]."""
    Cof = np.zeros((3, 3))
    Cof[0, 0] = F[1, 1] * F[2, 2] - F[1, 2] * F[2, 1]
    Cof[0, 1] = -(F[1, 0] * F[2, 2] - F[1, 2] * F[2, 0])
    Cof[0, 2] = F[1, 0] * F[2, 1] - F[1, 1] * F[2, 0]
    Cof[1, 0] = -(F[0, 1] * F[2, 2] - F[0, 2] * F[2, 1])
    Cof[1, 1] = F[0, 0] * F[2, 2] - F[0, 2] * F[2, 0]
    Cof[1, 2] = -(F[0, 0] * F[2, 1] - F[0, 1] * F[2, 0])
    Cof[2, 0] = F[0, 1] * F[1, 2] - F[0, 2] * F[1, 1]
    Cof[2, 1] = -(F[0, 0] * F[1, 2] - F[0, 2] * F[1, 0])  # <-- FIXED: F[1,0]
    Cof[2, 2] = F[0, 0] * F[1, 1] - F[0, 1] * F[1, 0]
    detF = np.linalg.det(F)
    return Cof / detF


# ---- synthetic element: nqp quadrature points, ndof trial functions ----
NQP = 4
NDOF = 5
w = np.random.rand(NQP) + 0.5                   # positive quadrature weights
grad_phi = np.random.randn(NDOF, NQP, 3) * 0.7  # reference-config shape gradients
u = np.random.randn(NDOF, 3) * 0.12             # nodal displacements (nonuniform, non-axis-aligned)


def grad_u_qp(uvec):
    gu = np.zeros((NQP, 3, 3))
    for q in range(NQP):
        for b in range(NDOF):
            gu[q] += np.outer(uvec[b], grad_phi[b, q])  # gu[q][i][l] = sum_b u[b][i] grad_phi[b][q][l]
    return gu


def residual(uvec):
    """R[a][alpha] = sum_q w_q sum_J P_q[alpha,J] grad_phi[a,q,J], P = F_ust S(F_stab)."""
    gu = grad_u_qp(uvec)
    F = I3[None] + gu
    vol = w.sum()
    F_avg = (w[:, None, None] * F).sum(0) / vol
    detFavg = np.linalg.det(F_avg)
    R = np.zeros((NDOF, 3))
    for q in range(NQP):
        gamma = (detFavg / np.linalg.det(F[q])) ** (1.0 / 3.0)
        S, _ = svk_pk2(gamma * F[q])
        P = F[q] @ S
        for a in range(NDOF):
            R[a] += w[q] * (P @ grad_phi[a, q])
    return R


def analytic_K(invT_fn):
    """Assemble K exactly as the kernel does, using the given per-qp F^{-T} routine."""
    gu = grad_u_qp(u)
    F = I3[None] + gu
    vol = w.sum()
    F_avg = (w[:, None, None] * F).sum(0) / vol
    detFavg = np.linalg.det(F_avg)
    F_avg_invT = invT_fixed(F_avg)  # F-bar-average file is verified correct
    avg_grad_phi = (w[None, :, None] * grad_phi).sum(1) / vol  # [b][3]

    # per-qp local tangent dpk1_d_grad_u and the A / gamma3 auxiliaries
    dpk1 = np.zeros((NQP, 3, 3, 3, 3))
    A = np.zeros((NQP, 3, 3))
    g3 = np.zeros(NQP)
    for q in range(NQP):
        gamma = (detFavg / np.linalg.det(F[q])) ** (1.0 / 3.0)
        S, C = svk_pk2(gamma * F[q])
        FinvT = invT_fn(F[q])
        Aq = np.einsum("iK,KJmn,mn->iJ", F[q], C, F[q])
        A[q] = Aq
        g3[q] = gamma / 3.0
        for i in range(3):
            for J in range(3):
                for k in range(3):
                    for L in range(3):
                        v = (i == k) * S[L, J]
                        v += gamma * np.dot(F[q, i, :], C[:, J, k, L])
                        dpk1[q, i, J, k, L] = v - g3[q] * Aq[i, J] * FinvT[k, L]

    K = np.zeros((NDOF, 3, NDOF, 3))
    for a in range(NDOF):
        for alpha in range(3):
            for b in range(NDOF):
                for beta in range(3):
                    acc = 0.0
                    scalar_contr = np.dot(F_avg_invT[beta], avg_grad_phi[b])
                    for q in range(NQP):
                        val = np.zeros(3)
                        for J in range(3):
                            val[J] = np.dot(dpk1[q, alpha, J, beta, :], grad_phi[b, q])
                            val[J] += g3[q] * scalar_contr * A[q, alpha, J]
                        acc += w[q] * np.dot(val, grad_phi[a, q])
                    K[a, alpha, b, beta] = acc
    return K


def fd_K(eps=1e-7):
    K = np.zeros((NDOF, 3, NDOF, 3))
    for b in range(NDOF):
        for beta in range(3):
            up = u.copy(); up[b, beta] += eps
            um = u.copy(); um[b, beta] -= eps
            K[:, :, b, beta] = (residual(up) - residual(um)) / (2 * eps)
    return K


Kfd = fd_K()
Kfix = analytic_K(invT_fixed)
Kbug = analytic_K(invT_buggy)


def relerr(A, B):
    return np.linalg.norm(A - B) / np.linalg.norm(B)


print("deformation is nonuniform / non-axis-aligned (F varies per qp):")
print("  det range:", [round(np.linalg.det(I3 + grad_u_qp(u)[q]), 4) for q in range(NQP)])
print()
print("relative Frobenius error  ||K - Kfd|| / ||Kfd||")
print("  analytic (FIXED Cof[2][1]) vs FD :", f"{relerr(Kfix, Kfd):.3e}")
print("  analytic (BUGGY Cof[2][1]) vs FD :", f"{relerr(Kbug, Kfd):.3e}")
print()
# Show the buggy error is localized to trial component k=2, dir L=1 contributions.
print("||Kbug - Kfix|| / ||Kfix|| (pure effect of the typo):",
      f"{relerr(Kbug, Kfix):.3e}")
