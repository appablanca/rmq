#!/usr/bin/env python3

import os
import matplotlib.pyplot as plt
import pandas as pd

os.makedirs("plots", exist_ok=True)

df = pd.read_csv("data.csv")

# Verify all methods produce the same checksum for each n (ignore NaN, e.g. skipped runs)
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

print("Saved plots/query_time.png, plots/space.png, plots/space_time_tradeoff.png")
