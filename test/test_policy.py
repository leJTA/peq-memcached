from pymemcache.client import base

import random
import time
import string
import unittest
import subprocess

class ReplacementPolicyTestCase(unittest.TestCase):
   def setUp(self):
      # cache size = 8MB
      # item size = 512KB
      # hot buffer capacity ~ 1.6MB (20%)
      # warm buffer capacity ~ 3.2MB (40%)
      # cold buffer capacity ~ 3.2MB (40%)
      # history buffer capacity = 4 items

      # Launch memcached server
      self.proc_server = subprocess.Popen(["../memcached", "-m", "8m", "-vv"], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
      # Launch memcached client
      self.client = base.Client(('127.0.0.1', 11211))

      self.data = []
      for i in range(0, 16):
         d = ''.join(random.choices(string.ascii_letters + string.digits + string.punctuation, k=512*2**10))
         self.data.append(d)
      return super().setUp()
   
   def tearDown(self):
      self.client.close()
      try:
         self.proc_server.terminate()
         self.proc_server.wait(timeout=2)
      except subprocess.TimeoutExpired:
         self.proc_server.kill()
   
   
   def test_add_element(self):
      self.client.set("data0", self.data[0])
      
      res = self.client.get("data0")

      self.assertIsNotNone(res, "cache should contains 'data0'")
      self.assertEqual(res.decode(), self.data[0], "cache data integrity not preserved")

   def test_overflow_hot_buffer(self):
      self.client.set("data0", self.data[0])
      self.client.set("data1", self.data[1])
      self.client.set("data2", self.data[2]) # Hot buffer is now full
      self.client.set("data3", self.data[3])

      time.sleep(0.5)
      res = self.client.get("data0")

      self.assertIsNone(res, "'data0' should not remains in the cache")

   def test_move_to_warm(self):
      self.client.set("data0", self.data[0])
      self.client.set("data1", self.data[1])
      self.client.set("data2", self.data[2]) # Hot buffer is now full
      self.client.set("data3", self.data[3]) # push "data0" to history buffer
      self.client.set("data0", self.data[0]) # "data0" is now inserted in the warm buffer
      self.client.set("data4", self.data[4])
      self.client.set("data5", self.data[5])
      self.client.set("data6", self.data[6])
      self.client.set("data7", self.data[7])
      self.client.set("data8", self.data[8])
      

      res = self.client.get("data0")

      self.assertIsNotNone(res, "'data0' should remains in the cache")
      self.assertEqual(res.decode(), self.data[0], "cache data integrity not preserved")
   
   def test_overflow_history_buffer(self):
      self.client.set("data0", self.data[0])
      self.client.set("data1", self.data[1])
      self.client.set("data2", self.data[2]) # Hot buffer is now full
      self.client.set("data3", self.data[3]) # push "data0" to history buffer
      self.client.set("data4", self.data[4]) # push "data1" to history buffer
      self.client.set("data5", self.data[5]) # push "data2" to history buffer
      self.client.set("data6", self.data[6]) # push "data3" to history buffer
      self.client.set("data7", self.data[7]) # push "data4" to history buffer, data0 is evicted from the history buffer
      self.client.set("data0", self.data[0]) # "data0" is inserted to the hot buffer
      self.client.set("data8", self.data[8])
      self.client.set("data9", self.data[9])
      self.client.set("data10", self.data[10])
      self.client.set("data11", self.data[11]) # data0 is evicted

      time.sleep(0.5)
      res = self.client.get("data0")

      self.assertIsNone(res, "'data0' should not remain in the cache")

   def test_move_to_cold(self):
      self.client.set("data0", self.data[0])
      self.client.set("data1", self.data[1])
      self.client.set("data2", self.data[2]) # Hot buffer is now full
      self.client.set("data3", self.data[3]) # push "data0" to history buffer
      self.client.set("data0", self.data[0]) # insert "data0" in the warm buffer
      self.client.set("data4", self.data[4]) # push "data1" to history buffer
      self.client.set("data1", self.data[1]) # insert "data1" in the warm buffer
      self.client.set("data5", self.data[5]) # push "data2" to history buffer
      self.client.set("data2", self.data[2]) # insert "data2" in the warm buffer
      self.client.set("data6", self.data[6]) # push "data3" to history buffer
      self.client.set("data3", self.data[3]) # insert "data3" in the warm buffer
      self.client.set("data7", self.data[7]) # push "data4" to history buffer
      self.client.set("data4", self.data[4]) # insert "data4" in the warm buffer
      self.client.set("data8", self.data[8]) # push "data5" to history buffer
      self.client.set("data5", self.data[5]) # insert "data5" in the warm buffer, warm buffer is now full
      self.client.set("data9", self.data[9]) # push "data6" to history buffer
      self.client.set("data6", self.data[6]) # insert "data6" in the warm buffer, move "data0" to the cold buffer

      time.sleep(0.5)
      res = self.client.get("data0")

      self.assertIsNotNone(res, "'data0' should remains in the cache")

if __name__ == "__main__":
   unittest.main()