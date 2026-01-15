#!/usr/bin/env python3
import argparse
from pymemcache.client import Client
import os


def main():
    parser = argparse.ArgumentParser(description="Preload objects into Memcached.")
    parser.add_argument(
        "-c", "--count", type=int, required=True, help="Number of objects to insert."
    )
    parser.add_argument(
        "-k", "--size-kb", type=int, required=True, help="Size of each object in KB."
    )
    parser.add_argument(
        "--host", type=str, default="127.0.0.1", help="Memcached server address."
    )
    parser.add_argument(
        "-p", "--port", type=int, default=11211, help="Memcached server port."
    )
    args = parser.parse_args()

    client = Client((args.host, args.port))

    object_size = args.size_kb * 1024
    value = os.urandom(object_size)

    for i in range(args.count):
        key = f"obj_{i}"
        client.set(key, value)
        
    for i in range(args.count):
        key = f"obj_{i}"
        client.delete(key)

    print(
        f"Inserted {args.count} objects of {args.size_kb} KB "
        f"into Memcached at {args.host}:{args.port}."
    )


if __name__ == "__main__":
    main()
