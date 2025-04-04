#!/usr/bin/env python

import pandas as pd
import matplotlib.pyplot as plt

# Filepaths with markers and labels
filepaths = {
    ('-', 'o', r'$N_{\mathrm{proc}} = 1$'): './output_1proc/solution.csv',
    ('--', 's', r'$N_{\mathrm{proc}} = 2$'): './output_2proc/solution.csv',
    ('-.', '^', r'$N_{\mathrm{proc}} = 4$'): './output_4proc/solution.csv',
    ('--', 'D', r'$N_{\mathrm{proc}} = 8$'): './output_8proc/solution.csv',
    ('-.', '*', r'$N_{\mathrm{proc}} = 12$'): './output_12proc/solution.csv',
}

plt.rcParams.update({
    "text.usetex": True,
    "font.family": "serif",
    "font.serif": "Times New Roman",
    "axes.titlesize": 16,
    "axes.labelsize": 14,
    "xtick.labelsize": 12,
    "ytick.labelsize": 12,
})

# --- Plot AvgUx ---
fig, ax = plt.subplots(figsize=(8, 6))

for (linestyle_, marker_, label), filepath in filepaths.items():
    try:
        df = pd.read_csv(filepath)
        x = df.iloc[1:-1, 0]
        y = df.iloc[1:-1, 1]
        ax.plot(x, y, linestyle=linestyle_, marker=marker_, label=label,
                alpha=0.8, markersize=6, markevery=10)
    except Exception as e:
        print(f"Error reading {filepath}: {e}")

ax.set_xlabel(r"$t$")
ax.set_ylabel(r"$\bar{Ux}$")
ax.legend(loc="lower right")
ax.grid(True, linestyle='--', alpha=0.6)
plt.tight_layout()
plt.savefig("AvgUx.pdf", bbox_inches='tight')

# --- Plot AvgUy ---
fig, ax = plt.subplots(figsize=(8, 6))

for (linestyle_, marker_, label), filepath in filepaths.items():
    try:
        df = pd.read_csv(filepath)
        x = df.iloc[1:-1, 0]
        y = df.iloc[1:-1, 2]
        ax.plot(x, y, linestyle=linestyle_, marker=marker_, label=label,
                alpha=0.8, markersize=6, markevery=10)
    except Exception as e:
        print(f"Error reading {filepath}: {e}")

ax.set_xlabel(r"$t$")
ax.set_ylabel(r"$\bar{Uy}$")
ax.legend(loc="lower right")
ax.grid(True, linestyle='--', alpha=0.6)
plt.tight_layout()
plt.savefig("AvgUy.pdf", bbox_inches='tight')

plt.show()
