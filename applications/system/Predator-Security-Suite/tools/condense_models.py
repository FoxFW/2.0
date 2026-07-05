import csv
import sys
from collections import defaultdict, Counter
from pathlib import Path

PRIORITY_FREQ = [433920000, 315000000, 868350000]
FREQ_WEIGHT = {f: (len(PRIORITY_FREQ) - i) for i, f in enumerate(PRIORITY_FREQ)}

def norm_freq(s: str) -> int:
    s = s.strip()
    try:
        if s.lower().endswith('mhz'):
            return int(round(float(s[:-3]) * 1_000_000))
        if s.lower().endswith('khz'):
            return int(round(float(s[:-3]) * 1_000))
        if s.lower().endswith('hz'):
            return int(round(float(s[:-2])))
        # plain int
        return int(s)
    except Exception:
        # fallback: keep only digits
        digits = ''.join(ch for ch in s if ch.isdigit())
        return int(digits) if digits else 0

def weight_key(make: str, freq: int, count: int) -> tuple:
    # Higher weight first: priority freq, then larger groups, then brand name
    return (
        FREQ_WEIGHT.get(freq, 0),
        count,
        -abs(freq - 433920000),
        make.lower(),
    )

def read_rows(in_path: Path):
    rows = []
    with in_path.open('r', newline='', encoding='utf-8') as f:
        reader = csv.reader(f)
        for row in reader:
            if not row or len(row) < 4:
                continue
            make = row[0].strip()
            model = row[1].strip()
            freq = norm_freq(row[2])
            rtype = row[3].strip()
            rows.append((make, model, freq, rtype))
    return rows

def write_rows(out_path: Path, rows):
    out_path.parent.mkdir(parents=True, exist_ok=True)
    with out_path.open('w', newline='', encoding='utf-8') as f:
        writer = csv.writer(f)
        for r in rows:
            writer.writerow(r)

def condense_max128(rows):
    # Stage 1: Group by (make, freq, rtype)
    g1 = defaultdict(list)
    for make, model, freq, rtype in rows:
        g1[(make, freq, rtype)].append(model)

    groups = []
    for (make, freq, rtype), models in g1.items():
        m = models[0] if len(models) == 1 else "Various"
        groups.append((make, m, freq, rtype, len(models)))

    # Sort groups by priority
    groups.sort(key=lambda x: weight_key(x[0], x[2], x[4]), reverse=True)

    if len(groups) <= 128:
        return [(g[0], g[1], g[2], g[3]) for g in groups]

    # Stage 2: Group by (make, freq) -> merge rtypes
    g2 = defaultdict(lambda: {"models": set(), "rtypes": set(), "count": 0})
    for make, model, freq, rtype in rows:
        d = g2[(make, freq)]
        d["models"].add(model)
        d["rtypes"].add(rtype)
        d["count"] += 1

    groups2 = []
    for (make, freq), d in g2.items():
        model = next(iter(d["models"])) if len(d["models"]) == 1 else "Various"
        rtype = next(iter(d["rtypes"])) if len(d["rtypes"]) == 1 else "Mixed"
        groups2.append((make, model, freq, rtype, d["count"]))
    groups2.sort(key=lambda x: weight_key(x[0], x[2], x[4]), reverse=True)

    if len(groups2) <= 128:
        return [(g[0], g[1], g[2], g[3]) for g in groups2]

    # Stage 3: Group by frequency only -> single row per freq
    freq_counts = Counter()
    for _, __, freq, ___ in rows:
        freq_counts[freq] += 1
    groups3 = []
    for freq, cnt in freq_counts.items():
        groups3.append(("Mixed", "Various", freq, "Mixed", cnt))
    groups3.sort(key=lambda x: weight_key(x[0], x[2], x[4]), reverse=True)

    if len(groups3) <= 128:
        return [(g[0], g[1], g[2], g[3]) for g in groups3]

    # Final fallback: trim to 128
    trimmed = groups3[:128]
    return [(g[0], g[1], g[2], g[3]) for g in trimmed]


def main():
    if len(sys.argv) < 3:
        print("Usage: condense_models.py <input_csv> <output_csv>")
        sys.exit(1)
    in_path = Path(sys.argv[1])
    out_path = Path(sys.argv[2])
    rows = read_rows(in_path)
    condensed = condense_max128(rows)
    write_rows(out_path, condensed)
    print(f"Wrote {len(condensed)} rows to {out_path}")

if __name__ == "__main__":
    main()
