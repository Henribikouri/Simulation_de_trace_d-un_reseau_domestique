Dataset builder
================

Ce dossier contient un script minimal `build_dataset.py` qui convertit un PCAP (capturé seulement sur l'AP)
en un jeu de données windowed (CSV) utile pour de l'apprentissage supervisé.

Prérequis:
- `tshark` installé et dans le `PATH` (commande `tshark -v` pour vérifier)
- Python 3 avec `pandas` et `numpy` (voir `requirements.txt`)

Exemple d'utilisation:

```
python3 tools/build_dataset.py --pcap traces-simulation-domestique-ap-0.pcap --out dataset_sample.csv --window 1.0
```

Le label est déterminé par le port destination dominant (9001..9011) et mappé en 0..10.

Notes:
- Le script est volontairement simple ; pour des besoins avancés (features L7, TCP flags, etc.)
  il faudra enrichir l'extraction tshark ou utiliser scapy/pyshark.
