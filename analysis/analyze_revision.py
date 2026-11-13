#!/usr/bin/env python3
"""Tables and numbers of the revised manuscript from results_rev/ (revision 2026-11).

Usage:  python analysis_opt/analyze_revision.py [results_rev] [--platform NAME]
Reads   A_session<k>_matrix.csv, A_session<k>_calls.csv, D_controlled_variants.csv,
        B_representation.csv, C_h3_context.csv, E_buffer_decision_margins.csv, F_global_validation.csv
Writes  REVISION_NUMBERS.md, numbers.json and tex/*.tex in the same folder.
No measurement is selected or removed: every session is reported as it was recorded.
"""
import csv, glob, json, os, random, statistics, sys

args = [a for a in sys.argv[1:] if not a.startswith('--')]
ROOT = args[0] if args else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'results_rev')
PLATFORM = sys.argv[sys.argv.index('--platform') + 1] if '--platform' in sys.argv else ''
TEX = os.path.join(ROOT, 'tex'); os.makedirs(TEX, exist_ok=True)
FLAT, ATOMIC, GPU = 'cpu_flat_hash_frontier', 'cpu_atomic_hash_frontier', 'cuda_resident_hash_frontier'
CLASSES = [('up to 3\\,000', 0, 3000), ('3\\,000 to 40\\,000', 3000, 40000), ('40\\,000 to 120\\,000', 40000, 120000),
           ('120\\,000 to 400\\,000', 120000, 400000), ('0.4 to 1.2 million', 400000, 1200000),
           ('1.2 to 4 million', 1200000, 4000000), ('4 to 11 million', 4000000, 11000000)]
REPRESENTATIVE = [('buildings', 28, 8, 100000), ('streets', 20, 8, 99657), ('buildings', 28, 8, 10000),
                  ('boundary', 28, 8, 19717), ('buildings', 12, 4, 1914), ('boundary', 12, 1, 287)]
med = statistics.median
out = []
numbers = {'platform': PLATFORM}
def say(s=''): out.append(s); print(s)
def read(path):
    with open(path, newline='', encoding='utf-8-sig') as f: rows = list(csv.DictReader(f))
    for r in rows:   # every CUDA variant reports its own backend name; they are one implementation family here
        if r.get('backend', '').startswith('cuda'): r['backend'] = GPU
    return rows
def fmt(x):
    if x >= 100: return f'{x:.0f}'
    if x >= 10: return f'{x:.1f}'
    if x >= 1: return f'{x:.2f}'
    return f'{x:.3g}'
def key(r): return (r['scenario'], int(r['level_or_resolution']), int(r['hops']), int(r['input_cells']))

# ------------------------------------------------------------------ A
sessions = {}
for path in sorted(glob.glob(os.path.join(ROOT, 'A_session*_matrix.csv'))):
    s = int(os.path.basename(path).split('session')[1].split('_')[0])
    table = {}
    for r in read(path):
        if r.get('timer', 'engine') != 'wall': continue
        table.setdefault(key(r), {})[(r['backend'], r['delivery'])] = r
    sessions[s] = table

def config_rows(table, delivery):
    rows = []
    for k, d in sorted(table.items()):
        if not all((b, delivery) in d for b in (FLAT, ATOMIC, GPU)): continue
        flat, atom, gpu = (float(d[(b, delivery)]['median_ms']) for b in (FLAT, ATOMIC, GPU))
        g = d[(GPU, delivery)]
        spread = max(float(d[(b, delivery)]['max_ms']) / max(1e-12, float(d[(b, delivery)]['min_ms'])) for b in (FLAT, ATOMIC, GPU))
        mins = [float(d[(b, delivery)]['min_ms']) for b in (FLAT, ATOMIC, GPU)]
        rows.append(dict(key=k, out=int(g['output_cells']), flat=flat, atomic=atom, gpu=gpu, best=min(flat, atom),
                         ratio_min=min(mins[0], mins[1]) / mins[2], atomic_spread=float(d[(ATOMIC, delivery)]['p95_ms']) / atom,
                         gpu_spread=float(d[(GPU, delivery)]['p95_ms']) / gpu, flat_spread=float(d[(FLAT, delivery)]['p95_ms']) / flat,
                         best_name='serial' if flat <= atom else 'threads', ratio=min(flat, atom) / gpu, spread=spread,
                         refs=int(g['candidate_references']), mapping=float(g['mapping_ms']), peak=int(g['peak_memory_bytes'])))
    return rows

def summary(rows):
    r = [x['ratio'] for x in rows]
    return dict(configs=len(rows), gpu_faster=sum(x > 1 for x in r), median_ratio=med(r), min_ratio=min(r), max_ratio=max(r))

def rule_ok(row, threshold):
    scenario, level, hops, seeds = row['key']
    use_gpu = level >= 20 and seeds * 3 * hops * (hops + 1) // 2 >= threshold
    return use_gpu == (row['ratio'] > 1)

for s, table in sessions.items():
    say(f'\n## A. Session {s}: wall clock around the complete call, {len(table)} configurations' + (f' ({PLATFORM})' if PLATFORM else ''))
    numbers[f'A_session{s}'] = {}
    for delivery in ('fresh', 'reused'):
        rows = config_rows(table, delivery)
        if not rows: continue
        sm = summary(rows); numbers[f'A_session{s}'][delivery] = sm
        say(f'\n### Delivery: {delivery}  (GPU faster in {sm["gpu_faster"]} of {sm["configs"]}; median ratio {sm["median_ratio"]:.2f}; range {sm["min_ratio"]:.2f} to {sm["max_ratio"]:.2f})')
        say('| output cells | conf. | serial flat | threads flat | GPU | ratio median (range) | GPU faster |')
        say('|---|---|---|---|---|---|---|')
        for name, lo, hi in CLASSES:
            g = [x for x in rows if lo < x['out'] <= hi or (lo == 0 and x['out'] <= hi)]
            if not g: continue
            rr = [x['ratio'] for x in g]
            say(f'| {name.replace(chr(92)+",", ",")} | {len(g)} | {fmt(med(x["flat"] for x in g))} | {fmt(med(x["atomic"] for x in g))} | {fmt(med(x["gpu"] for x in g))} | {med(rr):.2f} ({min(rr):.2f} to {max(rr):.2f}) | {sum(v > 1 for v in rr)} of {len(g)} |')
        for label, sel in (('level', 1), ('hops', 2)):
            vals = sorted(set(x['key'][sel] for x in rows))
            say(f'by {label}: ' + ', '.join(f'{v}: {med(x["ratio"] for x in rows if x["key"][sel] == v):.2f}' for v in vals))
        for scenario in sorted(set(x['key'][0] for x in rows)):
            g = [x['ratio'] for x in rows if x['key'][0] == scenario]
            say(f'{scenario}: median {med(g):.2f}, range {min(g):.2f} to {max(g):.2f}, GPU faster {sum(v > 1 for v in g)} of {len(g)}')
        small = [x for x in rows if x['ratio'] <= 1]
        if small: say(f'CPU faster: {len(small)} configurations, largest output among them {max(x["out"] for x in small)} cells; smallest output with GPU faster {min((x["out"] for x in rows if x["ratio"] > 1), default=0)}')
        rm = [x['ratio_min'] for x in rows]
        say(f'robustness, ratio of the fastest calls (minimum / minimum): median {med(rm):.2f}, range {min(rm):.2f} to {max(rm):.2f}, GPU faster {sum(v > 1 for v in rm)} of {len(rm)}; '
            f'for outputs above 4 million cells: ' + ', '.join(f'{x["ratio_min"]:.2f}' for x in rows if x['out'] > 4000000))
        say(f'p95 / median of the calls, median over configurations: serial {med(x["flat_spread"] for x in rows):.2f}, threads {med(x["atomic_spread"] for x in rows):.2f}, GPU {med(x["gpu_spread"] for x in rows):.2f}')
        big = [x for x in rows if x['out'] > 3000]
        if big:
            rb = [x['ratio'] for x in big]
            say(f'configurations above 3,000 returned cells: {len(big)}, smallest output {min(x["out"] for x in big)}, ratio median {med(rb):.2f}, range {min(rb):.2f} to {max(rb):.2f}')
            numbers[f'A_session{s}'][delivery].update(above_configs=len(big), above_min_output=min(x['out'] for x in big), above_median=med(rb), above_min=min(rb), above_max=max(rb),
                small_gpu_median_ms=med(x['gpu'] for x in rows if x['out'] <= 3000), small_gpu_min_ms=min(x['gpu'] for x in rows if x['out'] <= 3000), small_gpu_max_ms=max(x['gpu'] for x in rows if x['out'] <= 3000),
                ratio_min_median=med(rm), large_ratios=[x['ratio'] for x in rows if x['out'] > 4000000], large_ratio_min=[x['ratio_min'] for x in rows if x['out'] > 4000000])
        flagged = sum(x['spread'] > 3 for x in rows)
        say(f'configurations with max/min call spread above 3 in any implementation: {flagged} (kept)')
        # routing rule
        grid = [1000, 2000, 3000, 4000, 5000, 6000, 8000, 10000, 15000, 20000, 25000, 30000, 40000, 50000]
        acc = {t: sum(rule_ok(x, t) for x in rows) for t in grid}
        best = max(acc.values()); ties = [t for t in grid if acc[t] == best]
        say(f'routing rule (level >= 20 and budget >= T): best T in {ties} with {best} of {len(rows)} correct; T=5000 gives {acc[5000]}')
        folds = []
        for held in sorted(set(x['key'][0] for x in rows)):
            train = [x for x in rows if x['key'][0] != held]; test = [x for x in rows if x['key'][0] == held]
            a = {t: sum(rule_ok(x, t) for x in train) for t in grid}; t_star = min(t for t in grid if a[t] == max(a.values()))
            folds.append((held, t_star, sum(rule_ok(x, t_star) for x in test), len(test)))
        say('held out dataset check: ' + '; '.join(f'{h}: T={t}, {c} of {n}' for h, t, c, n in folds))
        numbers[f'A_session{s}'][delivery].update(routing_best=ties, routing_5000=acc[5000], folds=folds,
            cpu_wins_max_output=max((x['out'] for x in small), default=0), flagged=flagged,
            mapping_median=med(x['mapping'] for x in rows), peak_bytes_max=max(x['peak'] for x in rows))
        # mapping inclusive view
        mi = [(x['best'] + x['mapping']) / (x['gpu'] + x['mapping']) for x in rows]
        say(f'point mapping: median {med(x["mapping"] for x in rows):.3f} ms, max {max(x["mapping"] for x in rows):.3f} ms; with mapping added to both sides the ratio has median {med(mi):.2f}, max {max(mi):.2f}')
        # latex table rows
        with open(os.path.join(TEX, f'A_s{s}_{delivery}_rows.tex'), 'w') as f:
            for name, lo, hi in CLASSES:
                g = [x for x in rows if (x['out'] <= hi if lo == 0 else lo < x['out'] <= hi)]
                if not g: continue
                rr = [x['ratio'] for x in g]
                f.write(f'{name} & {len(g)} & {fmt(med(x["flat"] for x in g))} & {fmt(med(x["atomic"] for x in g))} & {fmt(med(x["gpu"] for x in g))} & {med(rr):.2f} ({min(rr):.2f} to {max(rr):.2f}) & {sum(v > 1 for v in rr)} of {len(g)} \\\\\n')

# one table with both delivery scopes (the layout of tab:speedup-by-output)
for s, table in sessions.items():
    fr, ru = config_rows(table, 'fresh'), config_rows(table, 'reused')
    if not fr or not ru: continue
    with open(os.path.join(TEX, f'A_s{s}_combined_rows.tex'), 'w') as f:
        for name, lo, hi in CLASSES:
            sel = lambda rows: [x for x in rows if (x['out'] <= hi if lo == 0 else lo < x['out'] <= hi)]
            a, b = sel(fr), sel(ru)
            if not a: continue
            ra, rb = [x['ratio'] for x in a], [x['ratio'] for x in b]
            f.write(f'{name} & {len(a)} & {fmt(med(x["flat"] for x in a))} & {fmt(med(x["atomic"] for x in a))} & {fmt(med(x["gpu"] for x in a))} & {med(ra):.2f} ({min(ra):.2f} to {max(ra):.2f}) & '
                    f'{fmt(med(x["gpu"] for x in b))} & {med(rb):.2f} ({min(rb):.2f} to {max(rb):.2f}) & {sum(v > 1 for v in ra)} / {sum(v > 1 for v in rb)} \\\\\n')
        ra, rb = [x['ratio'] for x in fr], [x['ratio'] for x in ru]
        f.write(f'\\midrule\nall & {len(fr)} & -- & -- & -- & {med(ra):.2f} ({min(ra):.2f} to {max(ra):.2f}) & -- & {med(rb):.2f} ({min(rb):.2f} to {max(rb):.2f}) & {sum(v > 1 for v in ra)} / {sum(v > 1 for v in rb)} \\\\\n')

# representative queries with a bootstrap interval of the ratio of medians
def bootstrap(a, b, n=10000, seed=20261112):
    rng = random.Random(seed); r = []
    for _ in range(n):
        r.append(med(rng.choices(a, k=len(a))) / med(rng.choices(b, k=len(b))))
    r.sort(); return r[int(0.025 * n)], r[int(0.975 * n)]
for s in sessions:
    path = os.path.join(ROOT, f'A_session{s}_calls.csv')
    if not os.path.exists(path): continue
    calls = {}
    for r in read(path):
        calls.setdefault((r['scenario'], int(r['level']), int(r['hops']), int(r['input_cells']), r['backend'], r['delivery']), []).append(float(r['wall_ms']))
    say(f'\n### Session {s}: representative queries (median of 30 calls, ms; ratio = best CPU / GPU with 95% bootstrap interval)')
    say('| query | delivery | output | serial flat | threads flat | GPU | ratio [95% interval] |'); say('|---|---|---|---|---|---|---|')
    numbers[f'A_session{s}']['representative'] = []
    for (sc, lv, hp, seeds) in REPRESENTATIVE:
        for delivery in ('fresh', 'reused'):
            try:
                f_, a_, g_ = (calls[(sc, lv, hp, seeds, b, delivery)] for b in (FLAT, ATOMIC, GPU))
            except KeyError: continue
            best = f_ if med(f_) <= med(a_) else a_
            lo, hi = bootstrap(best, g_)
            outc = sessions[s][(sc, lv, hp, seeds)][(GPU, delivery)]['output_cells']
            say(f'| {sc} L{lv} {seeds} seeds {hp} hops | {delivery} | {outc} | {fmt(med(f_))} | {fmt(med(a_))} | {fmt(med(g_))} | {med(best)/med(g_):.2f} [{lo:.2f}, {hi:.2f}] |')
            numbers[f'A_session{s}']['representative'].append(dict(query=f'{sc}_L{lv}_h{hp}_n{seeds}', delivery=delivery, output=int(outc),
                serial=med(f_), threads=med(a_), gpu=med(g_), ratio=med(best)/med(g_), lo=lo, hi=hi))

# ------------------------------------------------------------------ D
path = os.path.join(ROOT, 'D_controlled_variants.csv')
if os.path.exists(path):
    rows = [r for r in read(path) if r['backend'] == GPU]
    ref = {(key(r), r['delivery']): float(r['median_ms']) for r in rows if r['variant'] == 'reference'}
    say('\n## D. Controlled GPU variants (ratio of the variant median to the reference median of the same session, query and delivery)')
    order = []
    for r in rows:
        if r['variant'] not in order: order.append(r['variant'])
    configs = sorted(set(key(r) for r in rows), key=lambda k: -max(int(r['output_cells']) for r in rows if key(r) == k))
    numbers['D'] = {}
    for delivery in ('fresh', 'reused'):
        say(f'\n### Delivery: {delivery}')
        say('| variant | ' + ' | '.join(f'{k[0]} L{k[1]} {k[3]} seeds {k[2]} hops' for k in configs) + ' |'); say('|---|' + '---|' * len(configs))
        say('| reference median (ms) | ' + ' | '.join(fmt(ref[(k, delivery)]) if (k, delivery) in ref else '--' for k in configs) + ' |')
        for v in order:
            if v == 'reference': continue
            cells = []
            for k in configs:
                m = [float(r['median_ms']) for r in rows if r['variant'] == v and key(r) == k and r['delivery'] == delivery]
                cells.append(f'{m[0] / ref[(k, delivery)]:.2f}' if m and (k, delivery) in ref else '--')
            say(f'| {v} | ' + ' | '.join(cells) + ' |'); numbers['D'][f'{v}_{delivery}'] = cells

# ------------------------------------------------------------------ B
path = os.path.join(ROOT, 'B_representation.csv')
if os.path.exists(path):
    rows = read(path)
    say('\n## B. Representation on one CPU thread (median of 30 calls, ms; ratio to the compact index)')
    numbers['B'] = []
    groups = {}
    for r in rows:
        table = 'auto' if 'table=auto' in r['notes'] else 'tight'
        groups.setdefault((table, r['scenario'], int(r['level_or_resolution'])), []).append(r)
    for (table, scenario, level), g in sorted(groups.items()):
        base = next(float(r['median_ms']) for r in g if r['variant'] == 'compact')
        say(f'\n{scenario} level {level}, discovered set {table}, seeds {g[0]["unique_input_cells"]}, output {g[0]["output_cells"]}:')
        for r in g:
            say(f'  {r["variant"]:13s} {float(r["median_ms"]):10.3f} ms  ratio {float(r["median_ms"]) / base:5.2f}  adjacency storage {int(r["peak_memory_bytes"]):,} bytes')
            numbers['B'].append(dict(table=table, scenario=scenario, level=level, variant=r['variant'], median_ms=float(r['median_ms']),
                                     ratio=float(r['median_ms']) / base, adjacency_bytes=int(r['peak_memory_bytes']), output=int(r['output_cells'])))

# ------------------------------------------------------------------ C, E, F
for name, title in (('C_h3_context.csv', 'C. H3 context (one thread)'), ('E_buffer_decision_margins.csv', 'E. Buffer decision margins'),):
    path = os.path.join(ROOT, name)
    if not os.path.exists(path): continue
    say(f'\n## {title}')
    for r in read(path):
        if name.startswith('C'):
            say(f'  {r["backend"]:28s} level/res {r["level_or_resolution"]:>2} depth {r["hops"]} seeds {r["unique_input_cells"]:>6} output {r["output_cells"]:>9} median {float(r["median_ms"]):9.3f} ms  {1e6 * float(r["median_ms"]) / int(r["output_cells"]):8.1f} ns per cell')
        else:
            say('  ' + ', '.join(f'{k}={v}' for k, v in r.items()))
path = os.path.join(ROOT, 'F_global_validation.csv')
if os.path.exists(path):
    rows = read(path)
    ring = [r for r in rows if r['scenario'] in ('edges', 'vertices')]
    impl = sorted(set(r['implementation'] for r in ring))
    cases = len(set((r['scenario'], r['level'], r['hops']) for r in ring))
    cross = sum(int(r['base_edge_crossings']) for r in ring if r['implementation'] == impl[0])
    say(f'\n## F. Global ring validation: {cases} cases x {len(impl)} implementations = {len(ring)} comparisons, all {"PASS" if all(r["status"] == "PASS" for r in rows) else "NOT ALL PASS"}; '
        f'{sum(int(r["rings"]) for r in ring)} ring comparisons; {cross} base mesh edge crossings in the reference; '
        f'capacity error then retry checks: {sum(r["scenario"] == "capacity_error_then_retry" for r in rows)}')
    say('implementations: ' + ', '.join(impl))
    numbers['F'] = dict(cases=cases, implementations=impl, comparisons=len(ring), crossings=cross)

with open(os.path.join(ROOT, 'REVISION_NUMBERS.md'), 'w', encoding='utf-8') as f: f.write('# Revision numbers' + (f' ({PLATFORM})' if PLATFORM else '') + '\n' + '\n'.join(out) + '\n')
with open(os.path.join(ROOT, 'numbers.json'), 'w') as f: json.dump(numbers, f, indent=1, default=str)
