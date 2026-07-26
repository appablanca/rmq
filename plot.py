#!/usr/bin/env python3

import os
import matplotlib.pyplot as plt
import pandas as pd
import re
import numpy as np

os.makedirs("plots", exist_ok=True)

df = pd.read_csv("data.csv")

# Verify all methods produce the same checksum for each n (ignore NaN, e.g. skipped runs)
checksums = df.pivot(index="n", columns="name", values="sum")
ref = checksums.stack().groupby("n").first()
assert (checksums.eq(ref, axis=0) | checksums.isna()).all(
    axis=None
), f"Checksum mismatch:\n{checksums}"

plt.rcParams["axes.grid"] = True

# bytes -> bits per element
df["space"] = df["space"] * 8 / df["n"]

names = sorted(df["name"].unique())
colors = {name: f"C{i}" for i, name in enumerate(names)}


def pivot(field):
    return df.pivot(index="n", columns="name", values=field)[names]


def pareto_frontier(df):
    """
    Pareto frontier minimizing both space and query time.
    """
    df = df.sort_values("space")

    frontier = []
    best_time = float("inf")

    for _, row in df.iterrows():
        if row["time"] < best_time:
            frontier.append(row)
            best_time = row["time"]

    return pd.DataFrame(frontier)


# ------------------------------------------------------------
# Plot 1: Query time
# ------------------------------------------------------------

fig, ax = plt.subplots(figsize=(10, 7))

pivot("time").plot(
    ax=ax,
    marker="o",
    color=[colors[n] for n in names],
)

ax.set_xlabel("n")
ax.set_ylabel("Query time (ns/query)")
ax.set_xscale("log")
ax.set_yscale("log", base=2)
ax.set_title("Query time per n")
ax.legend(
    loc="upper center",
    bbox_to_anchor=(0.5, -0.1),
    ncol=4,
)

fig.tight_layout()
fig.savefig("plots/query_time.png", dpi=150, bbox_inches="tight")
plt.close(fig)


# ------------------------------------------------------------
# Plot 2: Space usage
# ------------------------------------------------------------

fig, ax = plt.subplots(figsize=(10, 7))

pivot("space").plot(
    ax=ax,
    marker="o",
    color=[colors[n] for n in names],
)

ax.set_xlabel("n")
ax.set_ylabel("Space (bits/element)")
ax.set_xscale("log")
ax.set_yscale("log", base=2)
ax.set_title("Space usage per n")
ax.legend(
    loc="upper center",
    bbox_to_anchor=(0.5, -0.1),
    ncol=4,
)

fig.tight_layout()
fig.savefig("plots/space.png", dpi=150, bbox_inches="tight")
plt.close(fig)


# ------------------------------------------------------------
# Plot 3: Tradeoff at largest n
# ------------------------------------------------------------

max_n = df["n"].max()

tradeoff = df[df["n"] == max_n]
frontier = pareto_frontier(tradeoff)
frontier_names = set(frontier["name"])

fig, ax = plt.subplots(figsize=(10, 7))

for _, r in tradeoff.iterrows():

    on_frontier = r["name"] in frontier_names

    ax.scatter(
        r["space"],
        r["time"],
        color=colors[r["name"]],
        edgecolors="black" if on_frontier else "none",
        linewidths=1.5 if on_frontier else 0,
        s=70,
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

ax.plot(
    frontier["space"],
    frontier["time"],
    "-k",
    linewidth=2,
    label="Pareto frontier",
    zorder=2,
)

handles, labels = ax.get_legend_handles_labels()
by_label = dict(zip(labels, handles))

ax.legend(
    by_label.values(),
    by_label.keys(),
    loc="upper center",
    bbox_to_anchor=(0.5, -0.1),
    ncol=4,
)

ax.set_xlabel("Space (bits/element)")
ax.set_ylabel("Query time (ns/query)")
ax.set_xscale("log", base=2)
ax.set_yscale("log", base=2)
ax.set_title(f"Space-time tradeoff (n={max_n})")

fig.tight_layout()
fig.savefig(
    "plots/space_time_tradeoff.png",
    dpi=150,
    bbox_inches="tight",
)
plt.close(fig)


# ------------------------------------------------------------
# Plot 4: Tradeoff for every n
# ------------------------------------------------------------

for current_n in sorted(df["n"].unique()):

    tradeoff = df[df["n"] == current_n]
    frontier = pareto_frontier(tradeoff)
    frontier_names = set(frontier["name"])

    fig, ax = plt.subplots(figsize=(10, 7))

    for _, r in tradeoff.iterrows():

        name = r["name"]

        label = name

        on_frontier = r["name"] in frontier_names

        ax.scatter(
            r["space"],
            r["time"],
            color=colors[r["name"]],
            edgecolors="black" if on_frontier else "none",
            linewidths=1.5 if on_frontier else 0,
            s=70,
            label=label,
            zorder=3,
        )

        ax.annotate(
            label,
            (r["space"], r["time"]),
            textcoords="offset points",
            xytext=(6, 4),
            fontsize="small",
            color=colors[r["name"]],
        )

    ax.plot(
        frontier["space"],
        frontier["time"],
        "-k",
        linewidth=2,
        label="Pareto frontier",
        zorder=2,
    )

    handles, labels = ax.get_legend_handles_labels()
    by_label = dict(zip(labels, handles))

    ax.legend(
        by_label.values(),
        by_label.keys(),
        loc="upper center",
        bbox_to_anchor=(0.5, -0.1),
        ncol=4,
    )

    ax.set_xlabel("Space (bits/element)")
    ax.set_ylabel("Query time (ns/query)")
    ax.set_xscale("log", base=2)
    ax.set_yscale("log", base=2)
    ax.set_title(f"Space-time tradeoff (n={current_n})")

    fig.tight_layout()
    fig.savefig(
        f"plots/space_time_tradeoff_{current_n}.png",
        dpi=150,
        bbox_inches="tight",
    )

    plt.close(fig)


print("Saved all plots.")
