import matplotlib.pyplot as plt
import matplotlib.patches as patches
import matplotlib.patheffects as pe
import numpy as np

# ─── Color Palette ────────────────────────────────────────────────────────────
C_BG       = '#0B1220'      # Deep navy void
C_IVORY    = '#F0EDE6'      # Primary label
C_AZURE    = '#4A9EDB'      # Major chapter nodes
C_AMBER    = '#E8A838'      # Sub-concept label
C_GREY     = '#4A5568'      # Connector lines
C_DIM      = '#2D3A4F'      # Dim structural
C_NODE_BG  = '#0F1929'      # Node background
C_BORDER   = '#1E3050'      # Node border

# ─── Font Setup ──────────────────────────────────────────────────────────────
import os, glob

FONT_DIR = os.path.join(os.path.dirname(__file__), 'canvas-fonts')
AVAILABLE_FONTS = []
if os.path.isdir(FONT_DIR):
    AVAILABLE_FONTS = glob.glob(os.path.join(FONT_DIR, '*.ttf')) + \
                       glob.glob(os.path.join(FONT_DIR, '*.otf'))

if AVAILABLE_FONTS:
    font_paths = {}
    for fp in AVAILABLE_FONTS:
        name = os.path.splitext(os.path.basename(fp))[0]
        font_paths[name.lower()] = fp
    # Prefer thin/light geometric fonts
    for preferred in ['geometricultralight', 'spacemono', 'rajdhani', 'chinese', 'noto']:
        for k, v in font_paths.items():
            if preferred.lower() in k:
                plt.rcParams['font.family'] = 'sans-serif'
                plt.rcParams['font.sans-serif'] = [v, 'sans-serif']
                break
else:
    plt.rcParams['font.family'] = 'sans-serif'
    plt.rcParams['font.sans-serif'] = ['Microsoft YaHei', 'SimHei', 'DejaVu Sans']

# ─── Canvas ──────────────────────────────────────────────────────────────────
DPI = 150
FW, FH = 40, 28          # inches
fig = plt.figure(figsize=(FW, FH), dpi=DPI, facecolor=C_BG)
ax  = fig.add_axes([0, 0, 1, 1], facecolor=C_BG)
ax.set_xlim(0, FW)
ax.set_ylim(0, FH)
ax.axis('off')

# ─── Helper: draw a node ─────────────────────────────────────────────────────
def draw_node(ax, cx, cy, w, h, text, sub_text=None,
              fill=C_NODE_BG, border=C_AZURE, text_color=C_IVORY,
              font_size=13, sub_font_size=8, zorder=5):
    # Background rect
    rect = patches.FancyBboxPatch((cx - w/2, cy - h/2), w, h,
        boxstyle="round,pad=0.08",
        linewidth=1.2, edgecolor=border, facecolor=fill, zorder=zorder)
    ax.add_patch(rect)

    # Main label
    ax.text(cx, cy + (h*0.15 if sub_text else 0),
            text, ha='center', va='center',
            fontsize=font_size, fontweight='light',
            color=text_color, zorder=zorder+1,
            linespacing=1.4)
    if sub_text:
        ax.text(cx, cy - h*0.22,
                sub_text, ha='center', va='center',
                fontsize=sub_font_size, fontweight='light',
                color=C_AMBER, zorder=zorder+1,
                linespacing=1.3)

def draw_connector(ax, x1, y1, x2, y2, color=C_GREY, lw=0.8, zorder=2):
    ax.plot([x1, x2], [y1, y2], color=color, linewidth=lw, zorder=zorder,
            solid_capstyle='round')

def draw_dot(ax, x, y, r=0.12, color=C_AZURE, zorder=6):
    c = plt.Circle((x, y), r, color=color, zorder=zorder, alpha=0.9)
    ax.add_patch(c)

# ════════════════════════════════════════════════════════════════════════════
#  LAYOUT CONSTANTS
# ════════════════════════════════════════════════════════════════════════════
COL_CENTERS = [3.5, 10.0, 16.5, 23.0, 29.5, 36.5]
CHAP_H = 1.9

# Chapter title positions (y=22 top row)
CHAP_YS = [22.2, 19.5, 16.8, 14.1, 11.4, 8.7, 6.0, 3.3]

# Sub-concept y rows relative to chapter
SUB_Y_DELTAS = [-0.9, -1.6, -2.3]

# ════════════════════════════════════════════════════════════════════════════
#  CHAPTER NODES
# ════════════════════════════════════════════════════════════════════════════

chapters = [
    {
        'title': '随机事件与概率',
        'sub':   'Probability of Events',
        'x': COL_CENTERS[0], 'y': CHAP_YS[0],
        'subs': [
            '样本空间 · 随机事件 · 事件的运算',
            '概率的公理化定义 · 古典概型 · 几何概型',
            '条件概率 · 乘法公式 · 全概率公式 · 贝叶斯公式',
        ]
    },
    {
        'title': '随机变量及其分布',
        'sub':   'Random Variables & Distributions',
        'x': COL_CENTERS[1], 'y': CHAP_YS[1],
        'subs': [
            '离散型：二项分布 · 泊松分布 · 几何分布',
            '连续型：均匀分布 · 指数分布 · 正态分布',
            '分布函数 F(x) · 概率密度函数 f(x)',
        ]
    },
    {
        'title': '多维随机变量',
        'sub':   'Multi-Dimensional Variables',
        'x': COL_CENTERS[2], 'y': CHAP_YS[2],
        'subs': [
            '联合分布函数与联合密度',
            '边缘分布 · 条件分布',
            '随机变量的独立性',
        ]
    },
    {
        'title': '数字特征',
        'sub':   'Numerical Characteristics',
        'x': COL_CENTERS[3], 'y': CHAP_YS[3],
        'subs': [
            '数学期望（均值）· 方差与标准差',
            '协方差 · 相关系数',
            '矩：原点矩 · 中心矩',
        ]
    },
    {
        'title': '极限定理',
        'sub':   'Limit Theorems',
        'x': COL_CENTERS[4], 'y': CHAP_YS[4],
        'subs': [
            '大数定律（切比雪夫 / 辛钦 / 伯努利）',
            '中心极限定理（独立同分布 / 棣莫弗-拉普拉斯）',
        ]
    },
    {
        'title': '统计估计',
        'sub':   'Statistical Estimation',
        'x': COL_CENTERS[5], 'y': CHAP_YS[5],
        'subs': [
            '点估计：矩估计 · 最大似然估计（MLE）',
            '估计量的评价标准：无偏性 · 有效性 · 相合性',
            '区间估计 · 置信区间',
        ]
    },
    {
        'title': '假设检验',
        'sub':   'Hypothesis Testing',
        'x': COL_CENTERS[0] + 3, 'y': CHAP_YS[6],
        'subs': [
            '参数检验：Z 检验 · t 检验 · χ² 检验',
            '非参数检验：K-S 检验 · 拟合优度检验',
        ]
    },
    {
        'title': '随机过程初步',
        'sub':   'Stochastic Processes',
        'x': COL_CENTERS[0] + 3, 'y': CHAP_YS[7],
        'subs': [
            '马尔可夫链：状态 · 转移矩阵 · 平稳分布',
            '泊松过程：计数过程 · 时间间隔分布',
        ]
    },
]

# ─── Draw all chapters ────────────────────────────────────────────────────────
for ch in chapters:
    w_node = 7.5
    h_node = 1.5
    draw_node(ax, ch['x'], ch['y'], w_node, h_node,
              ch['title'], ch['sub'],
              border=C_AZURE, fill='#0D1A30',
              font_size=13, sub_font_size=7.5)

    # Sub-concepts
    for i, sub in enumerate(ch['subs']):
        sy = ch['y'] - 1.3 - i * 1.0
        sw = 8.0
        sh = 0.72
        rect = patches.FancyBboxPatch(
            (ch['x'] - sw/2, sy - sh/2), sw, sh,
            boxstyle="round,pad=0.06",
            linewidth=0.6, edgecolor=C_BORDER, facecolor='#09111F', zorder=4)
        ax.add_patch(rect)
        ax.text(ch['x'], sy, sub,
                ha='center', va='center',
                fontsize=7.0, fontweight='light',
                color='#8FA8C0', zorder=5, linespacing=1.35)

        # Connect chapter to sub
        draw_connector(ax, ch['x'], ch['y'] - h_node/2,
                              ch['x'], sy + sh/2, color=C_DIM, lw=0.6)

# ─── Draw vertical spine column (left guide line) ───────────────────────────
spine_x = COL_CENTERS[0] - 5.0
for y_start, y_end in zip(CHAP_YS, CHAP_YS[1:]):
    draw_connector(ax, spine_x, y_start - 0.75, spine_x, y_end + 0.75,
                   color='#1A2D4A', lw=0.8)

# ─── Draw horizontal relationship arrows between columns ─────────────────────
# Arrow: 列1 → 列2 连接（章节点之间）
for i, (ch_from, ch_to) in enumerate(zip(chapters[:5], chapters[1:5])):
    x1 = ch_from['x'] + 3.75
    x2 = ch_to['x'] - 3.75
    y  = ch_from['y']
    draw_connector(ax, x1, y, x2, y, color='#2A4060', lw=0.8)
    # Arrow head
    ax.annotate('', xy=(x2 - 0.05, y), xytext=(x2 - 0.4, y),
                arrowprops=dict(arrowstyle='->', color='#2A4060', lw=0.8),
                zorder=3)

# ─── Title block ─────────────────────────────────────────────────────────────
ax.text(FW/2, FH - 1.8,
        'UNIVERSITY PROBABILITY THEORY',
        ha='center', va='center',
        fontsize=9, fontweight='light', color='#3A5070',
        zorder=5)
ax.text(FW/2, FH - 2.7,
        '知识框架图',
        ha='center', va='center',
        fontsize=26, fontweight='light', color=C_IVORY,
        zorder=5)
# Decorative line under title
ax.plot([FW/2 - 5, FW/2 + 5], [FH - 3.3, FH - 3.3],
        color=C_AZURE, linewidth=0.8, alpha=0.6, zorder=5)
ax.plot([FW/2 - 3, FW/2 + 3], [FH - 3.45, FH - 3.45],
        color=C_AZURE, linewidth=0.4, alpha=0.3, zorder=5)

# ─── Legend / key ─────────────────────────────────────────────────────────────
lx, ly = FW - 8.0, 1.8
legend_items = [
    (C_AZURE,   '■', '核心章节'),
    (C_AMBER,   '■', '关键概念'),
    ('#8FA8C0', '■', '细分知识点'),
]
for i, (col, sym, lbl) in enumerate(legend_items):
    ax.text(lx + i*4.5, ly, sym, fontsize=9, color=col, va='center', zorder=5)
    ax.text(lx + i*4.5 + 0.4, ly, lbl,
            fontsize=7.5, color='#4A6080', va='center', zorder=5)

# ─── Bottom decoration: probability curve silhouette ─────────────────────────
t = np.linspace(0, FW, 500)
y_curve = 0.5 + 0.25 * np.exp(-0.5 * ((t - FW/2) / 6) ** 2)
ax.plot(t, y_curve, color='#1A3050', linewidth=0.8, zorder=1, alpha=0.7)

# ─── Page number ─────────────────────────────────────────────────────────────
ax.text(FW - 1.0, 0.5, '01', fontsize=7, color='#2A3A50',
        ha='right', va='center', zorder=5)

# ─── Corner markers (design registration marks) ─────────────────────────────
for cx_, cy_ in [(1.5, 0.7), (FW-1.5, 0.7), (1.5, FH-0.7), (FW-1.5, FH-0.7)]:
    ax.plot(cx_, cy_, '+', color='#1E3050', markersize=8, zorder=2)

# ─── Save ────────────────────────────────────────────────────────────────────
out_path = os.path.join(os.path.dirname(__file__), 'probability_theory_framework.png')
plt.savefig(out_path, dpi=DPI, bbox_inches='tight',
            facecolor=C_BG, edgecolor='none', format='png')
plt.close()
print(f'Saved: {out_path}')
