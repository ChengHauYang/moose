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

# Load and combine all datasets
df1 = load_convergence_data("convergence_results_one_layer.txt", "OneLayer")
df2 = load_convergence_data("convergence_results_same_span.txt", "SameSpan")
df3 = load_convergence_data("convergence_results_whole.txt", "Whole")
df_all = pd.concat([df1, df2, df3], ignore_index=True)

# Plotting setup
plt.rcParams.update({
    "font.family": "serif",
    "axes.labelsize": 14,
    "font.size": 14,
    "legend.fontsize": 10,
    "xtick.labelsize": 12,
    "ytick.labelsize": 12
})

plt.figure(figsize=(10, 6))

# Mapping
color_map = {"OneLayer": "C0", "SameSpan": "C1", "Whole": "C2"}
marker_map = {"QUAD4": "o", "QUAD9": "s"}
linestyle_map = {"approx": "-", "exact": "--"}

# Group and plot
for (approach, element, error_type), group in df_all.groupby(["approach", "element", "error_type"]):
    label = f"{approach} {element} {error_type}"
    plt.loglog(group["nx"], group["error"],
               color=color_map[approach],
               marker=marker_map[element],
               linestyle=linestyle_map[error_type],
               label=label)

plt.xlabel("Number of elements in x direction (nx)")
plt.ylabel("L2 Error")
plt.grid(True, which="both", ls="--", linewidth=0.5)
plt.legend(loc="center left", bbox_to_anchor=(1.01, 0.5), ncol=1)
plt.tight_layout()
plt.savefig("mesh_convergence_categorized.pdf", dpi=300, bbox_inches='tight')
plt.show()
