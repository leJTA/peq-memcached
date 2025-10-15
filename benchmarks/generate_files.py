#!/usr/bin/env python3
"""
generate_files.py
-----------------
Génère un ensemble de fichiers binaires contenant des données aléatoires.
Par défaut : 1000 fichiers de 256 KiB chacun, nommés key_0000, key_0001, etc.

Usage :
    python generate_files.py --output-dir data --num-items 1000 --item-size-kib 256
"""

import os
import argparse
import random
import string


def generate_payload(size_bytes):
    """Génère une séquence pseudo-aléatoire de taille size_bytes."""
    rnd = random.Random(0)
    base = "".join(rnd.choices(string.ascii_letters + string.digits, k=1024)).encode(
        "ascii"
    )
    reps = (size_bytes // len(base)) + 1
    return (base * reps)[:size_bytes]


def main():
    parser = argparse.ArgumentParser(
        description="Génère des fichiers binaires aléatoires nommés par clé"
    )
    parser.add_argument("--output-dir", default="data", help="Répertoire de sortie")
    parser.add_argument(
        "--num-items", type=int, default=1000, help="Nombre de fichiers à générer"
    )
    parser.add_argument(
        "--item-size-kib", type=int, default=256, help="Taille de chaque fichier en KiB"
    )
    parser.add_argument(
        "--seed", type=int, default=None, help="Seed aléatoire (optionnel)"
    )
    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)
    size_bytes = args.item_size_kib * 1024
    rng = random.Random(args.seed)

    print(
        f"Génération de {args.num_items} fichiers de {args.item_size_kib} KiB dans '{args.output_dir}' ..."
    )

    for i in range(args.num_items):
        key = f"key_{i:04d}"
        filepath = os.path.join(args.output_dir, key)
        payload = generate_payload(size_bytes)
        with open(filepath, "wb") as f:
            f.write(payload)

    total_mb = args.num_items * size_bytes / (1024 * 1024)
    print(f"Terminé. {args.num_items} fichiers écrits ({total_mb:.2f} MiB au total).")


if __name__ == "__main__":
    main()
