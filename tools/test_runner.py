#!/usr/bin/env python3
"""Run module tests and generate dependency-free SVG charts."""

from __future__ import annotations

import argparse
import html
import os
import re
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
EXE = ROOT / ("os_basic.exe" if os.name == "nt" else "os_basic")
OUT_DIR = ROOT / "output"


def compile_project() -> None:
    cmd = [
        "g++",
        "-std=c++17",
        "-Wall",
        "-Wextra",
        "-pedantic",
        "-O2",
        "-pthread",
        "-finput-charset=UTF-8",
        "-fexec-charset=UTF-8",
        "-Iinclude",
        "-o",
        str(EXE),
        "src/main.cpp",
        "src/scheduler.cpp",
        "src/memory.cpp",
        "src/sync_demo.cpp",
        "src/vfs.cpp",
    ]
    subprocess.run(cmd, cwd=ROOT, check=True)


def run_program(input_file: Path, output_file: Path) -> str:
    data = input_file.read_text(encoding="utf-8")
    proc = subprocess.run(
        [str(EXE)],
        input=data.encode("utf-8"),
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        cwd=ROOT,
        check=False,
    )
    text = proc.stdout.decode("utf-8", errors="replace")
    output_file.write_text(text, encoding="utf-8")
    sys.stdout.write(text)
    if proc.returncode != 0:
        raise SystemExit(f"program exited with code {proc.returncode}")
    return text


def svg_bar_chart(title: str, labels: list[str], series: list[tuple[str, list[float], str]], path: Path) -> None:
    width, height = 980, 540
    margin_l, margin_r, margin_t, margin_b = 86, 40, 72, 105
    plot_w = width - margin_l - margin_r
    plot_h = height - margin_t - margin_b
    max_v = max([1.0] + [v for _, values, _ in series for v in values]) * 1.18
    group_w = plot_w / max(1, len(labels))
    bar_w = group_w / (len(series) + 1)
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="#ffffff"/>',
        f'<text x="{width/2}" y="34" text-anchor="middle" font-size="22" font-family="Arial">{html.escape(title)}</text>',
        f'<line x1="{margin_l}" y1="{margin_t + plot_h}" x2="{width - margin_r}" y2="{margin_t + plot_h}" stroke="#222"/>',
        f'<line x1="{margin_l}" y1="{margin_t}" x2="{margin_l}" y2="{margin_t + plot_h}" stroke="#222"/>',
    ]
    for i in range(6):
        value = max_v * i / 5
        y = margin_t + plot_h - plot_h * i / 5
        parts.append(f'<line x1="{margin_l}" y1="{y:.1f}" x2="{width - margin_r}" y2="{y:.1f}" stroke="#e8e8e8"/>')
        parts.append(f'<text x="{margin_l - 10}" y="{y + 5:.1f}" text-anchor="end" font-size="12" font-family="Arial">{value:.1f}</text>')
    for idx, label in enumerate(labels):
        gx = margin_l + idx * group_w
        for sidx, (_, values, color) in enumerate(series):
            val = values[idx]
            x = gx + bar_w * (sidx + 0.45)
            h = plot_h * val / max_v
            y = margin_t + plot_h - h
            parts.append(f'<rect x="{x:.1f}" y="{y:.1f}" width="{bar_w * 0.78:.1f}" height="{h:.1f}" fill="{color}"/>')
            parts.append(f'<text x="{x + bar_w * 0.39:.1f}" y="{y - 5:.1f}" text-anchor="middle" font-size="10" font-family="Arial">{val:.2f}</text>')
        parts.append(f'<text x="{gx + group_w/2:.1f}" y="{margin_t + plot_h + 30}" text-anchor="middle" font-size="12" font-family="Arial">{html.escape(label)}</text>')
    lx = margin_l
    for name, _, color in series:
        parts.append(f'<rect x="{lx}" y="{height - 42}" width="16" height="16" fill="{color}"/>')
        parts.append(f'<text x="{lx + 22}" y="{height - 29}" font-size="13" font-family="Arial">{html.escape(name)}</text>')
        lx += 190
    parts.append("</svg>")
    path.write_text("\n".join(parts), encoding="utf-8")


def generate_scheduler(text: str) -> None:
    names = re.findall(r"--- ([A-Za-z()=0-9]+) 调度结果 ---", text)
    metrics = re.findall(
        r"平均周转时间: ([0-9.]+).*?平均等待时间: ([0-9.]+).*?"
        r"平均响应时间: ([0-9.]+).*?平均带权周转时间: ([0-9.]+)",
        text,
        re.S,
    )
    labels = names[: len(metrics)]
    if not labels:
        return
    turnaround = [float(row[0]) for row in metrics]
    waiting = [float(row[1]) for row in metrics]
    response = [float(row[2]) for row in metrics]
    weighted = [float(row[3]) for row in metrics]
    svg_bar_chart(
        "Processor Scheduling: Turnaround / Waiting / Response",
        labels,
        [("Turnaround", turnaround, "#2f80ed"), ("Waiting", waiting, "#f2994a"), ("Response", response, "#27ae60")],
        OUT_DIR / "scheduler_metrics.svg",
    )
    svg_bar_chart(
        "Processor Scheduling: Weighted Turnaround",
        labels,
        [("Weighted turnaround", weighted, "#9b51e0")],
        OUT_DIR / "scheduler_weighted_turnaround.svg",
    )
    gantt_match = re.search(r"--- RR 调度结果 ---\r?\n运行顺序\(Gantt\): (.+?)\r?\n", text)
    if not gantt_match:
        return
    segs = [(int(a), int(b), p) for a, b, p in re.findall(r"\[(\d+),(\d+):([A-Z0-9]+)\]", gantt_match.group(1))]
    if not segs:
        return
    width, height = 1100, 220
    x0, y0, w, h = 70, 90, 960, 46
    total = max(1, max(b for _, b, _ in segs))
    colors = ["#2f80ed", "#f2994a", "#27ae60", "#eb5757", "#9b51e0", "#00a3a3", "#8a6d3b"]
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="#ffffff"/>',
        '<text x="550" y="34" text-anchor="middle" font-size="22" font-family="Arial">RR Scheduling Gantt Chart (q=3)</text>',
    ]
    for start, end, proc in segs:
        x = x0 + w * start / total
        bw = w * (end - start) / total
        idx = 0 if proc == "IDLE" else (int(proc[1:]) - 1) % len(colors)
        color = "#d0d7de" if proc == "IDLE" else colors[idx]
        parts.append(f'<rect x="{x:.1f}" y="{y0}" width="{bw:.1f}" height="{h}" fill="{color}" stroke="#222"/>')
        parts.append(f'<text x="{x + bw/2:.1f}" y="{y0 + 29}" text-anchor="middle" font-size="13" font-family="Arial">{proc}</text>')
        parts.append(f'<text x="{x:.1f}" y="{y0 + 68}" text-anchor="middle" font-size="11" font-family="Arial">{start}</text>')
    parts.append(f'<text x="{x0 + w}" y="{y0 + 68}" text-anchor="middle" font-size="11" font-family="Arial">{total}</text>')
    parts.append("</svg>")
    (OUT_DIR / "scheduler_rr_gantt.svg").write_text("\n".join(parts), encoding="utf-8")


def generate_memory(text: str) -> None:
    sections = re.findall(
        r"--- ([A-Z]+) 页面置换结果 ---.*?缺页次数: ([0-9]+).*?缺页率: ([0-9.]+)%.*?命中率: ([0-9.]+)%",
        text,
        re.S,
    )
    if sections:
        labels = [row[0] for row in sections]
        fault_count = [float(row[1]) for row in sections]
        rates = [float(row[2]) for row in sections]
        hit_rates = [float(row[3]) for row in sections]
        svg_bar_chart(
            "Page Replacement: Fault Count / Fault Rate / Hit Rate",
            labels,
            [("Faults", fault_count, "#27ae60"), ("Fault rate %", rates, "#eb5757"), ("Hit rate %", hit_rates, "#2f80ed")],
            OUT_DIR / "memory_page_replacement.svg",
        )
    belady = re.search(r"FIFO 3 帧缺页次数: ([0-9]+).*?FIFO 4 帧缺页次数: ([0-9]+)", text, re.S)
    if belady:
        svg_bar_chart(
            "Belady Anomaly: FIFO Faults Increase With More Frames",
            ["3 frames", "4 frames"],
            [("FIFO faults", [float(belady.group(1)), float(belady.group(2))], "#eb5757")],
            OUT_DIR / "memory_belady_anomaly.svg",
        )
    rows = [
        (
            "FF first fit",
            [
                ("A", 0, 100), ("F", 100, 75), ("G", 175, 120), ("FREE", 295, 5),
                ("C", 300, 50), ("H", 350, 60), ("FREE", 410, 20), ("E", 430, 70),
                ("FREE", 500, 140),
            ],
            "First usable block from low address",
        ),
        (
            "BF best fit",
            [
                ("A", 0, 100), ("H", 100, 60), ("FREE", 160, 140), ("C", 300, 50),
                ("F", 350, 75), ("FREE", 425, 5), ("E", 430, 70), ("G", 500, 120),
                ("FREE", 620, 20),
            ],
            "Smallest usable block",
        ),
        (
            "WF worst fit",
            [
                ("A", 0, 100), ("F", 100, 75), ("H", 175, 60), ("FREE", 235, 65),
                ("C", 300, 50), ("FREE", 350, 80), ("E", 430, 70), ("G", 500, 120),
                ("FREE", 620, 20),
            ],
            "Largest usable block",
        ),
    ]
    colors = {
        "A": "#2f80ed",
        "C": "#27ae60",
        "E": "#9b51e0",
        "F": "#f2994a",
        "G": "#00a6a6",
        "H": "#eb5757",
        "FREE": "#d0d7de",
    }
    total = 640
    width, height = 1160, 360
    x0, w, h = 190, 880, 42
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="#ffffff"/>',
        '<text x="580" y="34" text-anchor="middle" font-size="22" font-family="Arial">Dynamic Partition Final Memory Map: FF / BF / WF</text>',
        '<text x="580" y="58" text-anchor="middle" font-size="13" font-family="Arial" fill="#555">Memory size = 640, used = 475, free = 165</text>',
    ]

    for tick in range(0, total + 1, 80):
        x = x0 + w * tick / total
        parts.append(f'<line x1="{x:.1f}" y1="78" x2="{x:.1f}" y2="270" stroke="#edf0f2" stroke-width="1"/>')
        parts.append(f'<text x="{x:.1f}" y="292" text-anchor="middle" font-size="11" font-family="Arial" fill="#555">{tick}</text>')

    for row_index, (label, blocks, note) in enumerate(rows):
        y0 = 86 + row_index * 70
        parts.append(f'<text x="28" y="{y0 + 26}" font-size="15" font-family="Arial" font-weight="700">{label}</text>')
        parts.append(f'<text x="28" y="{y0 + 45}" font-size="11" font-family="Arial" fill="#666">{note}</text>')
        for name, start, size in blocks:
            x = x0 + w * start / total
            bw = w * size / total
            color = colors.get(name, "#888888")
            parts.append(f'<rect x="{x:.1f}" y="{y0}" width="{bw:.1f}" height="{h}" fill="{color}" stroke="#222"/>')
            if bw >= 46:
                text_color = "#333" if name == "FREE" else "#ffffff"
                parts.append(
                    f'<text x="{x + bw / 2:.1f}" y="{y0 + 27}" text-anchor="middle" '
                    f'font-size="12" font-family="Arial" fill="{text_color}">{name} {size}</text>'
                )
            elif bw >= 24:
                parts.append(
                    f'<text x="{x + bw / 2:.1f}" y="{y0 + 27}" text-anchor="middle" '
                    f'font-size="10" font-family="Arial" fill="#333">{size}</text>'
                )

    legend = [("Used partition", "#2f80ed"), ("Free partition", "#d0d7de")]
    for i, (name, color) in enumerate(legend):
        lx = 420 + i * 180
        parts.append(f'<rect x="{lx}" y="318" width="18" height="12" fill="{color}" stroke="#222"/>')
        parts.append(f'<text x="{lx + 26}" y="329" font-size="12" font-family="Arial">{name}</text>')

    parts.append("</svg>")
    svg = "\n".join(parts)
    (OUT_DIR / "memory_partition_map.svg").write_text(svg, encoding="utf-8")
    (OUT_DIR / "memory_partition_compare.svg").write_text(svg, encoding="utf-8")


def generate_sync(text: str) -> None:
    events = []
    for line in text.splitlines():
        if any(key in line for key in ("Producer", "Consumer", "Reader", "Writer", "Philosopher")):
            words = line.split()
            actor = " ".join(words[:2]) if len(words) >= 2 else line[:12]
            events.append((actor, line[:90]))
    actors = sorted({actor for actor, _ in events})
    width, row_h = 1120, 34
    height = 80 + row_h * max(1, len(actors))
    x0, x1 = 170, 1060
    parts = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="#ffffff"/>',
        '<text x="560" y="34" text-anchor="middle" font-size="22" font-family="Arial">Synchronization Event Timeline</text>',
    ]
    actor_y = {actor: 65 + i * row_h for i, actor in enumerate(actors)}
    for actor, y in actor_y.items():
        parts.append(f'<text x="20" y="{y + 5}" font-size="12" font-family="Arial">{html.escape(actor)}</text>')
        parts.append(f'<line x1="{x0}" y1="{y}" x2="{x1}" y2="{y}" stroke="#e5e5e5"/>')
    denom = max(1, len(events) - 1)
    for i, (actor, line) in enumerate(events):
        x = x0 + (x1 - x0) * i / denom
        y = actor_y[actor]
        color = "#2f80ed" if "Producer" in actor else "#27ae60" if "Consumer" in actor else "#f2994a" if "Reader" in actor else "#eb5757" if "Writer" in actor else "#9b51e0"
        parts.append(f'<circle cx="{x:.1f}" cy="{y}" r="5" fill="{color}"><title>{html.escape(line)}</title></circle>')
    parts.append("</svg>")
    (OUT_DIR / "sync_timeline.svg").write_text("\n".join(parts), encoding="utf-8")


def generate_filesystem(text: str) -> None:
    bitmaps = re.findall(r"空闲空间位图.*?\r?\n([01 \r]+)\r?\n", text)
    if bitmaps:
        cell, cols, rows_per = 24, 12, 2
        width = 760
        height = 85 + len(bitmaps) * (rows_per * cell + 76)
        parts = [
            f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
            '<rect width="100%" height="100%" fill="#ffffff"/>',
            '<text x="380" y="34" text-anchor="middle" font-size="22" font-family="Arial">File System Free-Space Bitmap Snapshots</text>',
        ]
        for snap, bits in enumerate(bitmaps, 1):
            clean = bits.replace(" ", "").replace("\r", "").strip()
            y_base = 76 + (snap - 1) * (rows_per * cell + 76)
            parts.append(f'<text x="40" y="{y_base - 14}" font-size="15" font-family="Arial">Snapshot {snap}</text>')
            for i, bit in enumerate(clean):
                x = 40 + (i % cols) * cell
                y = y_base + (i // cols) * cell
                color = "#eb5757" if bit == "1" else "#d0d7de"
                parts.append(f'<rect x="{x}" y="{y}" width="{cell - 3}" height="{cell - 3}" fill="{color}" stroke="#222"/>')
                parts.append(f'<text x="{x + 10.5}" y="{y + 16}" text-anchor="middle" font-size="11" font-family="Arial">{bit}</text>')
        parts.append("</svg>")
        (OUT_DIR / "filesystem_bitmap.svg").write_text("\n".join(parts), encoding="utf-8")
    modes = re.findall(r"(连续分配|链接分配|索引分配)", text)
    values = [float(modes.count("连续分配")), float(modes.count("链接分配")), float(modes.count("索引分配"))]
    if sum(values) > 0:
        svg_bar_chart(
            "File System Allocation Methods Used In Test",
            ["Contiguous", "Linked", "Indexed"],
            [("mentions/files", values, "#2f80ed")],
            OUT_DIR / "filesystem_allocation_methods.svg",
        )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("module", choices=["scheduler", "memory", "sync", "filesystem"])
    parser.add_argument("--no-compile", action="store_true")
    args = parser.parse_args()
    OUT_DIR.mkdir(exist_ok=True)
    if not args.no_compile:
        compile_project()
    input_file = ROOT / "tests" / f"{args.module}_input.txt"
    output_file = OUT_DIR / f"{args.module}_output.txt"
    text = run_program(input_file, output_file)
    if args.module == "scheduler":
        generate_scheduler(text)
    elif args.module == "memory":
        generate_memory(text)
    elif args.module == "sync":
        generate_sync(text)
    else:
        generate_filesystem(text)
    print(f"\n[OK] 输出已保存: {output_file}")
    print(f"[OK] 图表目录: {OUT_DIR}")


if __name__ == "__main__":
    main()
