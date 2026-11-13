"""End to end workflow figure (reviewer 3, comment 1). Writes PDF and SVG with editable text."""
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch

plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 7.5,
                     "pdf.fonttype": 42, "svg.fonttype": "none", "mathtext.fontset": "dejavusans"})

INK = "#1F2933"; LINE = "#52606D"
GEO_F, GEO_E = "#FCEBD5", "#C77B2B"      # geometric stages (CPU)
HALO_F, HALO_E = "#DCE9F7", "#2F6DB3"    # halo component (CPU or GPU)
IO_F, IO_E = "#F2F4F7", "#7B8794"        # inputs and outputs
IN_F = "#FFFFFF"

fig = plt.figure(figsize=(7.2, 4.0))
ax = fig.add_axes([0, 0, 1, 1]); ax.set_xlim(0, 100); ax.set_ylim(0, 55.56); ax.axis("off")

def box(x, y, w, h, fc, ec, lw=1.0, r=0.9, z=2):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle=f"round,pad=0,rounding_size={r}", fc=fc, ec=ec, lw=lw, zorder=z))

def text(x, y, s, size=7.2, weight="normal", color=INK, ha="center", va="center"):
    ax.text(x, y, s, fontsize=size, fontweight=weight, color=color, ha=ha, va=va, zorder=5, linespacing=1.3)

def arrow(p, q, color=LINE, lw=1.1):
    ax.add_patch(FancyArrowPatch(p, q, arrowstyle="-|>", mutation_scale=8, lw=lw, color=color, zorder=4, shrinkA=0, shrinkB=0))

def stage(x, w, y, h, num, title, body, fc, ec):
    box(x, y, w, h, fc, ec)
    text(x + w / 2, y + h - 2.3, num, 7.0, "bold", color=ec)
    text(x + w / 2, y + h - 5.4, title, 7.4, "bold")
    text(x + w / 2, y + (h - 7.2) / 2 + 0.3, body, 7.0)

def io(x, w, y, h, title, body):
    box(x, y, w, h, IO_F, IO_E)
    text(x + w / 2, y + h - 2.8, title, 7.4, "bold")
    text(x + w / 2, y + (h - 4.6) / 2 + 0.2, body, 7.0)

X0, W0 = 0.8, 14.2      # inputs
X1, W1 = 18.0, 14.5     # stage 1
X2, W2 = 35.5, 28.0     # stage 2
X3, W3 = 66.0, 14.6     # stage 3
X4, W4 = 85.0, 14.3     # stage 4
yT, hT = 32.0, 17.0     # upper row
yB, hB = 11.5, 12.5     # lower row
ym = yT + hT / 2; yb = yB + hB / 2

io(X0, W0, yT, hT, "Buffer query", "source curve $\\mathcal{S}$\ndistance $d$ (m)\nlevel $r$")
stage(X1, W1, yT, hT, "Stage 1", "Source covering", "all cells that\ntouch $\\mathcal{S}$\n→ seed set $P$", GEO_F, GEO_E)
stage(X3, W3, yT, hT, "Stage 3", "Stopping test", "whole ring $H_K$:\n$\\min\\,\\lambda(c) > d$ ?\n(or $H_K$ empty)", GEO_F, GEO_E)
stage(X4, W4, yT, hT, "Stage 4", "Distance filter", "keep $c \\in V$\nwith $\\lambda(c) \\leq d$", GEO_F, GEO_E)

# stage 2: halo query, spans both rows
box(X2, yB, W2, yT + hT - yB, HALO_F, HALO_E, lw=1.3)
text(X2 + W2 / 2, yT + hT - 2.3, "Stage 2", 7.0, "bold", color=HALO_E)
text(X2 + W2 / 2, yT + hT - 5.4, "Halo query", 7.4, "bold")
xi, wi = X2 + 1.6, W2 - 3.2
box(xi, 37.3, wi, 4.4, IN_F, HALO_E, lw=0.7, r=0.6)
text(X2 + W2 / 2, 39.5, "decode once: public → compact", 6.7)
box(xi, 18.4, wi, 17.4, IN_F, HALO_E, lw=0.7, r=0.6)
text(X2 + W2 / 2, 33.4, "for $k = 1,\\ldots,K$", 6.9, "bold")
text(X2 + W2 / 2, 30.6, "frontier = completed ring $H_{k-1}$", 6.6)
text(X2 + W2 / 2, 25.9, "three adjacent cells per frontier cell:\narithmetic inside a face,\nACM record at a base mesh edge", 6.5)
text(X2 + W2 / 2, 20.5, "first discovery in the discovered\nset $V$ → cell joins ring $H_k$", 6.5)
ax.plot([xi + 2, xi + wi - 2], [22.7, 22.7], color=HALO_E, lw=0.5, zorder=4)
box(xi, 12.6, wi, 4.4, IN_F, HALO_E, lw=0.7, r=0.6)
text(X2 + W2 / 2, 14.8, "encode once: compact → public", 6.7)

io(X0, 19.5, yB, hB, "Fixed hop halo query", "seed cells $P$\n(stored indices)\nhop depth $K$")
io(X3, W3, yB, hB, "Halo output", "rings $H_1,\\ldots,H_K$\nand ring offsets")
io(X4, W4, yB, hB, "Buffer output", "cover $Q_{r,d}$\ndepth $K_\\star$\nsurplus bound $\\eta$")

arrow((X0 + W0, ym), (X1, ym)); arrow((X1 + W1, ym), (X2, ym))
arrow((X2 + W2, ym), (X3, ym)); arrow((X3 + W3, ym), (X4, ym))
text((X3 + W3 + X4) / 2, ym + 1.5, "yes", 6.4, color=LINE)
arrow((X0 + 19.5, yb), (X2, yb)); arrow((X2 + W2, yb), (X3, yb))
arrow((X4 + W4 / 2, yT), (X4 + W4 / 2, yB + hB))
# "no" loop from stage 3 back into stage 2
xa, xb, yl2 = X3 + W3 / 2, X2 + W2 - 5.0, yT + hT + 3.2
ax.plot([xa, xa, xb], [yT + hT, yl2, yl2], color=LINE, lw=1.1, ls=(0, (3, 2)), zorder=4, solid_capstyle="butt")
arrow((xb, yl2), (xb, yT + hT))
text((xa + xb) / 2, yl2 + 1.6, "no: run the halo query again with $K \\leftarrow 2K$", 6.6, color=LINE)

# legend
yl = 4.6
box(0.8, yl - 1.3, 3.2, 2.6, HALO_F, HALO_E, r=0.4); text(4.9, yl, "halo component (CPU or GPU implementation)", 6.8, ha="left")
box(47.0, yl - 1.3, 3.2, 2.6, GEO_F, GEO_E, r=0.4); text(51.1, yl, "geometric stages (CPU)", 6.8, ha="left")
box(74.0, yl - 1.3, 3.2, 2.6, IO_F, IO_E, r=0.4); text(78.1, yl, "inputs and outputs", 6.8, ha="left")

for ext in ("pdf", "svg", "png"):
    fig.savefig(f"Figure_End_to_End_Workflow.{ext}", dpi=220)
print("ok")
