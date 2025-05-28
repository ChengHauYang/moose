import matplotlib.pyplot as plt
import numpy as np

# Load the data (skip the header)
data = np.genfromtxt('convergence_results.txt', skip_header=1, dtype=str)

# Extract columns
nx = data[:, 0].astype(int)
elem = data[:, 1]
order = data[:, 2]
error = data[:, 3].astype(float)

# Separate by element type
quad4_mask = elem == "QUAD4"
quad9_mask = elem == "QUAD9"

# Apply LaTeX and Time New Roman settings
plt.rcParams.update({
    "text.usetex": True,
    "font.family": "serif",
    "font.serif": ["Times New Roman"],
    "axes.labelsize": 14,
    "font.size": 14,
    "legend.fontsize": 12,
    "xtick.labelsize": 12,
    "ytick.labelsize": 12
})

# Plot
plt.figure(figsize=(6.5, 5))
plt.loglog(nx[quad4_mask], error[quad4_mask], 'o-', label=r'\textbf{QUAD4}')
plt.loglog(nx[quad9_mask], error[quad9_mask], 's--', label=r'\textbf{QUAD9}')

# Labeling
plt.xlabel(r'Number of elements in $x$ direction ($n_x$)')
plt.ylabel(r'$L^2$ Error')
# plt.title(r'\textbf{Mesh Convergence Study}', pad=10)
plt.grid(True, which="both", ls="--", linewidth=0.5)
plt.legend()
plt.tight_layout()

# Save figure
plt.savefig("mesh_convergence.pdf", dpi=300)
plt.show()

