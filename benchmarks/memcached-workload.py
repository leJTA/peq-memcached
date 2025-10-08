#!/usr/bin/env python3
"""
memcache_workload.py

Génère un ensemble de données et exécute des requêtes get/set aléatoires
avec pymemcache. Usage example:

    python memcache_workload.py --host localhost --port 11211 --ops 20000 --threads 8

"""

import argparse
import random
import string
import time

from concurrent.futures import ThreadPoolExecutor, as_completed
from pymemcache.client.base import Client


def generate_payload(size_bytes):
    """Génère un bytes de taille size_bytes."""
    # Génère une chaîne répétitive basée sur alphanumérics pour lisibilité.
    # b = "".join(random.choices(string.ascii_letters + string.digits, k=size_bytes))
    base = "".join(
        random.choices(string.ascii_letters + string.digits, k=1024)
    ).encode("ascii")
    reps = (size_bytes // len(base)) + 1
    return (base * reps)[:size_bytes]


def worker_thread(host, port, keys, data_list, ops_per_thread, set_ratio, thread_id):
    """
    Thread worker : effectue des opérations GET/SET sur Memcached.
    Retourne un dict de statistiques.
    """
    client = Client((host, port), connect_timeout=1, timeout=2)
    stats = {
        "gets": 0,
        "sets": 0,
        "hits": 0,
        "misses": 0,
        "get_time": 0.0,
        "set_time": 0.0,
        "errors": 0,
    }
    try:
        for _ in range(ops_per_thread):
            idx = random.randrange(len(keys))
            key = keys[idx]
            if random.random() < set_ratio:
                # set
                payload = data_list[idx]
                t0 = time.perf_counter()
                try:
                    client.set(key, payload)
                except Exception:
                    stats["errors"] += 1
                t1 = time.perf_counter()
                stats["sets"] += 1
                stats["set_time"] += t1 - t0
            else:
                # get
                t0 = time.perf_counter()
                try:
                    value = client.get(key)
                except Exception:
                    stats["errors"] += 1
                else:
                    if value is not None:
                        stats["hits"] += 1
                    else:
                        stats["misses"] += 1
                t1 = time.perf_counter()
                stats["gets"] += 1
                stats["get_time"] += t1 - t0
    finally:
        try:
            client.close()
        except Exception:
            pass
    return stats


def main():
    parser = argparse.ArgumentParser(
        description="Workload memcached get/set avec pymemcache"
    )
    parser.add_argument("--host", default="localhost", help="Host memcached")
    parser.add_argument("--port", type=int, default=11211, help="Port memcached")
    parser.add_argument(
        "--num-items", type=int, default=1000, help="Nombre d'items à générer"
    )
    parser.add_argument(
        "--item-size-kib", type=int, default=256, help="Taille d'un item en KiB"
    )
    parser.add_argument(
        "--ops", type=int, default=10000, help="Nombre total d'opérations (get+set)"
    )
    parser.add_argument(
        "--set-ratio",
        type=float,
        default=0.1,
        help="Fraction des ops qui sont des set (0..1)",
    )
    parser.add_argument("--threads", type=int, default=4, help="Nombre de threads")
    parser.add_argument(
        "--seed", type=int, default=None, help="Seed aléatoire pour génération stable"
    )
    args = parser.parse_args()

    num_items = args.num_items
    item_size = args.item_size_kib * 1024  # KiB -> bytes
    total_ops = args.ops
    set_ratio = args.set_ratio
    threads = max(1, args.threads)
    ops_per_thread = total_ops // threads
    remaining = total_ops - ops_per_thread * threads

    print(
        f"Paramètres : num_items={num_items}, item_size={item_size} bytes ({args.item_size_kib} KiB), "
        f"total_ops={total_ops}, set_ratio={set_ratio}, threads={threads}"
    )
    print("Génération des clés et des payloads (ça peut prendre quelques secondes)...")

    # Génération des clés
    keys = [f"key_{i:04d}" for i in range(num_items)]

    # Génération des données
    data_list = []
    for i in range(num_items):
        payload = generate_payload(item_size)
        data_list.append(payload)
    print(
        "Données générées. Total payload RAM approximative: "
        f"{(num_items * item_size) / (1024*1024):.2f} MiB"
    )

    # Lancer les threads
    print("Démarrage du workload...")
    start = time.perf_counter()
    results = []
    with ThreadPoolExecutor(max_workers=threads) as ex:
        futures = []
        for t in range(threads):
            ops_for_this = ops_per_thread + (1 if t < remaining else 0)
            futures.append(
                ex.submit(
                    worker_thread,
                    args.host,
                    args.port,
                    keys,
                    data_list,
                    ops_for_this,
                    set_ratio,
                    t,
                )
            )
        for future in as_completed(futures):
            results.append(future.result())
    elapsed = time.perf_counter() - start

    # Agrégation
    total_gets = sum(r["gets"] for r in results)
    total_sets = sum(r["sets"] for r in results)
    total_hits = sum(r["hits"] for r in results)
    total_misses = sum(r["misses"] for r in results)
    total_errors = sum(r["errors"] for r in results)
    total_get_time = sum(r["get_time"] for r in results)
    total_set_time = sum(r["set_time"] for r in results)

    hit_ratio = (total_hits / total_gets * 100) if total_gets else 0.0

    # Affichage final
    print("\n--- Résumé ---")
    print(f"Durée totale        : {elapsed:.3f} s")
    print(f"Opérations totales  : {total_gets + total_sets}")
    print(
        f"  GETs  : {total_gets} ({hit_ratio:.1f}% hits, {total_hits} hits / {total_misses} miss)"
    )
    print(f"  SETs  : {total_sets}")
    print(f"Erreurs             : {total_errors}")
    if total_gets:
        print(f"Latence moyenne GET : {(total_get_time / total_gets) * 1000:.3f} ms")
    if total_sets:
        print(f"Latence moyenne SET : {(total_set_time / total_sets) * 1000:.3f} ms")
    print(f"Throughput moyen    : {(total_gets + total_sets) / elapsed:.1f} ops/s\n")


if __name__ == "__main__":
    main()
