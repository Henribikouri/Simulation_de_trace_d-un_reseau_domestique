import csv
import sys
import time
import glob
from collections import defaultdict
from scapy.all import PcapReader, IP, UDP, TCP

# --- CONFIGURATION ---
# On cherche tous les fichiers commençant par trace-ml-ip générés par la simulation
PCAP_PATTERN = "trace-ml-ip-*.pcap" 
OUTPUT_CSV = "dataset_ml_features.csv"
CHUNK_SIZE = 5.0 

# Mapping exact des ports aux labels
PORT_TO_LABEL = {
    9001: 1, 9002: 2, 9003: 3, 9004: 4, 9005: 5, 
    9006: 6, 9007: 7, 9008: 8, 9009: 9, 9010: 10
}

DATASET_HEADER = [
    'LABEL', 'CHUNK_ID', 'NB_PAQUETS', 'VOL_BYTES', 
    'PROTO_TCP_RATIO', 'IAT_MEAN', 'IAT_STD'
]

def process_pcap_files():
    pcap_files = glob.glob(PCAP_PATTERN)
    if not pcap_files:
        print(f"❌ Erreur : Aucun fichier correspondant à '{PCAP_PATTERN}' trouvé.")
        return

    print(f"Traitement de {len(pcap_files)} fichiers de traces IP...")
    final_dataset = []
    total_packets = 0
    chunk_counter = 0

    for pcap_file in pcap_files:
        print(f" -> Lecture : {pcap_file}")
        try:
            reader = PcapReader(pcap_file)
        except:
            continue

        # Stockage temporaire pour ce fichier : { FlowKey : [packets...] }
        # FlowKey = (SrcIP, DstIP, SrcPort, DstPort, Proto)
        flows = defaultdict(list)

        for pkt in reader:
            total_packets += 1
            if IP not in pkt: continue
            
            ip = pkt[IP]
            # Identifier le port d'application (Label)
            label = None
            
            if UDP in ip:
                sport, dport = ip[UDP].sport, ip[UDP].dport
                proto_code = 0 # UDP
            elif TCP in ip:
                sport, dport = ip[TCP].sport, ip[TCP].dport
                proto_code = 1 # TCP
            else:
                continue

            # Vérifier si c'est un flux d'intérêt
            if dport in PORT_TO_LABEL: label = PORT_TO_LABEL[dport]
            elif sport in PORT_TO_LABEL: label = PORT_TO_LABEL[sport]
            
            if label:
                # Clé unique pour grouper par flux et par fenêtre de temps (Chunk)
                chunk_idx = int(ip.time // CHUNK_SIZE)
                flow_key = (label, chunk_idx)
                
                flows[flow_key].append({
                    'time': float(ip.time),
                    'len': len(pkt),
                    'proto': proto_code
                })
        
        # Agrégation des chunks pour ce fichier
        for (label, chunk_idx), packets in flows.items():
            if not packets: continue
            
            # Calculs statistiques simples et robustes
            count = len(packets)
            vol = sum(p['len'] for p in packets)
            tcp_count = sum(p['proto'] for p in packets)
            
            # IAT (Inter-Arrival Time)
            times = sorted([p['time'] for p in packets])
            diffs = [t2 - t1 for t1, t2 in zip(times, times[1:])]
            
            if diffs:
                iat_mean = sum(diffs) / len(diffs)
                # Variance simple
                mean = iat_mean
                var = sum((x - mean) ** 2 for x in diffs) / len(diffs)
                iat_std = var ** 0.5
            else:
                iat_mean = 0.0
                iat_std = 0.0

            final_dataset.append({
                'LABEL': label,
                'CHUNK_ID': f"chunk_{chunk_idx}",
                'NB_PAQUETS': count,
                'VOL_BYTES': vol,
                'PROTO_TCP_RATIO': tcp_count / count,
                'IAT_MEAN': iat_mean,
                'IAT_STD': iat_std
            })
            chunk_counter += 1

    # Écriture CSV
    if final_dataset:
        with open(OUTPUT_CSV, 'w', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=DATASET_HEADER)
            writer.writeheader()
            writer.writerows(final_dataset)
        print(f"\n✅ Succès ! {chunk_counter} chunks générés dans '{OUTPUT_CSV}'")
    else:
        print("❌ Aucun chunk généré. Vérifiez que la simulation a tourné.")

if __name__ == "__main__":
    process_pcap_files()