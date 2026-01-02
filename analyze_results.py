import os
import glob
import matplotlib.pyplot as plt
import numpy as np

# --- SETTINGS ---
RESULTS_DIR = "simulations/results"
OUTPUT_DIR = "analysis_output"

if not os.path.exists(OUTPUT_DIR):
    os.makedirs(OUTPUT_DIR)

plt.style.use('bmh') 

def get_latest_file(extension):
    """Finds the most recently created .sca or .vec file."""
    files = glob.glob(f"{RESULTS_DIR}/*{extension}")
    if not files: return None
    return max(files, key=os.path.getmtime)

def parse_scalars(filepath):
    stats = {
        "sat_throughput": {},
        "sat_pdr": {},
        "gs_throughput": {},
        "gs_pdr": {},
        "gs_drops": {},
        "gs_sent": {}
    }
    
    print(f"[READING] Scalar File: {filepath}")
    with open(filepath, 'r') as f:
        for line in f:
            line = line.strip()
            if line.startswith("scalar"):
                parts = line.split()
                if len(parts) < 4: continue
                module_full_name = parts[1] 
                stat_name = parts[2].replace('"', '')
                value = float(parts[3])
                
                # Parse Satellite Data
                if "sat[" in module_full_name:
                    name = module_full_name.split('.')[-1]
                    if stat_name == "Throughput_bps":
                        stats["sat_throughput"][name] = value / 1_000_000.0
                    elif stat_name == "PacketDeliveryRatio":
                        stats["sat_pdr"][name] = value * 100.0
                        
                # Parse Ground Station Data
                elif any(city in module_full_name for city in ["istanbul", "sivas", "kastamonu", "ordu", "giresun", "tokat", "erzurum", "malatya", "samsun", "trabzon", "sinop"]):
                    name = module_full_name.split('.')[-1]
                    
                    if stat_name == "Throughput_bps":
                        stats["gs_throughput"][name] = value / 1_000_000.0
                    elif stat_name == "PacketsReceived":
                        if name not in stats["gs_pdr"]: stats["gs_pdr"][name] = {"rx": 0, "sent": 0}
                        stats["gs_pdr"][name]["rx"] = value
                    elif stat_name == "PacketsSent":
                        if name not in stats["gs_pdr"]: stats["gs_pdr"][name] = {"rx": 0, "sent": 0}
                        stats["gs_pdr"][name]["sent"] = value
                        stats["gs_sent"][name] = value
                    elif stat_name == "PacketsDropped":
                        stats["gs_drops"][name] = value

    return stats

def parse_vectors(filepath):
    sat_delays = []
    istanbul_delays = []
    vector_map = {} 
    
    print(f"[READING] Vector File: {filepath}")
    with open(filepath, 'r') as f:
        for line in f:
            if line.startswith("vector"):
                parts = line.split()
                vec_id = parts[1]
                module = parts[2]
                name = parts[3]
                
                if "endToEndDelay" in name:
                    if "istanbul" in module:
                        vector_map[vec_id] = "istanbul"
                    elif "sat" in module:
                        vector_map[vec_id] = "sat"
            
            elif line[0].isdigit():
                parts = line.split()
                vec_id = parts[0]
                if vec_id in vector_map:
                    val = float(parts[3]) * 1000.0 # ms
                    if vector_map[vec_id] == "istanbul":
                        istanbul_delays.append(val)
                    else:
                        sat_delays.append(val)

    return sat_delays, istanbul_delays

def plot_bar(data, title, ylabel, filename, color, ylim=None):
    if not data: return
    names = list(data.keys())
    values = list(data.values())
    
    plt.figure(figsize=(12, 6))
    bars = plt.bar(names, values, color=color, edgecolor='black')
    plt.title(title, fontsize=14)
    plt.ylabel(ylabel, fontsize=12)
    plt.xticks(rotation=45)
    plt.grid(True, axis='y', linestyle='--', alpha=0.7)
    if ylim: plt.ylim(0, ylim)
    
    for bar in bars:
        yval = bar.get_height()
        plt.text(bar.get_x() + bar.get_width()/2, yval, f'{yval:.1f}', ha='center', va='bottom')

    plt.tight_layout()
    plt.savefig(f"{OUTPUT_DIR}/{filename}")
    plt.close()

def plot_efficiency(sent_data, drops_data):
    if not sent_data: return
    efficiency_map = {city: (sent / (sent + drops_data.get(city, 0)) * 100 if (sent + drops_data.get(city, 0)) > 0 else 0) 
                     for city, sent in sent_data.items()}
    plot_bar(efficiency_map, "Ground Station Link Efficiency (Access Success)", "Efficiency (%)", "Link_Efficiency.png", "#16a085", ylim=110)

def plot_delay_hist(delays, title, filename):
    if not delays: 
        print(f"WARNING: No delay data found for {title}")
        return
    
    # --- DETAILED DEBUG PRINT ---
    total_packets = len(delays)
    avg = np.mean(delays)
    min_delay = np.min(delays)
    max_delay = np.max(delays)
    
    print(f"\n--- DEBUG: {title} ---")
    print(f"Total Packets Plotted: {total_packets}")
    print(f"Min Delay: {min_delay:.2f} ms")
    print(f"Max Delay: {max_delay:.2f} ms")
    print(f"Avg Delay: {avg:.2f} ms")
    
    # Percentile Logic
    limit_99 = np.percentile(delays, 99)
    if limit_99 < 50: limit_99 = 100
    print(f"Zoom Limit (99%): {limit_99:.2f} ms")

    plt.figure(figsize=(10, 6))
    
    # Get histogram data to print bin counts
    counts, bins, _ = plt.hist(delays, bins=50, range=(0, limit_99), color='#9b59b6', edgecolor='black', alpha=0.8)
    
    print(f"Max Packet Count in a Single Bin: {int(max(counts))}")
    print("----------------------------\n")
    
    plt.axvline(avg, color='r', linestyle='--', label=f'Avg: {avg:.2f} ms')
    plt.title(title, fontsize=14)
    plt.xlabel("Delay (ms)")
    plt.ylabel("Packet Count")
    plt.legend()
    plt.savefig(f"{OUTPUT_DIR}/{filename}")
    plt.close()

def main():
    print("--- Ultimate Network Analysis Started ---\n")
    
    sca = get_latest_file(".sca")
    if sca:
        stats = parse_scalars(sca)
        plot_bar(stats["sat_throughput"], "Satellite Backbone Throughput", "Throughput (Mbps)", "Sat_Throughput.png", "#3498db")
        plot_bar(stats["sat_pdr"], "Satellite Packet Delivery Ratio (Congestion)", "Success Rate (%)", "Sat_PDR.png", "#e74c3c", ylim=110)
        plot_bar(stats["gs_throughput"], "Delivered City Throughput (Actual)", "Throughput (Mbps)", "City_Throughput.png", "#e67e22")
        plot_efficiency(stats["gs_sent"], stats["gs_drops"])
        
        if "istanbul" in stats["gs_throughput"]:
            print(f"=== ISTANBUL GATEWAY SUMMARY ===")
            print(f"Received Throughput: {stats['gs_throughput']['istanbul']:.2f} Mbps")
            sent = stats['gs_sent'].get('istanbul', 0)
            drops = stats['gs_drops'].get('istanbul', 0)
            rx = stats['gs_pdr']['istanbul']['rx'] if 'istanbul' in stats['gs_pdr'] else 0
            
            total_attempts = sent + drops
            eff = (sent / total_attempts * 100) if total_attempts > 0 else 0
            
            print(f"Total Attempts (Sent+Drop): {int(total_attempts)}")
            print(f"Packets Sent to Sat: {int(sent)}")
            print(f"Packets Received (Rx): {int(rx)}")
            print(f"Connectivity Drops: {int(drops)}")
            print(f"Link Efficiency: %{eff:.2f}")
            
    vec = get_latest_file(".vec")
    if vec:
        sat_d, ist_d = parse_vectors(vec)
        plot_delay_hist(ist_d, "Istanbul End-to-End Delay Distribution", "Istanbul_Delay.png")

    print("\n--- Analysis Completed. Check 'analysis_output' folder. ---")

if __name__ == "__main__":
    main()
