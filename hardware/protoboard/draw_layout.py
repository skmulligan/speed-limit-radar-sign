"""Reproduce planning diagrams, not fabrication geometry. Requires Matplotlib."""
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.patches import FancyArrowPatch, Rectangle

HERE = Path(__file__).resolve().parent
OUT = HERE.parents[1] / "output" / "protoboard"
BG, INK, MUTED = "#f7f4ed", "#173e45", "#53666a"
TEAL, GOLD, BLUE = "#d6e9e3", "#f3dfb9", "#dce7f2"
plt.rcParams.update({"font.family": "DejaVu Sans", "svg.fonttype": "none", "svg.hashsalt": "street-speed-reminder"})


def box(ax, x, y, w, h, label="", color=TEAL, fs=10, dashed=False):
    ax.add_patch(Rectangle((x, y), w, h, facecolor=color, edgecolor=MUTED,
                           linewidth=1.3, linestyle="--" if dashed else "-"))
    if label:
        ax.text(x + w / 2, y + h / 2, label, ha="center", va="center",
                fontsize=fs, color=INK, linespacing=1.5)


def arrow(ax, start, end, color=MUTED):
    ax.add_patch(FancyArrowPatch(start, end, arrowstyle="-|>",
                                mutation_scale=12, color=color, linewidth=1.6))


def base(title, subtitle):
    fig = plt.figure(figsize=(14, 10), facecolor=BG)
    fig.text(.05, .95, title, fontsize=23, weight="bold", color=INK)
    fig.text(.05, .916, subtitle, fontsize=11, color=MUTED)
    fig.text(.05, .035, "WIRING / PLACEMENT GUIDE  •  Check each assembly  •  Protection / endurance pending",
             fontsize=10, color="#965639", weight="bold")
    return fig


def save(fig, name):
    OUT.mkdir(parents=True, exist_ok=True)
    path = HERE / f"{name}.svg"
    fig.savefig(path, facecolor=BG, metadata={"Date": None, "Creator": "Street Speed Reminder"})
    path.write_text("\n".join(line.rstrip() for line in path.read_text().splitlines()) + "\n")
    fig.savefig(OUT / f"{name}.png", dpi=150, facecolor=BG, metadata={"Software": "Street Speed Reminder"})
    plt.close(fig)


def placement():
    fig = base("PROTOBOARD / MODULE PLACEMENT",
               "One 150 × 100 mm isolated-pad board • Component-side view • Zones, not measured footprints")
    ax = fig.add_axes([.05, .30, .65, .57])
    ax.set(xlim=(-5, 155), ylim=(112, -10), aspect="equal")
    ax.axis("off")
    box(ax, 0, 0, 150, 100, color="#e9e5d9")
    ax.annotate("", xy=(0, -6), xytext=(150, -6),
                arrowprops={"arrowstyle": "<->", "color": MUTED})
    ax.text(75, -8, "150 mm nominal", ha="center", fontsize=10, color=MUTED)
    ax.text(153, 50, "100 mm nominal", rotation=90, va="center", fontsize=10, color=MUTED)
    box(ax, 6, 0, 45, 80, color="#eff4ef", dashed=True)
    box(ax, 14, 8, 29, 56, "ESP32-S3\n\nRemovable\n2 × 22 sockets", fs=11)
    box(ax, 14, -4, 29, 12, "ANTENNA", color=BG, fs=9)
    box(ax, 14, 64, 29, 9, "USB sockets", color=BLUE, fs=9)
    ax.text(28.5, 83, "USB access / plug clearance", ha="center", fontsize=8, color=MUTED)
    arrow(ax, (28.5, 86), (28.5, 105))
    box(ax, 64, 30, 40, 50, color="#eff4ef", dashed=True)
    box(ax, 69, 34, 30, 33, "LEVEL\nSHIFTER", fs=11)
    ax.text(71, 39, "B", fontsize=10, color=INK)
    ax.text(94, 39, "A", fontsize=10, color=INK)
    ax.text(71, 62, "VCB", fontsize=8, color=INK)
    ax.text(91, 65, "VCA", fontsize=8, color=INK)
    ax.text(84, 73, "C1 / C2 close to supplies", ha="center", fontsize=7.8, color=MUTED)
    ax.text(84, 78, "R1 / R2 enable pull-downs", ha="center", fontsize=7.8, color=MUTED)
    box(ax, 68, 7, 29, 16, "RAD  2×5\ncustom harness", color=BLUE, fs=9)
    box(ax, 115, 34, 25, 30, "DISP\n2×8\nto P10 JIN", color=BLUE, fs=9)
    arrow(ax, (46, 50), (67, 50))
    ax.text(56.5, 46, "6 signals", ha="center", fontsize=8, color=MUTED)
    arrow(ax, (101, 50), (113, 50))
    box(ax, 64, 87, 30, 9, "ESP-LINK", color=GOLD, fs=9)
    box(ax, 100, 87, 20, 9, "PWR-E", color=GOLD, fs=9)
    box(ax, 124, 87, 20, 9, "PWR-P", color=GOLD, fs=9)
    ax.text(134, 83, "C3 nearby", ha="center", fontsize=8, color=MUTED)
    ax.text(94, 105, "LOWER SERVICE EDGE", ha="center", fontsize=9, color=MUTED)

    fig.text(.735, .84, "PLACEMENT RULES", fontsize=13, weight="bold", color=INK)
    notes = [
        ("REMOVABLE MODULES", "Dry-fit sockets before soldering.\nDo not bend pins to fit the grid."),
        ("SHIFTER ORIENTATION", "B row left; A row right.\nVCA lower-right; VCB one pitch up."),
        ("ANTENNA / ACCESS", "Keep antenna clear of copper,\nwiring and nearby metal.\nLeave USB and ribbon plug room."),
        ("POWER", "Two separate fused +5 V feeds.\nCommon ground. Panel current\nstays off this board."),
    ]
    for y, (title, body) in zip([.79, .68, .57, .43], notes):
        fig.text(.735, y, title, fontsize=10, weight="bold", color=INK)
        fig.text(.735, y - .028, body, fontsize=10, color=MUTED, va="top", linespacing=1.6)
    fig.text(.05, .26, "ENCLOSURE / REAR VIEW", fontsize=13, weight="bold", color=INK)
    rear = fig.add_axes([.05, .085, .9, .15])
    rear.set(xlim=(0, 100), ylim=(24, 0))
    rear.axis("off")
    box(rear, 0, 0, 100, 24, color="#e9e5d9")
    box(rear, 2, 2, 28, 11, "POWER: cutoff + converter\nMount to insulated service plate", color=GOLD, fs=9)
    box(rear, 42, 2, 56, 11, "LOGIC BOARD: 150 × 100 mm\nBehind panel depth + connector clearance", fs=9)
    box(rear, 2, 15, 28, 7, "BATTERY + ADAPTER / fuses", color=GOLD, fs=9)
    box(rear, 73, 15, 25, 7, "RADAR / front-left window", color=BLUE, fs=9)
    arrow(rear, (85, 13), (85, 15))
    rear.text(50, 19, "Zones only • enclosure depth TBD", ha="center", va="center", fontsize=9, color=MUTED)
    save(fig, "placement")


def table(ax, headers, rows, widths, fs=10):
    ax.axis("off")
    tbl = ax.table(cellText=rows, colLabels=headers, colWidths=widths,
                   cellLoc="left", colLoc="left", bbox=[0, 0, 1, 1])
    tbl.auto_set_font_size(False)
    tbl.set_fontsize(fs)
    for (r, _), cell in tbl.get_celld().items():
        cell.set_edgecolor("#ced6d2")
        cell.set_facecolor(TEAL if r == 0 else ("#ffffff" if r % 2 else "#edf0e9"))
        cell.get_text().set_color(INK)
        if r == 0:
            cell.get_text().set_weight("bold")


def connections():
    fig = base("PROTOBOARD / CONNECTION MAP",
               "Logical connections only • Use printed module labels • Check actual connector pin 1 and cable continuity")
    ax = fig.add_axes([.05, .51, .9, .36])
    ax.set(xlim=(0, 120), ylim=(51, 0))
    ax.axis("off")
    box(ax, 0, 0, 23, 10, "Battery / switched adapter\nexisting adapter fuse", GOLD, 9)
    box(ax, 29, 0, 19, 10, "5 A branch fuse\nexisting input protection", GOLD, 8.5)
    box(ax, 54, 0, 18, 10, "XH-M609\ncutoff", GOLD, 10)
    box(ax, 78, 0, 18, 10, "Buck converter\nnominal +5 V", GOLD, 9)
    for a, b in [(23, 29), (48, 54), (72, 78)]:
        arrow(ax, (a, 5), (b, 5))
    ax.plot([96, 102, 102, 9, 9], [5, 5, 15, 15, 40], color=MUTED, lw=1.6)
    ax.text(45, 14, "+5 V distribution • fuses at branch takeoffs", ha="center", fontsize=9, color=MUTED)
    for y in (20, 30, 40):
        arrow(ax, (9, y), (20, y))
    box(ax, 20, 16, 30, 8, "Separate panel power harness", BLUE, 9)
    arrow(ax, (50, 20), (70, 20))
    box(ax, 70, 16, 45, 8, "P10 POWER • output protection TBD", BLUE, 9)
    box(ax, 20, 26, 30, 8, "Existing 1 A ESP output fuse", GOLD, 9)
    arrow(ax, (50, 30), (70, 30))
    box(ax, 70, 26, 45, 8, "PWR-E → ESP-LINK → ESP32 5Vin", TEAL, 9)
    box(ax, 20, 36, 30, 8, "Proposed 1 A peripheral fuse", GOLD, 9)
    arrow(ax, (50, 40), (70, 40))
    box(ax, 70, 36, 45, 8, "PWR-P → radar VIN + shifter VCA", TEAL, 9)
    ax.text(60, 49, "COMMON GROUND • Separate panel return to distribution • Never bridge PWR-E+ and PWR-P+ on board",
            ha="center", fontsize=9.2, color=INK, weight="bold")

    fig.text(.05, .47, "DISPLAY / 6 CHANNELS THROUGH SHIFTER", fontsize=12, weight="bold", color=INK)
    a = fig.add_axes([.05, .20, .53, .245])
    table(a, ["GPIO", "B → A", "DISP", "Function"], [
        ["11", "B1 → A1", "12", "Data"], ["12", "B2 → A2", "8", "Clock"],
        ["10", "B3 → A3", "10", "Latch"], ["9", "B4 → A4", "1", "ENABLE (high = on)"],
        ["4", "B5 → A5", "2", "Row A"], ["5", "B6 → A6", "4", "Row B"],
    ], [.13, .25, .15, .47])
    fig.text(.05, .174, "DISP grounds: 3, 5, 7, 9, 11, 13, 15  •  NC: 6, 14, 16  •  No +5 V", fontsize=9.6, color=MUTED)
    fig.text(.05, .132, "SHIFTER: VCA = peripheral +5 V; VCB = ESP32 3V3.\nDIR, /OE, B7, B8 and GND → ground. A7/A8 unused.\nC1/C2: 100 nF at supplies; R1/R2: 10 kΩ from B4/A4 to GND.",
             fontsize=10, color=INK, va="top", linespacing=1.6)
    fig.text(.63, .47, "RADAR / DIRECT 3.3 V UART", fontsize=12, weight="bold", color=INK)
    b = fig.add_axes([.63, .20, .32, .245])
    table(b, ["RAD", "LD2451 / connection"], [
        ["1", "1 VIN ← peripheral +5 V"], ["2, 4, 6", "2 GND (insulated splice)"],
        ["3", "4 TX → GPIO17"], ["5", "5 RX ← GPIO18"],
        ["7–10", "Unconnected"], ["—", "OT1 / OT2 unconnected"],
    ], [.22, .78], 9.5)
    fig.text(.63, .174, "Custom 20–30 cm harness • Not straight-through", fontsize=9.6, color=MUTED)
    fig.text(.63, .132, "POWER INPUTS: terminal 1 = its fused +5 V;\nterminal 2 = common GND. C3: 10 µF across PWR-P.\nUSB service requires full external/peripheral isolation.",
             fontsize=10, color=INK, va="top", linespacing=1.6)
    save(fig, "connections")


if __name__ == "__main__":
    placement()
    connections()
    print(f"SVG sources: {HERE}\nPNG previews: {OUT}")
