import matplotlib.pyplot as plt
import matplotlib.patches as patches
import numpy as np
import os

# ─── Palette ─────────────────────────────────────────────────────────────────
BG      = '#0A0F1C'
SURFACE = '#0D1525'
BORDER  = '#1B3055'
ACCENT  = '#4A9EDB'
AMBER   = '#D4930A'
GREY_L  = '#4A6080'
TEXT    = '#D8D4CC'
TEXT_DIM= '#5A7090'
LINE    = '#1E3055'
LINE_L  = '#2A4570'
GOLD    = '#C8973A'
DARK    = '#08111E'

# ─── Canvas 36×26 inches @ 150 DPI ──────────────────────────────────────────
W, H = 36, 26
DPI  = 150
fig = plt.figure(figsize=(W, H), dpi=DPI, facecolor=BG)
ax  = fig.add_axes([0, 0, 1, 1], facecolor=BG)
ax.set_xlim(0, W); ax.set_ylim(0, H); ax.axis('off')

# ─── Font ─────────────────────────────────────────────────────────────────────
import glob
FONT_DIR = os.path.join(os.path.dirname(__file__), 'canvas-fonts')
CHINESE_FONT = 'C:\\Windows\\Fonts\\msyh.ttc'   # Microsoft YaHei
THIN_FONT     = None

for fp in glob.glob(os.path.join(FONT_DIR, '*.ttf')):
    name = os.path.basename(fp).lower()
    if any(k in name for k in ['ultralight', 'thin', 'light']):
        THIN_FONT = fp
        break

plt.rcParams['font.family']  = 'sans-serif'
plt.rcParams['font.sans-serif'] = [CHINESE_FONT, 'SimHei', 'sans-serif']

# ════════════════════════════════════════════════════════════════════════════
#  HELPERS
# ════════════════════════════════════════════════════════════════════════════
# Chinese font helpers
from matplotlib import font_manager as fm

def make_font_prop(size, weight='normal'):
    return fm.FontProperties(
        fname='C:\\Windows\\Fonts\\msyh.ttc', size=size, weight=weight)

def node(ax, cx, cy, w, h, title, en_title=None,
         fc=SURFACE, bc=ACCENT, tc=TEXT, ec=ACCENT,
         fs=12, es=7.5, zorder=5, lw=1.0):
    r = patches.FancyBboxPatch((cx-w/2, cy-h/2), w, h,
        boxstyle="round,pad=0.1",
        lw=lw, edgecolor=ec, facecolor=fc, zorder=zorder)
    ax.add_patch(r)
    y_mid = cy + (h*0.15 if en_title else 0)
    ax.text(cx, y_mid, title,
            ha='center', va='center', fontsize=fs,
            fontweight='light', color=tc, zorder=zorder+1,
            linespacing=1.5,
            fontproperties=make_font_prop(fs))
    if en_title:
        ax.text(cx, cy - h*0.22, en_title,
                ha='center', va='center', fontsize=es,
                fontweight='light', color=GREY_L, zorder=zorder+1)

def sub_node(ax, cx, cy, w, h, text,
             fc='#09101C', bc=BORDER, tc='#8AAFC8', fs=7.0, zorder=4):
    r = patches.FancyBboxPatch((cx-w/2, cy-h/2), w, h,
        boxstyle="round,pad=0.06",
        lw=0.5, edgecolor=bc, facecolor=fc, zorder=zorder)
    ax.add_patch(r)
    ax.text(cx, cy, text,
            ha='center', va='center', fontsize=fs,
            fontweight='light', color=tc, zorder=zorder+1,
            linespacing=1.4,
            fontproperties=make_font_prop(fs))

def conn(ax, x1, y1, x2, y2, color=LINE, lw=0.7, zorder=2):
    ax.plot([x1, x2], [y1, y2], color=color, lw=lw,
            solid_capstyle='round', zorder=zorder)

def dot(ax, x, y, r=0.08, color=ACCENT, zorder=6):
    ax.add_patch(plt.Circle((x, y), r, color=color, zorder=zorder, alpha=0.85))

# ════════════════════════════════════════════════════════════════════════════
#  KNOWLEDGE DATA
# ════════════════════════════════════════════════════════════════════════════
# 8 chapters × 3 sub-concepts each
# x positions: left cluster (cols 1-3), right cluster (cols 4-6)
# We'll lay them out in two rows of 4, two columns of 4 → actually
# 4 chapters across the top half, 4 across the bottom half

CHAPTERS = [
    # ── ROW 1 ──────────────────────────────────────────────
    dict(title='随机事件与概率',
         en='Probability of Events',
         x=5.0, y=22.0,
         subs=['样本空间·随机事件·事件的运算',
               '古典概型·几何概型·公理化定义',
               '条件概率·乘法公式\n全概率公式·贝叶斯公式']),
    dict(title='一维随机变量',
         en='Random Variables',
         x=13.0, y=22.0,
         subs=['离散型：二项·泊松·几何分布',
               '连续型：均匀·指数·正态分布',
               '分布函数 F(x)·概率密度 f(x)']),
    dict(title='多维随机变量',
         en='Multi-Dimensional Variables',
         x=21.0, y=22.0,
         subs=['联合分布函数与联合密度函数',
               '边缘分布·条件分布',
               '随机变量的独立性判定']),
    dict(title='数字特征',
         en='Numerical Characteristics',
         x=29.0, y=22.0,
         subs=['数学期望·方差与标准差',
               '协方差·相关系数',
               '矩：原点矩·中心矩']),
    # ── ROW 2 ──────────────────────────────────────────────
    dict(title='极限定理',
         en='Limit Theorems',
         x=5.0, y=12.5,
         subs=['大数定律\n（切比雪夫·辛钦·伯努利）',
               '中心极限定理\n（独立同分布·棣莫弗-拉普拉斯）']),
    dict(title='统计估计',
         en='Statistical Estimation',
         x=13.0, y=12.5,
         subs=['点估计：矩估计法·最大似然估计',
               '评价标准：无偏性·有效性·相合性',
               '区间估计与置信区间']),
    dict(title='假设检验',
         en='Hypothesis Testing',
         x=21.0, y=12.5,
         subs=['参数检验：Z 检验·t 检验·χ² 检验',
               '非参数检验：K-S 检验·拟合优度']),
    dict(title='随机过程初步',
         en='Stochastic Processes',
         x=29.0, y=12.5,
         subs=['马尔可夫链：转移矩阵·平稳分布',
               '泊松过程：计数过程·时间间隔']),
]

# ════════════════════════════════════════════════════════════════════════════
#  DRAW CHAPTER NODES
# ════════════════════════════════════════════════════════════════════════════
NW, NH = 6.8, 1.5   # node width / height
SW, SH = 6.4, 0.85 # sub-node width / height

for ch in CHAPTERS:
    node(ax, ch['x'], ch['y'], NW, NH, ch['title'], ch['en'],
         fs=12.5, es=7.5)

    for i, sub in enumerate(ch['subs']):
        sy = ch['y'] - NH/2 - SH - 0.15 - i * (SH + 0.12)
        sub_node(ax, ch['x'], sy, SW, SH, sub)

        # Connector: chapter bottom → sub top
        y_top_ch = ch['y'] - NH/2
        y_bot_sub = sy + SH/2
        conn(ax, ch['x'], y_top_ch, ch['x'], y_bot_sub,
             color=LINE, lw=0.65)
        dot(ax, ch['x'], y_bot_sub, r=0.06, color=LINE_L)

# ════════════════════════════════════════════════════════════════════════════
#  INTER-CHAPTER CONNECTORS (horizontal arrows, top row)
# ════════════════════════════════════════════════════════════════════════════
row1 = CHAPTERS[:4]
for a, b in zip(row1, row1[1:]):
    x1 = a['x'] + NW/2
    x2 = b['x'] - NW/2
    y  = a['y']
    conn(ax, x1, y, x2, y, color=LINE_L, lw=0.8)
    # arrowhead
    ax.annotate('', xy=(x2-0.1, y), xytext=(x2-0.45, y),
                arrowprops=dict(arrowstyle='->', color=LINE_L, lw=0.8),
                zorder=3)

row2 = CHAPTERS[4:]
for a, b in zip(row2, row2[1:]):
    x1 = a['x'] + NW/2
    x2 = b['x'] - NW/2
    y  = a['y']
    conn(ax, x1, y, x2, y, color=LINE_L, lw=0.8)
    ax.annotate('', xy=(x2-0.1, y), xytext=(x2-0.45, y),
                arrowprops=dict(arrowstyle='->', color=LINE_L, lw=0.8),
                zorder=3)

# Vertical connector between row 1 and row 2 for each column
for ch in row1:
    # Find corresponding row2 chapter
    idx = row1.index(ch)
    ch2 = row2[idx]
    x = ch['x']
    y1 = ch['y'] - NH/2
    y2 = ch2['y'] + NH/2
    # find bottom-most sub node of row1 chapter
    n_subs = len(ch['subs'])
    y_bot = ch['y'] - NH/2 - SH - 0.15 - (n_subs-1)*(SH+0.12) - SH/2
    conn(ax, x, y1, x, y2, color=LINE, lw=0.6)
    dot(ax, x, y2, r=0.07, color=LINE_L)

# ════════════════════════════════════════════════════════════════════════════
#  TITLE
# ════════════════════════════════════════════════════════════════════════════
# Top accent bar
ax.plot([2, W-2], [H-1.0, H-1.0], color=ACCENT, lw=0.6, alpha=0.5, zorder=3)
ax.text(W/2, H-1.6,
        'UNIVERSITY PROBABILITY THEORY',
        ha='center', va='center', fontsize=10,
        fontweight='light', color=GREY_L, zorder=5,
        style='italic')
ax.text(W/2, H-2.5,
        '概率论知识脉络框架图',
        ha='center', va='center', fontsize=26,
        fontweight='light', color=TEXT, zorder=5,
        fontproperties=make_font_prop(26))

# ─── Divider between rows ─────────────────────────────────────────────────────
ax.plot([2.5, W-2.5], [17.0, 17.0], color=BORDER, lw=0.5, alpha=0.8, zorder=1)

# ─── Core stats legend (bottom left) ─────────────────────────────────────────
legend_x, legend_y = 2.8, 1.5
ax.text(legend_x, legend_y+0.4, '体系结构', fontsize=8, color=GREY_L, va='center')
items = [('■', ACCENT, '核心章节'),
         ('■', AMBER,  '关键概念'),
         ('■', '#8AAFC8', '细分知识点')]
for i, (sym, col, lbl) in enumerate(items):
    lx = legend_x + 2.5 + i*5.2
    ax.text(lx, legend_y, sym, fontsize=8, color=col, va='center', zorder=5)
    ax.text(lx+0.35, legend_y, lbl, fontsize=7.5, color=TEXT_DIM, va='center', zorder=5)

# ─── Page / corner marks ───────────────────────────────────────────────────────
ax.text(W-1.2, 0.7, '01', fontsize=7, color='#2A3A50', ha='right', va='center')
ax.text(1.2, 0.7, 'Prob. Theory · Knowledge Framework',
        fontsize=6.5, color='#2A3A50', ha='left', va='center')

for cx_, cy_ in [(1.5,0.8),(W-1.5,0.8),(1.5,H-0.8),(W-1.5,H-0.8)]:
    ax.plot(cx_, cy_, '+', color='#1E3050', markersize=8, zorder=2)

# ─── Subtle grid dots in background ──────────────────────────────────────────
np.random.seed(42)
for _ in range(300):
    gx = np.random.uniform(2, W-2)
    gy = np.random.uniform(2, H-2)
    ax.plot(gx, gy, 'o', color='#151F35', markersize=0.8, zorder=0, alpha=0.7)

# ─── Bottom waveform decoration ──────────────────────────────────────────────
tx = np.linspace(1, W-1, 600)
ty = 0.5 + 0.18 * np.sin(tx*0.4) * np.exp(-((tx-W/2)/20)**2)
ax.plot(tx, ty, color='#1A3050', lw=0.7, zorder=1, alpha=0.5)

# ════════════════════════════════════════════════════════════════════════════
#  SAVE
# ════════════════════════════════════════════════════════════════════════════
out = os.path.join(os.path.dirname(__file__), 'probability_theory_framework_v2.png')
plt.savefig(out, dpi=DPI, bbox_inches='tight',
            facecolor=BG, edgecolor='none', format='png')
plt.close()
print(f'Saved: {out}')
