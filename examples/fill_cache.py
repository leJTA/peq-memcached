from pymemcache.client import base

import random
import string

def main():
   client = base.Client(('127.0.0.1', 11211))
   data = ''.join(random.choices(string.ascii_letters + string.digits + string.punctuation, k=512*2**10))
   for i in range(1, 128):
      input(f"press ENTER to send data (id = {i})")
      client.set(f"data{i}", data)
   
if __name__ == "__main__":
   main()


