tmpdir=$(mktemp -d)

for i in {1..21}; do
    ./rmq input > "$tmpdir/run_$i.csv"
done

python3 - "$tmpdir" > data.csv <<'PY'
import csv
import glob
import statistics
import sys
from collections import defaultdict

files = sorted(glob.glob(f"{sys.argv[1]}/run_*.csv"))
rows = defaultdict(list)
metadata = {}

for filename in files:
    with open(filename, newline="") as f:
        for row in csv.DictReader(f):
            key = (row["n"], row["q"], row["name"])
            rows[key].append(float(row["time"]))
            metadata[key] = (row["space"], row["sum"])

writer = csv.writer(sys.stdout)
writer.writerow(["n", "q", "name", "space", "sum", "time"])

for key, times in rows.items():
    n, q, name = key
    space, checksum = metadata[key]

    if len(times) != 21:
        raise RuntimeError(f"{key} has {len(times)} measurements instead of 21")

    writer.writerow([
        n,
        q,
        name,
        space,
        checksum,
        statistics.median(times),
    ])
PY

rm -rf "$tmpdir"