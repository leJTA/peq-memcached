#!/usr/bin/env python3
"""
generate_files.py
-----------------
Generates a set of binary files containing random data.
By default: 1000 files of 256 KiB each, named key_0000, key_0001, etc.

Usage:
    python generate_files.py --output-dir data --num-items 1000 --item-size-kib 256
"""

import os
import argparse
import random
import string
import xxhash


def generate_payload(size_bytes):
    """Generate a pseudo-random byte sequence of size size_bytes."""
    base = "".join(
        random.choices(string.ascii_letters + string.digits, k=128 * 1024)
    ).encode("ascii")
    reps = (size_bytes // len(base)) + 1
    return (base * reps)[:size_bytes]


def get_xxhash_prefix(key):
    return xxhash.xxh64(key).hexdigest()[:2].upper()


def main():
    parser = argparse.ArgumentParser(
        description="Generate random binary files named by key"
    )
    parser.add_argument("-o", "--output-dir", default="data", help="Output directory")
    parser.add_argument(
        "-n", "--num-items", type=int, default=1000, help="Number of files to generate"
    )
    parser.add_argument(
        "-k", "--item-size-kib", type=int, default=256, help="Size of each file in KiB"
    )
    parser.add_argument("-s", "--seed", type=int, default=0, help="Random seed (optional)")
    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)
    size_bytes = args.item_size_kib * 1024
    random.seed(args.seed)

    print(
        f"Generating {args.num_items} files of {args.item_size_kib} KiB in '{args.output_dir}'..."
    )

    for i in range(args.num_items):
        key = f"key_{i:06d}"
        subdir = os.path.join(args.output_dir, get_xxhash_prefix(key))
        if not os.path.exists(subdir):
            os.makedirs(subdir)
        filepath = os.path.join(subdir, key)
        payload = generate_payload(size_bytes)
        with open(filepath, "wb") as f:
            f.write(payload)

    total_mb = args.num_items * size_bytes / (1024 * 1024)
    print(f"Done. {args.num_items} files written ({total_mb:.2f} MiB total).")


if __name__ == "__main__":
    main()
