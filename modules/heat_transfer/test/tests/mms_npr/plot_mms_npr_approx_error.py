import matplotlib.pyplot as plt
import numpy as np

# Load data, skipping header
raw_data = []
with open("convergence_results.txt") as f:
    for line in f:
        if line.startswith("#") or not line.strip():
            continue
        tokens = line.split()
        if len(tokens) < 5:
            continue  # Skip incomplete lines
        try:
            nx = int(tokens[0])
            elem = tokens[1]
            order = tokens[2]
            order_number = int(tokens[3])  # not use
            approx = float(tokens[4])
            exact = float(tokens[5])
            raw_data.append([nx, elem, order, approx, exact])
        except:
            continue
# Convert to NumPy array
data = np.array(raw_data, dtype=object)

# Split columns
nx = data[:, 0].astype(int)
elem = data[:, 1]
order = data[:, 2]
error_approx = data[:, 3].astype(float)
error_exact = data[:, 4].astype(float)

# Plotting (same as before)
quad4_mask = elem == "QUAD4"
quad9_mask = elem == "QUAD9"

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

plt.figure(figsize=(6.5, 5))
plt.loglog(nx[quad4_mask], error_approx[quad4_mask], 'o-', color='C0', label=r'\textbf{QUAD4 (Approx)}')
plt.loglog(nx[quad4_mask], error_exact[quad4_mask], 'o--', color='C0', label=r'\textbf{QUAD4 (Exact)}')
plt.loglog(nx[quad9_mask], error_approx[quad9_mask], 's-', color='C1', label=r'\textbf{QUAD9 (Approx)}')
plt.loglog(nx[quad9_mask], error_exact[quad9_mask], 's--', color='C1', label=r'\textbf{QUAD9 (Exact)}')

plt.xlabel(r'Number of elements in $x$ direction ($n_x$)')
plt.ylabel(r'$L^2$ Error')
plt.grid(True, which="both", ls="--", linewidth=0.5)
plt.legend(loc="upper left", bbox_to_anchor=(1.02, 1))
plt.tight_layout()
plt.savefig("mesh_convergence_combined.pdf", dpi=300, bbox_inches='tight')
plt.show()
