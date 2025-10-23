#!/usr/bin/env python3
"""
memcache_workload.py

Generates a dataset and performs random GET requests
to a filecache server using HTTP. Usage example:

    python memcache_workload.py --host localhost --port 11211 --ops 20000 --threads 4
"""

import argparse
import time
import asyncio
import aiohttp
import statistics
import numpy as np


def percentile(data, p):
    """Return the value of percentile p (e.g., p=95 -> P95)."""
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
    """Compute mean, P50, P95, P99 latency statistics."""
    if not data:
        return (0, 0, 0, 0)
    data_sorted = sorted(data)
    mean = statistics.mean(data_sorted)
    p50 = percentile(data_sorted, 50)
    p95 = percentile(data_sorted, 95)
    p99 = percentile(data_sorted, 99)
    return (mean, p50, p95, p99)


def precompute_indices(n_items, n_ops, a):
    """
    Precompute access indices following a pareto distribution,
    truncated to [0, n_items-1].
    """
    np.random.seed(0)
    raw = (np.random.pareto(a, n_ops) % n_items).astype(int)
    return raw


async def worker(session, host, port, keys, access_indices, results):
    """Worker coroutine: performs GET requests and measures latencies."""
    latencies = []
    errors = 0
    ok = 0

    for idx in access_indices:
        key = keys[idx]
        url = f"http://{host}:{port}/{key}"
        t0 = time.perf_counter()
        try:
            async with session.get(url) as resp:
                if resp.status == 200:
                    await resp.read()
                    ok += 1
                else:
                    errors += 1
        except Exception:
            errors += 1
        t1 = time.perf_counter()
        latencies.append((t1 - t0) * 1000)

    results.append({"ok": ok, "errors": errors, "latencies": latencies})


async def main_async(args):
    num_items = args.num_items
    total_ops = args.ops
    threads = max(1, args.threads)
    ops_per_thread = total_ops // threads
    remaining = total_ops - ops_per_thread * threads

    print(f"\n--- Parameters ---")
    print(f"  file server = {args.host}:{args.port}")
    print(f"  Skewness    = {args.skewness}\n")

    # Generate keys
    print("Generating keys...")
    keys = [f"key_{i:06d}" for i in range(num_items)]
    access_indices = precompute_indices(num_items, total_ops, args.skewness)

    # Split indices across threads
    print("Distributing indices across workers...")
    chunks = []
    start = 0
    for t in range(threads):
        ops_for_thread = ops_per_thread + (1 if t < remaining else 0)
        end = start + ops_for_thread
        chunks.append(access_indices[start:end])
        start = end

    connector = aiohttp.TCPConnector(limit_per_host=threads * 2)
    timeout = aiohttp.ClientTimeout(total=None, sock_connect=5, sock_read=30)
    results = []

    # Launch workload
    print("Starting workload...")
    start_time = time.perf_counter()
    async with aiohttp.ClientSession(connector=connector, timeout=timeout) as session:
        tasks = [
            worker(session, args.host, args.port, keys, chunks[t], results)
            for t in range(threads)
        ]
        await asyncio.gather(*tasks)
    elapsed = time.perf_counter() - start_time

    total_ok = sum(r["ok"] for r in results)
    total_errors = sum(r["errors"] for r in results)
    all_latencies = [lat for r in results for lat in r["latencies"]]

    mean, p50, p95, p99 = latency_stats(all_latencies)

    print("\n--- Summary ---")
    print(f"Duration (s)           : {elapsed:.3f}")
    print(f"Total requests         : {total_ok + total_errors}")
    print(f"  Successes            : {total_ok}")
    print(f"  Errors               : {total_errors}")
    print(f"Average throughput     : {(total_ok + total_errors) / elapsed:.1f} req/s\n")
    print("Latencies (ms):")
    print(f"  Mean   : {mean:.3f}")
    print(f"  P50    : {p50:.3f}")
    print(f"  P95    : {p95:.3f}")
    print(f"  P99    : {p99:.3f}")
    print("")


def main():
    parser = argparse.ArgumentParser(
        description="HTTP workload generator for filecache_server"
    )
    parser.add_argument("--host", default="localhost", help="Host of filecache_server")
    parser.add_argument(
        "-p", "--port", type=int, default=8000, help="Port of filecache_server"
    )
    parser.add_argument(
        "-n", "--num-items", type=int, default=1000, help="Total number of items"
    )
    parser.add_argument(
        "-c", "--ops", type=int, default=10000, help="Total number of GET operations"
    )
    parser.add_argument(
        "-t", "--threads", type=int, default=4, help="Number of concurrent workers"
    )
    parser.add_argument(
        "-a", "--skewness", type=float, default=0.86, help="Skewness (default = 0.86)"
    )
    args = parser.parse_args()

    asyncio.run(main_async(args))


if __name__ == "__main__":
    main()
