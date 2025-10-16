#!/usr/bin/env python3
"""
memcache_workload.py

Generates a dataset and performs random get/set requests
against a Memcached server using pymemcache.

Example usage:

    python memcache_workload.py --host localhost --port 11211 --ops 20000 --threads 8
"""

import argparse
import random
import string
import time
import statistics
import numpy as np

from concurrent.futures import ThreadPoolExecutor, as_completed
from pymemcache.client.base import Client


def generate_payload(size_bytes):
    """Generate a random bytes payload of size size_bytes."""
    random.seed(0)
    base = "".join(random.choices(string.ascii_letters + string.digits, k=1024)).encode(
        "ascii"
    )
    reps = (size_bytes // len(base)) + 1
    return (base * reps)[:size_bytes]


def percentile(data, p):
    """Return the p-th percentile value (e.g., p=95 -> P95)."""
    if not data:
        return 0.0
    k = (len(data) - 1) * (p / 100.0)
    f = int(k)
    c = min(f + 1, len(data) - 1)
    if f == c:
        return data[int(k)]
    d0 = data[f] * (c - k)
    d1 = data[c] * (k - f)
    return d0 + d1


def latency_stats(data):
    """Compute latency statistics: mean, P50, P95, P99."""
    if not data:
        return (0, 0, 0, 0)
    data_sorted = sorted(data)
    mean = statistics.mean(data_sorted)
    p50 = percentile(data_sorted, 50)
    p95 = percentile(data_sorted, 95)
    p99 = percentile(data_sorted, 99)
    return (mean, p50, p95, p99)


def precompute_zipf_indices(n_items, n_ops, s):
    """
    Precompute access indices following a truncated Zipf distribution in [0, n_items-1].
    """
    np.random.seed(0)
    raw = np.random.zipf(s, n_ops)
    raw = raw % n_items
    return raw


def worker_thread(
    host, port, keys, data_list, access_indices, ops_per_thread, set_ratio, thread_id
):
    """
    Worker thread: performs GET/SET operations on Memcached.
    Returns a statistics dictionary.
    """
    client = Client((host, port), connect_timeout=1, timeout=2)
    stats = {
        "gets": 0,
        "sets": 0,
        "hits": 0,
        "misses": 0,
        "get_latency": [],
        "set_latency": [],
        "errors": 0,
    }

    try:
        for idx in access_indices:
            key = keys[idx]
            if random.random() < set_ratio:
                # SET operation
                payload = data_list[idx]
                t0 = time.perf_counter()
                try:
                    client.set(key, payload)
                except Exception:
                    stats["errors"] += 1
                t1 = time.perf_counter()
                stats["sets"] += 1
                stats["set_latency"].append((t1 - t0) * 1000)
            else:
                # GET operation
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
                stats["get_latency"].append((t1 - t0) * 1000)
    finally:
        try:
            client.close()
        except Exception:
            pass

    return stats


def main():
    parser = argparse.ArgumentParser(
        description="Memcached workload generator (GET/SET) using pymemcache"
    )
    parser.add_argument("--host", default="localhost", help="Memcached host")
    parser.add_argument("--port", type=int, default=11211, help="Memcached port")
    parser.add_argument(
        "--num-items", type=int, default=1000, help="Number of items to generate"
    )
    parser.add_argument(
        "--item-size-kib", type=int, default=256, help="Item size in KiB"
    )
    parser.add_argument(
        "--ops", type=int, default=10000, help="Total number of operations (get+set)"
    )
    parser.add_argument(
        "--set-ratio",
        type=float,
        default=0.1,
        help="Fraction of operations that are SET (0..1)",
    )
    parser.add_argument("--threads", type=int, default=4, help="Number of threads")
    parser.add_argument(
        "--zipf-s", type=float, default=1.16, help="Zipf distribution parameter s (>0)"
    )
    args = parser.parse_args()

    num_items = args.num_items
    item_size = args.item_size_kib * 1024  # KiB → bytes
    total_ops = args.ops
    set_ratio = args.set_ratio
    threads = max(1, args.threads)
    ops_per_thread = total_ops // threads
    remaining = total_ops - ops_per_thread * threads

    print(f"\n--- Parameters ---")
    print(f"  Host = {args.host}:{args.port}")
    print(
        f"  Items = {num_items}, size = {args.item_size_kib} KiB, total ops = {total_ops}"
    )
    print(f"  SET ratio = {set_ratio*100:.1f}%, threads = {threads}")
    print(f"  Zipf (a = {args.zipf_s})")
    print("Generating keys and payloads (this may take a few seconds)...")

    # Data generation
    keys = [f"key_{i:04d}" for i in range(num_items)]
    data_list = []
    for i in range(num_items):
        payload = generate_payload(item_size)
        data_list.append(payload)
    print(
        f"Data generated. Approx. memory footprint: {(num_items * item_size) / (1024*1024):.2f} MiB"
    )

    # Zipf distribution
    print("Preparing Zipf access pattern...")
    access_indices = precompute_zipf_indices(num_items, total_ops, args.zipf_s)

    # Split indices among threads
    chunks = []
    start = 0
    for t in range(threads):
        ops_for_thread = ops_per_thread + (1 if t < remaining else 0)
        end = start + ops_for_thread
        chunks.append(access_indices[start:end])
        start = end

    # Run workload
    print("Starting workload...")
    start_time = time.perf_counter()
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
                    chunks[t],
                    ops_for_this,
                    set_ratio,
                    t,
                )
            )
        for future in as_completed(futures):
            results.append(future.result())
    elapsed = time.perf_counter() - start_time

    # Aggregate results
    total_gets = sum(r["gets"] for r in results)
    total_sets = sum(r["sets"] for r in results)
    total_hits = sum(r["hits"] for r in results)
    total_misses = sum(r["misses"] for r in results)
    total_errors = sum(r["errors"] for r in results)
    all_get_latency = [lat for r in results for lat in r["get_latency"]]
    all_set_latency = [lat for r in results for lat in r["set_latency"]]

    hit_ratio = (total_hits / total_gets * 100) if total_gets else 0.0
    mean_g, p50_g, p95_g, p99_g = latency_stats(all_get_latency)
    mean_s, p50_s, p95_s, p99_s = latency_stats(all_set_latency)

    # Display summary
    print("\n--- Summary ---")
    print(f"Total duration     : {elapsed:.3f} s")
    print(f"Total operations   : {total_gets + total_sets}")
    print(
        f"  GETs  : {total_gets} ({hit_ratio:.1f}% hits, {total_hits} hits / {total_misses} misses)"
    )
    print(f"  SETs  : {total_sets}")
    print(f"Errors             : {total_errors}")
    print(f"Average throughput : {(total_gets + total_sets) / elapsed:.1f} ops/s\n")

    if all_get_latency:
        print("GET latencies (ms):")
        print(f"  Mean : {mean_g:.3f}")
        print(f"  P50  : {p50_g:.3f}")
        print(f"  P95  : {p95_g:.3f}")
        print(f"  P99  : {p99_g:.3f}")
    if all_set_latency:
        print("\nSET latencies (ms):")
        print(f"  Mean : {mean_s:.3f}")
        print(f"  P50  : {p50_s:.3f}")
        print(f"  P95  : {p95_s:.3f}")
        print(f"  P99  : {p99_s:.3f}")
    print("")


if __name__ == "__main__":
    main()
