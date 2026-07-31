#!/usr/bin/env python3

import os
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np

os.makedirs("plots", exist_ok=True)

df = pd.read_csv("data.csv")


checksums = df.pivot(index="n", columns="name", values="sum")
ref = checksums.stack().groupby("n").first()  # first non-NaN per row
assert (checksums.eq(ref, axis=0) | checksums.isna()).all(
    axis=None
), f"Checksum mismatch:\n{checksums}"

plt.rcParams["axes.grid"] = True

df["space"] = df["space"] * 8 / df["n"]  # bytes → bits per element

names = sorted(df["name"].unique())
colors = {name: f"C{i}" for i, name in enumerate(names)}


def pivot(field):
    return df.pivot(index="n", columns="name", values=field)[names]


# Plot 1: query time per n
fig, ax = plt.subplots(figsize=(10, 7))
time = pivot("time")
time.plot(ax=ax, marker="o", color=[colors[n] for n in names])

for name in names:
    ax.annotate(
        name,
        (time.index[-1], time[name].iloc[-1]),
        textcoords="offset points",
        xytext=(6, 0),
        color=colors[name],
        fontsize="small",
    )

ax.set_xlabel("n")
ax.set_ylabel("Query time (ns/query)")
ax.set_xscale("log")
ax.set_yscale("log", base=2)
ax.set_title("Query time per n")
ax.legend(loc="upper center", bbox_to_anchor=(0.5, -0.1), ncol=4)
fig.tight_layout()
fig.savefig("plots/query_time.png", dpi=150, bbox_inches="tight")
plt.close(fig)

# Plot 2: space usage per n
fig, ax = plt.subplots(figsize=(10, 7))
space = pivot("space")
space.plot(ax=ax, marker="o", color=[colors[n] for n in names])

for name in names:
    ax.annotate(
        name,
        (space.index[-1], space[name].iloc[-1]),
        textcoords="offset points",
        xytext=(6, 0),
        color=colors[name],
        fontsize="small",
    )
ax.set_xlabel("n")
ax.set_ylabel("Space (bits/element)")
ax.set_xscale("log")
ax.set_yscale("log", base=2)
ax.set_title("Space usage per n")
ax.legend(loc="upper center", bbox_to_anchor=(0.5, -0.1), ncol=4)
fig.tight_layout()
fig.savefig("plots/space.png", dpi=150, bbox_inches="tight")
plt.close(fig)

# Plot 3: space-time tradeoff at max n
max_n = df["n"].max()
tradeoff = df[df["n"] == max_n]
fig, ax = plt.subplots(figsize=(10, 7))
for _, r in tradeoff.iterrows():
    ax.scatter(
        r["space"], r["time"], color=colors[r["name"]], label=r["name"], zorder=3
    )
    ax.annotate(
        r["name"],
        (r["space"], r["time"]),
        textcoords="offset points",
        xytext=(6, 4),
        fontsize="small",
        color=colors[r["name"]],
    )
ax.set_xlabel("Space (bits/element)")
ax.set_ylabel("Query time (ns/query)")
ax.set_xscale("log", base=2)
ax.set_yscale("log", base=2)
ax.set_title(f"Space-time tradeoff (n={max_n})")
ax.legend(loc="upper center", bbox_to_anchor=(0.5, -0.1), ncol=4)
fig.tight_layout()
fig.savefig("plots/space_time_tradeoff.png", dpi=150, bbox_inches="tight")
plt.close(fig)

# Plot 4: space-time tradeoff across all ns
all_ns = sorted(df["n"].unique())

ncols = 4
nrows = -(-len(all_ns) // ncols)

fig, axes = plt.subplots(
    nrows,
    ncols,
    figsize=(4.5 * ncols, 4 * nrows),
    layout="constrained",
)
axes = np.atleast_1d(axes).flatten()

for ax, n_val in zip(axes, all_ns):
    sub = df[df["n"] == n_val]
    for _, r in sub.iterrows():
        ax.scatter(r["space"], r["time"], color=colors[r["name"]], zorder=3, s=30)
        ax.annotate(
            r["name"],
            (r["space"], r["time"]),
            textcoords="offset points",
            xytext=(4, 3),
            fontsize="x-small",
            color=colors[r["name"]],
        )
    ax.set_xscale("log", base=2)
    ax.set_yscale("log", base=2)
    ax.set_title(f"n = {n_val:,}", fontsize="medium")

for ax in axes[len(all_ns) :]:
    ax.axis("off")

fig.supxlabel("Space (bits/element)")
fig.supylabel("Query time (ns/query)")
fig.suptitle("Space-time tradeoff across n", fontsize=14)

fig.savefig("plots/space_time_tradeoff_by_n.png", dpi=150, bbox_inches="tight")
plt.close(fig)

print("Saved plots/query_time.png, plots/space.png, plots/space_time_tradeoff.png")
