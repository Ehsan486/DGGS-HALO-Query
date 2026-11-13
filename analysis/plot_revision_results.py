#!/usr/bin/env python3
"""Main results figure of the revised manuscript (revision 2026-11).

Usage:  python analysis_opt/plot_revision_results.py [results_rev] [--session 1]
Reads   A_session<k>_matrix.csv (wall clock, both delivery scopes)
Writes  figures/Figure_Halo_Results.{pdf,svg,png}
Panel (a): latency of the three implementations against the number of returned cells (fresh container).
Panel (b): faster CPU implementation divided by GPU implementation, for both delivery scopes.
No configuration is removed.
"""
import csv, os, statistics, sys
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

args = [a for a in sys.argv[1:] if not a.startswith('--')]
ROOT = args[0] if args else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'results_rev')
S = int(sys.argv[sys.argv.index('--session') + 1]) if '--session' in sys.argv else 1
FLAT, ATOMIC = 'cpu_flat_hash_frontier', 'cpu_atomic_hash_frontier'
INK, GRID = '#1F2933', '#D5DAE0'
C_SERIAL, C_THREADS, C_GPU, C_GPU_LIGHT = '#7B8794', '#C77B2B', '#2F6DB3', '#DCE9F7'

table = {}
with open(os.path.join(ROOT, f'A_session{S}_matrix.csv'), newline='', encoding='utf-8-sig') as f:
    for r in csv.DictReader(f):
        if r.get('timer') != 'wall': continue
        b = 'gpu' if r['backend'].startswith('cuda') else r['backend']
        k = (r['scenario'], int(r['level_or_resolution']), int(r['hops']), int(r['input_cells']))
        table.setdefault(k, {})[(b, r['delivery'])] = (int(r['output_cells']), float(r['median_ms']))
rows = {d: [] for d in ('fresh', 'reused')}
for k, v in table.items():
    for d in rows:
        if all((b, d) in v for b in (FLAT, ATOMIC, 'gpu')):
            rows[d].append(dict(out=v[('gpu', d)][0], flat=v[(FLAT, d)][1], atomic=v[(ATOMIC, d)][1], gpu=v[('gpu', d)][1]))
for d in rows: rows[d].sort(key=lambda x: x['out'])

plt.rcParams.update({'font.family': 'DejaVu Sans', 'font.size': 8, 'axes.labelsize': 8, 'axes.titlesize': 8.5,
                     'xtick.labelsize': 7.5, 'ytick.labelsize': 7.5, 'legend.fontsize': 7.5, 'axes.linewidth': 0.6,
                     'axes.edgecolor': INK, 'text.color': INK, 'axes.labelcolor': INK, 'xtick.color': INK, 'ytick.color': INK,
                     'svg.fonttype': 'none', 'pdf.fonttype': 42})
fig, (a, b) = plt.subplots(1, 2, figsize=(7.2, 3.25), layout='constrained')

fr = rows['fresh']
x = [r['out'] for r in fr]
a.scatter(x, [r['flat'] for r in fr], s=13, marker='s', facecolor='white', edgecolor=C_SERIAL, linewidth=0.9, label='CPU, one thread', zorder=3)
a.scatter(x, [r['atomic'] for r in fr], s=15, marker='^', facecolor='white', edgecolor=C_THREADS, linewidth=0.9, label='CPU, 20 threads', zorder=3)
a.scatter(x, [r['gpu'] for r in fr], s=13, marker='o', facecolor=C_GPU, edgecolor=C_GPU, linewidth=0.6, label='GPU', zorder=4)
a.set_xscale('log'); a.set_yscale('log')
a.set_xlabel('Returned cells'); a.set_ylabel('Latency of one call (ms)')
a.set_title('(a) Latency, fresh container', loc='left')
a.grid(True, which='major', color=GRID, linewidth=0.5); a.set_axisbelow(True)
a.legend(frameon=False, loc='upper left', handletextpad=0.3, borderaxespad=0.3)

for d, kw, lab in (('fresh', dict(facecolor=C_GPU, edgecolor=C_GPU), 'Fresh container'),
                   ('reused', dict(facecolor='white', edgecolor=C_GPU), 'Reused buffer')):
    rr = rows[d]
    b.scatter([r['out'] for r in rr], [min(r['flat'], r['atomic']) / r['gpu'] for r in rr], s=14, marker='o', linewidth=0.9, label=lab, zorder=3, **kw)
b.axhline(1.0, color=INK, linewidth=0.7, linestyle=(0, (4, 3)), zorder=2)
b.set_xscale('log'); b.set_yscale('log')
b.set_yticks([0.1, 0.2, 0.5, 1, 2, 5, 10]); b.set_yticklabels(['0.1', '0.2', '0.5', '1', '2', '5', '10'])
b.minorticks_off(); b.tick_params(axis='x', which='minor', bottom=True)
b.set_xlabel('Returned cells'); b.set_ylabel('Faster CPU time / GPU time')
b.set_title('(b) Ratio in the same process', loc='left')
b.grid(True, which='major', color=GRID, linewidth=0.5); b.set_axisbelow(True)
xmin = min(x) * 0.6
b.text(9.5e3, 1.12, 'GPU faster', fontsize=7, va='bottom', ha='center')
b.text(9.5e3, 0.89, 'CPU faster', fontsize=7, va='top', ha='center')
b.legend(frameon=False, loc='lower right', handletextpad=0.3, borderaxespad=0.3)
for ax in (a, b):
    ax.set_xlim(xmin, max(x) * 1.8)
    for s in ('top', 'right'): ax.spines[s].set_visible(False)

out = os.path.join(ROOT, 'figures'); os.makedirs(out, exist_ok=True)
for ext in ('pdf', 'svg', 'png'):
    fig.savefig(os.path.join(out, f'Figure_Halo_Results.{ext}'), dpi=300)
print('configurations plotted:', {d: len(v) for d, v in rows.items()}, '->', out)
