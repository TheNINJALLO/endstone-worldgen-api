import sys,unittest
sys.path.insert(0,"python")
from endstone_worldgen import *
class TestWorldGen(unittest.TestCase):
 def test_deterministic(self):
  s=GenerationScheduler(2);c1=GenerationContext(99,"overworld",ChunkPos(2,3));c2=GenerationContext(99,"overworld",ChunkPos(2,3));g=FlatGenerator();a=s.generate(c1,g).result();b=s.generate(c2,g).result();self.assertEqual(a.fingerprint(),b.fingerprint());self.assertEqual(a.get(0,64,0),2);s.close()
if __name__=="__main__":unittest.main()
