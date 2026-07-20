"""
University Linear Algebra — Knowledge Framework Map
Design: Vector Space Architecture
Hybrid: matplotlib (geometry) + Pillow (Chinese text rendering)
"""

import os
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.patches as patches
from PIL import Image, ImageDraw, ImageFont

# ─── Canvas Size ───────────────────────────────────────────────────────────────
W, H   = 36, 24          # inches
DPI    = 150
PX_W   = int(W * DPI)
PX_H   = int(H * DPI)

# ─── Colors (two namespaces: hex for matplotlib, RGBA tuples for Pillow) ────────
# Matplotlib hex palette
BG_M    = '#0D0D1A'; SURFACE_M = '#141428'; NODEBG_M  = '#090916'
ACC_M   = '#D4A72C'; TEAL_M = '#2BBFA0'; VIOLET_M = '#8B7EC8'
BORDER_M= '#2A2A45'; CONN_M = '#252538'; TEXTL_M   = '#9090B0'
TEXTS_M = '#5C6A8A'; TITLE_M = '#F0EDE6'

# Pillow RGBA palette
BG_R    = ( 13,  13,  26, 255)
SF_R    = ( 20,  20,  40, 255)
NB_R    = (  9,   9,  22, 255)
AC_R    = (212, 167,  44, 255)
TL_R    = ( 43, 191, 160, 255)
VI_R    = (139, 126, 200, 255)
BD_R    = ( 42,  42,  69, 180)
TX_R    = (144, 144, 176, 255)
TS_R    = ( 92, 106, 138, 255)
TT_R    = (240, 237, 230, 255)

# ─── Geometry ─────────────────────────────────────────────────────────────────
NODE_W  = 7.2; NODE_H = 4.8; GAP_X = 0.55
COLS    = 4
total_w = COLS * NODE_W + (COLS - 1) * GAP_X
margin_x = (W - total_w) / 2
COL_X   = [margin_x + i * (NODE_W + GAP_X) + NODE_W / 2 for i in range(COLS)]
ROW1_Y  = H - 7.0
ROW2_Y  = H - 15.5

# ─── Chapter Data ─────────────────────────────────────────────────────────────
CHAPTERS = [
    {'row':1,'col':0,'title':'行列式',         'en':'DETERMINANTS',
     'accent':AC_R,'accent_m':ACC_M,
     'subs':['定义与性质','展开定理','克莱姆法则','Vandermonde']},
    {'row':1,'col':1,'title':'矩阵与向量',     'en':'MATRICES & VECTORS',
     'accent':AC_R,'accent_m':ACC_M,
     'subs':['矩阵运算','初等变换','逆矩阵','分块矩阵','向量运算']},
    {'row':1,'col':2,'title':'向量空间',       'en':'VECTOR SPACES',
     'accent':AC_R,'accent_m':ACC_M,
     'subs':['线性相关','基与维数','坐标变换','子空间','秩']},
    {'row':1,'col':3,'title':'线性方程组',     'en':'LINEAR SYSTEMS',
     'accent':AC_R,'accent_m':ACC_M,
     'subs':['解的存在性','高斯消元','齐次与非齐次','解的结构']},
    {'row':2,'col':0,'title':'线性变换',       'en':'LINEAR TRANSFORMATIONS',
     'accent':TL_R,'accent_m':TEAL_M,
     'subs':['映射与表示','核与像','变换的矩阵','可逆变换']},
    {'row':2,'col':1,'title':'特征值与特征向量','en':'EIGENVALUES',
     'accent':VI_R,'accent_m':VIOLET_M,
     'subs':['特征多项式','相似矩阵','可对角化条件','Jordan 标准形']},
    {'row':2,'col':2,'title':'内积空间',       'en':'INNER PRODUCT SPACES',
     'accent':TL_R,'accent_m':TEAL_M,
     'subs':['内积定义','正交性','正交投影','最小二乘法']},
    {'row':2,'col':3,'title':'二次型',          'en':'QUADRATIC FORMS',
     'accent':VI_R,'accent_m':VIOLET_M,
     'subs':['标准形','正定判别','惯性定理','正交替换']},
]

# ─── Pillow Fonts ─────────────────────────────────────────────────────────────
def pfont(size_pt):
    return ImageFont.truetype(r'C:\Windows\Fonts\msyh.ttc',
                               int(size_pt * DPI / 72))

PF_TITLE = pfont(24)
PF_CHAP  = pfont(12)
PF_EN    = pfont(7.5)
PF_SUB   = pfont(7)
PF_LEG   = pfont(6.5)
PF_ROW   = pfont(10)

# ─── Pixel helpers ────────────────────────────────────────────────────────────
def ix(v): return int(v * DPI)
def iy(v): return int(v * DPI)

# ─── Round-rect helper for Pillow ─────────────────────────────────────────────
def pill_rrect(draw, cx, cy, w, h, fc, ec, lw=1):
    r = max(4, h // 5)
    draw.rounded_rectangle([cx-w//2, cy-h//2, cx+w//2, cy+h//2],
                            radius=r, fill=fc, outline=ec, width=lw)

# ─── Centered text helper ─────────────────────────────────────────────────────
def ctext(draw, cx, cy, text, font, fill):
    bb = draw.textbbox((0, 0), text, font=font)
    tw, th = bb[2]-bb[0], bb[3]-bb[1]
    draw.text((cx - tw//2, cy - th//2), text, font=font, fill=fill)

# ═══════════════════════════════════════════════════════════════════════════════
# STEP 1 — matplotlib: geometry + arrows (no text)
# ═══════════════════════════════════════════════════════════════════════════════
fig, ax = plt.subplots(figsize=(W, H), dpi=DPI)
ax.set_xlim(0, W); ax.set_ylim(0, H); ax.axis('off')
fig.patch.set_facecolor(BG_M); ax.set_facecolor(BG_M)

# Horizontal arrows (within rows)
for row_y in [ROW1_Y, ROW2_Y]:
    for ci in range(COLS - 1):
        x1 = COL_X[ci]   + NODE_W/2 + 0.1
        x2 = COL_X[ci+1] - NODE_W/2 - 0.1
        ax.annotate('',
            xy=(x2 - 0.25, row_y), xytext=(x1 + 0.25, row_y),
            arrowprops=dict(arrowstyle='->', color=CONN_M,
                            lw=0.6, mutation_scale=6),
            zorder=2)

# Vertical bridge arrows (row1 → row2)
for col_i, ac_m in [(1, TEAL_M), (2, VIOLET_M), (3, VIOLET_M)]:
    x    = COL_X[col_i]
    ytop = ROW1_Y - NODE_H/2
    ybot = ROW2_Y + NODE_H/2
    ax.plot([x, x], [ytop, ybot],
            color=ac_m, lw=0.7, ls='--', alpha=0.45, zorder=2)
    ax.annotate('',
        xy=(x, ybot + 0.35), xytext=(x, ytop - 0.35),
        arrowprops=dict(arrowstyle='->', color=ac_m,
                        lw=0.7, mutation_scale=7),
        zorder=2)

# Top accent line
ax.plot([2.5, W-2.5], [H-1.5, H-1.5],
        color=ACC_M, lw=0.5, alpha=0.35, zorder=3)

# Bottom footer line
ax.plot([2, W-2], [0.5, 0.5], color=BORDER_M, lw=0.4, zorder=3)

# Save background
bg_path = os.path.join(os.path.dirname(__file__), '_la_bg.png')
fig.savefig(bg_path, dpi=DPI, bbox_inches='tight',
            facecolor=BG_M, edgecolor='none')
plt.close(fig)

# ═══════════════════════════════════════════════════════════════════════════════
# STEP 2 — Pillow: all text + node boxes overlaid on background
# ═══════════════════════════════════════════════════════════════════════════════
img     = Image.open(bg_path).convert('RGBA')
overlay = Image.new('RGBA', img.size, (0, 0, 0, 0))
d       = ImageDraw.Draw(overlay)

# ─── Sub-concept geometry ──────────────────────────────────────────────────────
sub_h_in   = 0.55
sub_gap_in = 0.18
sub_pad_in = 0.30

# ─── Loop: Chapter nodes + sub-concepts ───────────────────────────────────────
for ch in CHAPTERS:
    cx_in = COL_X[ch['col']]
    cy_in = ROW1_Y if ch['row'] == 1 else ROW2_Y
    ac    = ch['accent'];   ac_m  = ch['accent_m']
    n_sub = len(ch['subs'])
    sub_w = ix(NODE_W - 2 * sub_pad_in)

    # Chapter pillar (rounded rect)
    pill_rrect(d, ix(cx_in), iy(cy_in),
               ix(NODE_W), ix(NODE_H),
               fc=SF_R, ec=ac, lw=2)

    # Chapter title (Chinese)
    cy_t = iy(cy_in + NODE_H * 0.04)
    ctext(d, ix(cx_in), cy_t, ch['title'], PF_CHAP, TT_R)

    # English subtitle
    cy_e = iy(cy_in - NODE_H * 0.22)
    ctext(d, ix(cx_in), cy_e, ch['en'], PF_EN, TX_R)

    # Sub-concepts
    total_sub = n_sub * sub_h_in + (n_sub - 1) * sub_gap_in
    start_y   = cy_in - NODE_H/2 - total_sub - 0.30
    sx_px     = ix(cx_in)

    # Common stem from top of first sub to dot at chapter edge
    first_sub_cy = iy(start_y + sub_h_in / 2)
    dot_y        = iy(cy_in - NODE_H / 2)
    stem_top_y   = iy(start_y + n_sub * (sub_h_in + sub_gap_in) - sub_gap_in)
    d.line([(sx_px, stem_top_y), (sx_px, dot_y)],
           fill=(*BD_R[:3], 160), width=ix(0.28))

    for i, s in enumerate(ch['subs']):
        sy_in = start_y + i * (sub_h_in + sub_gap_in) + sub_h_in / 2
        sy_px = iy(sy_in)

        # Sub-box
        pill_rrect(d, sx_px, sy_px, sub_w, ix(sub_h_in),
                   fc=NB_R, ec=BD_R, lw=1)

        # Sub-text
        ctext(d, sx_px, sy_px, s, PF_SUB, TS_R)

    # Dot at chapter edge (drawn after all subs)
    r = ix(0.07)
    d.ellipse([sx_px-r, dot_y-r, sx_px+r, dot_y+r], fill=ac)

# ─── Title ─────────────────────────────────────────────────────────────────────
ctext(d, ix(W/2), iy(H-2.2), '线性代数知识脉络框架图', PF_TITLE, TT_R)
ctext(d, ix(W/2), iy(H-3.1),
      'UNIVERSITY LINEAR ALGEBRA  |  VECTOR SPACE ARCHITECTURE',
      PF_EN, TX_R)

# ─── Row labels ────────────────────────────────────────────────────────────────
for row_y, lbl, ac in [(ROW1_Y, 'I', AC_R), (ROW2_Y, 'II', TL_R)]:
    ctext(d, ix(0.9), iy(row_y), lbl, PF_ROW, ac)

# ─── Legend ───────────────────────────────────────────────────────────────────
leg_y = 1.5
ctext(d, ix(W/2), iy(leg_y + 0.6), '知识流向  KNOWLEDGE FLOW', PF_EN, TX_R)

legend_items = [
    (AC_R,  '-',  '基础理论'),
    (TL_R,  '-',  '结构与应用'),
    (VI_R,  '--', '核心工具'),
]
for i, (c, ls, label) in enumerate(legend_items):
    lx = ix(W/2 - 5 + i * 3.5)
    # Line
    ls_str = '-' if ls == '-' else '--'
    d.line([(lx - ix(0.5), iy(leg_y)), (lx + ix(0.5), iy(leg_y))],
           fill=(*c[:3], 200), width=3)
    d.text((lx + ix(0.7), iy(leg_y - ix(0.15))), label,
           font=PF_LEG, fill=(*TX_R[:3], 180))

# ─── Composite & Save ──────────────────────────────────────────────────────────
final = Image.alpha_composite(img, overlay).convert('RGB')
out   = os.path.join(os.path.dirname(__file__), 'linear_algebra_framework.png')
final.save(out, quality=95)
os.remove(bg_path)
print(f'Saved: {out}')
