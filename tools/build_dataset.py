#!/usr/bin/env python3
"""
Simple PCAP -> windowed CSV dataset builder.

Requirements: tshark (in PATH), python packages in requirements.txt

Usage:
  python3 build_dataset.py --pcap traces-simulation-domestique-ap-0.pcap --out dataset_sample.csv --window 1.0

This script extracts basic packet fields using tshark, groups packets into fixed time windows
and computes per-window features (pkt_count, byte_count, mean_pkt_len, iat_mean, etc.).
Label is assigned by dominant destination port (9001..9011) when present.
"""
import argparse
import subprocess
import sys
import os
import pandas as pd
import numpy as np

def extract_tshark_fields(pcap, fields=None):
    if fields is None:
        fields = ["frame.time_epoch","ip.src","ip.dst","tcp.srcport","tcp.dstport","udp.srcport","udp.dstport","frame.len"]
    cmd = ["tshark","-r",pcap,"-T","fields"]
    for f in fields:
        cmd += ["-e", f]
    cmd += ["-E","separator=,","-E","quote=d","-E","occurrence=f" ]
    proc = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
    if proc.returncode != 0:
        print("tshark failed:", proc.stderr, file=sys.stderr)
        raise SystemExit(1)
    data = []
    for line in proc.stdout.splitlines():
        parts = [p if p != "" else None for p in line.split(',')]
        # ensure length
        while len(parts) < len(fields):
            parts.append(None)
        data.append(parts)
    df = pd.DataFrame(data, columns=fields)
    return df

def normalize_ports(df):
    # prefer tcp ports, else udp
    df['srcport'] = df['tcp.srcport'].combine_first(df['udp.srcport'])
    df['dstport'] = df['tcp.dstport'].combine_first(df['udp.dstport'])
    df['frame.len'] = pd.to_numeric(df['frame.len'], errors='coerce').fillna(0).astype(int)
    df['frame.time_epoch'] = pd.to_numeric(df['frame.time_epoch'], errors='coerce')
    return df

def window_and_aggregate(df, window_sec=1.0):
    df = df.dropna(subset=['frame.time_epoch']).copy()
    t0 = df['frame.time_epoch'].min()
    df['win'] = ((df['frame.time_epoch'] - t0) // window_sec).astype(int)
    groups = df.groupby('win')
    rows = []
    for win, g in groups:
        pkt_count = len(g)
        byte_count = int(g['frame.len'].sum())
        mean_pkt = float(g['frame.len'].mean()) if pkt_count>0 else 0.0
        std_pkt = float(g['frame.len'].std()) if pkt_count>1 else 0.0
        times = g['frame.time_epoch'].sort_values().values
        iat = np.diff(times) if len(times)>1 else np.array([0.0])
        iat_mean = float(iat.mean()) if len(iat)>0 else 0.0
        iat_std = float(iat.std()) if len(iat)>1 else 0.0
        # dominant destination port by bytes
        by_port = g.groupby('dstport')['frame.len'].sum()
        if len(by_port)>0:
            dominant_port = int(by_port.idxmax()) if pd.notnull(by_port.idxmax()) else -1
        else:
            dominant_port = -1
        rows.append({
            'win': int(win),
            'pkt_count': int(pkt_count),
            'byte_count': int(byte_count),
            'mean_pkt_len': mean_pkt,
            'std_pkt_len': std_pkt,
            'iat_mean': iat_mean,
            'iat_std': iat_std,
            'dominant_dst_port': int(dominant_port)
        })
    out = pd.DataFrame(rows)
    return out

def port_to_label(port):
    # map ports 9001..9011 to labels 0..10, else -1
    try:
        p = int(port)
    except Exception:
        return -1
    if 9001 <= p <= 9011:
        return p - 9001
    return -1

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--pcap', required=True)
    parser.add_argument('--out', required=True)
    parser.add_argument('--window', type=float, default=1.0)
    args = parser.parse_args()

    if not os.path.exists(args.pcap):
        print('PCAP not found:', args.pcap, file=sys.stderr)
        raise SystemExit(1)

    print('Extraction tshark...')
    df = extract_tshark_fields(args.pcap)
    df = normalize_ports(df)
    ds = window_and_aggregate(df, window_sec=args.window)
    ds['label'] = ds['dominant_dst_port'].apply(port_to_label)
    ds.to_csv(args.out, index=False)
    print('Saved dataset to', args.out)

if __name__ == '__main__':
    main()
