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


def pareto_front(df):
    """
    Return Pareto-optimal points (minimize space and time),
    sorted by increasing space.
    """
    pts = df[["space", "time"]].sort_values("space").to_numpy()

    front = []
    best_time = np.inf
    for space, time in pts:
        if time < best_time:
            front.append((space, time))
            best_time = time

    return np.array(front)


# ------------------------------------------------------------------
# Plot 1: query time per n
# ------------------------------------------------------------------
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


# ------------------------------------------------------------------
# Plot 2: space usage per n
# ------------------------------------------------------------------
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


# ------------------------------------------------------------------
# Plot 3: space-time tradeoff at largest n
# ------------------------------------------------------------------
max_n = df["n"].max()
tradeoff = df[df["n"] == max_n]

fig, ax = plt.subplots(figsize=(10, 7))

for _, r in tradeoff.iterrows():
    ax.scatter(
        r["space"],
        r["time"],
        color=colors[r["name"]],
        label=r["name"],
        zorder=3,
    )
    ax.annotate(
        r["name"],
        (r["space"], r["time"]),
        textcoords="offset points",
        xytext=(6, 4),
        fontsize="small",
        color=colors[r["name"]],
    )

# Pareto front
front = pareto_front(tradeoff)
if len(front) > 1:
    ax.plot(
        front[:, 0],
        front[:, 1],
        "--k",
        linewidth=2,
        label="Pareto front",
        zorder=2,
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


# ------------------------------------------------------------------
# Plot 4: space-time tradeoff for all n
# Four n values per figure (2x2 layout)
# ------------------------------------------------------------------
all_ns = sorted(df["n"].unique())
groups = [all_ns[i : i + 4] for i in range(0, len(all_ns), 4)]

saved_plot4_files = []

for group_idx, n_group in enumerate(groups, start=1):

    fig, axes = plt.subplots(
        2,
        2,
        figsize=(12, 10),
        layout="constrained",
    )

    axes = axes.flatten()

    for ax, n_val in zip(axes, n_group):

        sub = df[df["n"] == n_val]

        for _, r in sub.iterrows():
            ax.scatter(
                r["space"],
                r["time"],
                color=colors[r["name"]],
                s=30,
                zorder=3,
            )

            ax.annotate(
                r["name"],
                (r["space"], r["time"]),
                textcoords="offset points",
                xytext=(4, 3),
                fontsize="x-small",
                color=colors[r["name"]],
            )

        # Pareto front
        front = pareto_front(sub)
        if len(front) > 1:
            ax.plot(
                front[:, 0],
                front[:, 1],
                "--k",
                linewidth=1.8,
                zorder=2,
            )

        ax.set_xscale("log", base=2)
        ax.set_yscale("log", base=2)
        ax.set_title(f"n = {n_val:,}", fontsize="medium")

    # Hide unused subplots
    for ax in axes[len(n_group) :]:
        ax.axis("off")

    fig.supxlabel("Space (bits/element)")
    fig.supylabel("Query time (ns/query)")
    fig.suptitle(
        f"Space-time tradeoff across n — Part {group_idx}",
        fontsize=14,
    )

    output_path = f"plots/space_time_tradeoff_by_n_{group_idx}.png"
    fig.savefig(output_path, dpi=150, bbox_inches="tight")
    plt.close(fig)

    saved_plot4_files.append(output_path)

print("Saved:")
print("  plots/query_time.png")
print("  plots/space.png")
print("  plots/space_time_tradeoff.png")
for f in saved_plot4_files:
    print(f"  {f}")
