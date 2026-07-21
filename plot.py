#!/usr/bin/env python3

import os
import matplotlib.pyplot as plt
import pandas as pd

os.makedirs("plots", exist_ok=True)

df = pd.read_csv("data.csv")

# Verify all methods produce the same checksum for each n (ignore NaN, e.g. skipped runs)
checksums = df.pivot(index="n", columns="name", values="sum")
ref = checksums.stack().groupby("n").first()
assert (checksums.eq(ref, axis=0) | checksums.isna()).all(
    axis=None
), f"Checksum mismatch:\n{checksums}"

plt.rcParams["axes.grid"] = True

df["space"] = df["space"] * 8 / df["n"]  # bytes → bits per element


# Desired order
names = [
    "OnTheFlyNaive",
    "PrecomputedNaive",
    "SparseTable",
    "SegmentTree",
    "Block2",
    "Block4",
    "Block8",
    "Block16",
    "Block32",
    "BlocksPrecompute2",
    "BlocksPrecompute4",
    "BlocksPrecompute8",
    "BlocksPrecompute16",
    "BlocksPrecompute32",
]

colors = {name: f"C{i}" for i, name in enumerate(names)}

markers = {
    "OnTheFlyNaive": "o",
    "PrecomputedNaive": "s",
    "SparseTable": "^",
    "SegmentTree": "D",
    "Block2": "X",
    "Block4": "X",
    "Block8": "X",
    "Block16": "X",
    "Block32": "X",
    "BlocksPrecompute2": "p",
    "BlocksPrecompute4": "p",
    "BlocksPrecompute8": "p",
    "BlocksPrecompute16": "p",
    "BlocksPrecompute32": "p",
}


def pivot(field):
    return df.pivot(index="n", columns="name", values=field)[names]


# ------------------------------------------------------------
# Plot 1: Query time per n
# ------------------------------------------------------------
fig, ax = plt.subplots(figsize=(10, 7))

time = pivot("time")

for name in names:
    ax.plot(
        time.index,
        time[name],
        color=colors[name],
        marker=markers[name],
        linewidth=2,
        markersize=6,
        label=name,
    )

ax.set_xlabel("n")
ax.set_ylabel("Query time (ns/query)")
ax.set_xscale("log")
ax.set_yscale("log", base=2)
ax.set_title("Query time per n")
ax.legend(loc="upper center", bbox_to_anchor=(0.5, -0.1), ncol=3)

fig.tight_layout()
fig.savefig("plots/query_time.png", dpi=150, bbox_inches="tight")
plt.close(fig)


# ------------------------------------------------------------
# Plot 2: Space usage per n
# ------------------------------------------------------------
fig, ax = plt.subplots(figsize=(10, 7))

space = pivot("space")

for name in names:
    ax.plot(
        space.index,
        space[name],
        color=colors[name],
        marker=markers[name],
        linewidth=2,
        markersize=6,
        label=name,
    )

ax.set_xlabel("n")
ax.set_ylabel("Space (bits/element)")
ax.set_xscale("log")
ax.set_yscale("log", base=2)
ax.set_title("Space usage per n")
ax.legend(loc="upper center", bbox_to_anchor=(0.5, -0.1), ncol=3)

fig.tight_layout()
fig.savefig("plots/space.png", dpi=150, bbox_inches="tight")
plt.close(fig)


# ------------------------------------------------------------
# Plot 3: Space-time tradeoff at maximum n
# ------------------------------------------------------------
max_n = df["n"].max()
tradeoff = df[df["n"] == max_n]

fig, ax = plt.subplots(figsize=(10, 7))

for _, r in tradeoff.iterrows():
    ax.scatter(
        r["space"],
        r["time"],
        color=colors[r["name"]],
        marker=markers[r["name"]],
        s=90,
        zorder=3,
    )

    ax.annotate(
        r["name"],
        (r["space"], r["time"]),
        xytext=(6, 4),
        textcoords="offset points",
        fontsize="small",
        color=colors[r["name"]],
    )

ax.set_xlabel("Space (bits/element)")
ax.set_ylabel("Query time (ns/query)")
ax.set_xscale("log", base=2)
ax.set_yscale("log", base=2)
ax.set_title(f"Space–time tradeoff (n={max_n})")

fig.tight_layout()
fig.savefig("plots/space_time_tradeoff.png", dpi=150, bbox_inches="tight")
plt.close(fig)

print("Saved:")
print("  plots/query_time.png")
print("  plots/space.png")
print("  plots/space_time_tradeoff.png")
