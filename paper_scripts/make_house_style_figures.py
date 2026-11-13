"""Redrawn method figures in the house style of the revision (one typeface, one palette).
Writes PDF, SVG (editable text) and PNG for: integer triangular coordinates, adjacency and atlas, hash decision."""
import math
import os
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import Polygon, FancyBboxPatch, FancyArrowPatch

plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 7.5, "pdf.fonttype": 42,
                     "svg.fonttype": "none", "mathtext.fontset": "dejavusans"})
INK, LINE, GRID = "#1F2933", "#52606D", "#9AA5B1"
HALO_F, HALO_E = "#DCE9F7", "#2F6DB3"      # blue: halo component, new ring
GEO_F, GEO_E = "#FCEBD5", "#C77B2B"        # orange
SEED_F, SEED_E = "#FBE7A1", "#B88A00"      # yellow: seeds / current cell
RING_F, RING_E = "#CFE8CF", "#3F8F4F"      # green: current ring / accepted
IO_F, IO_E = "#F2F4F7", "#7B8794"
S3 = math.sqrt(3) / 2

def save(fig, name):
    os.makedirs("figs", exist_ok=True)
    for ext in ("pdf", "svg", "png"):
        fig.savefig(f"figs/{name}.{ext}", dpi=200)
    plt.close(fig)

def pt(i, j, n, A=(0.0, 0.0), B=(1.0, 0.0), C=(0.5, S3)):
    return (A[0] + (i / n) * (B[0] - A[0]) + (j / n) * (C[0] - A[0]),
            A[1] + (i / n) * (B[1] - A[1]) + (j / n) * (C[1] - A[1]))
def up(i, j, n, **k): return [pt(i, j, n, **k), pt(i + 1, j, n, **k), pt(i, j + 1, n, **k)]
def down(i, j, n, **k): return [pt(i + 1, j, n, **k), pt(i + 1, j + 1, n, **k), pt(i, j + 1, n, **k)]
def centroid(p): return (sum(x for x, _ in p) / 3, sum(y for _, y in p) / 3)
def grid(ax, n, lw=0.6, **k):
    for i in range(n):
        for j in range(n - i):
            ax.add_patch(Polygon(up(i, j, n, **k), closed=True, fc="white", ec=GRID, lw=lw, zorder=1))
            if i + j + 1 < n:
                ax.add_patch(Polygon(down(i, j, n, **k), closed=True, fc="white", ec=GRID, lw=lw, zorder=1))
def arrow(ax, p, q, color=LINE, lw=1.1, ls="-", ms=8):
    ax.add_patch(FancyArrowPatch(p, q, arrowstyle="-|>", mutation_scale=ms, lw=lw, color=color, linestyle=ls,
                                 zorder=6, shrinkA=0, shrinkB=0))

# ------------------------------------------------------------ 1. integer triangular coordinates
fig = plt.figure(figsize=(3.9, 3.35)); ax = fig.add_axes([0, 0, 1, 1]); ax.set_aspect("equal"); ax.axis("off")
ax.set_xlim(-0.2, 1.2); ax.set_ylim(-0.24, 0.9626)
n = 4
grid(ax, n)
ax.add_patch(Polygon([(0, 0), (1, 0), (0.5, S3)], closed=True, fc="none", ec=INK, lw=1.2, zorder=3))
U, D = up(1, 1, n), down(1, 1, n)
ax.add_patch(Polygon(U, closed=True, fc=RING_F, ec=RING_E, lw=1.4, zorder=4))
ax.add_patch(Polygon(D, closed=True, fc=HALO_F, ec=HALO_E, lw=1.4, zorder=4))
cu, cd = centroid(U), centroid(D)
ax.text(cu[0], cu[1] + 0.012, "$U(i,j)$", ha="center", va="center", fontsize=7.5, fontweight="bold", color=INK, zorder=7)
ax.text(cu[0], cu[1] - 0.045, "$o=0$", ha="center", va="center", fontsize=6.5, color=INK, zorder=7)
ax.text(cd[0], cd[1] + 0.035, "$D(i,j)$", ha="center", va="center", fontsize=7.5, fontweight="bold", color=INK, zorder=7)
ax.text(cd[0], cd[1] - 0.02, "$o=1$", ha="center", va="center", fontsize=6.5, color=INK, zorder=7)
a = pt(1, 1, n); ax.plot([a[0]], [a[1]], "o", ms=3.5, color=INK, zorder=8)
ax.annotate("anchor $(i,j)=(1,1)$", xy=a, xytext=(0.375, -0.045), fontsize=6.8, color=INK, ha="center", va="center",
            arrowprops=dict(arrowstyle="-", color=LINE, lw=0.7, shrinkA=0, shrinkB=2), zorder=8)
for name, p, dx, dy in (("$A$", (0, 0), -0.035, -0.035), ("$B$", (1, 0), 0.035, -0.035), ("$C$", (0.5, S3), 0, 0.045)):
    ax.text(p[0] + dx, p[1] + dy, name, ha="center", va="center", fontsize=8.5, fontweight="bold", color=INK)
arrow(ax, (0.03, -0.1), (0.97, -0.1), color=GEO_E, lw=1.3); ax.text(0.5, -0.155, "$i$  (from $A$ toward $B$),  $0,\\ldots,N-1$", ha="center", va="center", fontsize=7, color=GEO_E)
off = (-0.07, 0.04)
arrow(ax, (0.03 * 0.5 + off[0], 0.03 * S3 + off[1]), (0.97 * 0.5 + off[0], 0.97 * S3 + off[1]), color=HALO_E, lw=1.3)
ax.text(0.5 * 0.5 + off[0] - 0.05, 0.5 * S3 + off[1] + 0.029, "$j$  (from $A$ toward $C$)", ha="center", va="center", fontsize=7, color=HALO_E, rotation=60, rotation_mode="anchor")
ax.text(1.18, 0.9, "level $r=2$\n$N=2^r=4$", ha="right", va="top", fontsize=7, color=INK, linespacing=1.35)
save(fig, "Figure_Integer_Triangular_Coordinates")

# ------------------------------------------------------------ 2. adjacency inside a face and across a base mesh edge
fig = plt.figure(figsize=(7.2, 2.95)); ax = fig.add_axes([0, 0, 1, 1]); ax.set_aspect("equal"); ax.axis("off")
ax.set_xlim(-0.3, 3.3); ax.set_ylim(-0.33, 1.145)
ax.text(0.5, 1.09, "(a) Inside a face: arithmetic only", ha="center", va="center", fontsize=8, fontweight="bold", color=INK)
grid(ax, n)
ax.add_patch(Polygon([(0, 0), (1, 0), (0.5, S3)], closed=True, fc="none", ec=INK, lw=1.2, zorder=3))
ax.add_patch(Polygon(up(1, 1, n), closed=True, fc=SEED_F, ec=SEED_E, lw=1.3, zorder=4))
for (i, j), lab in (((1, 1), "$D(i,j)$"), ((1, 0), "$D(i,j{-}1)$"), ((0, 1), "$D(i{-}1,j)$")):
    P = down(i, j, n); ax.add_patch(Polygon(P, closed=True, fc=HALO_F, ec=HALO_E, lw=1.1, zorder=4))
c = centroid(up(1, 1, n)); ax.text(c[0], c[1] - 0.012, "$U(i,j)$", ha="center", va="center", fontsize=6.6, fontweight="bold", color=INK, zorder=7)
for (i, j), lab, xy in (((1, 1), "$D(i,j)$", (0.93, 0.56)), ((1, 0), "$D(i,j{-}1)$", (1.1, 0.1)), ((0, 1), "$D(i{-}1,j)$", (0.0, 0.5))):
    c = centroid(down(i, j, n))
    ax.annotate(lab, xy=c, xytext=xy, fontsize=6.8, color=HALO_E, ha="center", va="center",
                arrowprops=dict(arrowstyle="-", color=HALO_E, lw=0.7, shrinkA=1, shrinkB=1), zorder=8)
ax.text(0.5, -0.12, "three fixed offsets of $(i,j)$ and a change of orientation;\nno table is read", ha="center", va="center", fontsize=6.9, color=INK, linespacing=1.3)

x0 = 1.36
A, B, C = (x0, 0.0), (x0 + 1, 0.0), (x0 + 0.5, S3); A2 = (x0 + 1.5, S3)
ax.text(x0 + 0.75, 1.09, "(b) Across a base mesh edge: one atlas record", ha="center", va="center", fontsize=8, fontweight="bold", color=INK)
grid(ax, n, A=A, B=B, C=C); grid(ax, n, A=A2, B=C, C=B)
ax.add_patch(Polygon([A, B, C], closed=True, fc="none", ec=INK, lw=1.2, zorder=3))
ax.add_patch(Polygon([A2, C, B], closed=True, fc="none", ec=INK, lw=1.2, zorder=3))
ax.plot([B[0], C[0]], [B[1], C[1]], color=GEO_E, lw=2.4, zorder=5, solid_capstyle="round")
t = 1
src = up(n - 1 - t, t, n, A=A, B=B, C=C); dst = up(n - 1 - t, t, n, A=A2, B=C, C=B)
ax.add_patch(Polygon(src, closed=True, fc=SEED_F, ec=SEED_E, lw=1.3, zorder=4))
ax.add_patch(Polygon(dst, closed=True, fc=HALO_F, ec=HALO_E, lw=1.3, zorder=4))
cs, cdst = centroid(src), centroid(dst)
ax.text(cs[0], cs[1], "$t$", ha="center", va="center", fontsize=7.5, fontweight="bold", color=INK, zorder=7)
ax.text(cdst[0], cdst[1], "$t'$", ha="center", va="center", fontsize=7.5, fontweight="bold", color=INK, zorder=7)
ax.text(x0 + 0.33, 0.2, "face $f$", ha="center", va="center", fontsize=7.5, color=INK, zorder=7,
        bbox=dict(boxstyle="round,pad=0.15", fc="white", ec="none", alpha=0.85))
ax.text(x0 + 1.17, 0.67, "face $g$", ha="center", va="center", fontsize=7.5, color=INK, zorder=7,
        bbox=dict(boxstyle="round,pad=0.15", fc="white", ec="none", alpha=0.85))
ax.annotate("shared base mesh edge\n(edge $e$ of $f$, edge $e'$ of $g$)", xy=((B[0] + C[0]) / 2 - 0.19, (B[1] + C[1]) / 2 + 0.33),
            xytext=(x0 + 0.02, 0.9), fontsize=6.8, color=GEO_E, ha="center", va="center", linespacing=1.3,
            arrowprops=dict(arrowstyle="-", color=GEO_E, lw=0.7, shrinkA=1, shrinkB=1), zorder=8)
bx, bw = 2.68, 0.58
ax.add_patch(FancyBboxPatch((bx, 0.02), bw, 0.5, boxstyle="round,pad=0,rounding_size=0.03", fc=GEO_F, ec=GEO_E, lw=1.0, zorder=2))
ax.text(bx + bw / 2, 0.44, "atlas record", ha="center", va="center", fontsize=7.2, fontweight="bold", color=INK, zorder=7)
ax.text(bx + bw / 2, 0.32, "$T(f,e)=(g,e',\\rho)$", ha="center", va="center", fontsize=7, color=INK, zorder=7)
ax.text(bx + bw / 2, 0.19, "$t'=t$  if $\\rho=0$", ha="center", va="center", fontsize=6.8, color=INK, zorder=7)
ax.text(bx + bw / 2, 0.09, "$t'=N-1-t$  if $\\rho=1$", ha="center", va="center", fontsize=6.8, color=INK, zorder=7)
ax.text(x0 + 0.75, -0.12, "$t$, $t'$: positions of the two cells along the shared edge;\n$3F$ records in total, the same at every level", ha="center", va="center", fontsize=6.9, color=INK, linespacing=1.3)
ax.add_patch(Polygon([(1.02, -0.30), (1.08, -0.30), (1.05, -0.248)], closed=True, fc=SEED_F, ec=SEED_E, lw=0.9)); ax.text(1.11, -0.278, "frontier cell", ha="left", va="center", fontsize=6.8, color=INK)
ax.add_patch(Polygon([(1.56, -0.30), (1.62, -0.30), (1.59, -0.248)], closed=True, fc=HALO_F, ec=HALO_E, lw=0.9)); ax.text(1.65, -0.278, "adjacent cell", ha="left", va="center", fontsize=6.8, color=INK)
save(fig, "Figure_Adjacency_and_Atlas")

# ------------------------------------------------------------ 3. hash decision
fig = plt.figure(figsize=(7.2, 2.75)); ax = fig.add_axes([0, 0, 1, 1]); ax.set_xlim(0, 100); ax.set_ylim(0, 38.2); ax.axis("off")
def box(x, y, w, h, fc, ec, lw=1.0, r=0.9):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle=f"round,pad=0,rounding_size={r}", fc=fc, ec=ec, lw=lw, zorder=2))
def label(x, y, s, size=7.0, weight="normal", color=INK):
    ax.text(x, y, s, fontsize=size, fontweight=weight, color=color, ha="center", va="center", zorder=5, linespacing=1.3)
yT, hT = 17.5, 13.0; ym = yT + hT / 2
# frontier cell with three neighbours (small triangle drawing)
tx, ty, s_ = 2.6, 19.6, 10.0
T = lambda u, v: (tx + s_ * (u + 0.5 * v), ty + s_ * S3 * v * (38.2 / 100) * (7.2 / 2.75))
big = [T(0, 0), T(1, 0), T(0, 1)]
mid = [T(0.5, 0), T(0.5, 0.5), T(0, 0.5)]
ax.add_patch(Polygon(big, closed=True, fc=HALO_F, ec=HALO_E, lw=1.0, zorder=2))
ax.add_patch(Polygon(mid, closed=True, fc=SEED_F, ec=SEED_E, lw=1.0, zorder=3))
for P in ([T(0, 0), T(0.5, 0), T(0, 0.5)], [T(0.5, 0), T(1, 0), T(0.5, 0.5)], [T(0, 0.5), T(0.5, 0.5), T(0, 1)]):
    ax.add_patch(Polygon(P, closed=True, fc="none", ec=HALO_E, lw=0.8, zorder=3))
cm = centroid(mid); label(cm[0], cm[1], "$c$", 7.5, "bold")
cx = centroid([T(0.5, 0), T(1, 0), T(0.5, 0.5)]); label(cx[0], cx[1], "$x$", 7.5, "bold")
label(tx + s_ / 2, 13.6, "frontier cell\n$c\\in H_{k-1}$ and its\nthree adjacent cells", 6.6)
# boxes
box(17.5, yT, 14.0, hT, IO_F, IO_E); label(24.5, ym + 3.3, "Candidate", 7.3, "bold"); label(24.5, ym - 1.6, "compact index $x$\nof one adjacent cell", 6.7)
box(34.5, yT, 15.0, hT, HALO_F, HALO_E); label(42.0, ym + 3.3, "Mix and address", 7.3, "bold"); label(42.0, ym - 1.6, "slot $p=m(x)$ & $(M-1)$\n(64 bit mixing)", 6.7)
box(52.5, yT, 16.5, hT, HALO_F, HALO_E, lw=1.3); label(60.75, ym + 3.3, "Atomic claim", 7.3, "bold"); label(60.75, ym - 1.6, "replace $E$ by $x$ in slot $p$\nonly if the slot holds $E$", 6.7)
arrow(ax, (cx[0] + 2.0, ym), (17.5, ym)); arrow(ax, (31.5, ym), (34.5, ym)); arrow(ax, (49.5, ym), (52.5, ym))
# outcomes
xo, wo, ho = 74.5, 24.8, 8.4
box(xo, 28.4, wo, ho, RING_F, RING_E); label(xo + wo / 2, 34.3, "slot held $E$: first discovery", 7.0, "bold"); label(xo + wo / 2, 30.7, "append $x$ to ring $H_k$", 6.7)
box(xo, 17.6, wo, ho, IO_F, IO_E); label(xo + wo / 2, 23.5, "slot holds $x$: already present", 7.0, "bold"); label(xo + wo / 2, 19.9, "do not append", 6.7)
box(xo, 6.8, wo, ho, GEO_F, GEO_E); label(xo + wo / 2, 12.7, "slot holds $y\\neq x$: collision", 7.0, "bold"); label(xo + wo / 2, 9.1, "try the next slot", 6.7)
arrow(ax, (69.0, ym + 3.0), (xo, 32.6)); arrow(ax, (69.0, ym), (xo, 21.8)); arrow(ax, (69.0, ym - 3.0), (xo, 11.0))
# retry loop
ax.plot([xo + wo / 2, xo + wo / 2, 60.75], [6.8, 3.4, 3.4], color=GEO_E, lw=1.1, ls=(0, (3, 2)), zorder=4)
arrow(ax, (60.75, 3.4), (60.75, yT), color=GEO_E)
label(73.5, 1.5, "$p\\leftarrow(p+1)$ & $(M-1)$", 6.6, color=GEO_E)
label(26.0, 5.2, "$E$: empty marker (all bits set)      $M$: table capacity, a power of two\n$x$, $y$: compact integer triangular indices", 6.6, color=LINE)
save(fig, "Figure_Hash_Decision")
print("ok")
