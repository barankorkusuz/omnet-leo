#!/usr/bin/env python3
"""
LEO Satellite Network Simulation Results Analyzer
Parses OMNeT++ .sca and .vec files and generates analysis graphs.
"""

import os
import sys
from pathlib import Path
from collections import defaultdict
import matplotlib.pyplot as plt
import numpy as np

# Configuration
RESULTS_DIR = Path(__file__).parent / "simulations" / "results"
OUTPUT_DIR = Path(__file__).parent / "analysis_output"
SCA_FILE = RESULTS_DIR / "TurkeyCoverage-#0.sca"
VEC_FILE = RESULTS_DIR / "TurkeyCoverage-#0.vec"

# Ensure output directory exists
OUTPUT_DIR.mkdir(exist_ok=True)

# Plot style
plt.style.use("seaborn-v0_8-whitegrid")
plt.rcParams["figure.figsize"] = (12, 6)
plt.rcParams["font.size"] = 11
plt.rcParams["axes.titlesize"] = 14
plt.rcParams["axes.labelsize"] = 12


def parse_sca_file(filepath):
    """
    Parse OMNeT++ scalar (.sca) file.
    Returns dict with module names as keys and their scalar values.
    """
    print(f"[INFO] Parsing scalar file: {filepath}")

    scalars = defaultdict(dict)

    with open(filepath, "r") as f:
        for line in f:
            line = line.strip()
            if line.startswith("scalar "):
                # Format: scalar <module> <name> <value>
                parts = line.split()
                if len(parts) >= 4:
                    module = parts[1]
                    name = parts[2]
                    value = float(parts[3])
                    scalars[module][name] = value

    print(f"[INFO] Found {len(scalars)} modules with scalar data")
    return dict(scalars)


def parse_vec_file_sampled(filepath, sample_size=50000):
    """
    Parse OMNeT++ vector (.vec) file with sampling.
    The file can be very large (GB), so we sample data points.
    Returns dict with vector names and their sampled values.
    """
    print(f"[INFO] Parsing vector file (sampled): {filepath}")
    print(f"[INFO] Target sample size per vector: {sample_size}")

    vectors = {}  # vector_id -> {module, name, values}

    # First pass: get vector definitions
    with open(filepath, "r") as f:
        for line in f:
            if line.startswith("vector "):
                # Format: vector <id> <module> <name> <columns>
                parts = line.strip().split()
                if len(parts) >= 4:
                    vec_id = int(parts[1])
                    module = parts[2]
                    name = parts[3]
                    vectors[vec_id] = {"module": module, "name": name, "values": []}
            elif line and line[0].isdigit():
                # Data line: <vector_id> <event_num> <time> <value>
                # But file is huge, so we'll do reservoir sampling
                break

    print(f"[INFO] Found {len(vectors)} vector definitions")

    # Second pass: reservoir sampling for data
    # Count lines first to determine sampling rate
    file_size = os.path.getsize(filepath)
    print(f"[INFO] Vector file size: {file_size / (1024**3):.2f} GB")

    # Use reservoir sampling with limited memory
    reservoir = defaultdict(list)
    counts = defaultdict(int)

    print("[INFO] Sampling vector data (this may take a while)...")

    with open(filepath, "r") as f:
        for line_num, line in enumerate(f):
            if line_num % 5000000 == 0 and line_num > 0:
                print(f"[INFO] Processed {line_num:,} lines...")

            if not line or not line[0].isdigit():
                continue

            parts = line.strip().split("\t")
            if len(parts) >= 4:
                try:
                    vec_id = int(parts[0])
                    time = float(parts[2])
                    value = float(parts[3])

                    counts[vec_id] += 1

                    # Reservoir sampling
                    if len(reservoir[vec_id]) < sample_size:
                        reservoir[vec_id].append((time, value))
                    else:
                        # Randomly replace with decreasing probability
                        j = np.random.randint(0, counts[vec_id])
                        if j < sample_size:
                            reservoir[vec_id][j] = (time, value)
                except (ValueError, IndexError):
                    continue

    # Attach sampled data to vectors
    for vec_id, samples in reservoir.items():
        if vec_id in vectors:
            vectors[vec_id]["values"] = [v for t, v in samples]
            vectors[vec_id]["times"] = [t for t, v in samples]
            vectors[vec_id]["total_count"] = counts[vec_id]

    print(
        f"[INFO] Sampling complete. Total data points processed: {sum(counts.values()):,}"
    )

    return vectors


def extract_ground_station_metrics(scalars):
    """Extract metrics for ground stations."""
    ground_stations = {}

    gs_names = [
        "istanbul",
        "sivas",
        "kastamonu",
        "ordu",
        "giresun",
        "tokat",
        "erzurum",
        "malatya",
        "samsun",
        "trabzon",
        "sinop",
    ]

    hometown_names = [gs for gs in gs_names if gs != "istanbul"]

    # First pass: collect raw data
    raw_data = {}
    for gs in gs_names:
        module = f"LEONetwork.{gs}"
        if module in scalars:
            data = scalars[module]
            raw_data[gs] = {
                "sent": data.get("PacketsSent", 0),
                "received": data.get("PacketsReceived", 0),
                "dropped": data.get("PacketsDropped", 0),
                "throughput_bps": data.get("Throughput_bps", 0),
            }

    # Calculate proper PDR based on traffic model:
    # - Istanbul sends to hometowns -> hometowns receive from Istanbul
    # - Hometowns send to Istanbul -> Istanbul receives from hometowns

    istanbul_sent = raw_data.get("istanbul", {}).get("sent", 0)
    istanbul_received = raw_data.get("istanbul", {}).get("received", 0)

    # Total packets received by hometowns (sent by Istanbul)
    hometowns_total_received = sum(
        raw_data.get(gs, {}).get("received", 0) for gs in hometown_names
    )
    # Total packets sent by hometowns (to Istanbul)
    hometowns_total_sent = sum(
        raw_data.get(gs, {}).get("sent", 0) for gs in hometown_names
    )

    # Istanbul's sender PDR: what % of Istanbul's packets reached hometowns
    istanbul_sender_pdr = (
        (hometowns_total_received / istanbul_sent * 100) if istanbul_sent > 0 else 0
    )

    # Hometowns' collective sender PDR: what % of hometown packets reached Istanbul
    hometowns_sender_pdr = (
        (istanbul_received / hometowns_total_sent * 100)
        if hometowns_total_sent > 0
        else 0
    )

    for gs in gs_names:
        if gs not in raw_data:
            continue

        data = raw_data[gs]
        sent = data["sent"]
        received = data["received"]
        dropped = data["dropped"]
        throughput = data["throughput_bps"]

        if gs == "istanbul":
            # Istanbul's PDR = how many of its sent packets were delivered
            pdr = istanbul_sender_pdr
        else:
            # Individual hometown PDR approximation
            # Since all hometowns send to Istanbul, we use collective PDR
            # but weight by this station's contribution
            pdr = hometowns_sender_pdr

        total_attempted = sent + dropped
        ground_stations[gs] = {
            "sent": sent,
            "received": received,
            "dropped": dropped,
            "throughput_mbps": throughput / 1e6,
            "pdr": min(pdr, 100.0),  # Cap at 100%
            "drop_rate": (dropped / total_attempted * 100)
            if total_attempted > 0
            else 0,
        }

    return ground_stations


def extract_satellite_metrics(scalars):
    """Extract metrics for satellites."""
    satellites = {}

    # 18 satellites: 3 planes x 6 satellites
    for i in range(18):
        module = f"LEONetwork.sat[{i}]"
        if module in scalars:
            data = scalars[module]
            forwarded = data.get("PacketsForwarded", 0)
            dropped = data.get("PacketsDropped", 0)
            throughput = data.get("ForwardThroughput_bps", 0)
            success_rate = data.get("ForwardSuccessRate", 0)

            # Determine plane number for grouping
            plane = i // 6

            satellites[f"sat[{i}]"] = {
                "forwarded": forwarded,
                "dropped": dropped,
                "throughput_mbps": throughput / 1e6,
                "success_rate": success_rate * 100,
                "drop_rate": (dropped / (forwarded + dropped) * 100)
                if (forwarded + dropped) > 0
                else 0,
                "plane": plane,
            }

    return satellites


def extract_delay_vectors(vectors):
    """Extract end-to-end delay data from vectors."""
    delays = {}

    for vec_id, vec_data in vectors.items():
        if vec_data["name"] == "endToEndDelay":
            module = vec_data["module"]
            # Extract station name from module (e.g., LEONetwork.istanbul -> istanbul)
            station = module.split(".")[-1]
            if vec_data["values"]:
                # Convert to milliseconds
                delays[station] = [v * 1000 for v in vec_data["values"]]

    return delays


def extract_hop_count_vectors(vectors):
    """Extract hop count data from vectors."""
    hop_counts = {}

    for vec_id, vec_data in vectors.items():
        if vec_data["name"] == "hopCount":
            module = vec_data["module"]
            sat_name = module.split(".")[-1]
            if vec_data["values"]:
                hop_counts[sat_name] = vec_data["values"]

    return hop_counts


# ============== PLOTTING FUNCTIONS ==============


def plot_pdr_comparison(ground_stations):
    """Plot Packet Delivery Ratio for all ground stations."""
    print("[INFO] Generating PDR comparison plot...")

    stations = list(ground_stations.keys())
    pdr_values = [ground_stations[s]["pdr"] for s in stations]

    # Sort by PDR
    sorted_pairs = sorted(zip(stations, pdr_values), key=lambda x: x[1], reverse=True)
    stations, pdr_values = zip(*sorted_pairs)

    fig, ax = plt.subplots(figsize=(12, 6))

    colors = plt.cm.RdYlGn(np.array(pdr_values) / 100)
    bars = ax.bar(stations, pdr_values, color=colors, edgecolor="black", linewidth=0.5)

    ax.set_xlabel("Ground Station")
    ax.set_ylabel("Packet Delivery Ratio (%)")
    ax.set_title("Packet Delivery Ratio (PDR) by Ground Station")
    ax.set_ylim(0, 100)

    # Add value labels on bars
    for bar, val in zip(bars, pdr_values):
        ax.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height() + 1,
            f"{val:.1f}%",
            ha="center",
            va="bottom",
            fontsize=9,
        )

    # Add average line
    avg_pdr = np.mean(pdr_values)
    ax.axhline(
        y=avg_pdr,
        color="red",
        linestyle="--",
        linewidth=2,
        label=f"Average: {avg_pdr:.1f}%",
    )
    ax.legend()

    plt.xticks(rotation=45, ha="right")
    plt.tight_layout()
    plt.savefig(OUTPUT_DIR / "01_pdr_comparison.png", dpi=150)
    plt.close()
    print("[INFO] Saved: 01_pdr_comparison.png")


def plot_throughput_comparison(ground_stations):
    """Plot throughput for all ground stations."""
    print("[INFO] Generating throughput comparison plot...")

    stations = list(ground_stations.keys())
    throughput = [ground_stations[s]["throughput_mbps"] for s in stations]

    # Sort by throughput
    sorted_pairs = sorted(zip(stations, throughput), key=lambda x: x[1], reverse=True)
    stations, throughput = zip(*sorted_pairs)

    fig, ax = plt.subplots(figsize=(12, 6))

    colors = plt.cm.Blues(np.linspace(0.4, 0.9, len(stations)))
    bars = ax.bar(stations, throughput, color=colors, edgecolor="black", linewidth=0.5)

    ax.set_xlabel("Ground Station")
    ax.set_ylabel("Throughput (Mbps)")
    ax.set_title("Received Throughput by Ground Station")

    # Add value labels
    for bar, val in zip(bars, throughput):
        ax.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height() + 0.1,
            f"{val:.1f}",
            ha="center",
            va="bottom",
            fontsize=9,
        )

    plt.xticks(rotation=45, ha="right")
    plt.tight_layout()
    plt.savefig(OUTPUT_DIR / "02_throughput_comparison.png", dpi=150)
    plt.close()
    print("[INFO] Saved: 02_throughput_comparison.png")


def plot_packet_stats(ground_stations):
    """Plot sent/received/dropped packets as grouped bar chart."""
    print("[INFO] Generating packet statistics plot...")

    stations = list(ground_stations.keys())
    sent = [ground_stations[s]["sent"] / 1e6 for s in stations]
    received = [ground_stations[s]["received"] / 1e6 for s in stations]
    dropped = [ground_stations[s]["dropped"] / 1e6 for s in stations]

    _, ax = plt.subplots(figsize=(14, 6))

    x = np.arange(len(stations))
    width = 0.25

    ax.bar(x - width, sent, width, label="Sent", color="#3498db", edgecolor="black", linewidth=0.5)
    ax.bar(x, received, width, label="Received", color="#2ecc71", edgecolor="black", linewidth=0.5)
    ax.bar(x + width, dropped, width, label="Dropped", color="#e74c3c", edgecolor="black", linewidth=0.5)

    ax.set_xlabel("Ground Station")
    ax.set_ylabel("Packets (Millions)")
    ax.set_title("Packet Statistics by Ground Station")
    ax.set_xticks(x)
    ax.set_xticklabels(stations, rotation=45, ha="right")
    ax.legend()

    plt.tight_layout()
    plt.savefig(OUTPUT_DIR / "03_packet_stats.png", dpi=150)
    plt.close()
    print("[INFO] Saved: 03_packet_stats.png")


def plot_satellite_performance(satellites):
    """Plot satellite forwarding performance."""
    print("[INFO] Generating satellite performance plot...")

    sat_names = list(satellites.keys())
    success_rates = [satellites[s]["success_rate"] for s in sat_names]
    throughputs = [satellites[s]["throughput_mbps"] for s in sat_names]

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))

    # Success Rate
    colors1 = plt.cm.RdYlGn(np.array(success_rates) / 100)
    bars1 = ax1.bar(
        sat_names, success_rates, color=colors1, edgecolor="black", linewidth=0.5
    )
    ax1.set_xlabel("Satellite")
    ax1.set_ylabel("Forward Success Rate (%)")
    ax1.set_title("Satellite Forward Success Rate")
    min_success = min(success_rates) if success_rates else 0
    ax1.set_ylim(max(0, min_success - 5), 100)

    for bar, val in zip(bars1, success_rates):
        ax1.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height() + 0.3,
            f"{val:.1f}%",
            ha="center",
            va="bottom",
            fontsize=10,
        )

    # Throughput
    colors2 = plt.cm.Oranges(np.linspace(0.4, 0.9, len(sat_names)))
    bars2 = ax2.bar(
        sat_names, throughputs, color=colors2, edgecolor="black", linewidth=0.5
    )
    ax2.set_xlabel("Satellite")
    ax2.set_ylabel("Forward Throughput (Mbps)")
    ax2.set_title("Satellite Forward Throughput")

    for bar, val in zip(bars2, throughputs):
        ax2.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height() + 1,
            f"{val:.1f}",
            ha="center",
            va="bottom",
            fontsize=10,
        )

    plt.tight_layout()
    plt.savefig(OUTPUT_DIR / "04_satellite_performance.png", dpi=150)
    plt.close()
    print("[INFO] Saved: 04_satellite_performance.png")


def plot_delay_histogram(delays):
    """Plot end-to-end delay histogram for all stations."""
    print("[INFO] Generating delay histogram...")

    fig, ax = plt.subplots(figsize=(12, 6))

    all_delays = []
    for station, delay_values in delays.items():
        all_delays.extend(delay_values)

    if not all_delays:
        print("[WARNING] No delay data found!")
        return

    # Filter outliers (keep 99th percentile)
    percentile_99 = np.percentile(all_delays, 99)
    filtered_delays = [d for d in all_delays if d <= percentile_99]

    ax.hist(
        filtered_delays,
        bins=50,
        color="#3498db",
        edgecolor="black",
        linewidth=0.5,
        alpha=0.7,
    )

    ax.set_xlabel("End-to-End Delay (ms)")
    ax.set_ylabel("Frequency")
    ax.set_title("End-to-End Delay Distribution (All Stations)")

    # Add statistics
    mean_delay = np.mean(filtered_delays)
    median_delay = np.median(filtered_delays)
    p95_delay = np.percentile(filtered_delays, 95)

    stats_text = f"Mean: {mean_delay:.2f} ms\nMedian: {median_delay:.2f} ms\n95th %ile: {p95_delay:.2f} ms"
    ax.text(
        0.95,
        0.95,
        stats_text,
        transform=ax.transAxes,
        fontsize=11,
        verticalalignment="top",
        horizontalalignment="right",
        bbox=dict(boxstyle="round", facecolor="wheat", alpha=0.5),
    )

    ax.axvline(
        x=mean_delay,
        color="red",
        linestyle="--",
        linewidth=2,
        label=f"Mean: {mean_delay:.2f} ms",
    )
    ax.axvline(
        x=median_delay,
        color="green",
        linestyle="--",
        linewidth=2,
        label=f"Median: {median_delay:.2f} ms",
    )
    ax.legend()

    plt.tight_layout()
    plt.savefig(OUTPUT_DIR / "05_delay_histogram.png", dpi=150)
    plt.close()
    print("[INFO] Saved: 05_delay_histogram.png")


def plot_delay_cdf(delays):
    """Plot CDF of end-to-end delay."""
    print("[INFO] Generating delay CDF plot...")

    fig, ax = plt.subplots(figsize=(12, 6))

    colors = plt.cm.tab10(np.linspace(0, 1, len(delays)))

    for (station, delay_values), color in zip(delays.items(), colors):
        if not delay_values:
            continue

        sorted_delays = np.sort(delay_values)
        cdf = np.arange(1, len(sorted_delays) + 1) / len(sorted_delays)

        # Downsample for plotting if too many points
        if len(sorted_delays) > 1000:
            indices = np.linspace(0, len(sorted_delays) - 1, 1000, dtype=int)
            sorted_delays = sorted_delays[indices]
            cdf = cdf[indices]

        ax.plot(
            sorted_delays, cdf, label=station.capitalize(), linewidth=1.5, color=color
        )

    ax.set_xlabel("End-to-End Delay (ms)")
    ax.set_ylabel("CDF")
    ax.set_title("Cumulative Distribution Function of End-to-End Delay")
    ax.set_xlim(
        0,
        np.percentile([d for delays_list in delays.values() for d in delays_list], 99),
    )
    ax.legend(loc="lower right", ncol=2)
    ax.grid(True, alpha=0.3)

    plt.tight_layout()
    plt.savefig(OUTPUT_DIR / "06_delay_cdf.png", dpi=150)
    plt.close()
    print("[INFO] Saved: 06_delay_cdf.png")


def plot_delay_boxplot(delays):
    """Plot boxplot of delays per station."""
    print("[INFO] Generating delay boxplot...")

    fig, ax = plt.subplots(figsize=(14, 6))

    # Prepare data
    stations = list(delays.keys())
    delay_data = [delays[s] for s in stations]

    # Filter to 95th percentile for visualization
    max_val = np.percentile([d for data in delay_data for d in data], 95)
    delay_data_filtered = [[d for d in data if d <= max_val] for data in delay_data]

    bp = ax.boxplot(
        delay_data_filtered,
        labels=[s.capitalize() for s in stations],
        patch_artist=True,
    )

    colors = plt.cm.Set3(np.linspace(0, 1, len(stations)))
    for patch, color in zip(bp["boxes"], colors):
        patch.set_facecolor(color)

    ax.set_xlabel("Ground Station")
    ax.set_ylabel("End-to-End Delay (ms)")
    ax.set_title(
        "End-to-End Delay Distribution by Ground Station (up to 95th percentile)"
    )

    plt.xticks(rotation=45, ha="right")
    plt.tight_layout()
    plt.savefig(OUTPUT_DIR / "07_delay_boxplot.png", dpi=150)
    plt.close()
    print("[INFO] Saved: 07_delay_boxplot.png")


def plot_drop_rate_comparison(ground_stations, satellites):
    """Plot drop rate comparison for both ground stations and satellites."""
    print("[INFO] Generating drop rate comparison plot...")

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5))

    # Ground stations
    gs_names = list(ground_stations.keys())
    gs_drop_rates = [ground_stations[s]["drop_rate"] for s in gs_names]

    sorted_pairs = sorted(
        zip(gs_names, gs_drop_rates), key=lambda x: x[1], reverse=True
    )
    gs_names, gs_drop_rates = zip(*sorted_pairs)

    max_gs_drop = max(gs_drop_rates) if max(gs_drop_rates) > 0 else 1
    colors1 = plt.cm.Reds(np.array(gs_drop_rates) / max_gs_drop * 0.8 + 0.2)
    ax1.bar(gs_names, gs_drop_rates, color=colors1, edgecolor="black", linewidth=0.5)
    ax1.set_xlabel("Ground Station")
    ax1.set_ylabel("Drop Rate (%)")
    ax1.set_title("Packet Drop Rate - Ground Stations")
    ax1.tick_params(axis="x", rotation=45)

    # Satellites
    sat_names = list(satellites.keys())
    sat_drop_rates = [satellites[s]["drop_rate"] for s in sat_names]

    max_sat_drop = max(sat_drop_rates) if max(sat_drop_rates) > 0 else 1
    colors2 = plt.cm.Reds(np.array(sat_drop_rates) / max_sat_drop * 0.8 + 0.2)
    ax2.bar(sat_names, sat_drop_rates, color=colors2, edgecolor="black", linewidth=0.5)
    ax2.set_xlabel("Satellite")
    ax2.set_ylabel("Drop Rate (%)")
    ax2.set_title("Packet Drop Rate - Satellites")

    for ax in [ax1, ax2]:
        for bar in ax.patches:
            height = bar.get_height()
            ax.text(
                bar.get_x() + bar.get_width() / 2,
                height + 0.5,
                f"{height:.1f}%",
                ha="center",
                va="bottom",
                fontsize=9,
            )

    plt.tight_layout()
    plt.savefig(OUTPUT_DIR / "08_drop_rate_comparison.png", dpi=150)
    plt.close()
    print("[INFO] Saved: 08_drop_rate_comparison.png")


def plot_network_summary(ground_stations, satellites):
    """Plot overall network summary."""
    print("[INFO] Generating network summary plot...")

    fig, axes = plt.subplots(2, 2, figsize=(14, 10))

    # 1. Total packets pie chart
    ax1 = axes[0, 0]
    total_received = sum(gs["received"] for gs in ground_stations.values())
    total_dropped_gs = sum(gs["dropped"] for gs in ground_stations.values())
    total_dropped_sat = sum(sat["dropped"] for sat in satellites.values())

    labels = ["Delivered", "Dropped (GS)", "Dropped (Sat)"]
    sizes = [total_received, total_dropped_gs, total_dropped_sat]
    colors = ["#2ecc71", "#e74c3c", "#c0392b"]
    explode = (0.02, 0.02, 0.02)

    ax1.pie(
        sizes,
        explode=explode,
        labels=labels,
        colors=colors,
        autopct="%1.1f%%",
        shadow=True,
        startangle=90,
    )
    ax1.set_title("Overall Packet Delivery Status")

    # 2. Throughput summary
    ax2 = axes[0, 1]
    gs_throughput = sum(gs["throughput_mbps"] for gs in ground_stations.values())
    sat_throughput = sum(sat["throughput_mbps"] for sat in satellites.values())

    bars = ax2.bar(
        ["Ground Stations\n(Received)", "Satellites\n(Forwarded)"],
        [gs_throughput, sat_throughput],
        color=["#3498db", "#e67e22"],
        edgecolor="black",
        linewidth=0.5,
    )
    ax2.set_ylabel("Total Throughput (Mbps)")
    ax2.set_title("Aggregate Throughput")

    for bar in bars:
        ax2.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height() + 5,
            f"{bar.get_height():.1f}",
            ha="center",
            va="bottom",
            fontsize=11,
        )

    # 3. Average metrics
    ax3 = axes[1, 0]
    avg_pdr = np.mean([gs["pdr"] for gs in ground_stations.values()])
    avg_gs_drop = np.mean([gs["drop_rate"] for gs in ground_stations.values()])
    avg_sat_success = np.mean([sat["success_rate"] for sat in satellites.values()])
    avg_sat_drop = np.mean([sat["drop_rate"] for sat in satellites.values()])

    metrics = [
        "Avg PDR\n(GS)",
        "Avg Drop Rate\n(GS)",
        "Avg Success Rate\n(Sat)",
        "Avg Drop Rate\n(Sat)",
    ]
    values = [avg_pdr, avg_gs_drop, avg_sat_success, avg_sat_drop]
    colors = ["#2ecc71", "#e74c3c", "#3498db", "#e67e22"]

    bars = ax3.bar(metrics, values, color=colors, edgecolor="black", linewidth=0.5)
    ax3.set_ylabel("Percentage (%)")
    ax3.set_title("Average Network Metrics")
    ax3.set_ylim(0, 100)

    for bar in bars:
        ax3.text(
            bar.get_x() + bar.get_width() / 2,
            bar.get_height() + 1,
            f"{bar.get_height():.1f}%",
            ha="center",
            va="bottom",
            fontsize=10,
        )

    # 4. Per-station traffic volume
    ax4 = axes[1, 1]
    stations = list(ground_stations.keys())
    sent_millions = [ground_stations[s]["sent"] / 1e6 for s in stations]

    # Sort by sent
    sorted_pairs = sorted(
        zip(stations, sent_millions), key=lambda x: x[1], reverse=True
    )
    stations, sent_millions = zip(*sorted_pairs)

    colors = plt.cm.viridis(np.linspace(0.2, 0.8, len(stations)))
    ax4.barh(stations, sent_millions, color=colors, edgecolor="black", linewidth=0.5)
    ax4.set_xlabel("Packets Sent (Millions)")
    ax4.set_title("Traffic Volume by Ground Station")
    ax4.invert_yaxis()

    plt.tight_layout()
    plt.savefig(OUTPUT_DIR / "09_network_summary.png", dpi=150)
    plt.close()
    print("[INFO] Saved: 09_network_summary.png")


def plot_istanbul_vs_others(ground_stations):
    """Compare Istanbul (hub) with other stations."""
    print("[INFO] Generating Istanbul vs Others comparison...")

    _, axes = plt.subplots(1, 3, figsize=(15, 5))

    istanbul = ground_stations.get("istanbul", {})
    others = {k: v for k, v in ground_stations.items() if k != "istanbul"}

    # Metrics comparison
    metrics = ["Sent", "Received", "Dropped"]
    istanbul_vals = [
        istanbul.get("sent", 0) / 1e6,
        istanbul.get("received", 0) / 1e6,
        istanbul.get("dropped", 0) / 1e6,
    ]
    others_avg = [
        np.mean([gs["sent"] for gs in others.values()]) / 1e6,
        np.mean([gs["received"] for gs in others.values()]) / 1e6,
        np.mean([gs["dropped"] for gs in others.values()]) / 1e6,
    ]

    x = np.arange(len(metrics))
    width = 0.35

    ax1 = axes[0]
    ax1.bar(
        x - width / 2, istanbul_vals, width, label="Istanbul (Hub)", color="#e74c3c"
    )
    ax1.bar(x + width / 2, others_avg, width, label="Others (Avg)", color="#3498db")
    ax1.set_ylabel("Packets (Millions)")
    ax1.set_title("Packet Volume: Istanbul vs Others")
    ax1.set_xticks(x)
    ax1.set_xticklabels(metrics)
    ax1.legend()

    # PDR comparison
    ax2 = axes[1]
    istanbul_pdr = istanbul.get("pdr", 0)
    others_pdr = [gs["pdr"] for gs in others.values()]

    ax2.bar(
        ["Istanbul"], [istanbul_pdr], color="#e74c3c", edgecolor="black", linewidth=0.5
    )
    ax2.bar(
        ["Others (Avg)"],
        [np.mean(others_pdr)],
        color="#3498db",
        edgecolor="black",
        linewidth=0.5,
    )
    ax2.bar(
        ["Others (Max)"],
        [max(others_pdr)],
        color="#2ecc71",
        edgecolor="black",
        linewidth=0.5,
    )
    ax2.bar(
        ["Others (Min)"],
        [min(others_pdr)],
        color="#f39c12",
        edgecolor="black",
        linewidth=0.5,
    )
    ax2.set_ylabel("PDR (%)")
    ax2.set_title("PDR Comparison")
    ax2.set_ylim(0, 100)

    # Throughput comparison
    ax3 = axes[2]
    istanbul_tp = istanbul.get("throughput_mbps", 0)
    others_tp = [gs["throughput_mbps"] for gs in others.values()]

    ax3.bar(
        ["Istanbul"], [istanbul_tp], color="#e74c3c", edgecolor="black", linewidth=0.5
    )
    ax3.bar(
        ["Others (Avg)"],
        [np.mean(others_tp)],
        color="#3498db",
        edgecolor="black",
        linewidth=0.5,
    )
    ax3.set_ylabel("Throughput (Mbps)")
    ax3.set_title("Throughput Comparison")

    plt.tight_layout()
    plt.savefig(OUTPUT_DIR / "10_istanbul_vs_others.png", dpi=150)
    plt.close()
    print("[INFO] Saved: 10_istanbul_vs_others.png")


def generate_summary_report(ground_stations, satellites, delays):
    """Generate a text summary report."""
    print("[INFO] Generating summary report...")

    report_lines = []
    report_lines.append("=" * 70)
    report_lines.append("LEO SATELLITE NETWORK SIMULATION - ANALYSIS REPORT")
    report_lines.append("=" * 70)
    report_lines.append("")

    # Overall statistics
    total_sent = sum(gs["sent"] for gs in ground_stations.values())
    total_received = sum(gs["received"] for gs in ground_stations.values())
    total_dropped_gs = sum(gs["dropped"] for gs in ground_stations.values())
    total_dropped_sat = sum(sat["dropped"] for sat in satellites.values())
    total_attempted = total_sent + total_dropped_gs
    overall_pdr = (total_received / total_attempted * 100) if total_attempted > 0 else 0

    report_lines.append("OVERALL NETWORK STATISTICS")
    report_lines.append("-" * 40)
    report_lines.append(f"Total Packets Sent:        {total_sent:>15,}")
    report_lines.append(f"Total Packets Received:    {total_received:>15,}")
    report_lines.append(f"Total Packets Dropped (GS):{total_dropped_gs:>15,}")
    report_lines.append(f"Total Packets Dropped (Sat):{total_dropped_sat:>14,}")
    report_lines.append(f"Overall PDR:               {overall_pdr:>14.2f}%")
    report_lines.append("")

    # Ground station statistics
    report_lines.append("GROUND STATION STATISTICS")
    report_lines.append("-" * 40)
    report_lines.append(
        f"{'Station':<12} {'PDR':>8} {'Throughput':>12} {'Drop Rate':>10}"
    )
    report_lines.append(f"{'':12} {'(%)':>8} {'(Mbps)':>12} {'(%)':>10}")
    report_lines.append("-" * 42)

    for station, data in sorted(
        ground_stations.items(), key=lambda x: x[1]["pdr"], reverse=True
    ):
        report_lines.append(
            f"{station.capitalize():<12} {data['pdr']:>7.2f}% {data['throughput_mbps']:>11.2f} {data['drop_rate']:>9.2f}%"
        )

    report_lines.append("")

    # Satellite statistics
    report_lines.append("SATELLITE STATISTICS")
    report_lines.append("-" * 40)
    report_lines.append(
        f"{'Satellite':<10} {'Success Rate':>13} {'Throughput':>12} {'Drop Rate':>10}"
    )
    report_lines.append(f"{'':10} {'(%)':>13} {'(Mbps)':>12} {'(%)':>10}")
    report_lines.append("-" * 45)

    for sat, data in satellites.items():
        report_lines.append(
            f"{sat:<10} {data['success_rate']:>12.2f}% {data['throughput_mbps']:>11.2f} {data['drop_rate']:>9.2f}%"
        )

    report_lines.append("")

    # Delay statistics
    if delays:
        all_delays = [d for delays_list in delays.values() for d in delays_list]
        report_lines.append("END-TO-END DELAY STATISTICS")
        report_lines.append("-" * 40)
        report_lines.append(
            f"Mean Delay:                {np.mean(all_delays):>10.2f} ms"
        )
        report_lines.append(
            f"Median Delay:              {np.median(all_delays):>10.2f} ms"
        )
        report_lines.append(
            f"Std Dev:                   {np.std(all_delays):>10.2f} ms"
        )
        report_lines.append(
            f"Min Delay:                 {np.min(all_delays):>10.2f} ms"
        )
        report_lines.append(
            f"Max Delay:                 {np.max(all_delays):>10.2f} ms"
        )
        report_lines.append(
            f"95th Percentile:           {np.percentile(all_delays, 95):>10.2f} ms"
        )
        report_lines.append(
            f"99th Percentile:           {np.percentile(all_delays, 99):>10.2f} ms"
        )

    report_lines.append("")
    report_lines.append("=" * 70)
    report_lines.append("END OF REPORT")
    report_lines.append("=" * 70)

    report_text = "\n".join(report_lines)

    # Save report
    report_path = OUTPUT_DIR / "analysis_report.txt"
    with open(report_path, "w") as f:
        f.write(report_text)

    print("[INFO] Saved: analysis_report.txt")
    print("")
    print(report_text)


def main():
    """Main analysis function."""
    print("=" * 60)
    print("LEO SATELLITE NETWORK - RESULTS ANALYZER")
    print("=" * 60)
    print("")

    # Check if result files exist
    if not SCA_FILE.exists():
        print(f"[ERROR] Scalar file not found: {SCA_FILE}")
        print("[INFO] Please run the simulation first.")
        sys.exit(1)

    # Parse scalar file
    scalars = parse_sca_file(SCA_FILE)

    # Extract metrics
    ground_stations = extract_ground_station_metrics(scalars)
    satellites = extract_satellite_metrics(scalars)

    print(f"[INFO] Found {len(ground_stations)} ground stations")
    print(f"[INFO] Found {len(satellites)} satellites")
    print("")

    # Parse vector file (if exists and not too large)
    delays = {}
    if VEC_FILE.exists():
        file_size_gb = os.path.getsize(VEC_FILE) / (1024**3)
        if file_size_gb > 5:
            print(f"[WARNING] Vector file is very large ({file_size_gb:.2f} GB)")
            print("[INFO] Sampling will be used for delay analysis")

        vectors = parse_vec_file_sampled(VEC_FILE, sample_size=50000)
        delays = extract_delay_vectors(vectors)
        print(f"[INFO] Extracted delay data for {len(delays)} stations")
    else:
        print(f"[WARNING] Vector file not found: {VEC_FILE}")
        print("[INFO] Delay analysis will be skipped")

    print("")
    print("=" * 60)
    print("GENERATING PLOTS")
    print("=" * 60)
    print("")

    # Generate all plots
    plot_pdr_comparison(ground_stations)
    plot_throughput_comparison(ground_stations)
    plot_packet_stats(ground_stations)
    plot_satellite_performance(satellites)

    if delays:
        plot_delay_histogram(delays)
        plot_delay_cdf(delays)
        plot_delay_boxplot(delays)

    plot_drop_rate_comparison(ground_stations, satellites)
    plot_network_summary(ground_stations, satellites)
    plot_istanbul_vs_others(ground_stations)

    print("")
    print("=" * 60)
    print("SUMMARY REPORT")
    print("=" * 60)
    print("")

    # Generate summary report
    generate_summary_report(ground_stations, satellites, delays)

    print("")
    print("=" * 60)
    print(f"[INFO] All outputs saved to: {OUTPUT_DIR}")
    print("[INFO] Analysis complete!")
    print("=" * 60)


if __name__ == "__main__":
    main()
