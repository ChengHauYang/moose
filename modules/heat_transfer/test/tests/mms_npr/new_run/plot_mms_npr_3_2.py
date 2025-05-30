import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

def load_convergence_data(filename, label_prefix):
    data = []
    with open(filename) as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            tokens = line.split()
            if len(tokens) < 6:
                continue
            try:
                nx = int(tokens[0])
                elem = tokens[1]
                approx = float(tokens[4])
                exact = float(tokens[5])
                data.append([nx, label_prefix, elem, "approx", approx])
                data.append([nx, label_prefix, elem, "exact", exact])
            except:
                continue
    return pd.DataFrame(data, columns=["nx", "approach", "element", "error_type", "error"])

# === Load all six datasets ===
# Exact Coefficient
df1 = load_convergence_data("convergence_results_one_layer.txt", "OneLayer")
df2 = load_convergence_data("convergence_results_same_span.txt", "SameSpan")
df3 = load_convergence_data("convergence_results_whole.txt", "Whole")

# Coefficient from nx=640
df1b = load_convergence_data("../convergence_results_one_layer.txt", "OneLayer")
df2b = load_convergence_data("../convergence_results_same_span.txt", "SameSpan")
df3b = load_convergence_data("../convergence_results_whole.txt", "Whole")

# Plotting setup
plt.rcParams.update({
    "font.family": "serif",
    "axes.labelsize": 14,
    "font.size": 14,
    "legend.fontsize": 10,
    "xtick.labelsize": 12,
    "ytick.labelsize": 12
})

fig, axs = plt.subplots(2, 3, figsize=(18, 10), sharex=True, sharey=True)

color_map = {"OneLayer": "C0", "SameSpan": "C1", "Whole": "C2"}
marker_map = {"QUAD4": "o", "QUAD9": "s"}
linestyle_map = {"approx": "-", "exact": "--"}
title_map = {0: "OneLayer", 1: "SameSpan", 2: "Whole"}
row_label_map = {0: "Exact Coefficients", 1: "Coeff. from nx=640"}

# Top row: exact coefficients
dfs_exact = [df1, df2, df3]
# Bottom row: coeff from nx=640
dfs_from640 = [df1b, df2b, df3b]

for row, dfs in enumerate([dfs_exact, dfs_from640]):
    for col, df in enumerate(dfs):
        ax = axs[row, col]
        approach = title_map[col]
        df_subset = df[df["approach"] == approach]
        for (element, error_type), group in df_subset.groupby(["element", "error_type"]):
            label = f"{element} {error_type}"
            ax.loglog(group["nx"], group["error"],
                      color=color_map[approach],
                      marker=marker_map[element],
                      linestyle=linestyle_map[error_type],
                      label=label)
        if row == 0:
            ax.set_title(f"{approach}")
        if row == 1:
            ax.set_xlabel("nx")
        if col == 0:
            ax.set_ylabel(row_label_map[row] + "\nL2 Error")
        ax.grid(True, which="both", ls="--", linewidth=0.5)
        ax.legend(loc="best", frameon=False)

plt.tight_layout()
plt.savefig("mesh_convergence_2x3_subfig.pdf", dpi=300, bbox_inches='tight')
plt.show()
#!/usr/bin/env python

