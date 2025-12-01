# *************** CODE SOURCE DE BIKOURI HENRI **********************

#************* Mon site web : henribikouri.github.io *************************
#*********************Email : henri.bikouri@enspy-uy1.cm ****************************

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from sklearn.model_selection import train_test_split
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import classification_report, confusion_matrix, accuracy_score

# --- CONFIGURATION ---
INPUT_CSV = "dataset_ml_features.csv"
TEST_SIZE = 0.2
RANDOM_STATE = 42

# Mapping des labels (pour l'affichage)
LABEL_MAP = {
    1: 'Camera (UDP)', 2: 'Capteur (TCP)', 3: 'Assistant (TCP)', 
    4: 'Download (TCP)', 5: 'VoIP (UDP)', 6: 'Domotique (UDP)', 
    7: 'Streaming (TCP)', 8: 'Sonnette (TCP)', 9: 'Firmware (TCP)', 
    10: 'Monitoring (UDP)'
}

def train_model():
    print(f"--- Chargement du Dataset : {INPUT_CSV} ---")
    try:
        df = pd.read_csv(INPUT_CSV)
    except FileNotFoundError:
        print("❌ Erreur : Fichier CSV introuvable. Lancez d'abord pcap_to_dataset.py")
        return

    if df.empty:
        print("❌ Erreur : Le dataset est vide.")
        return

    print(f"Nombre d'échantillons (chunks) : {len(df)}")
    print("Aperçu des données :")
    print(df.head())

    
    # On retire l'identifiant CHUNK_ID qui n'est pas une feature prédictive
    X = df.drop(['LABEL', 'CHUNK_ID'], axis=1)
    y = df['LABEL']

    # Division Entraînement / Test
    X_train, X_test, y_train, y_test = train_test_split(
        X, y, test_size=TEST_SIZE, random_state=RANDOM_STATE, stratify=y
    )

    # Entraînement (Random Forest est robuste et performant pour ce type de données)
    print(f"\n--- Entraînement du modèle (Random Forest) ---")
    clf = RandomForestClassifier(n_estimators=100, random_state=RANDOM_STATE)
    clf.fit(X_train, y_train)

    # Prédiction
    y_pred = clf.predict(X_test)

    # Évaluation
    acc = accuracy_score(y_test, y_pred)
    print(f"\n Précision Globale (Accuracy) : {acc*100:.2f}%")

    # Rapport détaillé
    print("\n--- Rapport de Classification ---")
    # On récupère les noms de classes présents dans le test set
    unique_labels = sorted(y_test.unique())
    target_names = [LABEL_MAP.get(label, f"Label {label}") for label in unique_labels]
    
    print(classification_report(y_test, y_pred, target_names=target_names, zero_division=0))

    # Matrice de Confusion (Affichage texte)
    print("\n--- Matrice de Confusion ---")
    cm = confusion_matrix(y_test, y_pred)
    print(cm)

    # Importance des Features
    print("\n--- Importance des Caractéristiques ---")
    feature_imp = pd.Series(clf.feature_importances_, index=X.columns).sort_values(ascending=False)
    print(feature_imp)
    
    # Sauvegarde d'un graphique simple (Feature Importance)
    plt.figure(figsize=(10, 6))
    sns.barplot(x=feature_imp, y=feature_imp.index)
    plt.title("Importance des Features pour la Classification")
    plt.xlabel("Score d'importance")
    plt.ylabel("Caractéristiques")
    plt.tight_layout()
    plt.savefig("feature_importance.png")
    print("\nGraphique 'feature_importance.png' sauvegardé.")

if __name__ == "__main__":
    train_model()
