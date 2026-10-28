import argparse
import csv
import math
from pathlib import Path
import subprocess
import sys

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


def simulate(r_kohm, c_uf, vref, period_ms):
    tau = r_kohm * c_uf / 1000
    dt = period_ms / 1000
    charge_at = 0.5
    discharge_at = charge_at + 5 * tau
    duration = 1 + 10 * tau
    voltage = 0.0
    rows = []
    for i in range(math.ceil(duration / dt) + 1):
        moment = i * dt
        if i:
            left = (i - 1) * dt
            boundaries = [left]
            boundaries.extend(t for t in (charge_at, discharge_at) if left < t < moment)
            boundaries.append(moment)
            for start, end in zip(boundaries, boundaries[1:]):
                target = vref if charge_at <= start < discharge_at else 0.0
                voltage = target + (voltage - target) * math.exp(-(end - start) / tau)
        code = min(4095, max(0, round(voltage / vref * 4096)))
        rows.append((moment, code * vref / 4096, 27.0))
    return rows


def main():
    parser = argparse.ArgumentParser(description="Математическая симуляция RC-цепи, без Pico")
    parser.add_argument("--r", type=float, default=10, help="кОм")
    parser.add_argument("--c", type=float, default=100, help="мкФ")
    parser.add_argument("--vref", type=float, default=3.3)
    parser.add_argument("--period", type=int, default=10, help="мс")
    args = parser.parse_args()
    if not all(math.isfinite(x) and x > 0 for x in (args.r, args.c, args.vref)):
        parser.error("номиналы и опорное напряжение должны быть положительными и конечными")
    if args.period < 1:
        parser.error("период должен быть не меньше 1 мс")
    output = Path(__file__).resolve().parent / "simulation"
    output.mkdir(exist_ok=True)
    rows = simulate(args.r, args.c, args.vref, args.period)
    with (output / "rc.csv").open("w", newline="", encoding="utf-8") as stream:
        writer = csv.writer(stream)
        writer.writerow(["time_s", "voltage_v", "temp_c", "source"])
        for moment, voltage, temp in rows:
            writer.writerow([f"{moment:.6f}", f"{voltage:.3f}", f"{temp:.1f}", "simulation"])
    subprocess.run([sys.executable, str(output.parent / "rc.py"), "--r", str(args.r),
                    "--c", str(args.c), "--plot", "rc.csv"], cwd=output, check=True)
    with (output / "rc.log").open("a", encoding="utf-8") as stream:
        stream.write("source = simulation\nmodel = ideal RC, 12-bit ADC quantization\n")
        stream.write("temperature = fixed model value, not measured\n")
    fig, ax = plt.subplots(figsize=(8, 4.5))
    ax.plot([r[0] for r in rows], [r[1] for r in rows])
    ax.set(title=f"СИМУЛЯЦИЯ RC: R={args.r:g} кОм, C={args.c:g} мкФ",
           xlabel="Время, с", ylabel="Напряжение модели, В")
    ax.grid(True)
    fig.tight_layout()
    fig.savefig(output / "rc.png")
    plt.close(fig)
    print("Результаты симуляции: " + str(output))


if __name__ == "__main__":
    main()
