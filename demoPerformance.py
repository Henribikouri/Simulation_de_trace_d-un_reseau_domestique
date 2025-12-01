# *************** CODE SOURCE DE BIKOURI HENRI **********************

#************* Mon site web : henribikouri.github.io *************************
#*********************Email : henri.bikouri@enspy-uy1.cm ****************************

import pandas as pd
import matplotlib.pyplot as plt
import os

# Configuration
CSV_FILE_PATH = 'metrics_test.csv' 
OUTPUT_PLOT_PATH_THROUGHPUT = 'demo_results_throughput.png' # image de sortie pour debits
OUTPUT_PLOT_PATH_QOS = 'demo_results_qos.png' # images de sortie pour la QoS

# Mapping complet des ports aux applications
PORT_TO_APP_NAME = {
    9001: 'Caméra',
    9002: 'Capteur (Sporadique)',
    9003: 'Assistant Vocal (Rafale)',
    9004: 'Téléchargement (Débit Max)',
    9005: 'VoIP Montante',
    9006: 'VoIP Descendante',
    9007: 'Domotique (Aléatoire)',
    9008: 'Diffusion (Streaming)',
    9009: 'Sonnette (Très Sporadique)',
    9010: 'Mise à Jour Firmware (Inactif)',
    9011: 'Supervision',
}


def analyze_and_plot_results(csv_path):
    """
    Lit le CSV de FlowMonitor, agrège les métriques de Débit, Délai et Perte, 
    et trace deux graphiques.
    """
    if not os.path.exists(csv_path):
        print(f"ERREUR: Le fichier CSV est introuvable à l'adresse: {csv_path}")
        return

    df = pd.read_csv(csv_path)

    # colone utilisees a partir de mon dataset
    THROUGHPUT_COLUMN = 'throughputMbps'
    DELAY_COLUMN = 'meanDelayMs'
    LOSS_COLUMN = 'lossPct'
    
    # Filtrer les débits > 0 pour ne pas inclure les flux complètement inactifs dans les moyennes
    active_df = df[df[THROUGHPUT_COLUMN] > 0]

    if active_df.empty:
        print("Avertissement: Aucun flux actif trouvé (Débit > 0). Vérifiez la durée de la simulation.")
        return

    # Calculs des moyennes par port de destination (dstPort)
    throughput_by_app = active_df.groupby('dstPort')[THROUGHPUT_COLUMN].mean()
    delay_by_app = active_df.groupby('dstPort')[DELAY_COLUMN].mean()
    loss_by_app = active_df.groupby('dstPort')[LOSS_COLUMN].mean()

    # Remplace les ports par des noms d'applications pour l'affichage
    def get_app_names(series_index):
        return [PORT_TO_APP_NAME.get(port, f"Port {port} Inconnu") 
                for port in series_index]

    # --- 1. PLOT DU DÉBIT (THROUGHPUT) ---
    app_names_th = get_app_names(throughput_by_app.index)
    plt.style.use('ggplot')
    plt.figure(figsize=(14, 8))
    colors = ['#1f77b4' if port in [9004, 9008] else '#2ca02c' for port in throughput_by_app.index]
    bars = plt.bar(app_names_th, throughput_by_app.values, color=colors)
    plt.ylabel('Débit Moyen (Mbps)', fontsize=14)
    plt.title("Répartition du Débit Moyen par Application (Wi-Fi 802.11ac)", fontsize=16)
    plt.xticks(rotation=45, ha='right', fontsize=12) 
    for bar in bars:
        yval = bar.get_height()
        plt.text(bar.get_x() + bar.get_width()/2.0, yval + 0.05, f'{yval:.2f}', ha='center', va='bottom', fontsize=10, weight='bold')
    plt.ylim(0, max(throughput_by_app.values) * 1.15)
    plt.tight_layout()
    plt.savefig(OUTPUT_PLOT_PATH_THROUGHPUT)
    print(f"\nGraphique de Débit sauvegardé sous: {OUTPUT_PLOT_PATH_THROUGHPUT}")


    # --- 2. PLOT DES MÉTRIQUES QoS (DÉLAI ET PERTE) ---
    app_names_qos = get_app_names(delay_by_app.index)
    
    fig, ax1 = plt.subplots(figsize=(14, 8))

    # Axe Y1 : Délai Moyen (ms)
    color = '#d62728' # Rouge
    bars_delay = ax1.bar(app_names_qos, delay_by_app.values, color=color, alpha=0.6, label='Délai Moyen (ms)')
    ax1.set_xlabel('Application / Service', fontsize=14)
    ax1.set_ylabel('Délai Moyen (ms)', color=color, fontsize=14)
    ax1.tick_params(axis='y', labelcolor=color)
    ax1.set_xticks(range(len(app_names_qos)))
    ax1.set_xticklabels(app_names_qos, rotation=45, ha='right')

    # Ligne Critique (Seuil pour VoIP et Vidéo : 150-200ms)
    ax1.axhline(y=150, color='r', linestyle='--', linewidth=1, label='Seuil critique de Délai (~150ms)')
    
    # Axe Y2 : Taux de Perte de Paquets (%)
    ax2 = ax1.twinx()  # Créer un second axe Y partageant le même axe X
    color = '#1f77b4' # Bleu
    # J' utilise plot() et non bar() pour la perte pour la distinguer
    line_loss = ax2.plot(app_names_qos, loss_by_app.values, color=color, marker='o', linestyle='-', linewidth=2, label='Perte de Paquets (%)')
    ax2.set_ylabel('Perte de Paquets (%)', color=color, fontsize=14)
    ax2.tick_params(axis='y', labelcolor=color)
    ax2.set_ylim(0, max(max(loss_by_app.values) * 1.5 if loss_by_app.values.any() else 10, 10)) # Échelle de 0% à 10% min

    plt.title("Qualité de Service (QoS) : Délai et Perte de Paquets (Wi-Fi 802.11ac)", fontsize=16)
    fig.tight_layout()
    plt.savefig(OUTPUT_PLOT_PATH_QOS)
    print(f"Graphique de QoS (Délai et Perte) sauvegardé sous: {OUTPUT_PLOT_PATH_QOS}")


if __name__ == "__main__":
    analyze_and_plot_results(CSV_FILE_PATH)
